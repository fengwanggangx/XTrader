#include "CHQService.h"

#include "../../system/CTraderCenter.h"
#include "../../request/v1/market.pb.h"

#include <cmath>
#include <charconv>
#include <chrono>
#include <deque>
#include <utility>
#include <common/container.h>

namespace
{
	std::string ChannelName(MarketBarPeriod period)
	{
		return MarketBarPeriod::Minute == period ? "bar_1m" : "bar_1d";
	}

	std::string HistoryKey(const std::string& strSecurity, MarketBarPeriod period)
	{
		return strSecurity + ":" + ChannelName(period);
	}

	double ScaledPrice(std::int64_t nPrice, std::int32_t nScale)
	{
		double fScale = std::pow(10.0, nScale);
		return 0.0 < fScale ? static_cast<double>(nPrice) / fScale : 0.0;
	}

	std::string ExchangeSuffix(_TyMarketExchange exchange)
	{
		switch (exchange)
		{
		case hqmarket::market::v1::SSE:
			return "SSE";
		case hqmarket::market::v1::SZSE:
			return "SZSE";
		case hqmarket::market::v1::BSE:
			return "BSE";
		case hqmarket::market::v1::HKEX:
			return "HKEX";
		default:
			return {};
		}
	}

	Exchange ToExchange(_TyMarketExchange exchange)
	{
		return static_cast<Exchange>(static_cast<int>(exchange));
	}

	std::string SecurityName(const _TySecurity& security)
	{
		std::string strExchange = ExchangeSuffix(security.exchange());
		return strExchange.empty() ? security.symbol() : security.symbol() + "." + strExchange;
	}

	std::string JoinAliases(const std::vector<std::string>& aliases)
	{
		std::string result;
		for (const std::string& strAlias : aliases)
		{
			if (!result.empty())
			{
				result += ' ';
			}
			result += strAlias;
		}
		return result;
	}

	std::string MarketCategory(const std::string& strSecurity)
	{
		std::size_t dot = strSecurity.rfind('.');
		if (std::string::npos == dot)
		{
			return "未知";
		}
		std::string strCode = strSecurity.substr(0, dot);
		std::string strExchange = strSecurity.substr(dot + 1);
		if ("BSE" == strExchange)
		{
			return "北交所";
		}
		if ("SSE" == strExchange)
		{
			if (strCode.starts_with("000"))
			{
				return "指数";
			}
			return (strCode.starts_with("688") || strCode.starts_with("689")) ? "科创板" : "沪A";
		}
		if ("SZSE" == strExchange)
		{
			if (strCode.starts_with("399"))
			{
				return "指数";
			}
			return (strCode.starts_with("300") || strCode.starts_with("301")) ? "创业板" : "深A";
		}
		return "未知";
	}

	std::uint64_t ParseSequence(const request::_TyParams& values)
	{
		const std::string& strSequence = container::vfind(values, "sequence");
		if (strSequence.empty())
		{
			return 0;
		}
		std::uint64_t sequence = 0;
		const char* pBegin = strSequence.data();
		const char* pEnd = pBegin + strSequence.size();
		std::from_chars_result result = std::from_chars(pBegin, pEnd, sequence);
		return (std::errc() == result.ec) && (pEnd == result.ptr) ? sequence : 0;
	}

	CSectorInfo ParseSector(const hqmarket::market::v1::SectorInfo& value, SectorType type)
	{
		CSectorInfo sector;
		sector.m_type = type;
		sector.m_strCode = value.code();
		sector.m_strName = value.name();
		sector.m_fChangePercent = ScaledPrice(value.change_percent(), value.percent_scale());
		sector.m_nRisingCount = value.rising_count();
		sector.m_nFallingCount = value.falling_count();
		sector.m_nFlatCount = value.flat_count();
		sector.m_nMemberCount = value.member_count();
		sector.m_leadingSecurity = CSecurity(value.leading_security().symbol(), ToExchange(value.leading_security().exchange()));
		sector.m_strLeadingName = value.leading_name();
		sector.m_nSnapshotTime = value.snapshot_time_ms();
		return sector;
	}
} // namespace

