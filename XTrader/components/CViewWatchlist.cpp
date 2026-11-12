#include "CViewWatchlist.h"
#include "CMarketPageController.h"
#include "ui_CViewWatchlist.h"
#include "../service/watchlist/CWatchlistService.h"
#include "../system/CLoginService.h"

#include <QHeaderView>
#include <QInputDialog>
#include <QMargins>
#include <QMessageBox>
#include <QPointer>
#include <QShortcut>
#include <QTabBar>
#include <QTimer>
#include <QVBoxLayout>

namespace
{
	CurveMode CurveModeFromTab(int nIndex)
	{
		switch (nIndex)
		{
		case 0: return CurveMode::Day;
		case 1: return CurveMode::Week;
		case 2: return CurveMode::Month;
		case 3: return CurveMode::Minute5;
		case 4: return CurveMode::Minute15;
		case 5: return CurveMode::Minute30;
		case 6: return CurveMode::Minute60;
		default: return CurveMode::Day;
		}
	}
}

CViewWatchlist::CViewWatchlist(QWidget* pParent) : QWidget(pParent), m_ui(std::make_unique<Ui::CViewWatchlistClass>())
{
}

void CViewWatchlist::Initialize(CTraderServices& services)
{
	if (nullptr != m_pServices)
	{
		return;
	}
	m_pServices = &services;
	m_ui->setupUi(this);
	m_ui->table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	m_ui->table->sortByColumn(-1, Qt::AscendingOrder);
	m_ui->table->SetSearchColumns({ 0, 1, static_cast<int>(MarketQuoteColumn::PinyinFullAliases) - 1, static_cast<int>(MarketQuoteColumn::PinyinShortAliases) - 1 });
	connect(m_ui->searchEdit, &QLineEdit::textChanged, m_ui->table, &CUITable::Search);
	connect(m_ui->searchEdit, &QLineEdit::returnPressed, this, [this]()
	{
		m_ui->table->Search(m_ui->searchEdit->text());
	});
	connect(m_ui->searchButton, &QPushButton::clicked, this, [this]()
	{
		m_ui->table->Search(m_ui->searchEdit->text());
	});
	connect(m_ui->table, &CUITable::OperationRequested, this, [this](const CTableOperation& operation)
	{
		if (TableOperationKind::ResultsChanged != operation.m_kind)
		{
			return;
		}
		int nCount = operation.m_value.toInt();
		m_ui->countLabel->setText(QString("共%1只").arg(nCount));
	});
	m_controller = std::make_unique<CMarketPageController>(*m_pServices, MarketTableMode::Watchlist, m_ui->table, this);
	QPointer<CViewWatchlist> safeThis(this);
	m_watchlistToken = m_pServices->m_watchlist.Subscribe([safeThis](const CWatchlistEvent& event)
	{
		if (safeThis.isNull())
		{
			return;
		}
		QMetaObject::invokeMethod(safeThis.data(), [safeThis, event]()
		{
			if (safeThis.isNull())
			{
				return;
			}
			if (event.m_bReset)
			{
				safeThis->m_controller->SetWatchlist({});
				return;
			}
			if (!event.m_strError.empty())
			{
				QMessageBox::warning(safeThis.data(), "自选股", QString::fromStdString(event.m_strError));
				return;
			}
			safeThis->m_controller->SetWatchlist(event.m_securities);
			if (_TyRequest::WATCHLIST_ADD == event.m_type)
			{
				QMessageBox::information(safeThis.data(), "自选股", "已添加到自选");
			}
			else if (_TyRequest::WATCHLIST_REMOVE == event.m_type)
			{
				QMessageBox::information(safeThis.data(), "自选股", "已从自选中删除");
			}
		}, Qt::QueuedConnection);
	});
	if (m_pServices->m_login.IsAuthenticated())
	{
		m_pServices->m_watchlist.Query();
	}
	m_controller->SetCharts(m_ui->stockTitle, m_ui->priceLabel, m_ui->chartState, m_ui->intradayCurve, m_ui->candleCurve);
	m_ui->intradayLabel->hide();
	m_ui->intradayFrameLayout->insertWidget(1, m_controller->CreateIntradayControls(m_ui->intradayFrame));
	QMargins intradayMargins = m_ui->intradayFrameLayout->contentsMargins();
	int nIntradayLeft = intradayMargins.left();
	intradayMargins.setLeft(0);
	intradayMargins.setRight(0);
	intradayMargins.setBottom(0);
	m_ui->intradayFrameLayout->setContentsMargins(intradayMargins);
	QMargins stockMargins = m_ui->stockInfoLayout->contentsMargins();
	stockMargins.setLeft(nIntradayLeft);
	m_ui->stockInfoLayout->setContentsMargins(stockMargins);
	m_ui->intradayCurve->setContentsMargins(nIntradayLeft, 0, nIntradayLeft, 0);
	QMargins candleMargins = m_ui->candleFrameLayout->contentsMargins();
	int nCandleLeft = candleMargins.left();
	candleMargins.setLeft(0);
	candleMargins.setRight(0);
	candleMargins.setTop(0);
	candleMargins.setBottom(candleMargins.bottom() / 2);
	m_ui->candleFrameLayout->setContentsMargins(candleMargins);
	m_ui->candleCurve->setContentsMargins(nCandleLeft, 0, nCandleLeft, 0);
	m_ui->chartSplitter->setHandleWidth(24);
	QWidget* pPeriodBar = new QWidget(m_ui->candleFrame);
	pPeriodBar->setObjectName("candlePeriodBar");
	pPeriodBar->setAttribute(Qt::WA_StyledBackground, true);
	pPeriodBar->setFixedHeight(30);
	QVBoxLayout* pPeriodLayout = new QVBoxLayout(pPeriodBar);
	pPeriodLayout->setContentsMargins(0, 1, 0, 1);
	pPeriodLayout->setSpacing(0);
	m_ui->candleFrameLayout->removeWidget(m_ui->periodTabs);
	m_ui->periodTabs->setFixedHeight(28);
	pPeriodLayout->addWidget(m_ui->periodTabs);
	m_ui->candleFrameLayout->insertWidget(0, pPeriodBar);
	m_ui->periodTabs->tabBar()->setDrawBase(false);
	m_ui->periodTabs->addTab(new QWidget(m_ui->periodTabs), "5分");
	m_ui->periodTabs->addTab(new QWidget(m_ui->periodTabs), "15分");
	m_ui->periodTabs->addTab(new QWidget(m_ui->periodTabs), "30分");
	m_ui->periodTabs->addTab(new QWidget(m_ui->periodTabs), "60分");
	m_ui->periodTabs->tabBar()->setExpanding(false);
	connect(m_ui->periodTabs, &QTabWidget::currentChanged, this, [this](int nIndex)
	{
		m_controller->RequestHistory(CurveModeFromTab(nIndex));
	});
	m_ui->splitter->setStretchFactor(0, 50);
	m_ui->splitter->setStretchFactor(1, 50);
	m_ui->chartSplitter->setSizes({ 420, 420 });

	connect(m_ui->addButton, &QPushButton::clicked, this, [this]()
	{
		bool bAccepted = false;
		QString strCode = QInputDialog::getMultiLineText(this, "添加自选股", "输入一个或多个证券代码/名称，以换行、空格或逗号分隔", QString(), &bAccepted).trimmed();
		if (bAccepted && !strCode.isEmpty())
		{
			QString strResult = m_controller->AddWatchlist(strCode);
			if (!strResult.isEmpty())
			{
				QMessageBox::warning(this, "自选股", strResult);
			}
		}
	});
	QShortcut* pDeleteShortcut = new QShortcut(QKeySequence::Delete, m_ui->table);
	connect(pDeleteShortcut, &QShortcut::activated, this, [this]()
	{
		QString strResult = m_controller->RemoveSelectedWatchlist();
		if (!strResult.isEmpty())
		{
			QMessageBox::warning(this, "自选股", strResult);
		}
	});
}

CViewWatchlist::~CViewWatchlist()
{
	if ((nullptr != m_pServices) && (0 != m_watchlistToken))
	{
		m_pServices->m_watchlist.Unsubscribe(m_watchlistToken);
	}
}

void CViewWatchlist::showEvent(QShowEvent* pEvent)
{
	QWidget::showEvent(pEvent);
	if (nullptr == m_pServices)
	{
		return;
	}
	if (!m_bLayoutInitialized)
	{
		m_bLayoutInitialized = true;
		QTimer::singleShot(0, this, [this]()
		{
			int nWidth = m_ui->splitter->width() - m_ui->splitter->handleWidth();
			m_ui->splitter->setSizes({ nWidth / 2, nWidth / 2 });
		});
	}
}
