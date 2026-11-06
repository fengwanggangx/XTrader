#include "CStrategyService.h"

#include "../../system/CSession.h"

#include <algorithm>
#include <cctype>
#include <utility>
#include <charconv>
#include <set>
#include "../../request/MarketTypes.h"

CStrategyService::CStrategyService() = default;

CStrategyService::~CStrategyService() = default;

void CStrategyService::Initialize()
{
	std::call_once(m_initializeFlag, [this]()
	{
		CSession::InstanceRef().RegisterResponseHandler(std::bind_front(&CStrategyService::OnRequestReply, this));
		CSession::InstanceRef().RegisterStateHandler([this](SessionState state, const std::string&)
		{
			Invalidate(true);
			if (SessionState::Ready == state)
			{
				QueryStrategies();
				QueryRuntime();
			}
		});
	});
}

_TyCallbackId CStrategyService::AddQueryHandler(_TyQueryHandler&& handler)
{
	return m_queryPump.Subscribe([handler = std::move(handler)](const std::pair<_TyStrategyList, std::string>& result)
	{
		handler(result.first, result.second);
	});
}

void CStrategyService::RemoveQueryHandler(_TyCallbackId nToken)
{
	m_queryPump.Unsubscribe(nToken);
}

void CStrategyService::SetQueryHandler(_TyQueryHandler&& handler)
{
	std::lock_guard<std::mutex> lock(m_mtx_handlers);
	m_queryHandler = std::move(handler);
}

void CStrategyService::SetOperationHandler(_TyOperationHandler&& handler)
{
	std::lock_guard<std::mutex> lock(m_mtx_handlers);
	m_operationHandler = std::move(handler);
}

_TyCallbackId CStrategyService::AddOperationHandler(_TyOperationResultHandler&& handler)
{
	return m_operationPump.Subscribe(std::move(handler));
}

void CStrategyService::RemoveOperationHandler(_TyCallbackId nToken)
{
	m_operationPump.Unsubscribe(nToken);
}

_TyCallbackId CStrategyService::AddRuntimeHandler(_TyRuntimeHandler&& handler)
{
	return m_runtimePump.Subscribe([handler = std::move(handler)](const std::pair<_TyRuntimeMap, std::string>& result)
	{
		handler(result.first, result.second);
	});
}

void CStrategyService::RemoveRuntimeHandler(_TyCallbackId nToken)
{
	m_runtimePump.Unsubscribe(nToken);
}

void CStrategyService::Invalidate(bool bResetPending)
{
	std::unique_lock<std::shared_mutex> lock(m_mtx_strategies);
	++m_generation;
	m_bCacheValid = false;
	m_bRuntimeValid = false;
	m_configError = "配置等待刷新";
	m_runtimeError = "运行状态等待刷新";
	m_bQueryDirty = !bResetPending && (0 != m_queryRequestId);
	m_bRuntimeDirty = !bResetPending && (0 != m_runtimeRequestId);
	if (bResetPending)
	{
		m_queryRequestId = 0;
		m_runtimeRequestId = 0;
	}
}

bool CStrategyService::QueryStrategies()
{
	Initialize();
	CRequest req = request::QueryStrategies();
	{
		std::unique_lock<std::shared_mutex> lock(m_mtx_strategies);
		if (0 != m_queryRequestId)
		{
			return true;
		}
		m_queryRequestId = req.GetId();
		m_bQueryDirty = false;
	}
	if (CSession::InstanceRef().SendRequest(req))
	{
		return true;
	}
	std::unique_lock<std::shared_mutex> lock(m_mtx_strategies);
	if (req.GetId() == m_queryRequestId)
	{
		m_queryRequestId = 0;
	}
	return false;
}