CHQService::CHQService()
{
	m_request_handler = {
		{ _TyRequest::MARKET_SECTORS_QUERY, std::bind_front(&CHQService::OnSectorListReply, this) },
		{ _TyRequest::MARKET_SECTOR_CONSTITUENTS_QUERY, std::bind_front(&CHQService::OnSectorConstituentsReply, this) },
		{ _TyRequest::MARKET_SECURITIES_QUERY, std::bind_front(&CHQService::OnSecurityListReply, this) },
		{ _TyRequest::MARKET_DEPTH_PUSH, std::bind_front(&CHQService::OnDepthReply, this) },
		{ _TyRequest::MARKET_BARS_QUERY, std::bind_front(&CHQService::OnHistoryReply, this) },
		{ _TyRequest::MARKET_QUOTE_PUSH, std::bind_front(&CHQService::OnQuoteReply, this) },
		{ _TyRequest::MARKET_SUBSCRIBE, std::bind_front(&CHQService::OnSubscriptionAck, this) },
		{ _TyRequest::MARKET_UNSUBSCRIBE, std::bind_front(&CHQService::OnSubscriptionAck, this) }
	};
	m_quoteTable.AddColumn(CDataColumnSchema{ static_cast<_TyDataColumnId>(MarketQuoteColumn::Security), "代码", DataType::String });
	m_quoteTable.AddColumn(CDataColumnSchema{ static_cast<_TyDataColumnId>(MarketQuoteColumn::Name), "名称", DataType::String });
	m_quoteTable.AddColumn(CDataColumnSchema{ static_cast<_TyDataColumnId>(MarketQuoteColumn::LastPrice), "最新价", DataType::Double });
	m_quoteTable.AddColumn(CDataColumnSchema{ static_cast<_TyDataColumnId>(MarketQuoteColumn::Change), "涨跌额", DataType::Double });
	m_quoteTable.AddColumn(CDataColumnSchema{ static_cast<_TyDataColumnId>(MarketQuoteColumn::Percent), "涨跌幅", DataType::Double });
	m_quoteTable.AddColumn(CDataColumnSchema{ static_cast<_TyDataColumnId>(MarketQuoteColumn::PreClose), "昨收", DataType::Double });
	m_quoteTable.AddColumn(CDataColumnSchema{ static_cast<_TyDataColumnId>(MarketQuoteColumn::Volume), "成交量", DataType::Int64 });
	m_quoteTable.AddColumn(CDataColumnSchema{ static_cast<_TyDataColumnId>(MarketQuoteColumn::Status), "状态", DataType::String });
	m_quoteTable.AddColumn(CDataColumnSchema{ static_cast<_TyDataColumnId>(MarketQuoteColumn::Sequence), "序列", DataType::UInt64 });
	m_quoteTable.AddColumn(CDataColumnSchema{ static_cast<_TyDataColumnId>(MarketQuoteColumn::ListingStatus), "证券状态", DataType::String });
	m_quoteTable.AddColumn(CDataColumnSchema{ static_cast<_TyDataColumnId>(MarketQuoteColumn::Market), "市场", DataType::String });
	m_quoteTable.AddColumn(CDataColumnSchema{ static_cast<_TyDataColumnId>(MarketQuoteColumn::PinyinFullAliases), "拼音全称", DataType::String });
	m_quoteTable.AddColumn(CDataColumnSchema{ static_cast<_TyDataColumnId>(MarketQuoteColumn::PinyinShortAliases), "拼音简称", DataType::String });
}

CHQService::~CHQService()
{
	m_bStopping.store(true);
	m_cv_pendingQuotes.notify_all();
	if (m_quoteWorker.joinable())
	{
		m_quoteWorker.join();
	}
}

void CHQService::Initialize()
{
	if (nullptr == m_pTraderCenter)
	{
		return;
	}
	if (m_bInitialized)
	{
		return;
	}
	m_bInitialized = true;
	m_bStopping.store(false);
	m_quoteWorker = std::thread(&CHQService::QuoteWorkerLoop, this);
	m_pTraderCenter->RegisterStateHandler([this](SessionState state, const std::string&)
	{
		if (SessionState::Ready == state)
		{
			m_runtimeMetrics.m_nLastQuoteSequence.store(0);
			RestoreSubscriptions();
		}
		else if ((SessionState::Disconnected == state) || (SessionState::Reconnecting == state) || (SessionState::Stopping == state))
		{
			std::vector<CQuote> quotes;
			{
				std::unique_lock lock(m_mtx_quotes);
				quotes.reserve(m_quotes.size());
				for (auto& [strSecurity, quote] : m_quotes)
				{
					quote.m_bStale = true;
					quotes.emplace_back(quote);
				}
			}
			for (const auto& v : quotes)
			{
				EnqueueQuote(v);
			}
		}
	});
}

CHQService::_TyHandlerToken CHQService::AddQuoteHandler(_TyQuoteHandler&& handler)
{
	return m_pump_quote.Subscribe(std::move(handler));
}

void CHQService::RemoveQuoteHandler(_TyHandlerToken token)
{
	m_pump_quote.Unsubscribe(token);
}

CHQService::_TyHandlerToken CHQService::AddDepthHandler(_TyDepthHandler&& handler)
{
	return m_pump_depth.Subscribe(std::move(handler));
}

void CHQService::RemoveDepthHandler(_TyHandlerToken token)
{
	m_pump_depth.Unsubscribe(token);
}

CHQService::_TyHandlerToken CHQService::AddHistoryHandler(_TyHistoryHandler&& handler)
{
	return m_pump_history.Subscribe([handler = std::move(handler)](const CMarketHistoryEvent& event)
	{
		handler(event.m_requestId, event.m_strSecurity, event.m_period, event.m_bars, event.m_strError);
	});
}

void CHQService::RemoveHistoryHandler(_TyHandlerToken token)
{
	m_pump_history.Unsubscribe(token);
}

