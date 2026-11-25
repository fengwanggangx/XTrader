#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

template <class _TyData>
struct QNode
{
	std::atomic<std::size_t> m_sequence{ 0 };
	alignas(_TyData) std::byte m_addr[sizeof(_TyData)];

	_TyData* addr() noexcept
	{
		return reinterpret_cast<_TyData*>(m_addr);
	}

	_TyData* value() noexcept
	{
		return std::launder(reinterpret_cast<_TyData*>(m_addr));
	}
};

template <typename _TyData, std::size_t _Capacity>
class TQueue
{
	static_assert(_Capacity >= 2);
	static_assert((_Capacity & (_Capacity - 1)) == 0, "Capacity must be a power of two");

	// 已经抢占槽位后不能通过异常取消，否则会永久留下空洞
	static_assert(std::is_nothrow_destructible_v<_TyData>);

	using _TyNode = QNode<_TyData>;

	static constexpr std::size_t _Mask = _Capacity - 1;

public:
	TQueue()
	{
		for (std::size_t i = 0; i < _Capacity; ++i)
		{
			m_pool[i].m_sequence.store(i, std::memory_order_relaxed);
		}
	}

	TQueue(const TQueue&) = delete;
	TQueue& operator=(const TQueue&) = delete;

	~TQueue()
	{
		while (destroy_one())
		{
		}
	}

	template <typename _Ty>
		requires std::is_nothrow_constructible_v<_TyData, _Ty&&>
	[[nodiscard]] bool push(_Ty&& v) noexcept
	{
		return emplace(std::forward<_Ty>(v));
	}

	template <typename... _Types>
		requires std::is_nothrow_constructible_v<_TyData, _Types&&...>
	[[nodiscard]] bool emplace(_Types&&... args) noexcept
	{
		std::size_t pos = m_pos_push.load(std::memory_order_relaxed);
		_TyNode* pNode = nullptr;
		for (;;)
		{
			pNode = &m_pool[pos & _Mask];
			const std::size_t sequence = pNode->m_sequence.load(std::memory_order_acquire);
			const auto difference = static_cast<std::intptr_t>(sequence) - static_cast<std::intptr_t>(pos);
			if (0 == difference)
			{
				if (m_pos_push.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed, std::memory_order_relaxed))
				{
					break;
				}
			}
			else if (difference < 0)
			{
				return false;
			}
			else
			{
				pos = m_pos_push.load(std::memory_order_relaxed);
			}
		}

		std::construct_at(pNode->addr(), std::forward<_Types>(args)...);
		pNode->m_sequence.store(pos + 1, std::memory_order_release);
		return true;
	}

	[[nodiscard]] bool pop(_TyData& v) noexcept(std::is_nothrow_move_assignable_v<_TyData>)
	{
		static_assert(std::is_nothrow_move_assignable_v<_TyData>);

		std::size_t pos = m_pos_pop.load(std::memory_order_relaxed);
		_TyNode* pNode = nullptr;
		for (;;)
		{
			pNode = &m_pool[pos & _Mask];
			const std::size_t sequence = pNode->m_sequence.load(std::memory_order_acquire);
			const auto difference = static_cast<std::intptr_t>(sequence) - static_cast<std::intptr_t>(pos + 1);
			if (0 == difference)
			{
				if (m_pos_pop.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed, std::memory_order_relaxed))
				{
					break;
				}
			}
			else if (difference < 0)
			{
				return false;
			}
			else
			{
				pos = m_pos_pop.load(std::memory_order_relaxed);
			}
		}

		_TyData* value = pNode->value();
		v = std::move(*value);
		std::destroy_at(value);

		pNode->m_sequence.store(pos + _Capacity, std::memory_order_release);
		return true;
	}

private:
	bool destroy_one() noexcept
	{
		const std::size_t pos = m_pos_pop.load(std::memory_order_relaxed);
		_TyNode& node = m_pool[pos & _Mask];

		if (node.m_sequence.load(std::memory_order_acquire) != pos + 1)
		{
			return false;
		}

		std::destroy_at(node.value());

		node.m_sequence.store(pos + _Capacity, std::memory_order_release);
		m_pos_pop.store(pos + 1, std::memory_order_relaxed);
		return true;
	}

private:
	// 将热点变量分到不同 cache line
	alignas(64) std::atomic<std::size_t> m_pos_push{ 0 };
	alignas(64) std::atomic<std::size_t> m_pos_pop{ 0 };
	alignas(64) std::array<_TyNode, _Capacity> m_pool;
};
