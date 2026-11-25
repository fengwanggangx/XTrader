#include "CWatchlistService.h"
#include "../../system/CTraderCenter.h"
#include <algorithm>
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
	m_timeouts = std::jthread([this](std::stop_token stop)
	{
		while (!stop.stop_requested())
		{
			std::vector<CRequest> expired;
			{
				std::unique_lock<std::mutex> lock(m_mtx);
				m_wake.wait_for(lock, stop, std::chrono::seconds(1), []()
				{
					return false;
				});
				if (stop.stop_requested())
				{
					return;
				}
				std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
				for (const auto& [id, pending] : m_pending)
				{
					if (pending.m_deadline <= now)
					{
						CRequest& response = expired.emplace_back();
						response.SetId(id);
						response.SetType(pending.m_type);
						response.SetErrorInfo(1, "自选请求超时，请刷新确认操作结果");
					}
				}
			}
			for (const CRequest& response : expired)
			{
				OnRequestReply(response);
			}
		}
	});
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
				m_snapshot.reset();
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

std::optional<bool> CWatchlistService::Contains(const std::string& security) const
{
	std::lock_guard<std::mutex> lock(m_mtx);
	if (!m_snapshot.has_value())
	{
		return std::nullopt;
	}
	return m_snapshot->end() != std::ranges::find(*m_snapshot, security);
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
		m_pending.emplace(request.GetId(), CPending{ type, std::chrono::steady_clock::now() + std::chrono::seconds(20) });
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
	CWatchlistEvent event;
	{
		std::lock_guard<std::mutex> lock(m_mtx);
		const auto mIter = m_pending.find(response.GetId());
		if ((m_pending.end() == mIter) || (response.GetType() != mIter->second.m_type))
		{
			return false;
		}
		m_pending.erase(mIter);
		event.m_type = response.GetType();
		std::optional<std::pair<int, std::string>> error = response.GetErrorInfo();
		if (error.has_value())
		{
			event.m_strError = error->second.empty() ? "自选操作失败" : error->second;
		}
		else
		{
			std::istringstream stream(response.GetExtraData("securities"));
			std::string strSecurity;
			while (std::getline(stream, strSecurity))
			{
				event.m_securities.emplace_back(strSecurity);
			}
			m_snapshot = event.m_securities;
		}
	}
	m_events.Notify(event);
	return true;
}
