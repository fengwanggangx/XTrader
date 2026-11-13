#include "CViewStrategySettings.h"
#include "../system/CSession.h"
#include "CStrategyEditDialog.h"
#include "CViewTradeAccounts.h"
#include "ui_CViewStrategySettings.h"
#include <QTabWidget>

#include <QHeaderView>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QStringList>
#include <QTabBar>
#include "CTableData.h"
#include <QTimer>
#include <QSortFilterProxyModel>

namespace
{
	QString RuntimeStatus(bool enabled,
		const CStrategyService::CRuntimeSnapshot* snapshot,
		bool stale)
	{
		if (!enabled)
		{
			return "已禁用";
		}
		if (stale)
		{
			return "状态已过期";
		}
		if (nullptr == snapshot)
		{
			return "运行状态未知";
		}
		switch (snapshot->m_state)
		{
		case 0:
			return "已创建";
		case 1:
			return "已初始化";
		case 2:
			return "启动中";
		case 3:
			return "运行中";
		case 4:
			return "暂停中";
		case 5:
			return "已暂停";
		case 6:
			return "停止中";
		case 7:
			return "已停止";
		case 8:
			return "异常";
		default:
			return "运行状态未知";
		}
	}
} // namespace

class CStrategyFilterProxyModel final : public QSortFilterProxyModel
{
public:
	explicit CStrategyFilterProxyModel(QObject* parent) : QSortFilterProxyModel(parent)
	{
	}
	void SetFilter(const QString& keyword, const QString& status)
	{
		beginFilterChange();
		m_keyword = keyword;
		m_status = status;
		endFilterChange();
	}

protected:
	bool filterAcceptsRow(int row, const QModelIndex& parent) const override
	{
		bool matches = m_keyword.isEmpty();
		for (int column = 0; column < 4; ++column)
		{
			matches = matches || sourceModel()->index(row, column, parent).data().toString().contains(m_keyword, Qt::CaseInsensitive);
		}
		return matches && (m_status.isEmpty() || sourceModel()->index(row, 5, parent).data().toString() == m_status);
	}

private:
	QString m_keyword;
	QString m_status;
};

CViewStrategySettings::CViewStrategySettings(QWidget* pParent)
	: QWidget(pParent),
	  m_ui(std::make_unique<Ui::CViewStrategySettingsClass>())
{
}

