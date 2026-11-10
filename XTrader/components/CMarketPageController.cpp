#include "CMarketPageController.h"
#include "CUITable.h"
#include "../system/CSession.h"
#include "CDataTableModel.h"
#include "CQuoteTableUpdateState.h"
#include "CUICurve.h"

#include <QAbstractButton>
#include <QAction>
#include <QButtonGroup>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMetaObject>
#include <QPushButton>
#include <QPointer>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QSettings>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>
#include <QWidget>
#include <algorithm>
#include <map>
#include <iterator>

namespace
{
	void MergeBars(std::vector<CMarketBar>& destination, const std::vector<CMarketBar>& source)
	{
		std::map<std::int64_t, CMarketBar> values;
		for (const CMarketBar& bar : destination)
		{
			values.insert_or_assign(bar.m_nBeginTime, bar);
		}
		for (const CMarketBar& bar : source)
		{
			values.insert_or_assign(bar.m_nBeginTime, bar);
		}
		destination.clear();
		destination.reserve(values.size());
		for (const auto& value : values)
		{
			destination.emplace_back(value.second);
		}
	}

	bool IsTradingTime(const QDateTime& value)
	{
		int nDay = value.date().dayOfWeek();
		if ((Qt::Saturday == nDay) || (Qt::Sunday == nDay))
		{
			return false;
		}
		QTime time = value.time();
		return ((QTime(9, 30) <= time) && (QTime(11, 30) >= time)) || ((QTime(13, 0) <= time) && (QTime(15, 0) >= time));
	}

	class CMarketTableDelegate final : public QStyledItemDelegate
	{
	public:
		explicit CMarketTableDelegate(QObject* parent) : QStyledItemDelegate(parent)
		{
		}

	protected:
		void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override
		{
			QStyledItemDelegate::initStyleOption(option, index);
			int nColumn = index.column();
			if (0 == nColumn)
			{
				option->text = index.data().toString().section('.', 0, 0);
			}
			if ((2 <= nColumn) && (5 >= nColumn))
			{
				if (0.0 >= index.siblingAtColumn(2).data().toDouble())
				{
					option->text = "--";
					return;
				}
				double fValue = index.data().toDouble();
				option->text = QString::number(fValue, 'f', 2);
				if (4 == nColumn)
				{
					option->text += "%";
				}
				if (((3 == nColumn) || (4 == nColumn)) && (0.0 < fValue))
				{
					option->text.prepend('+');
				}
			}
			option->displayAlignment = 2 <= nColumn ? Qt::AlignRight | Qt::AlignVCenter : Qt::AlignLeft | Qt::AlignVCenter;
			if ((2 == nColumn) || (3 == nColumn) || (4 == nColumn))
			{
				double fPercent = index.siblingAtColumn(4).data().toDouble();
				QColor color = 0.0 <= fPercent ? QColor("#f04455") : QColor("#00b987");
				option->palette.setColor(QPalette::Text, color);
				option->palette.setColor(QPalette::HighlightedText, color);
			}
		}
	};
} // namespace

class CMarketFilterProxyModel final : public QSortFilterProxyModel
{
public:
	explicit CMarketFilterProxyModel(QObject* parent) : QSortFilterProxyModel(parent)
	{
		setDynamicSortFilter(false);
	}
	int m_hq{ -1 };
	const std::unordered_set<std::string>* m_watchlist{ nullptr };
	const std::unordered_set<std::string>* m_constituents{ nullptr };
	void Refresh()
	{
		invalidateFilter();
	}

protected:
	bool filterAcceptsRow(int nRow, const QModelIndex& parent) const override
	{
		QString strCode = sourceModel()->index(nRow, 0, parent).data().toString();
		if ((nullptr != m_watchlist) && !m_watchlist->contains(strCode.toStdString()))
		{
			return false;
		}
		if ((nullptr != m_constituents) && !m_constituents->contains(strCode.toStdString()))
		{
			return false;
		}
		QString strMarket = sourceModel()->index(nRow, 10, parent).data().toString();
		if (0 == m_hq)
		{
			return ("沪A" == strMarket) || ("深A" == strMarket) || ("创业板" == strMarket) || ("科创板" == strMarket);
		}
		return (0 > m_hq) || (1 == m_hq && "北交所" == strMarket) || (2 == m_hq && "创业板" == strMarket) || (3 == m_hq && "科创板" == strMarket);
	}
};

