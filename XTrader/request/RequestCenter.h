#ifndef XTRADER_SERVER_REQUESTCENTER_H
#define XTRADER_SERVER_REQUESTCENTER_H

#include "MarketTypes.h"
#include "request.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace request
{
	using _TyParams = std::unordered_map<std::string, std::string>;

	enum class AuthAction
	{
		Login,
		Register
	};

	CRequest Auth(AuthAction action, const std::string& strUser, const std::string& strPassword);
	CRequest Auth(const std::string& strToken);
	CRequest Subscription(const _TyParams& param);
	CRequest UnSubscription(const _TyParams& param);
	CRequest Subscription(const std::vector<CSecurity>& securities, const std::vector<Channel>& channels);
	CRequest UnSubscription(const std::vector<CSecurity>& securities, const std::vector<Channel>& channels);
	CRequest QueryMarketBars(const CSecurity& info, const std::string& strChannel, std::int64_t nBeginTime, std::int64_t nEndTime);
	CRequest QueryMarketSecurities();
	CRequest QueryMarketSectors(SectorType type);
	CRequest QueryMarketSectorConstituents(SectorType type, const std::string& strSectorCode);
	CRequest AddStrategy(const _TyStrategyInfo& strategy);
	CRequest ModifyStrategy(const _TyStrategyInfo& strategy);
	CRequest QueryStrategies();
	CRequest DeleteStrategy(std::uint64_t id);
	CRequest QueryStrategyRuntime();
	CRequest ControlStrategy(_TyRequest type, std::uint64_t id);
	CRequest HeartBeat();
} // namespace request

#endif
