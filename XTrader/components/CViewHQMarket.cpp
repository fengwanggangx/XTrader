#include "CViewHQMarket.h"
#include "CUIStyle.h"
#include "CDataTableModel.h"
#include "CQuoteTableUpdateState.h"
#include "../system/CSession.h"
#include "ui_CViewHQMarket.h"

#include <QHeaderView>
#include <QMetaObject>
#include <QPushButton>
#include <QTabBar>
#include <QTableWidgetItem>
#include <QTimer>
#include <cmath>
#include <algorithm>
#include <functional>

CViewHQMarket::CViewHQMarket(QWidget* pParent) : QWidget(pParent), m_ui(std::make_unique<Ui::CViewHQMarketClass>())
{
}

void CViewHQMarket::Initialize(CTraderServices& services)
{
	if (nullptr != m_pServices)
	{
		return;
	}
	m_pServices = &services;
	m_ui->setupUi(this);
	m_ui->watchlistPage->Initialize(services);
	m_ui->asharePage->Initialize(services);
	m_ui->constituentsPage->Initialize(services);
	m_ui->marketTabs->tabBar()->setDrawBase(false);
	m_ui->marketTabs->tabBar()->setExpanding(false);
	m_ui->marketTabs->tabBar()->setUsesScrollButtons(true);
	m_nFixedTabCount = m_ui->marketTabs->count();
	connect(m_ui->marketTabs, &QTabWidget::tabCloseRequested, this, [this](int nIndex)
	{
		if (m_nFixedTabCount > nIndex)
		{
			return;
		}
		bool bCurrent = m_ui->marketTabs->currentIndex() == nIndex;
		QWidget* pPage = m_ui->marketTabs->widget(nIndex);
		m_ui->marketTabs->removeTab(nIndex);
		pPage->deleteLater();
		if (bCurrent)
		{
			m_ui->marketTabs->setCurrentIndex(m_ui->marketTabs->count() > m_nFixedTabCount ? (std::min)(nIndex, m_ui->marketTabs->count() - 1) : 3);
		}
	});
	m_ui->rankingTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	m_ui->rankingTable->horizontalHeader()->setFixedHeight(24);

	CHQService& service = m_pServices->m_hq;
	service.Initialize();

	RefreshQuotes(service.GetQuoteTableView());

	QPointer<CViewHQMarket> pInstance(this);
	m_quoteUpdateState = std::make_shared<CQuoteTableUpdateState>();
	std::shared_ptr<CQuoteTableUpdateState> quoteUpdateState = m_quoteUpdateState;
	m_nQuoteTableToken = service.AddQuoteTableHandler([quoteUpdateState](const CDataTableView& view, const CDataChangeSet& changes)
	{
		quoteUpdateState->Publish(view, changes);
	});
	QTimer* pQuoteTimer = new QTimer(this);
	pQuoteTimer->setInterval(250);
	connect(pQuoteTimer, &QTimer::timeout, this, [this]()
	{
		CDataTableView view;
		CDataChangeSet changes;
		if (m_quoteUpdateState->Take(view, changes))
		{
			RefreshQuotes(view);
		}
	});
	pQuoteTimer->start();
	m_nSectorListToken = service.AddSectorListHandler(std::bind_front(&CViewHQMarket::OnSectorListUpdate, pInstance));
	m_nSectorConstituentsToken = service.AddSectorConstituentsHandler(std::bind_front(&CViewHQMarket::OnSectorConstituentsUpdate, pInstance));
	m_pServices->m_login.RegisterStateHandler(std::bind_front(&CViewHQMarket::OnSessionStateChanged, pInstance));

	connect(m_ui->marketTabs, &QTabWidget::currentChanged, this, &CViewHQMarket::OnTabChanged);
	connect(m_ui->rankingTable, &QTableWidget::cellClicked, this, &CViewHQMarket::OnRankingCellClicked);
	OnTabChanged(m_ui->marketTabs->currentIndex());
}

void CViewHQMarket::OnSectorListUpdate(QPointer<CViewHQMarket> pInstance, const CSectorListEvent& ev)
{
	if (pInstance.isNull())
	{
		return;
	}
	QMetaObject::invokeMethod(pInstance.data(), [pInstance, ev]()
	{
		if (!pInstance.isNull())
		{
			pInstance->RefreshSectors(ev);
		}
	}, Qt::QueuedConnection);
}