void CViewStrategySettings::Initialize(CTraderServices& services)
{
	if (nullptr != m_pServices)
	{
		return;
	}
	m_pServices = &services;
	m_ui->setupUi(this);
	QPushButton* refreshButton = new QPushButton("刷新", this);
	m_ui->toolbarLayout->addWidget(refreshButton);
	connect(refreshButton, &QPushButton::clicked, this, [this]()
	{
		if (m_pServices->m_strategy.IsAuthenticated())
		{
			m_pServices->m_strategy.Invalidate();
			m_pServices->m_strategy.QueryStrategies();
			m_pServices->m_strategy.QueryRuntime();
			RefreshConnection();
		}
	});
	m_pInstances = m_ui->instancesTable;
	DefineTable(m_instanceTable, { { 1, "实例名称", DataType::String }, { 2, "策略模板", DataType::String }, { 3, "交易账户", DataType::String }, { 4, "交易标的", DataType::String }, { 5, "模式", DataType::String }, { 6, "状态", DataType::String }, { 7, "总持仓", DataType::String }, { 8, "累计盈亏", DataType::String }, { 9, "运行时间", DataType::String } });
	DefineTable(m_parameterTable, { { 1, "参数", DataType::String }, { 2, "当前值", DataType::String }, { 3, "说明", DataType::String } });
	DefineTable(m_logTable, { { 1, "时间", DataType::String }, { 2, "级别", DataType::String }, { 3, "内容", DataType::String } });
	PublishTable(m_instanceTable, *m_pInstances, {});
	PublishTable(m_parameterTable, *m_ui->parametersTable, {});
	PublishTable(m_logTable, *m_ui->logsTable, {});
	m_instanceFilter = new CStrategyFilterProxyModel(m_pInstances);
	m_instanceFilter->setSourceModel(m_pInstances->DataModel());
	m_pInstances->setModel(m_instanceFilter);
	m_pDetails = m_ui->detailsLabel;
	m_pSearch = m_ui->searchEdit;
	m_pStatus = m_ui->statusCombo;
	m_pStatus->clear();
	m_pStatus->addItems({ "全部状态", "已创建", "已初始化", "启动中", "运行中",
		"暂停中", "已暂停", "停止中", "已停止", "已禁用", "异常",
		"状态已过期", "运行状态未知" });
	m_ui->runningCountTitle->setText("运行中");
	m_ui->pauseButton->setText("暂停");
	m_ui->deleteButton->setText("删除");
	m_ui->newButton->setToolTip("新建策略配置");
	m_ui->modifyButton->setToolTip("仅可修改已初始化或已停止、且无活跃委托的策略；请先等待配置及运行状态刷新");
	m_ui->pauseButton->setToolTip("暂停运行中的策略");
	m_ui->startButton->setToolTip("启动已初始化、已暂停或已停止的策略");
	m_ui->stopButton->setToolTip("停止选中的策略");
	m_ui->deleteButton->setToolTip("删除选中的策略配置");
	m_ui->totalProfitCard->hide();
	m_ui->todayProfitCard->hide();
	m_ui->recentLogsLabel->hide();
	m_ui->logsTable->hide();
	while (1 < m_ui->strategyTabs->count())
	{
		m_ui->strategyTabs->removeTab(1);
	}
	CViewTradeAccounts* accountsPage =
		new CViewTradeAccounts(*m_pServices, m_ui->strategyTabs);
	m_ui->strategyTabs->insertTab(0, accountsPage, "交易账户");
	m_ui->strategyTabs->setCurrentIndex(0);
	accountsPage->SetStrategyNavigation([this](std::uint64_t strategyId)
	{
		m_ui->strategyTabs->setCurrentWidget(m_ui->instancesPage);
		m_pSearch->clear();
		m_pStatus->setCurrentIndex(0);
		m_selectedStrategyId = strategyId;
		RefreshRows();
		m_pServices->m_strategy.QueryStrategies();
	});
	while (1 < m_ui->detailsTabs->count())
	{
		m_ui->detailsTabs->removeTab(1);
	}
	for (const auto& tabs :
		QList<QTabWidget*>{ m_ui->strategyTabs, m_ui->detailsTabs })
	{
		tabs->tabBar()->setDrawBase(false);
		tabs->tabBar()->setExpanding(false);
	}
	for (const auto& table : QList<CUITable*>{
			 m_pInstances, m_ui->parametersTable, m_ui->logsTable })
	{
		table->horizontalHeader()->setSectionResizeMode(
			QHeaderView::ResizeToContents);
	}
	m_pInstances->SetHeaderTable(QStringLiteral("strategy_instances"), { 5, 7, 8, 9 });
	m_ui->parametersTable->SetHeaderTable(QStringLiteral("strategy_parameters"));
	m_ui->logsTable->SetHeaderTable(QStringLiteral("strategy_logs"));
	m_ui->detailSplitter->setStretchFactor(0, 1);
	m_ui->detailSplitter->setStretchFactor(1, 1);
	connect(m_ui->strategyTabs, &QTabWidget::currentChanged, this, [this]()
	{
		if (m_ui->instancesPage->isVisible() &&
			m_pServices->m_strategy.IsAuthenticated())
		{
			m_pServices->m_strategy.QueryRuntime();
			if (!m_pServices->m_strategy.IsCacheValid())
			{
				m_pServices->m_strategy.QueryStrategies();
			}
		}
	});
	connect(m_pSearch, &QLineEdit::textChanged, this,
		&CViewStrategySettings::RefreshFilter);
	connect(m_pStatus, &QComboBox::currentIndexChanged, this,
		&CViewStrategySettings::RefreshFilter);
	connect(m_pInstances, &CUITable::OperationRequested, this, [this](const CTableOperation& operation)
	{
		if (TableOperationKind::Selection == operation.m_kind)
		{
			RefreshDetails();
		}
	});
	CStrategyService& service = m_pServices->m_strategy;
	service.Initialize();
	QPointer<CViewStrategySettings> safeThis(this);
	m_nAccountToken = m_pServices->m_account.Subscribe(
		[safeThis](const CAccountService::CResult& result)
	{
		if (!safeThis.isNull() &&
			((_TyRequest::TRADE_ACCOUNT_LIST_QUERY == result.m_type) ||
				(result.m_bSessionReset)))
		{
			QMetaObject::invokeMethod(
				safeThis.data(),
				[safeThis]()
			{
				if (!safeThis.isNull())
				{
					safeThis->RefreshRows();
				}
			},
				Qt::QueuedConnection);
		}
	});
	m_nQueryToken = service.AddQueryHandler(
		[safeThis](const CStrategyService::_TyStrategyList& strategies,
			const std::string& strError)
	{
		if (!safeThis.isNull())
		{
			QMetaObject::invokeMethod(
				safeThis.data(),
				[safeThis]()
			{
				if (!safeThis.isNull())
				{
					safeThis->RefreshSnapshot();
				}
			},
				Qt::QueuedConnection);
		}
	});
	m_nRuntimeToken = service.AddRuntimeHandler(
		[safeThis](const CStrategyService::_TyRuntimeMap& runtimes,
			const std::string& strError)
	{
		if (!safeThis.isNull())
		{
			QMetaObject::invokeMethod(
				safeThis.data(),
				[safeThis]()
			{
				if (!safeThis.isNull())
				{
					safeThis->RefreshSnapshot();
				}
			},
				Qt::QueuedConnection);
		}
	});
	m_nOperationToken = service.AddOperationHandler(
		[safeThis](const CStrategyService::COperationResult& result)
	{
		if (!safeThis.isNull())
		{
			QMetaObject::invokeMethod(
				safeThis.data(),
				[safeThis, result]()
			{
				if (!safeThis.isNull())
				{
					safeThis->HandleOperation(result);
				}
			},
				Qt::QueuedConnection);
		}
	});
	connect(m_ui->newButton, &QPushButton::clicked, this,
		[this]()
	{
		EditStrategy(true);
	});
	connect(m_ui->modifyButton, &QPushButton::clicked, this,
		[this]()
	{
		EditStrategy(false);
	});
	connect(m_ui->deleteButton, &QPushButton::clicked, this,
		&CViewStrategySettings::DeleteStrategy);
	connect(m_ui->startButton, &QPushButton::clicked, this,
		[this]()
	{
		ControlStrategy(_TyRequest::STRATEGY_START);
	});
	connect(m_ui->pauseButton, &QPushButton::clicked, this,
		[this]()
	{
		ControlStrategy(_TyRequest::STRATEGY_PAUSE);
	});
	connect(m_ui->stopButton, &QPushButton::clicked, this,
		[this]()
	{
		ControlStrategy(_TyRequest::STRATEGY_STOP);
	});
	QTimer* timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, [this]()
	{
		RefreshConnection();
		if (m_pServices->m_strategy.IsAuthenticated() &&
			m_ui->instancesPage->isVisible() && (0 == (++m_refreshTicks % 2)))
		{
			m_pServices->m_strategy.QueryRuntime();
			if (!m_pServices->m_strategy.IsCacheValid())
			{
				m_pServices->m_strategy.QueryStrategies();
			}
		}
	});
	timer->start(1000);
	RefreshConnection();
	RefreshSnapshot();
}

