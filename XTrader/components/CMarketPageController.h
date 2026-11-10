#ifndef XTRADER_COMPONENTS_CMARKETPAGECONTROLLER_H
#define XTRADER_COMPONENTS_CMARKETPAGECONTROLLER_H

#include "../system/CTraderServices.h"

#include "../service/hqmarket/CHQService.h"
#include <QObject>
#include <QString>
#include <memory>
#include <unordered_set>
#include <vector>

class CUITable;
class CUICurve;
class CDataTableModel;
class CMarketFilterProxyModel;
class QLabel;
class QTimer;
class QWidget;
enum class CurveMode;

enum class MarketTableMode
{
	Watchlist,
	AShare,
	Constituents
};

// Shared page-side market logic, with no layout or search controls.
class CMarketPageController final : public QObject
{
public:
	CMarketPageController(CTraderServices& services, MarketTableMode mode, CUITable* pTable, QObject* pParent);
	~CMarketPageController() override;
	void SetConstituents(const std::vector<CSecurity>& securities);
	void SetMarket(int nIndex);
	QString AddWatchlist(const QString& strInput);
	QString RemoveSelectedWatchlist();
	void SetCharts(QLabel* pTitle, QLabel* pPrice, QLabel* pState, CUICurve* pIntraday, CUICurve* pCandles);
	QWidget* CreateIntradayControls(QWidget* pParent);
	void SetIntradayDays(int nDays);
	void RequestHistory(CurveMode mode);

private:
	void BindService();
	void EnsureSelection();
	void RefreshSelection();
	void RequestMinuteHistory(bool bIncremental);
	void RefreshMinuteHistory();
	void UpdateMinuteCharts();
	void UpdateIntradayState();
	void HandleHistory(std::uint64_t nRequestId, const std::string& strSecurity, MarketBarPeriod period, const std::vector<CMarketBar>& bars, const std::string& strError);
	void OnQuoteTableUpdate(const CDataTableView& view, const CDataChangeSet& changes);
	CSecurity GetSecurity(const QModelIndex& index) const;

	MarketTableMode m_mode;
	std::string m_selectedSecurity;
	std::unordered_set<std::string> m_watchlist;
	std::vector<std::string> m_watchlistOrder;
	std::unordered_set<std::string> m_constituents;
	CUITable* m_table{ nullptr };
	CDataTableModel* m_model{ nullptr };
	CMarketFilterProxyModel* m_proxy{ nullptr };
	CUICurve* m_intraday{ nullptr };
	CUICurve* m_candles{ nullptr };
	QLabel* m_stockTitle{ nullptr };
	QLabel* m_price{ nullptr };
	QLabel* m_chartState{ nullptr };
	std::uint64_t m_quoteTableToken{ 0 };
	std::shared_ptr<class CQuoteTableUpdateState> m_quoteUpdateState;
	std::uint64_t m_historyToken{ 0 };
	std::uint64_t m_minuteRequestId{ 0 };
	std::uint64_t m_dayRequestId{ 0 };
	std::vector<CMarketBar> m_minuteBars;
	std::vector<CMarketBar> m_dayBars;
	QTimer* m_pHistoryTimer{ nullptr };
	int m_nIntradayDays{ 1 };
	bool m_bMinuteIncrementalRequest{ false };

private:
	CTraderServices* m_pServices{ nullptr };
};

#endif
