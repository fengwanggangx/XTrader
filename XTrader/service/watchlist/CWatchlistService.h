#ifndef XTRADER_SERVICE_CWATCHLISTSERVICE_H
#define XTRADER_SERVICE_CWATCHLISTSERVICE_H

#include "../../basic/TMessagePump.h"
#include "../../request/request.h"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class CTraderCenter;

struct CWatchlistEvent
{
	_TyRequest m_type{ _TyRequest::WATCHLIST_QUERY };
	std::vector<std::string> m_securities;
	std::string m_strError;
	bool m_bReset{ false };
};

class CWatchlistService final
{
public:
	void SetTraderCenter(CTraderCenter* pCenter);
	_TyCallbackId Subscribe(std::function<void(const CWatchlistEvent&)>&& handler);
	void Unsubscribe(_TyCallbackId token);
	bool Query();
	std::optional<bool> Contains(const std::string& security) const;
	bool Add(const std::vector<std::string>& securities);
	bool Remove(const std::vector<std::string>& securities);
	bool OnRequestReply(const CRequest& response);

private:
	bool Send(_TyRequest type, const std::vector<std::string>& securities);

	CTraderCenter* m_pTraderCenter{ nullptr };
	TMessagePump<CWatchlistEvent> m_events;
	mutable std::mutex m_mtx;
	std::optional<std::vector<std::string>> m_snapshot;
	struct CPending
	{
		_TyRequest m_type;
		std::chrono::steady_clock::time_point m_deadline;
	};
	std::unordered_map<_TyRequestId, CPending> m_pending;
	bool m_bInitialized{ false };
	std::condition_variable_any m_wake;
	std::jthread m_timeouts;
};

#endif