CMarketPageController::CMarketPageController(CTraderServices& services, MarketTableMode mode, CUITable* pTable, QObject* pParent)
	: QObject(pParent), m_mode(mode), m_table(pTable)
{
	m_pServices = &services;
	m_model = new CDataTableModel(this);
	m_proxy = new CMarketFilterProxyModel(this);
	m_proxy->setSourceModel(m_model);
	if (MarketTableMode::Constituents == mode)
	{
		m_proxy->m_constituents = &m_constituents;
	}
	if (MarketTableMode::Watchlist == mode)
	{
		QSettings settings("Mary", "Mary");
		for (const auto& strSecurity : settings.value("watchlist/securities").toStringList())
		{
			std::string strKey = strSecurity.toStdString();
			if (ParseSecurity(strKey).IsValid() && m_watchlist.emplace(strKey).second)
			{
				m_watchlistOrder.emplace_back(std::move(strKey));
			}
		}
		m_proxy->m_watchlist = &m_watchlist;
	}
	m_table->setModel(m_proxy);
	m_table->setItemDelegate(new CMarketTableDelegate(m_table));
	m_quoteUpdateState = std::make_shared<CQuoteTableUpdateState>();
	QTimer* pQuoteTimer = new QTimer(this);
	pQuoteTimer->setInterval(250);
	connect(pQuoteTimer, &QTimer::timeout, this, [this]()
	{
		CDataTableView view;
		CDataChangeSet changes;
		if (m_quoteUpdateState->Take(view, changes))
		{
			OnQuoteTableUpdate(view, changes);
		}
	});
	pQuoteTimer->start();
	connect(m_table, &CUITable::RowSelected, this, [this]()
	{
		RefreshSelection();
	});
	connect(m_table, &CUITable::ResultsChanged, this, [this]()
	{
		EnsureSelection();
	});
	BindService();
	m_pHistoryTimer = new QTimer(this);
	m_pHistoryTimer->setInterval(60000);
	connect(m_pHistoryTimer, &QTimer::timeout, this, [this]()
	{
		RefreshMinuteHistory();
	});
	m_pHistoryTimer->start();
}

CMarketPageController::~CMarketPageController()
{
	CHQService& service = m_pServices->m_hq;
	if (0 != m_quoteTableToken)
	{
		service.RemoveQuoteTableHandler(m_quoteTableToken);
		m_quoteTableToken = 0;
	}
	if (0 != m_historyToken)
	{
		service.RemoveHistoryHandler(m_historyToken);
		m_historyToken = 0;
	}
	if (nullptr != m_table)
	{
		QSignalBlocker tableSignals(m_table);
		m_table->setModel(nullptr);
	}
}

void CMarketPageController::SetCharts(QLabel* pTitle, QLabel* pPrice, QLabel* pState, CUICurve* pIntraday, CUICurve* pCandles)
{
	m_stockTitle = pTitle;
	m_price = pPrice;
	m_chartState = pState;
	m_intraday = pIntraday;
	m_candles = pCandles;
	m_intraday->SetMode(CurveMode::Intraday);
	RefreshSelection();
}

