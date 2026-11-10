#include "CAccountService.h"
#include "../../system/CTraderCenter.h"
#include <algorithm>
CAccountService::CAccountService() = default;
CAccountService::~CAccountService()
{
	m_timeouts.request_stop();
	m_wake.notify_all();
	if (m_timeouts.joinable())
	{
		m_timeouts.join();
	}
}
void CAccountService::Initialize()
{
	if (nullptr == m_pTraderCenter)
	{
		return;
	}
	std::call_once(m_initializeFlag, [this]()
	{
		m_pTraderCenter->RegisterStateHandler(
			[this](SessionState, const std::string&)
		{
			CResult result;
			result.m_bSessionReset = true;
			{
				std::lock_guard<std::mutex> lock(m_mtx_state);
				m_pending.clear();
				m_accounts.clear();
				m_supported = true;
				result.m_generation = ++m_generation;
			}
			m_results.Notify(result);
		});
		m_timeouts = std::jthread([this](std::stop_token stop)
		{
			while (!stop.stop_requested())
			{
				std::vector<CRequest> expired;
				{
					std::unique_lock<std::mutex> lock(m_mtx_state);
					m_wake.wait_for(lock, std::chrono::seconds(1),
						[&stop]()
					{
						return stop.stop_requested();
					});
					if (stop.stop_requested())
					{
						return;
					}
					for (const auto& item : m_pending)
					{
						if (item.second.m_deadline <= std::chrono::steady_clock::now())
						{
							CRequest response;
							response.SetId(item.first);
							response.SetType(item.second.m_type);
							response.SetErrorInfo(1203, "账户请求超时");
							expired.emplace_back(std::move(response));
						}
					}
				}
				for (const auto& response : expired)
				{
					OnRequestReply(response);
				}
			}
		});
	});
}
_TyCallbackId CAccountService::Subscribe(_TyHandler&& handler)
{
	Initialize();
	return m_results.Subscribe(std::move(handler));
}
void CAccountService::Unsubscribe(_TyCallbackId token)
{
	m_results.Unsubscribe(token);
}
std::vector<request::TradeAccountRecord>
CAccountService::GetAccounts() const
{
	std::lock_guard<std::mutex> lock(m_mtx_state);
	return m_accounts;
}
bool CAccountService::IsSupported() const
{
	std::lock_guard<std::mutex> lock(m_mtx_state);
	return m_supported;
}
std::uint64_t CAccountService::GetGeneration() const
{
	std::lock_guard<std::mutex> lock(m_mtx_state);
	return m_generation;
}
std::uint64_t CAccountService::Send(
	_TyRequest type, const _TyAccountIndex& account,
	const std::unordered_map<std::string, std::string>& extra)
{
	Initialize();
	if (!IsAuthenticated())
	{
		return 0;
	}
	if ((_TyRequest::TRADE_ACCOUNT_LIST_QUERY == type) &&
		((IsAccountValid(account)) || !extra.empty()))
	{
		return 0;
	}
	CRequest req;
	req.SetType(type);
	switch (type)
	{
	case _TyRequest::SUPPORTED_BROKER_QUERY:
	case _TyRequest::TRADE_ACCOUNT_LOGIN:
	case _TyRequest::TRADE_ACCOUNT_LIST_QUERY:
	case _TyRequest::TRADE_ACCOUNT_DISCONNECT:
	case _TyRequest::TRADE_FUNDS_QUERY:
		break;
	default:
		return 0;
	}
	if (_TyRequest::TRADE_FUNDS_QUERY == type &&
		(!IsAccountValid(account) || extra.contains("account_ids")))
	{
		return 0;
	}
	if (IsAccountValid(account))
	{
		WriteAccountIdentity(account, req);
	}
	for (const auto& item : extra)
	{
		req.SetExtraData(item.first, item.second);
	}
	{
		std::lock_guard<std::mutex> lock(m_mtx_state);
		if (!m_supported)
		{
			return 0;
		}
		for (const auto& item : m_pending)
		{
			if ((type == item.second.m_type) &&
				(account == item.second.m_accountIndex))
			{
				return 0;
			}
		}
		m_pending.emplace(req.GetId(), CPending{ account, type,
										   std::chrono::steady_clock::now() +
											   std::chrono::seconds(_TyRequest::TRADE_ACCOUNT_LOGIN == type ? 75 : 20) });
	}
	if (SendRequest(req))
	{
		return req.GetId();
	}
	std::lock_guard<std::mutex> lock(m_mtx_state);
	m_pending.erase(req.GetId());
	return 0;
}
bool CAccountService::OnRequestReply(const CRequest& response)
{
	CResult result;
	{
		std::lock_guard<std::mutex> lock(m_mtx_state);
		const auto mIter = m_pending.find(response.GetId());
		if (m_pending.end() == mIter)
		{
			return false;
		}
		if (response.GetType() != mIter->second.m_type)
		{
			return false;
		}
		result.m_accountIndex = mIter->second.m_accountIndex;
		result.m_type = mIter->second.m_type;
		result.m_requestId = response.GetId();
		result.m_generation = m_generation;
		m_pending.erase(mIter);
		result.m_data.CopyFrom(response.GetData());
		std::optional<std::pair<int, std::string>> error = response.GetErrorInfo();
		if (error.has_value())
		{
			result.m_error = error->second.empty() ? "账户请求失败，错误码：" +
														 std::to_string(error->first)
												   : error->second;
			if ((_TyRequest::TRADE_ACCOUNT_LIST_QUERY == result.m_type) &&
				((std::string::npos != result.m_error.find("unsupported")) ||
					(std::string::npos != result.m_error.find("unknown"))))
			{
				m_supported = false;
				m_accounts.clear();
			}
		}
		else if (_TyRequest::TRADE_FUNDS_QUERY == result.m_type)
		{
			if (!result.m_data.has_counter_reply() ||
				!result.m_data.counter_reply().has_funds() ||
				result.m_accountIndex !=
					AccountIndex(result.m_data.counter_reply()) ||
				result.m_accountIndex !=
					AccountIndex(result.m_data.counter_reply().funds()))
			{
				result.m_error = "账户响应缺少指定账户信息";
			}
			else if (0 != result.m_data.counter_reply().error_code())
			{
				result.m_error = result.m_data.counter_reply().error_message();
			}
		}
		else if (_TyRequest::TRADE_ACCOUNT_LOGIN == result.m_type ||
				 _TyRequest::TRADE_ACCOUNT_DISCONNECT == result.m_type)
		{
			std::erase_if(m_pending, [](const auto& item)
			{
				return _TyRequest::TRADE_ACCOUNT_LIST_QUERY == item.second.m_type ||
					   _TyRequest::TRADE_FUNDS_QUERY == item.second.m_type;
			});
		}
		else if (_TyRequest::TRADE_ACCOUNT_LIST_QUERY == result.m_type)
		{
			if (!result.m_data.has_account_list())
			{
				result.m_error = "当前 XEngine 不支持账户管理";
				m_supported = false;
				m_accounts.clear();
			}
			else
			{
				m_accounts.assign(result.m_data.account_list().accounts().begin(),
					result.m_data.account_list().accounts().end());
				std::sort(m_accounts.begin(), m_accounts.end(),
					[](const request::TradeAccountRecord& left,
						const request::TradeAccountRecord& right)
				{
					return AccountIndex(left) < AccountIndex(right);
				});
			}
		}
	}
	m_results.Notify(result);
	return true;
}

void CAccountService::SetTraderCenter(CTraderCenter* pCenter)
{
	m_pTraderCenter = pCenter;
}

bool CAccountService::IsAuthenticated() const noexcept
{
	return (nullptr != m_pTraderCenter) && m_pTraderCenter->IsAuthenticated();
}

bool CAccountService::SendRequest(const CRequest& request)
{
	return (nullptr != m_pTraderCenter) && m_pTraderCenter->SendRequest(request);
}