CViewStrategySettings::~CViewStrategySettings()
{
	if (nullptr == m_pServices)
	{
		return;
	}
	m_pServices->m_account.Unsubscribe(m_nAccountToken);
	m_pServices->m_strategy.RemoveQueryHandler(m_nQueryToken);
	m_pServices->m_strategy.RemoveRuntimeHandler(m_nRuntimeToken);
	m_pServices->m_strategy.RemoveOperationHandler(m_nOperationToken);
}

int CViewStrategySettings::SelectedStrategyRow() const
{
	if (!m_pInstances->currentIndex().isValid())
	{
		return -1;
	}
	_TyDataRowId id = m_pInstances->SelectedRowId();
	for (std::size_t row = 0; row < m_strategies.size(); ++row)
	{
		if (id == m_strategies[row].strategy_id())
		{
			return static_cast<int>(row);
		}
	}
	return -1;
}

void CViewStrategySettings::RefreshFilter()
{
	m_instanceFilter->SetFilter(m_pSearch->text().trimmed(), 0 == m_pStatus->currentIndex() ? QString() : m_pStatus->currentText());
	m_pInstances->Update();
	RefreshDetails();
}

void CViewStrategySettings::RefreshDetails()
{
	int nRow = SelectedStrategyRow();
	if ((0 > nRow) || (m_strategies.size() <= static_cast<std::size_t>(nRow)))
	{
		m_pDetails->setText(QStringLiteral("请选择策略实例"));
		PublishTable(m_parameterTable, *m_ui->parametersTable, {});
		RefreshConnection();
		return;
	}
	m_selectedStrategyId = m_strategies[nRow].strategy_id();
	_TyDataValue securities;
	m_instanceTable.GetView().GetValueById(m_selectedStrategyId, 4, securities);
	m_pDetails->setText(QString("%1 · %2 · %3\n配置%4；运行状态由服务器快照提供")
							.arg(QString::fromStdString(m_strategies[nRow].strategy_name()),
								 QString::fromStdString(m_strategies[nRow].strategy_type()),
								 QString::fromStdString(std::get<std::string>(securities)),
								 m_bConfigStale ? "已过期" : "已加载"));
	std::vector<CTableRow> rows;
	for (const auto& [name, value] : m_strategies[nRow].parameters())
	{
		std::pair<std::uint64_t, std::string> key{ m_strategies[nRow].strategy_id(), name };
		const auto mIter = m_parameterIds.find(key);
		if (m_parameterIds.end() == mIter)
		{
			m_parameterIds.emplace(key, m_nextParameterId++);
		}
		rows.push_back({ m_parameterIds.at(key), { name, value, std::string() } });
	}
	PublishTable(m_parameterTable, *m_ui->parametersTable, rows);
	RefreshConnection();
}