QWidget* CMarketPageController::CreateIntradayControls(QWidget* pParent)
{
	QWidget* pControls = new QWidget(pParent);
	pControls->setObjectName("intradayPeriodBar");
	pControls->setAttribute(Qt::WA_StyledBackground, true);
	pControls->setFixedHeight(30);
	QHBoxLayout* pLayout = new QHBoxLayout(pControls);
	pLayout->setContentsMargins(0, 1, 0, 1);
	pLayout->setSpacing(0);
	QButtonGroup* pGroup = new QButtonGroup(pControls);
	pGroup->setExclusive(true);
	for (int nDays = 1; 5 >= nDays; ++nDays)
	{
		QPushButton* pButton = new QPushButton(QString("%1日").arg(nDays), pControls);
		pButton->setCheckable(true);
		pButton->setProperty("chartPeriodButton", true);
		pButton->setProperty("selected", 1 == nDays);
		pButton->setFixedSize(48, 28);
		pGroup->addButton(pButton, nDays);
		pLayout->addWidget(pButton);
		if (1 == nDays)
		{
			pButton->setChecked(true);
		}
	}
	QToolButton* pMore = new QToolButton(pControls);
	pMore->setObjectName("intradayMoreButton");
	pMore->setText("6日");
	pMore->setProperty("selected", false);
	pMore->setPopupMode(QToolButton::InstantPopup);
	pMore->setFixedSize(58, 28);
	connect(pGroup, &QButtonGroup::idClicked, this, [this, pGroup, pMore](int nDays)
	{
		for (QAbstractButton* pButton : pGroup->buttons())
		{
			pButton->setProperty("selected", pGroup->id(pButton) == nDays);
			pButton->style()->unpolish(pButton);
			pButton->style()->polish(pButton);
		}
		pMore->setProperty("selected", false);
		pMore->style()->unpolish(pMore);
		pMore->style()->polish(pMore);
		SetIntradayDays(nDays);
	});
	QMenu* pMenu = new QMenu(pMore);
	pMenu->setObjectName("intradayDaysMenu");
	connect(pMenu, &QMenu::aboutToShow, pMore, [pMenu, pMore]()
	{
		pMenu->setFixedWidth(pMore->width());
	});
	for (int nDays = 6; 10 >= nDays; ++nDays)
	{
		QAction* pAction = pMenu->addAction(QString("%1日").arg(nDays));
		connect(pAction, &QAction::triggered, this, [this, pGroup, pMore, nDays]()
		{
			pGroup->setExclusive(false);
			for (QAbstractButton* pButton : pGroup->buttons())
			{
				pButton->setChecked(false);
				pButton->setProperty("selected", false);
				pButton->style()->unpolish(pButton);
				pButton->style()->polish(pButton);
			}
			pGroup->setExclusive(true);
			pMore->setText(QString("%1日").arg(nDays));
			pMore->setProperty("selected", true);
			pMore->style()->unpolish(pMore);
			pMore->style()->polish(pMore);
			SetIntradayDays(nDays);
		});
	}
	pMore->setMenu(pMenu);
	pLayout->addWidget(pMore);
	pLayout->addStretch(1);
	return pControls;
}

void CMarketPageController::SetIntradayDays(int nDays)
{
	m_nIntradayDays = (std::clamp)(nDays, 1, 10);
	if (nullptr != m_intraday)
	{
		m_intraday->SetIntradayDays(m_nIntradayDays);
		UpdateIntradayState();
	}
}

QString CMarketPageController::AddWatchlist(const QString& strInput)
{
	if (0 == m_model->rowCount())
	{
		return "证券清单尚未加载，请稍后重试";
	}
	std::vector<std::string> matches;
	int nRowCount = m_model->rowCount();
	for (int nRow = 0; nRowCount > nRow; ++nRow)
	{
		QString strKey = m_model->index(nRow, 0).data().toString();
		QString strName = m_model->index(nRow, 1).data().toString();
		QString strPinyinFullAliases = m_model->index(nRow, static_cast<int>(MarketQuoteColumn::PinyinFullAliases) - 1).data().toString();
		QString strPinyinShortAliases = m_model->index(nRow, static_cast<int>(MarketQuoteColumn::PinyinShortAliases) - 1).data().toString();
		if ((0 == QString::compare(strKey, strInput, Qt::CaseInsensitive)) || (strKey.section('.', 0, 0) == strInput) || strName.contains(strInput, Qt::CaseInsensitive) || strPinyinFullAliases.contains(strInput, Qt::CaseInsensitive) || strPinyinShortAliases.contains(strInput, Qt::CaseInsensitive))
		{
			matches.emplace_back(strKey.toStdString());
		}
	}
	if (matches.empty())
	{
		return "未找到匹配的证券";
	}
	if (1 != matches.size())
	{
		return "匹配到多个证券，请输入更完整的名称、拼音或市场代码";
	}
	if (!m_watchlist.emplace(matches.front()).second)
	{
		return "该证券已在自选中";
	}
	m_watchlistOrder.emplace_back(matches.front());
	QStringList securities;
	for (const std::string& strSecurity : m_watchlistOrder)
	{
		securities.append(QString::fromStdString(strSecurity));
	}
	QSettings("Mary", "Mary").setValue("watchlist/securities", securities);
	m_proxy->Refresh();
	m_table->Update();
	return "已添加到自选";
}

