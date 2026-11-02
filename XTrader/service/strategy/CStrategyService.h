#ifndef XTRADER_SERVER_CSTRATEGYSERVICE_H
#define XTRADER_SERVER_CSTRATEGYSERVICE_H

#include "../../common/ISingleton.h"
#include "../../request/request.pb.h"
#include "../../basic/TMessagePump.h"

#include <functional>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../request/request.h"

class CStrategyService final : public ISingleton<CStrategyService>
{
	DECLARE_SINGLE_DFAULT(CStrategyService)

	public:
	using _TyStrategyList = std::vector<request::StrategyInfo>;
	using _TyQueryHandler = std::function<void(const _TyStrategyList&, const std::string&)>;
	using _TyOperationHandler = std::function<void(_TyRequest, bool, const request::StrategyInfo&, const std::string&)>;
	struct COperationResult
	{
		std::uint64_t m_requestId{ 0 };
		_TyRequest m_type{ _TyRequest::UNKNOWN };
		bool m_success{ false };
		std::string m_error;
	};
	struct CRuntimeSnapshot
	{
		int m_state{ 0 };
		bool m_marketAvailable{ false };
		std::uint64_t m_queueLength{ 0 };
		std::uint64_t m_activeOrders{ 0 };
		std::string m_lastError;
	};
	using _TyRuntimeMap = std::unordered_map<std::uint64_t, CRuntimeSnapshot>;
	using _TyOperationResultHandler = std::function<void(const COperationResult&)>;
	using _TyRuntimeHandler = std::function<void(const _TyRuntimeMap&, const std::string&)>;

	void Initialize();
	void SetQueryHandler(_TyQueryHandler&& handler);
	_TyCallbackId AddQueryHandler(_TyQueryHandler&& handler);
	void RemoveQueryHandler(_TyCallbackId nToken);
	void SetOperationHandler(_TyOperationHandler&& handler);
	_TyCallbackId AddOperationHandler(_TyOperationResultHandler&& handler);
	void RemoveOperationHandler(_TyCallbackId nToken);
	_TyCallbackId AddRuntimeHandler(_TyRuntimeHandler&& handler);
	void RemoveRuntimeHandler(_TyCallbackId nToken);
	bool QueryStrategies();
	bool QueryRuntime();
	std::uint64_t AddStrategy(const request::StrategyInfo& strategy);
	std::uint64_t ModifyStrategy(const request::StrategyInfo& strategy);
	std::uint64_t DeleteStrategy(std::uint64_t id);
	std::uint64_t ControlStrategy(_TyRequest type, std::uint64_t id);
	_TyStrategyList GetStrategies() const;
	bool FindStrategy(std::uint64_t id, request::StrategyInfo& result) const;
	bool IsCacheValid() const;
	static bool ValidateStrategy(const request::StrategyInfo& strategy, std::string& error);

	private:
		bool OnRequestReply(const CRequest& response);

	private:
	std::once_flag m_initializeFlag;
	std::mutex m_mtx_handlers;
	_TyQueryHandler m_queryHandler;
	TMessagePump<std::pair<_TyStrategyList, std::string>> m_queryPump;
	TMessagePump<COperationResult> m_operationPump;
	TMessagePump<std::pair<_TyRuntimeMap, std::string>> m_runtimePump;
	_TyOperationHandler m_operationHandler;
	std::atomic<std::uint64_t> m_queryRequestId{ 0 };
	std::atomic<std::uint64_t> m_runtimeRequestId{ 0 };
	mutable std::shared_mutex m_mtx_strategies;
	_TyStrategyList m_strategies;
	bool m_bCacheValid{ false };
};

#endif