CHQService::_TyHandlerToken CHQService::AddQuoteTableHandler(_TyQuoteTableHandler&& handler)
{
	return m_pump_quote_table.Subscribe([handler = std::move(handler)](const CQuoteTableEvent& event)
	{
		handler(event.m_view, event.m_changes);
	});
}

void CHQService::RemoveQuoteTableHandler(_TyHandlerToken token)
{
	m_pump_quote_table.Unsubscribe(token);
}

CHQService::_TyHandlerToken CHQService::AddSecurityListHandler(_TySecurityListHandler&& handler)
{
	return m_pump_security_list.Subscribe(std::move(handler));
}

void CHQService::RemoveSecurityListHandler(_TyHandlerToken token)
{
	m_pump_security_list.Unsubscribe(token);
}

CHQService::_TyHandlerToken CHQService::AddSectorListHandler(_TySectorListHandler&& handler)
{
	return m_pump_sector_list.Subscribe(std::move(handler));
}

void CHQService::RemoveSectorListHandler(_TyHandlerToken token)
{
	m_pump_sector_list.Unsubscribe(token);
}

CHQService::_TyHandlerToken CHQService::AddSectorConstituentsHandler(_TySectorConstituentsHandler&& handler)
{
	return m_pump_sector_constituents.Subscribe(std::move(handler));
}

void CHQService::RemoveSectorConstituentsHandler(_TyHandlerToken token)
{
	m_pump_sector_constituents.Unsubscribe(token);
}

void CHQService::RegisterSecurity(const CSecurity& info)
{
	std::string strKey = info.String();
	if (strKey.empty())
	{
		return;
	}
	bool bRegistered = false;
	{
		std::lock_guard<std::mutex> lock(m_mtx_pendingQuotes);
		m_securityNames.insert_or_assign(strKey, info.m_strName);
		m_securityPinyinFullAliases.insert_or_assign(strKey, JoinAliases(info.m_pinyinFullAliases));
		m_securityPinyinShortAliases.insert_or_assign(strKey, JoinAliases(info.m_pinyinShortAliases));
		m_securityStatuses.insert_or_assign(strKey, info.m_status);
		m_pendingSecurityUpdates.emplace(strKey);
		bRegistered = m_registeredSecurities.emplace(strKey).second;
		if (bRegistered)
		{
			CQuote& quote = container::emplace_back(m_pendingQuotes, strKey, info);
			quote.m_bStale = true;
		}
		else if (!container::with(m_pendingQuotes, strKey))
		{
			CQuote& quote = container::emplace_back(m_pendingQuotes, strKey);
			bool bFind = false;
			{
				std::shared_lock quoteLock(m_mtx_quotes);
				bFind = container::try_vfind(m_quotes, strKey, quote);
			}
			quote.m_security = info;
			if (!bFind)
			{
				quote.m_bStale = true;
			}
		}
	}
	m_cv_pendingQuotes.notify_one();
}

bool CHQService::SubscribeQuote(const CSecurity& info)
{
	std::string strSecurity = info.String();
	request::_TyParams parameters{
		{ "security", strSecurity },
		{ "channel", "quote" }
	};
	return Subscribe("watchlist:" + strSecurity, parameters);
}

bool CHQService::UnsubscribeQuote(const CSecurity& info)
{
	std::string strSecurity = info.String();
	request::_TyParams parameters{
		{ "security", strSecurity },
		{ "channel", "quote" }
	};
	return Unsubscribe("watchlist:" + strSecurity, parameters);
}

bool CHQService::SubscribeQuotes(const std::vector<CSecurity>& securities)
{
	if (securities.empty())
	{
		return true;
	}
	{
		std::lock_guard<std::mutex> lock(m_mtx_subscriptions);
		for (const CSecurity& security : securities)
		{
			std::string strKey = "watchlist:" + security.String();
			m_subscriptions.insert_or_assign(strKey, request::_TyParams{ { "security", security.String() }, { "channel", "quote" } });
			m_failedSubscriptions.erase(strKey);
		}
	}
	return SendRequest(request::Subscription(securities, std::vector<Channel>{ Channel::quote }));
}

bool CHQService::UnsubscribeQuotes(const std::vector<CSecurity>& securities)
{
	if (securities.empty())
	{
		return true;
	}
	{
		std::lock_guard<std::mutex> lock(m_mtx_subscriptions);
		for (const CSecurity& security : securities)
		{
			std::string strKey = "watchlist:" + security.String();
			m_subscriptions.erase(strKey);
			m_confirmedSubscriptions.erase(strKey);
			m_failedSubscriptions.erase(strKey);
		}
	}
	return SendRequest(request::UnSubscription(securities, std::vector<Channel>{ Channel::quote }));
}

bool CHQService::SubscribeDepth(const CSecurity& info)
{
	std::string strSecurity = info.String();
	request::_TyParams parameters{
		{ "security", strSecurity },
		{ "channel", "depth" }
	};
	return Subscribe("depth:" + strSecurity, parameters);
}

bool CHQService::UnsubscribeDepth(const CSecurity& info)
{
	std::string strSecurity = info.String();
	request::_TyParams parameters{
		{ "security", strSecurity },
		{ "channel", "depth" }
	};
	return Unsubscribe("depth:" + strSecurity, parameters);
}

