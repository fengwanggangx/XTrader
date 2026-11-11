#ifndef XTRADER_SERVICE_CWATCHLISTSERVICE_H
#define XTRADER_SERVICE_CWATCHLISTSERVICE_H

#include "../../basic/TMessagePump.h"
#include "../../request/request.h"
#include <mutex>
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
	bool Add(const std::vector<std::string>& securities);
	bool Remove(const std::vector<std::string>& securities);
	bool OnRequestReply(const CRequest& response);

private:
	bool Send(_TyRequest type, const std::vector<std::string>& securities);

	CTraderCenter* m_pTraderCenter{ nullptr };
	TMessagePump<CWatchlistEvent> m_events;
	std::mutex m_mtx;
	std::unordered_map<_TyRequestId, _TyRequest> m_pending;
	bool m_bInitialized{ false };
};

#endif