void CViewStrategySettings::RefreshSnapshot()
{
	std::string configError;
	std::string runtimeError;
	m_snapshotGeneration = m_pServices->m_strategy.GetSnapshot(m_strategies, m_runtimes, configError, runtimeError);
	m_bConfigStale = !configError.empty();
	m_bRuntimeStale = !runtimeError.empty();
	m_ui->strategyCountHint->setText(m_bConfigStale ? "配置已过期：" + QString::fromStdString(configError) : "服务器配置");
	m_ui->runningCountHint->setText(m_bRuntimeStale ? "运行状态已过期：" + QString::fromStdString(runtimeError) : "服务器运行快照");
	RefreshRows();
}

void CViewStrategySettings::RefreshRows()
{
	std::vector<CTableRow> rows;
	int selectedRow = -1;
	int runningCount = 0;
	std::vector<request::TradeAccountRecord> accounts =
		m_pServices->m_account.GetAccounts();
	for (std::size_t index = 0; index < m_strategies.size(); ++index)
	{
		const request::StrategyInfo& strategy = m_strategies[index];
		const auto mIter = m_runtimes.find(strategy.strategy_id());
		const CStrategyService::CRuntimeSnapshot* snapshot =
			m_runtimes.end() == mIter ? nullptr : &mIter->second;
		if (strategy.enabled() && !m_bRuntimeStale && (nullptr != snapshot) &&
			(3 == snapshot->m_state))
		{
			++runningCount;
		}
		QStringList securities;
		for (const auto& subscription : strategy.subscriptions())
		{
			securities.append(QString::fromStdString(subscription.security() + "." +
													 subscription.exchange()));
		}
		QString accountName = "账户 #" + QString::fromStdString(strategy.account());
		for (const auto& account : accounts)
		{
			if (AccountIndex(account) == AccountIndex(strategy))
			{
				accountName = QString::fromStdString(account.broker_name() + " / " + account.site() + " / " + account.account());
				break;
			}
		}
		rows.push_back({ strategy.strategy_id(), { strategy.strategy_name(), strategy.strategy_type(), accountName.toStdString(), securities.join(", ").toStdString(), std::string(), RuntimeStatus(strategy.enabled(), snapshot, m_bRuntimeStale).toStdString(), std::string(), std::string(), std::string() } });
		if (m_selectedStrategyId == strategy.strategy_id())
		{
			selectedRow = static_cast<int>(index);
		}
	}
	PublishTable(m_instanceTable, *m_pInstances, rows);
	m_ui->strategyCountValue->setText(QString::number(m_strategies.size()));
	m_ui->runningCountValue->setText((m_bRuntimeStale || m_bConfigStale)
										 ? "--"
										 : QString::number(runningCount));
	RefreshFilter();
	if (0 <= selectedRow && m_pInstances->SelectRowId(m_strategies[selectedRow].strategy_id()))
	{
		m_pInstances->SelectRowId(m_strategies[selectedRow].strategy_id());
	}
	else
	{
		if (0 < m_pInstances->ResultCount())
		{
			m_pInstances->selectRow(0);
		}
	}
	RefreshDetails();
}