bool CStrategyService::QueryRuntime()
{
	Initialize();
	CRequest req = request::QueryStrategyRuntime();
	{
		std::unique_lock<std::shared_mutex> lock(m_mtx_strategies);
		if (0 != m_runtimeRequestId)
		{
			return true;
		}
		m_runtimeRequestId = req.GetId();
		m_bRuntimeDirty = false;
	}
	if (CSession::InstanceRef().SendRequest(req))
	{
		return true;
	}
	std::unique_lock<std::shared_mutex> lock(m_mtx_strategies);
	if (req.GetId() == m_runtimeRequestId)
	{
		m_runtimeRequestId = 0;
	}
	return false;
}

bool CStrategyService::IsRuntimeValid() const
{
	std::shared_lock<std::shared_mutex> lock(m_mtx_strategies);
	return m_bRuntimeValid;
}

std::uint64_t CStrategyService::GetGeneration() const
{
	std::shared_lock<std::shared_mutex> lock(m_mtx_strategies);
	return m_generation;
}

std::uint64_t CStrategyService::GetSnapshot(_TyStrategyList& strategies, _TyRuntimeMap& runtimes, std::string& configError, std::string& runtimeError) const
{
	std::shared_lock<std::shared_mutex> lock(m_mtx_strategies);
	strategies = m_strategies;
	runtimes = m_runtimes;
	configError = m_configError;
	runtimeError = m_runtimeError;
	return m_generation;
}

bool CStrategyService::CanModifyStrategy(std::uint64_t id) const
{
	std::shared_lock<std::shared_mutex> lock(m_mtx_strategies);
	if (!m_bCacheValid || !m_bRuntimeValid)
	{
		return false;
	}
	for (const auto& strategy : m_strategies)
	{
		if (id == strategy.strategy_id())
		{
			const auto mIter = m_runtimes.find(id);
			if (m_runtimes.end() == mIter)
			{
				return !strategy.enabled();
			}
			return (0 == mIter->second.m_activeOrders) && ((1 == mIter->second.m_state) || (7 == mIter->second.m_state));
		}
	}
	return false;
}

std::uint64_t CStrategyService::AddStrategy(const request::StrategyInfo& strategy)
{
	std::string error;
	if (!ValidateStrategy(strategy, error))
	{
		return 0;
	}
	Initialize();
	CRequest req = request::AddStrategy(strategy);
	return CSession::InstanceRef().SendRequest(req) ? req.GetId() : 0;
}

std::uint64_t CStrategyService::ModifyStrategy(const request::StrategyInfo& strategy)
{
	std::string error;
	if ((0 == strategy.strategy_id()) || !CanModifyStrategy(strategy.strategy_id()) || !ValidateStrategy(strategy, error))
	{
		return 0;
	}
	Initialize();
	CRequest req = request::ModifyStrategy(strategy);
	return CSession::InstanceRef().SendRequest(req) ? req.GetId() : 0;
}

std::uint64_t CStrategyService::DeleteStrategy(std::uint64_t id)
{
	if (0 == id)
	{
		return 0;
	}
	Initialize();
	CRequest req = request::DeleteStrategy(id);
	return CSession::InstanceRef().SendRequest(req) ? req.GetId() : 0;
}

std::uint64_t CStrategyService::ControlStrategy(_TyRequest type, std::uint64_t id)
{
	if ((0 == id) || ((_TyRequest::STRATEGY_START != type) && (_TyRequest::STRATEGY_PAUSE != type) && (_TyRequest::STRATEGY_STOP != type)))
	{
		return 0;
	}
	Initialize();
	CRequest req = request::ControlStrategy(type, id);
	return CSession::InstanceRef().SendRequest(req) ? req.GetId() : 0;
}

CStrategyService::_TyStrategyList CStrategyService::GetStrategies() const
{
	std::shared_lock<std::shared_mutex> lock(m_mtx_strategies);
	return m_strategies;
}

bool CStrategyService::FindStrategy(std::uint64_t id, request::StrategyInfo& result) const
{
	std::shared_lock<std::shared_mutex> lock(m_mtx_strategies);
	for (const auto& strategy : m_strategies)
	{
		if (id == strategy.strategy_id())
		{
			result = strategy;
			return true;
		}
	}
	return false;
}

