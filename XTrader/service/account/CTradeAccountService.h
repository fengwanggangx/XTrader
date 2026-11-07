#pragma once
#include "../../basic/TMessagePump.h"
#include "../../request/request.h"
#include "../../request/request.pb.h"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
class CTraderCenter;

class CTradeAccountService final
{
	friend class CTraderCenter;

public:
	CTradeAccountService();
	~CTradeAccountService();
	CTradeAccountService(const CTradeAccountService& arg) = delete;
	CTradeAccountService& operator=(const CTradeAccountService& arg) = delete;

	void SetTraderCenter(CTraderCenter* pCenter);
	bool IsAuthenticated() const noexcept;

	struct CResult
	{
		bool m_bSessionReset{ false };
		std::uint64_t m_requestId{ 0 };
		_TyAccountIndex m_accountIndex{};
		std::uint64_t m_generation{ 0 };
		_TyRequest m_type{ _TyRequest::UNKNOWN };
		std::string m_error;
		request::RequestData m_data;
	};
	using _TyHandler = std::function<void(const CResult&)>;
	void Initialize();
	_TyCallbackId Subscribe(_TyHandler&& handler);
	void Unsubscribe(_TyCallbackId token);
	std::uint64_t Send(_TyRequest type, const _TyAccountIndex& account = {}, const std::unordered_map<std::string, std::string>& extra = {});
	std::vector<request::TradeAccountRecord> GetAccounts() const;
	bool IsSupported() const;
	std::uint64_t GetGeneration() const;

private:
	bool SendRequest(const CRequest& request);
	bool OnResponse(const CRequest& response);
	struct CPending
	{
		_TyAccountIndex m_accountIndex{};
		_TyRequest m_type{ _TyRequest::UNKNOWN };
		std::chrono::steady_clock::time_point m_deadline;
	};
	std::once_flag m_initializeFlag;
	mutable std::mutex m_mtx_state;
	std::unordered_map<std::uint64_t, CPending> m_pending;
	std::vector<request::TradeAccountRecord> m_accounts;
	bool m_supported{ true };
	std::uint64_t m_generation{ 0 };
	TMessagePump<CResult> m_results;
	std::condition_variable m_wake;
	std::jthread m_timeouts;
	// Borrowed from CBootLoader; detached after network callbacks drain.
	CTraderCenter* m_pTraderCenter{ nullptr };
};