void CViewStrategySettings::RefreshConnection()
{
	bool bConnected = m_pServices->m_strategy.IsAuthenticated();
	if (bConnected && !m_bWasConnected)
	{
		m_pServices->m_strategy.QueryStrategies();
		if (m_ui->instancesPage->isVisible())
		{
			m_pServices->m_strategy.QueryRuntime();
			if (!m_pServices->m_strategy.IsCacheValid())
			{
				m_pServices->m_strategy.QueryStrategies();
			}
		}
	}
	if (!bConnected && m_bWasConnected)
	{
		m_bWasConnected = false;
		m_bConfigStale = true;
		m_bRuntimeStale = true;
		m_pendingOperations.clear();
		m_ui->strategyCountHint->setText("连接中断，配置已过期");
		m_ui->runningCountHint->setText("连接中断，运行状态已过期");
		RefreshRows();
	}
	m_bWasConnected = bConnected;
	bool bSnapshotChanged = m_snapshotGeneration != m_pServices->m_strategy.GetGeneration();
	m_bConfigStale = m_bConfigStale || bSnapshotChanged || !m_pServices->m_strategy.IsCacheValid();
	m_bRuntimeStale = m_bRuntimeStale || bSnapshotChanged || !m_pServices->m_strategy.IsRuntimeValid();
	bool bIdle = m_pendingOperations.empty() && !m_bConfigStale && !m_bRuntimeStale;
	m_ui->newButton->setEnabled(bConnected && bIdle);
	int nRow = SelectedStrategyRow();
	bool bSelected = (0 <= nRow) &&
					 (m_strategies.size() > static_cast<std::size_t>(nRow));
	m_ui->modifyButton->setEnabled(bConnected && bIdle && bSelected &&
								   !m_bConfigStale && m_pServices->m_strategy.CanModifyStrategy(m_strategies[nRow].strategy_id()));
	m_ui->deleteButton->setEnabled(bConnected && bIdle && bSelected &&
								   !m_bConfigStale);
	m_ui->startButton->setEnabled(false);
	m_ui->pauseButton->setEnabled(false);
	m_ui->stopButton->setEnabled(false);
	if (!bConnected)
	{
		m_ui->runtimeLabel->setText("连接不可用，等待登录或重连");
		return;
	}
	if (!bSelected)
	{
		m_ui->runtimeLabel->setText("请选择策略实例");
		return;
	}
	const request::StrategyInfo& strategy = m_strategies[nRow];
	const auto mIter = m_runtimes.find(strategy.strategy_id());
	const CStrategyService::CRuntimeSnapshot* snapshot =
		m_runtimes.end() == mIter ? nullptr : &mIter->second;
	QString status = RuntimeStatus(strategy.enabled(), snapshot, m_bRuntimeStale);
	QString runtimeText = status;
	if ((nullptr != snapshot) && !m_bRuntimeStale)
	{
		runtimeText += QString("\n行情%1 · 待处理事件 %2 · 活跃委托 %3")
						   .arg(snapshot->m_marketAvailable ? "可用" : "不可用")
						   .arg(snapshot->m_queueLength)
						   .arg(snapshot->m_activeOrders);
		if (!snapshot->m_lastError.empty())
		{
			runtimeText +=
				"\n最近错误：" + QString::fromStdString(snapshot->m_lastError);
		}
	}
	m_ui->runtimeLabel->setText(runtimeText);
	if (!bIdle || m_bRuntimeStale || !strategy.enabled() ||
		(nullptr == snapshot))
	{
		return;
	}
	m_ui->startButton->setEnabled((1 == snapshot->m_state) ||
								  (5 == snapshot->m_state) ||
								  (7 == snapshot->m_state));
	m_ui->pauseButton->setEnabled(3 == snapshot->m_state);
	m_ui->stopButton->setEnabled(
		(2 == snapshot->m_state) || (3 == snapshot->m_state) ||
		(4 == snapshot->m_state) || (5 == snapshot->m_state) ||
		(6 == snapshot->m_state) || (8 == snapshot->m_state));
}

