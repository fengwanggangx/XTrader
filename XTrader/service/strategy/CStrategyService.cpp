#include "CStrategyService.h"

#include "../../system/CSession.h"

#include <algorithm>
#include <cctype>
#include <utility>

CStrategyService::CStrategyService() = default;

CStrategyService::~CStrategyService() = default;

void CStrategyService::Initialize()
{
	std::call_once(m_initializeFlag, [this]()
	{
		CSession::InstanceRef().RegisterResponseHandler(std::bind_front(&CStrategyService::OnRequestReply, this));
		CSession::InstanceRef().RegisterStateHandler([this](SessionState state, const std::string&)
		{
			if (SessionState::Ready == state)
			{
				QueryStrategies();
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

bool CStrategyService::QueryStrategies()
{
	Initialize();
	CRequest req = request::QueryStrategies();
	std::uint64_t expected = 0;
	if (!m_queryRequestId.compare_exchange_strong(expected, req.GetId()))
	{
		return true;
	}
	if (CSession::InstanceRef().SendRequest(req))
	{
		return true;
	}
	expected = req.GetId();
	m_queryRequestId.compare_exchange_strong(expected, 0);
	return false;
}

bool CStrategyService::QueryRuntime()
{
	Initialize();
	CRequest req = request::QueryStrategyRuntime();
	std::uint64_t expected = 0;
	if (!m_runtimeRequestId.compare_exchange_strong(expected, req.GetId()))
	{
		return true;
	}
	if (CSession::InstanceRef().SendRequest(req))
	{
		return true;
	}
	expected = req.GetId();
	m_runtimeRequestId.compare_exchange_strong(expected, 0);
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
	if ((0 == strategy.strategy_id()) || !ValidateStrategy(strategy, error))
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
		std::uint64_t expected = response.GetId();
		if ((0 == expected) || !m_queryRequestId.compare_exchange_strong(expected, 0))
		{
			return false;
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
			std::unique_lock<std::shared_mutex> lock(m_mtx_strategies);
			m_strategies = strategies;
			m_bCacheValid = true;
		}
		else
		{
			std::unique_lock<std::shared_mutex> lock(m_mtx_strategies);
			m_bCacheValid = false;
		}
		if (!strError.empty())
		{
			strategies = GetStrategies();
		}
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
		std::uint64_t expected = response.GetId();
		if ((0 == expected) || !m_runtimeRequestId.compare_exchange_strong(expected, 0))
		{
			return false;
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
	if (strError.empty() && ((_TyRequest::STRATEGY_ADD == type) || (_TyRequest::STRATEGY_MODIFY == type) || (_TyRequest::STRATEGY_DELETE == type)))
	{
		// Mutation acknowledgements may omit the complete configuration or deleted ID.
		// Keep the last list, but require a fresh query before treating it as current.
		std::unique_lock<std::shared_mutex> lock(m_mtx_strategies);
		m_bCacheValid = false;
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
		if ((_TyRequest::STRATEGY_ADD == type) || (_TyRequest::STRATEGY_MODIFY == type) || (_TyRequest::STRATEGY_DELETE == type))
		{
			QueryStrategies();
		}
		QueryRuntime();
	}
	return true;
}