bool CHQService::QueryHistory(const CSecurity& info, MarketBarPeriod period, std::int64_t nBeginTime, std::int64_t nEndTime, _TyRequestId* requestId)
{
	CRequest request = request::QueryMarketBars(info, ChannelName(period), nBeginTime, nEndTime);
	if (nullptr != requestId)
	{
		*requestId = request.GetId();
	}
	{
		std::lock_guard<std::mutex> lock(m_mtx_historyRequests);
		m_historyRequests.emplace(request.GetId(), CMarketHistoryEvent{ request.GetId(), info.String(), period });
	}
	if (SendRequest(request))
	{
		return true;
	}
	request.SetErrorInfo(-1, "历史行情请求发送失败");
	OnRequestReply(request);
	return false;
}

bool CHQService::QuerySecurities()
{
	return SendRequest(request::QueryMarketSecurities());
}

bool CHQService::QuerySectors(SectorType type)
{
	CRequest req = request::QueryMarketSectors(type);
	m_nLatestSectorListRequest.store(req.GetId());
	if (SendRequest(req))
	{
		return true;
	}
	req.SetErrorInfo(-1, "板块查询请求发送失败");
	OnRequestReply(req);
	return false;
}

bool CHQService::QuerySectorConstituents(SectorType type, const std::string& strSectorCode)
{
	CRequest req = request::QueryMarketSectorConstituents(type, strSectorCode);
	m_nLatestSectorConstituentsRequest.store(req.GetId());
	if (SendRequest(req))
	{
		return true;
	}
	req.SetErrorInfo(-1, "成分股查询请求发送失败");
	OnRequestReply(req);
	return false;
}

bool CHQService::Subscribe(const std::string& strKey, const request::_TyParams& param)
{
	if (strKey.empty())
	{
		return false;
	}
	{
		std::lock_guard<std::mutex> lock(m_mtx_subscriptions);
		const auto mIter = m_subscriptions.find(strKey);
		if ((m_subscriptions.end() != mIter) && (param == mIter->second))
		{
			return IsAuthenticated();
		}
		m_subscriptions.insert_or_assign(strKey, param);
	}
	return SendRequest(request::Subscription(param));
}

bool CHQService::Unsubscribe(const std::string& strKey, const request::_TyParams& param)
{
	{
		std::lock_guard<std::mutex> lock(m_mtx_subscriptions);
		if (0 == m_subscriptions.erase(strKey))
		{
			return false;
		}
	}
	return SendRequest(request::UnSubscription(param));
}

void CHQService::RestoreSubscriptions()
{
	std::unordered_map<std::string, request::_TyParams> subscriptions;
	{
		std::lock_guard<std::mutex> lock(m_mtx_subscriptions);
		subscriptions = m_subscriptions;
	}
	std::vector<CSecurity> quoteSubscriptions;
	quoteSubscriptions.reserve(subscriptions.size());
	for (const auto& v : subscriptions)
	{
		const std::string& strSecurity = container::vfind(v.second, "security");
		const std::string& strChannel = container::vfind(v.second, "channel");
		CSecurity security = ParseSecurity(strSecurity);
		if (security.IsValid() && ("quote" == strChannel))
		{
			quoteSubscriptions.emplace_back(std::move(security));
		}
		else
		{
			SendRequest(request::Subscription(v.second));
		}
	}
	constexpr std::size_t BatchSize = 200;
	for (std::size_t nOffset = 0; quoteSubscriptions.size() > nOffset; nOffset += BatchSize)
	{
		std::size_t nEnd = (std::min)(quoteSubscriptions.size(), nOffset + BatchSize);
		std::vector<CSecurity> batch(quoteSubscriptions.begin() + nOffset, quoteSubscriptions.begin() + nEnd);
		SendRequest(request::Subscription(batch, std::vector<Channel>{ Channel::quote }));
	}
}

bool CHQService::FindQuote(const CSecurity& info, CQuote& result) const
{
	std::shared_lock lock(m_mtx_quotes);
	return container::try_vfind(m_quotes, info.String(), result);
}

bool CHQService::FindDepth(const CSecurity& info, CMarketDepth& result) const
{
	std::shared_lock lock(m_mtx_depths);
	return container::try_vfind(m_depths, info.String(), result);
}

bool CHQService::FindHistory(const CSecurity& info, MarketBarPeriod period, std::vector<CMarketBar>& result) const
{
	std::shared_lock lock(m_mtx_history);
	return container::try_vfind(m_history, HistoryKey(info.String(), period), result);
}

std::vector<CIndicatorPoint> CHQService::CalculateMovingAverage(const std::vector<CMarketBar>& bars, std::size_t nPeriod) const
{
	std::vector<CIndicatorPoint> values;
	values.reserve(bars.size());
	if (0 == nPeriod)
	{
		return values;
	}

	double fSum = 0.0;
	std::deque<double> window;
	for (const auto& v : bars)
	{
		window.emplace_back(v.m_fClose);
		fSum += v.m_fClose;
		if (nPeriod < window.size())
		{
			fSum -= window.front();
			window.pop_front();
		}
		bool bValid = nPeriod == window.size();
		values.emplace_back(CIndicatorPoint{ v.m_nBeginTime, bValid ? fSum / static_cast<double>(nPeriod) : 0.0, bValid });
	}
	return values;
}