void CViewStrategySettings::EditStrategy(bool bNew)
{
	if (!m_pServices->m_strategy.IsAuthenticated())
	{
		return;
	}
	std::uint64_t generation = m_pServices->m_strategy.GetGeneration();
	CStrategyEditDialog dialog(*m_pServices, this);
	if (!bNew)
	{
		int nRow = SelectedStrategyRow();
		if ((0 > nRow) || (m_strategies.size() <= static_cast<std::size_t>(nRow)))
		{
			return;
		}
		if (!m_pServices->m_strategy.CanModifyStrategy(m_strategies[nRow].strategy_id()))
		{
			QMessageBox::warning(this, "无法修改", "请先停止策略、处理活跃委托，并等待状态刷新");
			return;
		}
		dialog.SetStrategy(m_strategies[nRow]);
		const auto mIter = m_runtimes.find(m_strategies[nRow].strategy_id());
		dialog.SetAccountLocked(
			(m_runtimes.end() != mIter) &&
			((2 == mIter->second.m_state) || (3 == mIter->second.m_state) ||
				(4 == mIter->second.m_state) || (6 == mIter->second.m_state)));
	}
	if (QDialog::Accepted != dialog.exec())
	{
		return;
	}
	if (!m_pServices->m_strategy.IsAuthenticated() || (generation != m_pServices->m_strategy.GetGeneration()))
	{
		QMessageBox::warning(this, "配置已变化", "连接或配置已变化，请刷新后重新编辑");
		return;
	}
	request::StrategyInfo strategy = dialog.GetStrategy();
	std::uint64_t requestId =
		bNew ? m_pServices->m_strategy.AddStrategy(strategy)
			 : m_pServices->m_strategy.ModifyStrategy(strategy);
	if (0 == requestId)
	{
		QMessageBox::warning(this, "请求未发送", "请检查连接及策略配置");
	}
	else
	{
		m_pendingOperations.emplace(requestId, bNew ? "新建" : "修改");
		m_ui->strategyCountHint->setText("配置请求已发送，等待服务器确认");
		RefreshConnection();
	}
}

void CViewStrategySettings::DeleteStrategy()
{
	int row = SelectedStrategyRow();
	if (!m_pServices->m_strategy.IsAuthenticated() || (0 > row) ||
		(m_strategies.size() <= static_cast<std::size_t>(row)))
	{
		return;
	}
	request::StrategyInfo strategy = m_strategies[row];
	std::uint64_t generation = m_pServices->m_strategy.GetGeneration();
	if (QMessageBox::Yes !=
		QMessageBox::question(
			this, "删除策略",
			"确定删除策略“" + QString::fromStdString(strategy.strategy_name()) +
				"”？",
			QMessageBox::Yes | QMessageBox::No, QMessageBox::No))
	{
		return;
	}
	request::StrategyInfo current;
	if (!m_pServices->m_strategy.IsAuthenticated() || !m_pServices->m_strategy.IsCacheValid() || (generation != m_pServices->m_strategy.GetGeneration()) || !m_pServices->m_strategy.FindStrategy(strategy.strategy_id(), current))
	{
		QMessageBox::warning(this, "配置已变化", "策略配置已变化，请刷新后重试");
		return;
	}
	std::uint64_t requestId = m_pServices->m_strategy.DeleteStrategy(strategy.strategy_id());
	if (0 == requestId)
	{
		QMessageBox::warning(this, "请求未发送", "请检查连接状态");
		return;
	}
	m_pendingOperations.emplace(requestId, "删除");
	m_ui->strategyCountHint->setText("删除请求已发送，等待服务器确认");
	RefreshConnection();
}

void CViewStrategySettings::ControlStrategy(_TyRequest type)
{
	int row = SelectedStrategyRow();
	if (!m_pServices->m_strategy.IsAuthenticated() || (0 > row) ||
		(m_strategies.size() <= static_cast<std::size_t>(row)))
	{
		return;
	}
	std::uint64_t requestId = m_pServices->m_strategy.ControlStrategy(
		type, m_strategies[row].strategy_id());
	if (0 == requestId)
	{
		QMessageBox::warning(this, "请求未发送", "请检查连接和策略状态");
		return;
	}
	m_pendingOperations.emplace(
		requestId,
		(_TyRequest::STRATEGY_START == type)
			? "启动"
			: ((_TyRequest::STRATEGY_PAUSE == type) ? "暂停" : "停止"));
	m_ui->strategyCountHint->setText("运行控制请求已发送，等待服务器确认");
	RefreshConnection();
}

void CViewStrategySettings::HandleOperation(
	const CStrategyService::COperationResult& result)
{
	const auto mIter = m_pendingOperations.find(result.m_requestId);
	if (m_pendingOperations.end() == mIter)
	{
		return;
	}
	QString action = mIter->second;
	m_pendingOperations.erase(mIter);
	if (result.m_success)
	{
		m_ui->strategyCountHint->setText(action + "已确认，等待配置及运行状态刷新");
	}
	else
	{
		QString error = QString::fromStdString(result.m_error);
		m_ui->strategyCountHint->setText(action + "失败：" + error);
		QMessageBox::warning(this, action + "失败", error);
	}
	RefreshConnection();
}