void CViewHQMarket::OnSectorConstituentsUpdate(QPointer<CViewHQMarket> pInstance, const CSectorConstituentsEvent& ev)
{
	if (pInstance.isNull())
	{
		return;
	}
	QMetaObject::invokeMethod(pInstance.data(), [pInstance, ev]()
	{
		if (!pInstance.isNull())
		{
			pInstance->RefreshConstituents(ev);
		}
	}, Qt::QueuedConnection);
}

void CViewHQMarket::OnSessionStateChanged(QPointer<CViewHQMarket> pInstance, SessionState state, const std::string&)
{
	if ((SessionState::Ready != state) || pInstance.isNull())
	{
		return;
	}
	QMetaObject::invokeMethod(pInstance.data(), [pInstance]()
	{
		if (!pInstance.isNull() && (0 == pInstance->m_ui->marketTabs->currentIndex()))
		{
			pInstance->m_bOverviewRequested = false;
			pInstance->RequestSectors();
		}
	}, Qt::QueuedConnection);
}

CViewHQMarket::~CViewHQMarket()
{
	if (nullptr == m_pServices)
	{
		return;
	}
	m_pServices->m_hq.RemoveQuoteTableHandler(m_nQuoteTableToken);
	m_pServices->m_hq.RemoveSectorListHandler(m_nSectorListToken);
	m_pServices->m_hq.RemoveSectorConstituentsHandler(m_nSectorConstituentsToken);
}

void CViewHQMarket::OnTabChanged(int idx)
{
	if (0 == idx) // 概览
	{
		if (!m_bIndicesRequested)
		{
			m_bIndicesRequested = true;
			m_pServices->m_hq.SubscribeQuote(CSecurity("000001", Exchange::sse));
			m_pServices->m_hq.SubscribeQuote(CSecurity("399001", Exchange::szse));
			m_pServices->m_hq.SubscribeQuote(CSecurity("399006", Exchange::szse));
			RequestSectors();
		}
	}
	else if ((1 == idx) || (3 == idx)) // 自选股、A股
	{
		if (!m_bSecuritiesRequested)
		{
			m_bSecuritiesRequested = true;
			m_pServices->m_hq.QuerySecurities();
		}
	}
	else
	{
		m_bOverviewRequested = false;
	}
}

void CViewHQMarket::OnRankingCellClicked(int nRow, int)
{
	QTableWidgetItem* pItem = m_ui->rankingTable->item(nRow, 1);
	if (nullptr != pItem)
	{
		SelectSector(pItem->data(Qt::UserRole).toString());
	}
}

void CViewHQMarket::OnSectorButtonClicked()
{
	QPushButton* pButton = qobject_cast<QPushButton*>(sender());
	if (nullptr != pButton)
	{
		SelectSector(pButton->property("sectorCode").toString());
	}
}

void CViewHQMarket::RequestSectors()
{
	if (m_bOverviewRequested)
	{
		return;
	}
	if (!m_pServices->m_login.IsAuthenticated())
	{
		m_ui->sectorState->setText("等待行情服务连接");
		return;
	}
	m_bOverviewRequested = true;
	m_ui->sectorState->setText("正在加载行业板块…");
	m_pServices->m_hq.QuerySectors(SectorType::industry);
}

