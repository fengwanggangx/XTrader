#include <charconv>
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
				m_pushedStates.clear();
				m_statusRevision = 0;
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
	case _TyRequest::TRADE_ACCOUNT_ADD:
	case _TyRequest::TRADE_ACCOUNT_REMOVE:
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
				return _TyRequest::TRADE_FUNDS_QUERY == type ? item.first : 0;
			}
		}
		m_pending.emplace(req.GetId(), CPending{ account, type,
										   std::chrono::steady_clock::now() +
											   std::chrono::seconds(_TyRequest::TRADE_ACCOUNT_LOGIN == type ? 75 : 20), m_statusRevision });
	}
	if (SendRequest(req))
	{
		return req.GetId();
	}
	std::lock_guard<std::mutex> lock(m_mtx_state);
	m_pending.erase(req.GetId());
	return 0;
}
bool CAccountService::OnPush(const CRequest& response)
{
	CResult result;
	result.m_type = response.GetType();
	const request::RequestData& data = response.GetData();
	if (_TyRequest::TRADE_ACCOUNT_STATE_PUSH == result.m_type && data.has_account())
	{
		result.m_accountIndex = AccountIndex(data.account());
	}
	else
	{
		std::string broker = response.GetExtraData("broker_id");
		const char* end = broker.data() + broker.size();
		std::from_chars_result parsed = std::from_chars(broker.data(), end, result.m_accountIndex.first);
		if (std::errc{} != parsed.ec || end != parsed.ptr)
		{
			return false;
		}
		result.m_accountIndex.second = response.GetExtraData("account");
	}
	if (!IsAccountValid(result.m_accountIndex))
	{
		return false;
	}
	std::string sequence = response.GetExtraData("sequence");
	const char* sequenceEnd = sequence.data() + sequence.size();
	std::from_chars_result parsedSequence = std::from_chars(sequence.data(), sequenceEnd, result.m_sequence);
	if (std::errc{} != parsedSequence.ec || sequenceEnd != parsedSequence.ptr)
	{
		return false;
	}
	std::string fundsVersion = response.GetExtraData("funds_version");
	const char* versionEnd = fundsVersion.data() + fundsVersion.size();
	std::uint64_t version = 0;
	std::from_chars_result parsedVersion = std::from_chars(fundsVersion.data(), versionEnd, version);
	if (std::errc{} == parsedVersion.ec && versionEnd == parsedVersion.ptr)
	{
		result.m_fundsVersion = version;
	}
	{
		std::lock_guard<std::mutex> lock(m_mtx_state);
		result.m_generation = m_generation;
		if (_TyRequest::TRADE_ACCOUNT_STATE_PUSH == result.m_type && data.has_account())
		{
			m_pushedStates.insert_or_assign(result.m_accountIndex, std::make_pair(++m_statusRevision, data.account()));
			const auto account = std::ranges::find_if(m_accounts, [&result](const request::TradeAccountRecord& row)
			{
				return AccountIndex(row) == result.m_accountIndex;
			});
			if (m_accounts.end() != account)
			{
				account->set_state(data.account().state());
				account->set_last_error(data.account().last_error());
			}
		}
	}
	result.m_data.CopyFrom(data);
	m_results.Notify(result);
	return true;
}

bool CAccountService::OnRequestReply(const CRequest& response)
{
	switch (response.GetType())
	{
	case _TyRequest::TRADE_ACCOUNT_STATE_PUSH:
	case _TyRequest::TRADE_ORDER_PUSH:
	case _TyRequest::TRADE_TRADE_PUSH:
	case _TyRequest::TRADE_ACCOUNT_INFO_PUSH:
	case _TyRequest::TRADE_POSITION_PUSH:
		return (0 == response.GetId()) && OnPush(response);
	default:
		break;
	}
	CResult result;
	std::optional<CResult> canceledFunds;
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
		std::uint64_t statusRevision = mIter->second.m_statusRevision;
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
				const request::TradeCounterReply& reply = result.m_data.counter_reply();
				result.m_error = reply.error_message().empty() ? "资金查询失败，错误码：" + std::to_string(reply.error_code()) : reply.error_message();
			}
		}
		else if (_TyRequest::TRADE_ACCOUNT_LOGIN == result.m_type ||
				 _TyRequest::TRADE_ACCOUNT_DISCONNECT == result.m_type)
		{
			const auto pending = std::ranges::find_if(m_pending, [&result](const auto& item)
			{
				return _TyRequest::TRADE_FUNDS_QUERY == item.second.m_type && result.m_accountIndex == item.second.m_accountIndex;
			});
			if (m_pending.end() != pending)
			{
				CResult& canceled = canceledFunds.emplace();
				canceled.m_requestId = pending->first;
				canceled.m_type = pending->second.m_type;
				canceled.m_accountIndex = result.m_accountIndex;
				canceled.m_generation = result.m_generation;
				canceled.m_error = "账户状态已变化，资金查询已取消";
				m_pending.erase(pending);
			}
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
				for (request::TradeAccountRecord& row : m_accounts)
				{
					const auto pushed = m_pushedStates.find(AccountIndex(row));
					if (m_pushedStates.end() != pushed && statusRevision < pushed->second.first)
					{
						row.set_state(pushed->second.second.state());
						row.set_last_error(pushed->second.second.last_error());
					}
				}
				std::sort(m_accounts.begin(), m_accounts.end(),
					[](const request::TradeAccountRecord& left,
						const request::TradeAccountRecord& right)
				{
					return AccountIndex(left) < AccountIndex(right);
				});
			}
		}
	}
	if (canceledFunds.has_value())
	{
		m_results.Notify(*canceledFunds);
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