CDataTableView CHQService::GetQuoteTableView() const
{
	return m_quoteTable.GetView();
}

std::vector<CSecurity> CHQService::GetSecurities() const
{
	std::shared_lock lock(m_mtx_securities);
	return m_securities;
}

std::vector<CSectorInfo> CHQService::GetSectors() const
{
	std::shared_lock lock(m_mtx_sectors);
	return m_sectors;
}

CMarketRuntimeMetrics CHQService::GetRuntimeMetrics() const
{
	CMarketRuntimeMetrics metrics;
	metrics.m_nQuoteReceived = m_runtimeMetrics.m_nQuoteReceived.load();
	metrics.m_nQuoteAccepted = m_runtimeMetrics.m_nQuoteAccepted.load();
	metrics.m_nQuoteDuplicates = m_runtimeMetrics.m_nQuoteDuplicates.load();
	metrics.m_nQuoteOutOfOrder = m_runtimeMetrics.m_nQuoteOutOfOrder.load();
	metrics.m_nQuoteSequenceGaps = m_runtimeMetrics.m_nQuoteSequenceGaps.load();
	metrics.m_nQuoteWithoutSequence = m_runtimeMetrics.m_nQuoteWithoutSequence.load();
	metrics.m_nQuoteQueueOverwrites = m_runtimeMetrics.m_nQuoteQueueOverwrites.load();
	metrics.m_nQuoteQueueDrops = m_runtimeMetrics.m_nQuoteQueueDrops.load();
	metrics.m_nPendingQuoteLimit = m_runtimeMetrics.m_nPendingQuoteLimit.load();
	{
		std::lock_guard<std::mutex> lock(m_mtx_pendingQuotes);
		metrics.m_nPendingQuoteCount = m_pendingQuotes.size();
	}
	return metrics;
}

void CHQService::SetPendingQuoteLimit(std::size_t count)
{
	m_runtimeMetrics.m_nPendingQuoteLimit.store((std::max)(std::size_t(1), count));
}

bool CHQService::AcceptQuoteSequence(std::uint64_t sequence)
{
	if (0 == sequence)
	{
		++m_runtimeMetrics.m_nQuoteWithoutSequence;
		return true;
	}
	std::uint64_t previous = m_runtimeMetrics.m_nLastQuoteSequence.load();
	while (true)
	{
		if (sequence == previous)
		{
			++m_runtimeMetrics.m_nQuoteDuplicates;
			return false;
		}
		if (sequence < previous)
		{
			++m_runtimeMetrics.m_nQuoteOutOfOrder;
			return false;
		}
		if (m_runtimeMetrics.m_nLastQuoteSequence.compare_exchange_weak(previous, sequence))
		{
			break;
		}
	}
	if ((0 != previous) && (previous + 1 < sequence))
	{
		m_runtimeMetrics.m_nQuoteSequenceGaps.fetch_add(sequence - previous - 1);
	}
	return true;
}

void CHQService::EnqueueQuote(const CQuote& quote)
{
	std::string strSecurity = quote.m_security.String();
	{
		std::lock_guard<std::mutex> lock(m_mtx_pendingQuotes);
		const auto mIter = m_pendingQuotes.find(strSecurity);
		if (m_pendingQuotes.end() != mIter)
		{
			mIter->second = quote;
			++m_runtimeMetrics.m_nQuoteQueueOverwrites;
		}
		else if (m_runtimeMetrics.m_nPendingQuoteLimit.load() <= m_pendingQuotes.size())
		{
			++m_runtimeMetrics.m_nQuoteQueueDrops;
			return;
		}
		else
		{
			m_pendingQuotes.emplace(std::move(strSecurity), quote);
		}
	}
	m_cv_pendingQuotes.notify_one();
}

void CHQService::QuoteWorkerLoop()
{
	while (!m_bStopping.load())
	{
		decltype(m_pendingQuotes) quotes;
		{
			std::unique_lock<std::mutex> lock(m_mtx_pendingQuotes);
			m_cv_pendingQuotes.wait_for(lock, std::chrono::milliseconds(20), [this]()
			{
				return m_bStopping.load();
			});
			quotes.swap(m_pendingQuotes);
		}
		if (!quotes.empty())
		{
			FlushQuotes(quotes);
		}
	}
}