void CViewHQMarket::RefreshSectors(const CSectorListEvent& ev)
{
	while (nullptr != m_ui->sectorGrid->itemAt(0))
	{
		QLayoutItem* pItem = m_ui->sectorGrid->takeAt(0);
		delete pItem->widget();
		delete pItem;
	}
	m_ui->rankingTable->setRowCount(0);
	if (!ev.m_strError.empty())
	{
		m_ui->sectorState->setText(QString::fromStdString(ev.m_strError));
		return;
	}
	m_sectors = ev.m_sectors;
	std::sort(m_sectors.begin(), m_sectors.end(), [](const CSectorInfo& left, const CSectorInfo& right)
	{
		return left.m_fChangePercent > right.m_fChangePercent;
	});
	if (m_sectors.empty())
	{
		m_ui->sectorState->setText("暂无行业板块数据");
		return;
	}
	m_ui->sectorState->setText(QString("共%1个行业").arg(m_sectors.size()));
	int nTileCount = (std::min)(9, static_cast<int>(m_sectors.size()));
	for (int nIndex = 0; nTileCount > nIndex; ++nIndex)
	{
		const CSectorInfo& sector = m_sectors[static_cast<std::size_t>(nIndex)];
		QPushButton* pButton = new QPushButton(QString("%1\n%2%3%").arg(QString::fromStdString(sector.m_strName), 0.0 <= sector.m_fChangePercent ? "+" : "", QString::number(sector.m_fChangePercent, 'f', 2)), this);
		pButton->setCheckable(true);
		pButton->setProperty("sectorTile", true);
		pButton->setProperty("negative", 0.0 > sector.m_fChangePercent);
		pButton->setProperty("sectorCode", QString::fromStdString(sector.m_strCode));
		connect(pButton, &QPushButton::clicked, this, &CViewHQMarket::OnSectorButtonClicked);
		m_ui->sectorGrid->addWidget(pButton, nIndex / 5, nIndex % 5);
	}
	int nRankingCount = (std::min)(5, static_cast<int>(m_sectors.size()));
	m_ui->rankingTable->setRowCount(nRankingCount);
	for (int nIndex = 0; nIndex < nRankingCount; ++nIndex)
	{
		const CSectorInfo& sector = m_sectors[static_cast<std::size_t>(nIndex)];
		QTableWidgetItem* pRank = new QTableWidgetItem(QString::number(nIndex + 1));
		QTableWidgetItem* pName = new QTableWidgetItem(QString::fromStdString(sector.m_strName));
		QTableWidgetItem* pPercent = new QTableWidgetItem(QString("%1%2%").arg(0.0 <= sector.m_fChangePercent ? "+" : "", QString::number(sector.m_fChangePercent, 'f', 2)));
		pName->setData(Qt::UserRole, QString::fromStdString(sector.m_strCode));
		m_ui->rankingTable->setItem(nIndex, 0, pRank);
		m_ui->rankingTable->setItem(nIndex, 1, pName);
		m_ui->rankingTable->setItem(nIndex, 2, pPercent);
	}
	SelectSector(m_strSelectedSectorCode.isEmpty() ? QString::fromStdString(m_sectors.front().m_strCode) : m_strSelectedSectorCode);
}

void CViewHQMarket::SelectSector(const QString& strSectorCode)
{
	if (strSectorCode.isEmpty())
	{
		return;
	}
	m_strSelectedSectorCode = strSectorCode;
	m_ui->rankingTable->clearSelection();
	for (int nIndex = 0; m_ui->rankingTable->rowCount() > nIndex; ++nIndex)
	{
		QTableWidgetItem* pName = m_ui->rankingTable->item(nIndex, 1);
		if ((nullptr != pName) && (strSectorCode == pName->data(Qt::UserRole).toString()))
		{
			m_ui->rankingTable->selectRow(nIndex);
			break;
		}
	}
	for (int nIndex = 0; m_ui->sectorGrid->count() > nIndex; ++nIndex)
	{
		QPushButton* pButton = qobject_cast<QPushButton*>(m_ui->sectorGrid->itemAt(nIndex)->widget());
		if (nullptr != pButton)
		{
			pButton->setChecked(strSectorCode == pButton->property("sectorCode").toString());
		}
	}
	m_ui->sectorState->setText("正在加载成分股…");
	m_pServices->m_hq.QuerySectorConstituents(SectorType::industry, strSectorCode.toStdString());
}

void CViewHQMarket::RefreshConstituents(const CSectorConstituentsEvent& ev)
{
	if (!ev.m_strError.empty())
	{
		m_ui->sectorState->setText(QString::fromStdString(ev.m_strError));
		return;
	}
	if (m_strSelectedSectorCode != QString::fromStdString(ev.m_sector.m_strCode))
	{
		return;
	}
	m_ui->sectorState->setText(QString("%1 · %2只成分股").arg(QString::fromStdString(ev.m_sector.m_strName)).arg(ev.m_securities.size()));
	m_ui->constituentsPage->SetSector(ev.m_sector, ev.m_securities);
	m_pServices->m_hq.SubscribeQuotes(ev.m_securities);
}

