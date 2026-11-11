#include "CWatchlistService.h"
#include "../../system/CTraderCenter.h"
#include <sstream>
#include <utility>

void CWatchlistService::SetTraderCenter(CTraderCenter* pCenter)
{
	m_pTraderCenter = pCenter;
	if ((nullptr == pCenter) || m_bInitialized)
	{
		return;
	}
	m_bInitialized = true;
	pCenter->RegisterStateHandler([this](SessionState state, const std::string&)
	{
		if (SessionState::Ready == state)
		{
			Query();
		}
		else
		{
			{
				std::lock_guard<std::mutex> lock(m_mtx);
				m_pending.clear();
			}
			CWatchlistEvent event;
			event.m_bReset = true;
			m_events.Notify(event);
		}
	});
}

_TyCallbackId CWatchlistService::Subscribe(std::function<void(const CWatchlistEvent&)>&& handler)
{
	return m_events.Subscribe(std::move(handler));
}

void CWatchlistService::Unsubscribe(_TyCallbackId token)
{
	m_events.Unsubscribe(token);
}

bool CWatchlistService::Query()
{
	return Send(_TyRequest::WATCHLIST_QUERY, {});
}

bool CWatchlistService::Add(const std::vector<std::string>& securities)
{
	return Send(_TyRequest::WATCHLIST_ADD, securities);
}

bool CWatchlistService::Remove(const std::vector<std::string>& securities)
{
	return Send(_TyRequest::WATCHLIST_REMOVE, securities);
}

bool CWatchlistService::Send(_TyRequest type, const std::vector<std::string>& securities)
{
	if (nullptr == m_pTraderCenter)
	{
		return false;
	}
	CRequest request;
	request.SetType(type);
	std::string strList;
	for (const std::string& strSecurity : securities)
	{
		if (!strList.empty())
		{
			strList += '\n';
		}
		strList += strSecurity;
	}
	request.SetExtraData("securities", strList);
	{
		std::lock_guard<std::mutex> lock(m_mtx);
		m_pending.emplace(request.GetId(), type);
	}
	if (m_pTraderCenter->SendRequest(request))
	{
		return true;
	}
	std::lock_guard<std::mutex> lock(m_mtx);
	m_pending.erase(request.GetId());
	return false;
}

bool CWatchlistService::OnRequestReply(const CRequest& response)
{
	{
		std::lock_guard<std::mutex> lock(m_mtx);
		const auto mIter = m_pending.find(response.GetId());
		if ((m_pending.end() == mIter) || (response.GetType() != mIter->second))
		{
			return false;
		}
		m_pending.erase(mIter);
	}
	CWatchlistEvent event;
	event.m_type = response.GetType();
	std::optional<std::pair<int, std::string>> error = response.GetErrorInfo();
	if (error.has_value())
	{
		event.m_strError = error->second;
	}
	else
	{
		std::istringstream stream(response.GetExtraData("securities"));
		std::string strSecurity;
		while (std::getline(stream, strSecurity))
		{
			event.m_securities.emplace_back(strSecurity);
		}
	}
	m_events.Notify(event);
	return true;
}