void CHQService::FlushQuotes(std::unordered_map<std::string, CQuote>& quotes)
{
	CDataTableWriter writer = m_quoteTable.BeginWrite();
	for (const auto& v : quotes)
	{
		const CQuote& quote = v.second;
		const std::string& strSecurity = v.first;
		_TyDataRowId rowId = -1;
		if (!container::try_vfind(m_quoteRowIds, strSecurity, rowId))
		{
			rowId = m_nextQuoteRowId++;
			m_quoteRowIds.emplace(strSecurity, rowId);
			std::string strName;
			std::string strListingStatus;
			std::string strPinyinFullAliases;
			std::string strPinyinShortAliases;
			{
				std::lock_guard<std::mutex> lock(m_mtx_pendingQuotes);
				m_pendingSecurityUpdates.erase(strSecurity);
				container::try_vfind(m_securityNames, strSecurity, strName);
				container::try_vfind(m_securityPinyinFullAliases, strSecurity, strPinyinFullAliases);
				container::try_vfind(m_securityPinyinShortAliases, strSecurity, strPinyinShortAliases);
				MarketState status;
				if (container::try_vfind(m_securityStatuses, strSecurity, status))
				{
					strListingStatus = GetMarketStateString(status);
				}
			}
			double fChange = quote.m_fLastPrice - quote.m_fPreClose;
			double fPercent = 0.0 == quote.m_fPreClose ? 0.0 : fChange * 100.0 / quote.m_fPreClose;
			writer.AddRow(rowId, { strSecurity, strName, quote.m_fLastPrice, fChange, fPercent, quote.m_fPreClose, quote.m_nVolume, std::string(quote.m_bStale ? "已延迟" : "交易中"), quote.m_nSequence, strListingStatus, MarketCategory(strSecurity), strPinyinFullAliases, strPinyinShortAliases });
			continue;
		}

		std::string strName;
		std::string strListingStatus;
		std::string strPinyinFullAliases;
		std::string strPinyinShortAliases;
		bool bMetadataChanged = false;
		{
			std::lock_guard<std::mutex> lock(m_mtx_pendingQuotes);
			bMetadataChanged = 0 != m_pendingSecurityUpdates.erase(strSecurity);
			if (bMetadataChanged)
			{
				container::try_vfind(m_securityNames, strSecurity, strName);
				container::try_vfind(m_securityPinyinFullAliases, strSecurity, strPinyinFullAliases);
				container::try_vfind(m_securityPinyinShortAliases, strSecurity, strPinyinShortAliases);
				MarketState status;
				if (container::try_vfind(m_securityStatuses, strSecurity, status))
				{
					strListingStatus = GetMarketStateString(status);
				}
			}
		}
		double fChange = quote.m_fLastPrice - quote.m_fPreClose;
		double fPercent = 0.0 == quote.m_fPreClose ? 0.0 : fChange * 100.0 / quote.m_fPreClose;
		if (bMetadataChanged)
		{
			writer.SetValue(rowId, static_cast<_TyDataColumnId>(MarketQuoteColumn::Name), strName);
			writer.SetValue(rowId, static_cast<_TyDataColumnId>(MarketQuoteColumn::ListingStatus), strListingStatus);
			writer.SetValue(rowId, static_cast<_TyDataColumnId>(MarketQuoteColumn::Market), MarketCategory(strSecurity));
			writer.SetValue(rowId, static_cast<_TyDataColumnId>(MarketQuoteColumn::PinyinFullAliases), strPinyinFullAliases);
			writer.SetValue(rowId, static_cast<_TyDataColumnId>(MarketQuoteColumn::PinyinShortAliases), strPinyinShortAliases);
		}
		writer.SetValue(rowId, static_cast<_TyDataColumnId>(MarketQuoteColumn::LastPrice), quote.m_fLastPrice);
		writer.SetValue(rowId, static_cast<_TyDataColumnId>(MarketQuoteColumn::Change), fChange);
		writer.SetValue(rowId, static_cast<_TyDataColumnId>(MarketQuoteColumn::Percent), fPercent);
		writer.SetValue(rowId, static_cast<_TyDataColumnId>(MarketQuoteColumn::PreClose), quote.m_fPreClose);
		writer.SetValue(rowId, static_cast<_TyDataColumnId>(MarketQuoteColumn::Volume), quote.m_nVolume);
		writer.SetValue(rowId, static_cast<_TyDataColumnId>(MarketQuoteColumn::Status), std::string(quote.m_bStale ? "已延迟" : "交易中"));
		writer.SetValue(rowId, static_cast<_TyDataColumnId>(MarketQuoteColumn::Sequence), quote.m_nSequence);
	}

	auto [view, changes] = writer.Commit();
	m_pump_quote_table.Notify(CQuoteTableEvent{ std::move(view), std::move(changes) });
}

bool CHQService::OnRequestReply(const CRequest& req)
{
	const auto mIter = m_request_handler.find(req.GetType());
	if (m_request_handler.end() != mIter)
	{
		return mIter->second(req);
	}
	return false;
}

bool CHQService::OnSectorListReply(const CRequest& req)
{
	if (req.GetId() != m_nLatestSectorListRequest.load())
	{
		return false;
	}
	CSectorListEvent ev;
	ev.m_nRequestId = req.GetId();
	ev.m_type = SectorType::industry;
	std::optional<std::pair<int, std::string>> err = req.GetErrorInfo();
	const _TyReqData& message = req.GetData();
	if (err.has_value() && (0 != err->first))
	{
		ev.m_strError = err->second.empty() ? "板块查询失败" : err->second;
	}
	else if (!message.has_sector_list())
	{
		ev.m_strError = "板块响应缺少数据";
	}
	else
	{
		const _TySectorList& data = message.sector_list();
		ev.m_type = static_cast<SectorType>(data.type());
		ev.m_sectors.reserve(data.sectors_size());
		for (const auto& v : data.sectors())
		{
			ev.m_sectors.emplace_back(ParseSector(v, ev.m_type));
		}
		{
			std::unique_lock lock(m_mtx_sectors);
			m_sectors = ev.m_sectors;
		}
	}
	m_pump_sector_list.Notify(ev);
	return true;
}