QString CMarketPageController::RemoveSelectedWatchlist()
{
	CSecurity security = GetSecurity(m_table->currentIndex());
	if (!security.IsValid() || (0 == m_watchlist.erase(security.String())))
	{
		return "请先选择要删除的自选证券";
	}
	m_watchlistOrder.erase(std::remove(m_watchlistOrder.begin(), m_watchlistOrder.end(), security.String()), m_watchlistOrder.end());
	QStringList securities;
	for (const std::string& strSecurity : m_watchlistOrder)
	{
		securities.append(QString::fromStdString(strSecurity));
	}
	QSettings("Mary", "Mary").setValue("watchlist/securities", securities);
	m_proxy->Refresh();
	m_table->Update();
	return "已从自选中删除";
}

void CMarketPageController::BindService()
{
	CHQService& service = m_pServices->m_hq;
	service.Initialize();
	std::shared_ptr<CQuoteTableUpdateState> quoteUpdateState = m_quoteUpdateState;
	m_quoteTableToken = service.AddQuoteTableHandler([quoteUpdateState](const CDataTableView& view, const CDataChangeSet& changes)
	{
		quoteUpdateState->Publish(view, changes);
	});
	QPointer<CMarketPageController> safeThis(this);
	m_historyToken = service.AddHistoryHandler([safeThis](std::uint64_t nId, const std::string& strSecurity, MarketBarPeriod period, const std::vector<CMarketBar>& bars, const std::string& strError)
	{
		if (safeThis.isNull())
		{
			return;
		}
		QMetaObject::invokeMethod(safeThis.data(), [safeThis, nId, strSecurity, period, bars, strError]()
		{
			if (!safeThis.isNull())
			{
				safeThis->HandleHistory(nId, strSecurity, period, bars, strError);
			}
		}, Qt::QueuedConnection);
	});
	OnQuoteTableUpdate(service.GetQuoteTableView(), CDataChangeSet{});
	m_pServices->m_login.RegisterStateHandler([safeThis](SessionState state, const std::string& strMessage)
	{
		if (!safeThis.isNull())
		{
			QMetaObject::invokeMethod(safeThis.data(), [safeThis, state, strMessage]()
			{
				if (safeThis.isNull())
				{
					return;
				}
				if (SessionState::Ready == state)
				{
					safeThis->m_selectedSecurity.clear();
					safeThis->RefreshSelection();
				}
				else if (nullptr != safeThis->m_chartState)
				{
					safeThis->m_chartState->setText(QString::fromStdString(strMessage.empty() ? "连接不可用" : strMessage));
					safeThis->m_minuteRequestId = 0;
					safeThis->m_dayRequestId = 0;
					safeThis->m_intraday->Clear();
					safeThis->m_candles->Clear();
				}
			}, Qt::QueuedConnection);
		}
	});
}

void CMarketPageController::SetConstituents(const std::vector<CSecurity>& securities)
{
	m_constituents.clear();
	m_constituents.reserve(securities.size());
	for (const auto& security : securities)
	{
		m_constituents.emplace(security.String());
	}
	m_proxy->Refresh();
	m_table->Update();
	EnsureSelection();
}

void CMarketPageController::SetMarket(int nIndex)
{
	m_proxy->m_hq = nIndex;
	m_proxy->Refresh();
	m_table->Update();
	EnsureSelection();
}

void CMarketPageController::EnsureSelection()
{
	if (!m_table->currentIndex().isValid() && (0 < m_table->ResultCount()))
	{
		m_table->selectRow(0);
	}
	RefreshSelection();
}

CSecurity CMarketPageController::GetSecurity(const QModelIndex& index) const
{
	if (!index.isValid())
	{
		return CSecurity();
	}
	CSecurity security = ParseSecurity(index.siblingAtColumn(0).data().toString().toStdString());
	security.m_strName = index.siblingAtColumn(1).data().toString().toStdString();
	return security;
}

