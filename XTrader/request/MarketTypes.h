#ifndef XENGINE_HQMARKET_MARKETTYPES_H
#define XENGINE_HQMARKET_MARKETTYPES_H

#include <string>
#include <vector>

namespace hqmarket::market::v1
{
	enum Exchange : int;
	class Security;
	class SecurityInfo;
	class SecurityList;
	class PriceLevel;
	class SubscriptionAck;
	class QuoteData;
	class DepthData;
	class QueryResponse;
	class BarData;
	class SectorList;
	class SectorConstituents;
} // namespace hqmarket::market::v1

using _TyMarketExchange = hqmarket::market::v1::Exchange;
using _TySecurity = hqmarket::market::v1::Security;
using _TySecurityInfo = hqmarket::market::v1::SecurityInfo;
using _TySecurityList = hqmarket::market::v1::SecurityList;
using _TyPriceLevel = hqmarket::market::v1::PriceLevel;
using _TySubscriptionAck = hqmarket::market::v1::SubscriptionAck;
using _TyQuoteData = hqmarket::market::v1::QuoteData;
using _TyDepthData = hqmarket::market::v1::DepthData;
using _TyQueryResponse = hqmarket::market::v1::QueryResponse;
using _TyBarData = hqmarket::market::v1::BarData;
using _TySectorList = hqmarket::market::v1::SectorList;
using _TySectorConstituents = hqmarket::market::v1::SectorConstituents;

enum class Exchange
{
	unknown = 0,
	sse,
	szse,
	bse,
	hkex,
	cffex,
	shfe,
	dce,
	czce,
	ine,
	gfex,
	nasdaq,
	nyse,
	crypto
};

enum class Channel
{
	unknown = 0,
	quote,
	depth,
	trade,
	bar_1m,
	bar_1d,
	market_status
};

enum class MarketState
{
	unknown = 0,
	normal,
	suspended,
	delisted
};

enum class SectorType
{
	unknown = 0,
	industry,
	conceptSector,
	region
};

struct CSecurity
{
	std::string m_strCode;
	std::string m_strName;
	std::vector<std::string> m_pinyinFullAliases;
	std::vector<std::string> m_pinyinShortAliases;
	Exchange m_hq{ Exchange::unknown };
	MarketState m_status{ MarketState::unknown };

	CSecurity() = default;
	CSecurity(const std::string& strCode, Exchange mk, MarketState status = MarketState::unknown);
	CSecurity(const std::string& strCode, const std::string& strName, Exchange mk, MarketState status = MarketState::unknown);
	CSecurity(const CSecurity& arg);
	CSecurity& operator=(const CSecurity& arg);
	bool operator==(const CSecurity& arg) const;

	bool IsValid() const;
	std::string String() const;
};

std::string GetMarketString(Exchange exchange);
std::string GetChannelString(Channel channel);
std::string GetMarketStateString(MarketState status);
Exchange ParseMarket(const std::string& strExchange);
Channel ParseChannel(const std::string& strChannel);
MarketState ParseMarketState(const std::string& strStatus);
CSecurity ParseSecurity(const std::string& strSecurity);
std::string FmtSecurityString(const std::string& strCode, Exchange mk);

struct CQuoteInfo
{
	CQuoteInfo() = default;
	CQuoteInfo(const std::string& strCode, Exchange mk, Channel channel) : m_security(strCode, mk), m_channel(channel)
	{
	}

	bool IsValid() const;
	std::string String() const;

	CSecurity m_security;
	Channel m_channel{ Channel::unknown };
};

#endif
