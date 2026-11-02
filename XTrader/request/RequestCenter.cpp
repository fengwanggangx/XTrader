#include "RequestCenter.h"
#include "v1/market.pb.h"

#include <chrono>

namespace request
{
	namespace
	{
		CRequest MarketSubscription(const std::vector<CSecurity>& securities, const std::vector<Channel>& channels, bool bSubscribe)
		{
			CRequest req;
			req.SetType(bSubscribe ? _TyRequest::MARKET_SUBSCRIBE : _TyRequest::MARKET_UNSUBSCRIBE);
			CRequest::_TyExtraRows rows;
			rows.reserve(securities.size() * channels.size());
			for (const auto& security : securities)
			{
				for (Channel channel : channels)
				{
					rows.emplace_back(std::unordered_map<std::string, std::string>{ { "security", security.String() }, { "channel", GetChannelString(channel) } });
				}
			}
			req.SetExtraData("subscriptions", rows);
			return req;
		}
	}
	CRequest Auth(AuthAction action, const std::string& strUser, const std::string& strPassword)
	{
		CRequest req;
		req.SetType(AuthAction::Login == action ? _TyRequest::AUTH_LOGIN : _TyRequest::AUTH_REGISTER);
		req.SetExtraData("user", strUser);
		req.SetExtraData("password", strPassword);
		return req;
	}

	CRequest Auth(const std::string& strToken)
	{
		CRequest req;
		req.SetType(_TyRequest::AUTH_LOGIN);
		req.SetExtraData("token", strToken);
		return req;
	}

	CRequest Subscription(const _TyParams& param)
	{
		CRequest req;
		req.SetType(_TyRequest::MARKET_SUBSCRIBE);
		for (const auto& [k, v] : param)
		{
			req.SetExtraData(k, v);
		}
		return req;
	}

	CRequest UnSubscription(const _TyParams& param)
	{
		CRequest req;
		req.SetType(_TyRequest::MARKET_UNSUBSCRIBE);
		for (const auto& [k, v] : param)
		{
			req.SetExtraData(k, v);
		}
		return req;
	}

	CRequest Subscription(const std::vector<CSecurity>& securities, const std::vector<Channel>& channels)
	{
		return MarketSubscription(securities, channels, true);
	}

	CRequest UnSubscription(const std::vector<CSecurity>& securities, const std::vector<Channel>& channels)
	{
		return MarketSubscription(securities, channels, false);
	}

	CRequest QueryMarketBars(const CSecurity& info, const std::string& strChannel, std::int64_t nBeginTime, std::int64_t nEndTime)
	{
		CRequest req;
		req.SetType(_TyRequest::MARKET_BARS_QUERY);
		req.SetExtraData("security", info.String());
		req.SetExtraData("channel", strChannel);
		req.SetExtraData("begin_time_ms", std::to_string(nBeginTime));
		req.SetExtraData("end_time_ms", std::to_string(nEndTime));
		return req;
	}

	CRequest QueryMarketSecurities()
	{
		CRequest req;
		req.SetType(_TyRequest::MARKET_SECURITIES_QUERY);
		return req;
	}

	CRequest QueryMarketSectors(SectorType type)
	{
		CRequest req;
		req.SetType(_TyRequest::MARKET_SECTORS_QUERY);
		req.SetExtraData("sector_type", std::to_string(static_cast<int>(type)));
		return req;
	}

	CRequest QueryMarketSectorConstituents(SectorType type, const std::string& strSectorCode)
	{
		CRequest req;
		req.SetType(_TyRequest::MARKET_SECTOR_CONSTITUENTS_QUERY);
		req.SetExtraData("sector_type", std::to_string(static_cast<int>(type)));
		req.SetExtraData("sector_code", strSectorCode);
		return req;
	}

	CRequest AddStrategy(const _TyStrategyInfo& strategy)
	{
		CRequest req;
		req.SetType(_TyRequest::STRATEGY_ADD);
		req.SetData(strategy);
		return req;
	}

	CRequest ModifyStrategy(const _TyStrategyInfo& strategy)
	{
		CRequest req;
		req.SetType(_TyRequest::STRATEGY_MODIFY);
		req.SetData(strategy);
		return req;
	}

	CRequest QueryStrategies()
	{
		CRequest req;
		req.SetType(_TyRequest::STRATEGY_QUERY);
		return req;
	}

	CRequest DeleteStrategy(std::uint64_t id)
	{
		CRequest req;
		req.SetType(_TyRequest::STRATEGY_DELETE);
		req.SetExtraData("strategy_id", std::to_string(id));
		return req;
	}

	CRequest QueryStrategyRuntime()
	{
		CRequest req;
		req.SetType(_TyRequest::STRATEGY_RUNTIME_QUERY);
		return req;
	}

	CRequest ControlStrategy(_TyRequest type, std::uint64_t id)
	{
		CRequest req;
		req.SetType(type);
		req.SetExtraData("strategy_id", std::to_string(id));
		return req;
	}

	CRequest HeartBeat()
	{
		CRequest req;
		req.SetType(_TyRequest::HEARTBEAT);
		std::int64_t nClientTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		req.SetExtraData("client_time_ms", std::to_string(nClientTime));
		return req;
	}
} // namespace request