void CViewHQMarket::RefreshQuotes(const CDataTableView& view)
{
	CDataTableModel model;
	model.SetView(view, CDataChangeSet{});
	QList<QLabel*> prices{ m_ui->shanghaiPrice, m_ui->shenzhenPrice, m_ui->chinextPrice };
	QList<QLabel*> changes{ m_ui->shanghaiChange, m_ui->shenzhenChange, m_ui->chinextChange };
	QStringList indices{ "000001.SSE", "399001.SZSE", "399006.SZSE" };
	for (const auto& label : prices)
	{
		label->setText("--");
	}
	for (const auto& label : changes)
	{
		label->setText("--");
	}
	int nRising = 0;
	int nFalling = 0;
	int nFlat = 0;
	QVector<int> distribution(13, 0);
	for (int nRow = 0; model.rowCount() > nRow; ++nRow)
	{
		QString strSecurity = model.index(nRow, 0).data().toString();
		double fPrice = model.index(nRow, 2).data().toDouble();
		double fPreClose = model.index(nRow, 5).data().toDouble();
		if ((0.0 >= fPrice) || (0.0 >= fPreClose) || ("交易中" != model.index(nRow, 7).data().toString()))
		{
			continue;
		}
		double fPercent = model.index(nRow, 4).data().toDouble();
		int nIndex = static_cast<int>(indices.indexOf(strSecurity));
		if (0 <= nIndex)
		{
			prices[nIndex]->setText(QString::number(fPrice, 'f', 2));
			changes[nIndex]->setText(QString("%1%2%  %1%3").arg(0.0 <= fPercent ? "+" : "").arg(fPercent, 0, 'f', 2).arg(fPrice - fPreClose, 0, 'f', 2));
			prices[nIndex]->setProperty("rising", 0.0 <= fPercent);
			changes[nIndex]->setProperty("rising", 0.0 <= fPercent);
			UIStyle::Refresh(*prices[nIndex]);
			UIStyle::Refresh(*changes[nIndex]);
			continue;
		}
		QString strMarket = model.index(nRow, 10).data().toString();
		if (("沪A" != strMarket) && ("深A" != strMarket) && ("创业板" != strMarket) && ("科创板" != strMarket))
		{
			continue;
		}
		if (0.0 < fPercent)
		{
			++nRising;
		}
		else if (0.0 > fPercent)
		{
			++nFalling;
		}
		else
		{
			++nFlat;
		}
		int nBucket = std::clamp(static_cast<int>(std::floor(fPercent)) + 6, 0, 12);
		++distribution[nBucket];
	}
	bool bHasQuotes = 0 < (nRising + nFalling + nFlat);
	m_ui->metric1Value->setText(bHasQuotes ? QString::number(nRising) : "--");
	m_ui->metric2Value->setText(bHasQuotes ? QString::number(nFalling) : "--");
	m_ui->metric3Value->setText(bHasQuotes ? QString::number(nFlat) : "--");
	m_ui->distributionChart->SetValues(bHasQuotes ? distribution : QVector<int>{});
}

void CViewHQMarket::OpenSecurityTab(const CSecurity& security)
{
	QString strKey = QString::fromStdString(security.String());
	for (int nIndex = m_nFixedTabCount; m_ui->marketTabs->count() > nIndex; ++nIndex)
	{
		if (strKey == m_ui->marketTabs->widget(nIndex)->property("securityKey").toString())
		{
			m_ui->marketTabs->setCurrentIndex(nIndex);
			return;
		}
	}
	QWidget* pPage = new QWidget(m_ui->marketTabs);
	pPage->setProperty("securityKey", strKey);
	int nIndex = m_ui->marketTabs->addTab(pPage, QString::fromStdString(security.m_strCode + " " + security.m_strName));
	QPushButton* pClose = new QPushButton(QStringLiteral("×"), m_ui->marketTabs->tabBar());
	pClose->setFixedSize(18, 18);
	pClose->setToolTip(QStringLiteral("关闭证券页"));
	m_ui->marketTabs->tabBar()->setTabButton(nIndex, QTabBar::RightSide, pClose);
	connect(pClose, &QPushButton::clicked, m_ui->marketTabs, [this, pPage]()
	{
		m_ui->marketTabs->tabCloseRequested(m_ui->marketTabs->indexOf(pPage));
	});
	m_ui->marketTabs->setCurrentIndex(nIndex);
}