bool CHQService::OnSectorConstituentsReply(const CRequest& req)
{
	if (req.GetId() != m_nLatestSectorConstituentsRequest.load())
	{
		return false;
	}
	CSectorConstituentsEvent ev;
	ev.m_nRequestId = req.GetId();
	std::optional<std::pair<int, std::string>> err = req.GetErrorInfo();
	const _TyReqData& message = req.GetData();
	if (err.has_value() && (0 != err->first))
	{
		ev.m_strError = err->second.empty() ? "成分股查询失败" : err->second;
	}
	else if (!message.has_sector_constituents())
	{
		ev.m_strError = "成分股响应缺少数据";
	}
	else
	{
		const _TySectorConstituents& data = message.sector_constituents();
		SectorType type = static_cast<SectorType>(data.type());
		ev.m_sector = ParseSector(data.sector(), type);
		ev.m_securities.reserve(data.securities_size());
		for (const auto& v : data.securities())
		{
			CSecurity security(v.security().symbol(), v.name(), ToExchange(v.security().exchange()), ParseMarketState(v.status()));
			security.m_pinyinFullAliases.assign(v.pinyin_full_aliases().begin(), v.pinyin_full_aliases().end());
			security.m_pinyinShortAliases.assign(v.pinyin_short_aliases().begin(), v.pinyin_short_aliases().end());
			if (!security.IsValid())
			{
				continue;
			}
			ev.m_securities.emplace_back(security);
			RegisterSecurity(security);
		}
	}
	m_pump_sector_constituents.Notify(ev);
	return true;
}

bool CHQService::OnSecurityListReply(const CRequest& req)
{
	const _TyReqData& message = req.GetData();
	if (!message.has_security_list())
	{
		return false;
	}
	const _TySecurityList& data = message.security_list();
	CSecurityListEvent ev;
	ev.m_version = data.version();
	ev.m_securities.reserve(data.securities_size());
	for (const auto& v : data.securities())
	{
		std::string strExchange = ExchangeSuffix(v.security().exchange());
		if (strExchange.empty())
		{
			continue;
		}
		CSecurity security(v.security().symbol(), v.name(), ToExchange(v.security().exchange()), ParseMarketState(v.status()));
		security.m_pinyinFullAliases.assign(v.pinyin_full_aliases().begin(), v.pinyin_full_aliases().end());
		security.m_pinyinShortAliases.assign(v.pinyin_short_aliases().begin(), v.pinyin_short_aliases().end());
		ev.m_securities.emplace_back(std::move(security));
	}
	{
		std::unique_lock lock(m_mtx_securities);
		m_securities = ev.m_securities;
	}
	std::vector<CSecurity> subscriptions;
	subscriptions.reserve(ev.m_securities.size());
	for (const auto& v : ev.m_securities)
	{
		if ((Exchange::sse != v.m_hq) && (Exchange::szse != v.m_hq) && (Exchange::bse != v.m_hq))
		{
			continue;
		}
		RegisterSecurity(v);
		if (MarketState::delisted != v.m_status)
		{
			subscriptions.emplace_back(v);
		}
	}
	if (IsAuthenticated())
	{
		constexpr std::size_t BatchSize = 1000;
		std::size_t nSize = subscriptions.size();
		for (std::size_t nOffset = 0; nSize > nOffset; nOffset += BatchSize)
		{
			std::size_t nEnd = (std::min)(nSize, nOffset + BatchSize);
			SubscribeQuotes(std::vector<CSecurity>(subscriptions.begin() + nOffset, subscriptions.begin() + nEnd));
		}
	}
	m_pump_security_list.Notify(ev);
	return true;
}

bool CHQService::OnDepthReply(const CRequest& req)
{
	const _TyReqData& message = req.GetData();
	if (!message.has_depth())
	{
		return false;
	}
	const _TyDepthData& data = message.depth();
	CMarketDepth value;
	value.m_security = CSecurity(data.security().symbol(), ToExchange(data.security().exchange()));
	value.m_nExchangeTime = data.exchange_time_ms();
	value.m_bStale = data.stale();
	value.m_bids.reserve(data.bids_size());
	value.m_asks.reserve(data.asks_size());
	for (const auto& v : data.bids())
	{
		value.m_bids.emplace_back(CPriceLevel{ ScaledPrice(v.price(), v.price_scale()), v.volume() });
	}
	for (const auto& v : data.asks())
	{
		value.m_asks.emplace_back(CPriceLevel{ ScaledPrice(v.price(), v.price_scale()), v.volume() });
	}
	{
		std::unique_lock lock(m_mtx_depths);
		m_depths.insert_or_assign(value.m_security.String(), value);
	}
	m_pump_depth.Notify(value);
	return true;
}