bool CStrategyService::IsCacheValid() const
{
	std::shared_lock<std::shared_mutex> lock(m_mtx_strategies);
	return m_bCacheValid;
}

bool CStrategyService::ValidateStrategy(const request::StrategyInfo& strategy, std::string& error)
{
	error.clear();
	std::function<bool(const std::string&)> isBlank = [](const std::string& value)
	{
		return value.empty() || std::all_of(value.begin(), value.end(), [](unsigned char character)
		{
			return 0 != std::isspace(character);
		});
	};
	if (isBlank(strategy.strategy_name()) || isBlank(strategy.strategy_type()))
	{
		error = "策略名称和类型不能为空";
		return false;
	}
	if (0 == strategy.subscriptions_size())
	{
		error = "至少需要一个订阅标的";
		return false;
	}
	for (const auto& subscription : strategy.subscriptions())
	{
		if (isBlank(subscription.security()) || isBlank(subscription.exchange()) || isBlank(subscription.channel()))
		{
			error = "订阅的证券、交易所和通道不能为空";
			return false;
		}
	}
	if (!IsAccountValid(AccountIndex(strategy)) || ("moving_average" != strategy.strategy_type()))
	{
		error = "请选择有效交易账户及支持的策略类型 moving_average";
		return false;
	}
	if (1000000 < strategy.event_queue_limit())
	{
		error = "事件队列上限不能超过 1000000";
		return false;
	}
	std::size_t shortWindow = 5;
	std::size_t longWindow = 20;
	for (const auto& [key, value] : strategy.parameters())
	{
		std::size_t parsed = 0;
		std::from_chars_result result = std::from_chars(value.data(), value.data() + value.size(), parsed);
		if ((("short_window" != key) && ("long_window" != key)) ||
			(std::errc() != result.ec) ||
			(value.data() + value.size() != result.ptr) ||
			(0 == parsed) || (100000 < parsed))
		{
			error = "参数 " + key +
					" 无效：仅支持 short_window、long_window，值必须为 "
					"1～100000 的整数";
			return false;
		}
		("short_window" == key ? shortWindow : longWindow) = parsed;
	}
	if (shortWindow >= longWindow)
	{
		error = "short_window 必须小于 long_window";
		return false;
	}
	std::set<std::tuple<std::string, std::string, std::string>> subscriptions;
	bool hasQuote = false;
	for (const auto& subscription : strategy.subscriptions())
	{
		if ((Exchange::unknown == ParseMarket(subscription.exchange())) || (("quote" != subscription.channel()) && ("depth" != subscription.channel())))
		{
			error = "订阅交易所无效，通道仅支持 quote、depth";
			return false;
		}
		if (!subscriptions.emplace(subscription.security(), subscription.exchange(), subscription.channel()).second)
		{
			error = "订阅标的重复";
			return false;
		}
		hasQuote = hasQuote || ("quote" == subscription.channel());
	}
	if (!hasQuote)
	{
		error = "均线策略至少需要一个 quote 订阅";
		return false;
	}
	return true;
}