void CMarketPageController::OnQuoteTableUpdate(const CDataTableView& view, const CDataChangeSet& changes)
{
	m_model->SetView(view, changes);
	bool bFilterChanged = changes.m_bStructureChanged || !changes.m_insertedRows.empty() || !changes.m_deletedRows.empty();
	if (!bFilterChanged)
	{
		bFilterChanged = std::any_of(changes.m_changedCells.begin(), changes.m_changedCells.end(), [](const CDataCellChange& change)
		{
			return (static_cast<_TyDataColumnId>(MarketQuoteColumn::Security) == change.m_columnId) ||
				   (static_cast<_TyDataColumnId>(MarketQuoteColumn::Name) == change.m_columnId) ||
				   (static_cast<_TyDataColumnId>(MarketQuoteColumn::Market) == change.m_columnId) ||
				   (static_cast<_TyDataColumnId>(MarketQuoteColumn::PinyinFullAliases) == change.m_columnId) ||
				   (static_cast<_TyDataColumnId>(MarketQuoteColumn::PinyinShortAliases) == change.m_columnId);
		});
	}
	int nColumnCount = m_model->columnCount();
	for (int nColumn = 0; nColumnCount > nColumn; ++nColumn)
	{
		m_table->setColumnHidden(nColumn, MarketTableMode::Constituents == m_mode ? ((5 == nColumn) || (7 <= nColumn)) : ((3 == nColumn) || (5 <= nColumn)));
	}
	if (bFilterChanged)
	{
		m_proxy->Refresh();
		m_table->Update();
	}
	EnsureSelection();
}

void CMarketPageController::RefreshSelection()
{
	if (nullptr == m_intraday)
	{
		return;
	}
	QModelIndex current = m_table->currentIndex();
	CSecurity security = GetSecurity(current);
	if (!security.IsValid())
	{
		m_stockTitle->setText("没有匹配的证券");
		m_selectedSecurity.clear();
		m_price->setText("--");
		m_chartState->setText(m_pServices->m_login.IsAuthenticated() ? "暂无证券数据" : "连接不可用");
		m_intraday->Clear();
		m_candles->Clear();
		m_minuteBars.clear();
		m_dayBars.clear();
		m_minuteRequestId = 0;
		m_dayRequestId = 0;
		return;
	}
	m_stockTitle->setText(QString::fromStdString(security.m_strName + "  " + security.String()));
	double fPrice = current.siblingAtColumn(2).data().toDouble();
	double fPercent = current.siblingAtColumn(4).data().toDouble();
	CQuote quote;
	double fReferencePrice = 0.0;
	if (m_pServices->m_hq.FindQuote(security, quote))
	{
		fReferencePrice = quote.m_fPreClose;
	}
	if ((0.0 >= fReferencePrice) && (0.0 < fPrice) && (-100.0 < fPercent))
	{
		fReferencePrice = fPrice / (1.0 + fPercent / 100.0);
	}
	m_intraday->SetReferencePrice(fReferencePrice);
	m_price->setText(0.0 < fPrice ? QString("%1    %2%3%").arg(fPrice, 0, 'f', 2).arg(0 <= fPercent ? "+" : "").arg(fPercent, 0, 'f', 2) : "--");
	m_price->setProperty("rising", 0 <= fPercent);
	m_price->style()->unpolish(m_price);
	m_price->style()->polish(m_price);
	if (m_selectedSecurity == security.String())
	{
		return;
	}
	m_selectedSecurity = security.String();
	m_minuteBars.clear();
	m_dayBars.clear();
	m_minuteRequestId = 0;
	m_dayRequestId = 0;
	m_chartState->setText("正在查询历史行情");
	RequestHistory(CurveMode::Intraday);
	RequestHistory(m_candles->GetMode());
}

void CMarketPageController::RequestHistory(CurveMode mode)
{
	CSecurity security = GetSecurity(m_table->currentIndex());
	if (!security.IsValid())
	{
		return;
	}
	bool bMinute = (CurveMode::Intraday == mode) || (CurveMode::Minute5 == mode) || (CurveMode::Minute15 == mode) || (CurveMode::Minute30 == mode) || (CurveMode::Minute60 == mode);
	CUICurve* curve = bMinute ? m_intraday : m_candles;
	if (CurveMode::Intraday != mode)
	{
		curve = m_candles;
	}
	if (nullptr == curve)
	{
		return;
	}
	curve->SetMode(mode);
	if (bMinute)
	{
		if (!m_minuteBars.empty())
		{
			curve->SetBars(m_minuteBars);
			return;
		}
		RequestMinuteHistory(false);
		return;
	}
	if (!m_dayBars.empty())
	{
		curve->SetBars(m_dayBars);
		return;
	}
	curve->Clear();
	m_dayRequestId = 0;
	QDateTime now = QDateTime::currentDateTime();
	std::int64_t nBegin = now.addYears(-10).toMSecsSinceEpoch();
	if (!m_pServices->m_hq.QueryHistory(security, MarketBarPeriod::Day, nBegin, now.toMSecsSinceEpoch(), &m_dayRequestId))
	{
		m_chartState->setText("历史查询未发送，请检查连接");
	}
}