bool CHQService::OnHistoryReply(const CRequest& req)
{
	CMarketHistoryEvent ev;
	{
		std::lock_guard<std::mutex> lock(m_mtx_historyRequests);
		const auto mIter = m_historyRequests.find(req.GetId());
		if (m_historyRequests.end() == mIter)
		{
			return false;
		}
		ev = std::move(mIter->second);
		m_historyRequests.erase(mIter);
	}
	std::optional<std::pair<int, std::string>> err = req.GetErrorInfo();
	const _TyReqData& message = req.GetData();
	if (err.has_value() && (0 != err->first))
	{
		ev.m_strError = err->second.empty() ? "历史行情查询失败" : err->second;
	}
	else if (!message.has_query_response())
	{
		ev.m_strError = "历史行情响应缺少数据";
	}
	else
	{
		const _TyQueryResponse& data = message.query_response();
		MarketBarPeriod period = hqmarket::market::v1::CHANNEL_BAR_1M == data.channel() ? MarketBarPeriod::Minute : MarketBarPeriod::Day;
		if ((ev.m_strSecurity != SecurityName(data.security())) || (ev.m_period != period))
		{
			ev.m_strError = "历史行情响应与请求不匹配";
		}
		else
		{
			ev.m_bars.reserve(data.bars_size());
			for (const auto& v : data.bars())
			{
				ev.m_bars.emplace_back(CMarketBar{ v.begin_time_ms(), ScaledPrice(v.open_price(), v.price_scale()), ScaledPrice(v.high_price(), v.price_scale()), ScaledPrice(v.low_price(), v.price_scale()), ScaledPrice(v.close_price(), v.price_scale()), v.volume(), v.turnover() });
			}
			std::unique_lock lock(m_mtx_history);
			m_history.insert_or_assign(HistoryKey(ev.m_strSecurity, ev.m_period), ev.m_bars);
		}
	}
	m_pump_history.Notify(ev);
	return true;
}

bool CHQService::OnQuoteReply(const CRequest& req)
{
	const _TyReqData& message = req.GetData();
	if (!message.has_quote())
	{
		return false;
	}
	const _TyQuoteData& quote = message.quote();
	++m_runtimeMetrics.m_nQuoteReceived;
	std::uint64_t sequence = ParseSequence(req.GetExtraData());
	if (!AcceptQuoteSequence(sequence))
	{
		return false;
	}
	double fScale = std::pow(10.0, quote.price_scale());
	if (0.0 >= fScale)
	{
		return false;
	}

	CQuote value;
	value.m_security = CSecurity(quote.security().symbol(), ToExchange(quote.security().exchange()));
	value.m_nSequence = sequence;
	value.m_fLastPrice = static_cast<double>(quote.last_price()) / fScale;
	value.m_fPreClose = static_cast<double>(quote.pre_close()) / fScale;
	value.m_nVolume = quote.volume();
	value.m_bStale = quote.stale();
	++m_runtimeMetrics.m_nQuoteAccepted;
	EnqueueQuote(value);
	{
		std::unique_lock lock(m_mtx_quotes);
		m_quotes.insert_or_assign(value.m_security.String(), value);
	}

	m_pump_quote.Notify(value);
	return true;
}

bool CHQService::OnSubscriptionAck(const CRequest& req)
{
	const _TyReqData& message = req.GetData();
	if (!message.has_subscription_ack())
	{
		return false;
	}
	const _TySubscriptionAck& ack = message.subscription_ack();
	std::lock_guard<std::mutex> lock(m_mtx_subscriptions);
	for (const auto& result : ack.results())
	{
		if (hqmarket::market::v1::CHANNEL_QUOTE != result.channel())
		{
			continue;
		}
		CSecurity security(result.security().symbol(), ToExchange(result.security().exchange()));
		std::string strKey = "watchlist:" + security.String();
		if ((_TyRequest::MARKET_UNSUBSCRIBE == req.GetType()) && result.accepted() && !req.GetErrorInfo().has_value())
		{
			m_confirmedSubscriptions.erase(strKey);
			m_failedSubscriptions.erase(strKey);
		}
		else if (result.accepted() && !req.GetErrorInfo().has_value())
		{
			m_confirmedSubscriptions.emplace(strKey);
			m_failedSubscriptions.erase(strKey);
		}
		else
		{
			m_confirmedSubscriptions.erase(strKey);
			m_failedSubscriptions.emplace(strKey);
		}
	}
	return true;
}

void CHQService::SetTraderCenter(CTraderCenter* pCenter)
{
	m_pTraderCenter = pCenter;
}

bool CHQService::IsAuthenticated() const noexcept
{
	return (nullptr != m_pTraderCenter) && m_pTraderCenter->IsAuthenticated();
}

bool CHQService::SendRequest(const CRequest& request)
{
	return (nullptr != m_pTraderCenter) && m_pTraderCenter->SendRequest(request);
}