bool CStrategyService::OnRequestReply(const CRequest& response)
{
	_TyRequest type = response.GetType();
	std::optional<std::pair<int, std::string>> errorInfo = response.GetErrorInfo();
	std::string strError = (errorInfo.has_value() && (0 != errorInfo->first)) ? (errorInfo->second.empty() ? "策略请求失败" : errorInfo->second) : std::string();
	const _TyReqData& message = response.GetData();
	if (_TyRequest::STRATEGY_QUERY == type)
	{
		std::unique_lock<std::shared_mutex> cacheLock(m_mtx_strategies);
		if ((0 == response.GetId()) || (response.GetId() != m_queryRequestId))
		{
			return false;
		}
		m_queryRequestId = 0;
		if (m_bQueryDirty)
		{
			m_bQueryDirty = false;
			cacheLock.unlock();
			QueryStrategies();
			return true;
		}
		_TyStrategyList strategies;
		if (strError.empty() && !message.has_strategy_list())
		{
			strError = "策略查询响应缺少 strategy_list";
		}
		if (strError.empty() && message.has_strategy_list())
		{
			strategies.reserve(message.strategy_list().strategies_size());
			for (const request::StrategyInfo& strategy : message.strategy_list().strategies())
			{
				strategies.emplace_back(strategy);
			}
		}
		if (strError.empty())
		{
			m_strategies = strategies;
			m_bCacheValid = true;
		}
		else
		{
			m_bCacheValid = false;
		}
		if (!strError.empty())
		{
			strategies = m_strategies;
		}
		m_configError = strError;
		cacheLock.unlock();
		_TyQueryHandler handler;
		{
			std::lock_guard<std::mutex> lock(m_mtx_handlers);
			handler = m_queryHandler;
		}
		if (handler)
		{
			handler(strategies, strError);
		}
		m_queryPump.Notify({ std::move(strategies), std::move(strError) });
		return true;
	}

	if (_TyRequest::STRATEGY_RUNTIME_QUERY == type)
	{
		std::unique_lock<std::shared_mutex> cacheLock(m_mtx_strategies);
		if ((0 == response.GetId()) || (response.GetId() != m_runtimeRequestId))
		{
			return false;
		}
		m_runtimeRequestId = 0;
		if (m_bRuntimeDirty)
		{
			m_bRuntimeDirty = false;
			cacheLock.unlock();
			QueryRuntime();
			return true;
		}
		_TyRuntimeMap runtimes;
		if (strError.empty() && (!message.has_strategy_runtime_list() || (10000 < message.strategy_runtime_list().runtimes_size())))
		{
			strError = "运行快照列表无效";
		}
		for (const auto& record : message.strategy_runtime_list().runtimes())
		{
			if (!strError.empty())
			{
				break;
			}
			if ((0 == record.strategy_id()) || (0 > record.state()) || (8 < record.state()))
			{
				strError = "运行快照字段无效";
				break;
			}
			CRuntimeSnapshot snapshot;
			snapshot.m_state = record.state();
			snapshot.m_marketAvailable = record.market_available();
			snapshot.m_queueLength = record.queue_length();
			snapshot.m_activeOrders = record.active_orders();
			snapshot.m_lastError = record.last_error();
			if (!runtimes.emplace(record.strategy_id(), std::move(snapshot)).second)
			{
				strError = "运行快照含重复策略";
			}
		}
		m_bRuntimeValid = strError.empty();
		m_runtimeError = strError;
		if (m_bRuntimeValid)
		{
			m_runtimes = runtimes;
		}
		cacheLock.unlock();
		m_runtimePump.Notify({ std::move(runtimes), std::move(strError) });
		return true;
	}

	if ((_TyRequest::STRATEGY_ADD != type) && (_TyRequest::STRATEGY_MODIFY != type) && (_TyRequest::STRATEGY_DELETE != type) && (_TyRequest::STRATEGY_START != type) && (_TyRequest::STRATEGY_PAUSE != type) && (_TyRequest::STRATEGY_STOP != type))
	{
		return false;
	}
	request::StrategyInfo strategy;
	if (message.has_strategy())
	{
		strategy.CopyFrom(message.strategy());
	}
	if (strError.empty())
	{
		Invalidate();
	}
	_TyOperationHandler handler;
	{
		std::lock_guard<std::mutex> lock(m_mtx_handlers);
		handler = m_operationHandler;
	}
	if (handler)
	{
		handler(type, strError.empty(), strategy, strError);
	}
	m_operationPump.Notify({ response.GetId(), type, strError.empty(), strError });
	if (strError.empty())
	{
		QueryStrategies();
		QueryRuntime();
	}
	return true;
}