void CMarketPageController::RequestMinuteHistory(bool bIncremental)
{
	if (0 != m_minuteRequestId)
	{
		return;
	}
	CSecurity security = GetSecurity(m_table->currentIndex());
	if (!security.IsValid())
	{
		return;
	}
	QDateTime now = QDateTime::currentDateTime();
	std::int64_t nBegin = now.addDays(-21).toMSecsSinceEpoch();
	if (bIncremental && !m_minuteBars.empty())
	{
		nBegin = m_minuteBars.back().m_nBeginTime + 1;
	}
	m_bMinuteIncrementalRequest = bIncremental;
	if (!m_pServices->m_hq.QueryHistory(security, MarketBarPeriod::Minute, nBegin, now.toMSecsSinceEpoch(), &m_minuteRequestId))
	{
		m_minuteRequestId = 0;
		m_bMinuteIncrementalRequest = false;
		m_chartState->setText("历史查询未发送，请检查连接");
	}
}

void CMarketPageController::RefreshMinuteHistory()
{
	if ((nullptr == m_table) || !m_table->isVisible() || m_selectedSecurity.empty() || !IsTradingTime(QDateTime::currentDateTime()))
	{
		return;
	}
	RequestMinuteHistory(true);
}

void CMarketPageController::UpdateMinuteCharts()
{
	if (nullptr != m_intraday)
	{
		m_intraday->SetBars(m_minuteBars);
	}
	if ((nullptr != m_candles) && m_candles->IsMinuteMode())
	{
		m_candles->SetBars(m_minuteBars);
	}
	UpdateIntradayState();
}

void CMarketPageController::UpdateIntradayState()
{
	if ((nullptr == m_intraday) || (nullptr == m_chartState))
	{
		return;
	}
	int nAvailable = m_intraday->AvailableTradingDays();
	if (0 == nAvailable)
	{
		m_chartState->setText("暂无分时行情");
		return;
	}
	m_chartState->setText(nAvailable < m_nIntradayDays ? QString("仅有 %1 日数据").arg(nAvailable) : QString("%1 日分时已加载").arg(m_nIntradayDays));
}

void CMarketPageController::HandleHistory(std::uint64_t nId, const std::string& strSecurity, MarketBarPeriod period, const std::vector<CMarketBar>& bars, const std::string& strError)
{
	if (strSecurity != GetSecurity(m_table->currentIndex()).String())
	{
		return;
	}
	bool bMinute = MarketBarPeriod::Minute == period;
	if ((0 == nId) || (nId != (bMinute ? m_minuteRequestId : m_dayRequestId)))
	{
		return;
	}
	CUICurve* curve = bMinute ? m_intraday : m_candles;
	if (!strError.empty())
	{
		bool bIncremental = bMinute && m_bMinuteIncrementalRequest;
		if (!bIncremental)
		{
			curve->Clear();
		}
		if (bMinute)
		{
			m_minuteRequestId = 0;
			m_bMinuteIncrementalRequest = false;
			m_chartState->setText(bIncremental ? "分时自动更新失败" : QString::fromStdString(strError));
		}
		else
		{
			m_dayRequestId = 0;
			m_chartState->setText(QString::fromStdString(strError));
		}
		return;
	}
	if (bMinute)
	{
		MergeBars(m_minuteBars, bars);
		m_minuteRequestId = 0;
		m_bMinuteIncrementalRequest = false;
		UpdateMinuteCharts();
	}
	else
	{
		m_dayBars = bars;
		m_dayRequestId = 0;
		if (!m_candles->IsMinuteMode())
		{
			m_candles->SetBars(m_dayBars);
		}
	}
}
