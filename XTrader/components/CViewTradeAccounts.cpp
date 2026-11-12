#include "CViewTradeAccounts.h"
#include "../service/strategy/CStrategyService.h"
#include "CTradeAccountLoginDialog.h"
#include "../common/container.h"
#include <QFrame>
#include <QStyle>
#include <QColor>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include "CTableData.h"
#include <QTimer>
#include <QVBoxLayout>

CViewTradeAccounts::CViewTradeAccounts(CTraderServices& services, QWidget* parent) : QWidget(parent)
{
	m_pServices = &services;
	setObjectName("tradeAccountsPage");
	QVBoxLayout* layout = new QVBoxLayout(this);
	QHBoxLayout* actions = new QHBoxLayout();
	m_add = new QPushButton("新增账户", this);
	m_remove = new QPushButton("移除账户", this);
	QPushButton* refresh = new QPushButton("刷新", this);
	actions->addWidget(m_add);
	actions->addWidget(m_remove);
	actions->addWidget(refresh);
	actions->setSpacing(12);
	for (QPushButton* button : { m_add, m_remove, refresh })
	{
		button->setMinimumHeight(40);
		button->setCursor(Qt::PointingHandCursor);
		button->setStyleSheet("QPushButton { color: palette(button-text); background: palette(button); border: 1px solid palette(mid); border-radius: 6px; padding: 0 16px; } QPushButton:hover { border-color: palette(link); } QPushButton:disabled { color: palette(dark); background: palette(window); border-color: palette(mid); }");
	}
	m_add->setText("＋  新增账户");
	m_add->setStyleSheet("QPushButton { color: white; background: #2563EB; border: 1px solid #2563EB; border-radius: 6px; padding: 0 18px; } QPushButton:hover { background: #1D4ED8; } QPushButton:disabled { color: #CBD5E1; background: #93B4EE; border-color: #93B4EE; }");
	m_remove->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
	refresh->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
	QFrame* separator = new QFrame(this);
	separator->setFrameShape(QFrame::VLine);
	separator->setFixedHeight(24);
	separator->setStyleSheet("color: palette(mid);");
	actions->addSpacing(8);
	actions->addWidget(separator);
	actions->addSpacing(8);
	m_checkedCount = new QLabel("已勾选 <span style='color: #2563EB;'>0</span> 个账户", this);
	m_checkedCount->setStyleSheet("color: palette(text);");
	actions->addWidget(m_checkedCount);
	actions->addStretch();
	layout->addLayout(actions);
	m_notice = new QLabel(this);
	layout->addWidget(m_notice);
	m_logs = new CUITable(this);
	DefineTable(m_accountTable, { { 1, "券商", DataType::String }, { 2, "站点", DataType::String }, { 3, "账号", DataType::String }, { 4, "账户名称", DataType::String }, { 5, "状态", DataType::String } });
	PublishTable(m_accountTable, *m_logs, {});
	CTablePresentation presentation;
	presentation.m_columns = { { TableCheckColumn, "" }, { TableSequenceColumn, "序号", Qt::AlignCenter }, { 1, "券商" }, { 2, "站点" }, { 3, "账号" }, { 4, "账户名称" }, { 5, "状态" }, { TableActionColumn, "操作" } };
	m_logs->DataModel()->SetPresentation(std::move(presentation));
	m_logs->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	for (int column : { 0, 1, 7 })
	{
		m_logs->horizontalHeader()->setSectionResizeMode(column, QHeaderView::Fixed);
	}
	m_logs->setColumnWidth(0, 44);
	m_logs->setColumnWidth(1, 60);
	m_logs->setColumnWidth(7, 112);
	m_logs->verticalHeader()->setDefaultSectionSize(48);
	connect(m_logs, &CUITable::OperationRequested, this, &CViewTradeAccounts::HandleTableOperation);
	layout->addWidget(m_logs);
	m_heading = new QLabel("请选择账户", this);
	layout->addWidget(m_heading);
	QHBoxLayout* funds = new QHBoxLayout();
	for (std::size_t index = 0; index < m_funds.size(); ++index)
	{
		m_funds[index] = new QLabel("--", this);
		funds->addWidget(m_funds[index]);
	}
	layout->addLayout(funds);
	m_strategies = new CUITable(this);
	DefineTable(m_strategyTable, { { 1, "关联策略", DataType::String }, { 2, "策略类型", DataType::String } });
	PublishTable(m_strategyTable, *m_strategies, {});
	m_strategies->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	layout->addWidget(m_strategies);
	connect(m_add, &QPushButton::clicked, this, [this]()
	{
		EditAccount(true);
	});
	connect(m_remove, &QPushButton::clicked, this, &CViewTradeAccounts::RemoveCheckedAccounts);
	connect(refresh, &QPushButton::clicked, this, &CViewTradeAccounts::Refresh);
	connect(m_strategies, &CUITable::OperationRequested, this, [this](const CTableOperation& operation)
	{
		if (TableOperationKind::Activate != operation.m_kind || nullptr == m_navigate)
		{
			return;
		}
		for (const auto& strategy : m_pServices->m_strategy.GetStrategies())
		{
			if (strategy.strategy_id() == operation.m_rowId && AccountIndex(strategy) == m_selectedIndex)
			{
				m_navigate(operation.m_rowId);
				return;
			}
		}
	});
	QPointer<CViewTradeAccounts> self(this);
	m_accountToken = m_pServices->m_account.Subscribe(
		[self](const CAccountService::CResult& result)
	{
		if (nullptr != self)
		{
			QMetaObject::invokeMethod(
				self,
				[self, result]()
			{
				if (nullptr != self)
				{
					self->HandleResult(result);
				}
			},
				Qt::QueuedConnection);
		}
	});
	QTimer* timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, [this]()
	{
		if (isVisible())
		{
			Refresh();
		}
	});
	RefreshChecks();
	timer->start(3000);
}

CViewTradeAccounts::~CViewTradeAccounts()
{
	m_pServices->m_account.Unsubscribe(m_accountToken);
}

void CViewTradeAccounts::SetStrategyNavigation(
	std::function<void(std::uint64_t)>&& handler)
{
	m_navigate = std::move(handler);
}

void CViewTradeAccounts::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event);
	Refresh();
}

void CViewTradeAccounts::Refresh()
{
	m_pServices->m_account.Send(_TyRequest::TRADE_ACCOUNT_LIST_QUERY);
}

void CViewTradeAccounts::RefreshRows()
{
	std::vector<CTableRow> rows;
	rows.reserve(m_accounts.size());
	for (const auto& info : m_accounts)
	{
		_TyAccountIndex key = AccountIndex(info);
		if (!container::with(m_rowIds, key))
		{
			m_rowIds.emplace(key, m_nextRowId++);
		}
		std::string state = request::ACCOUNT_CONNECTED == info.state() ? "已连接" : request::ACCOUNT_ERROR == info.state()		 ? "错误"
																				: request::ACCOUNT_CONNECTING == info.state()	 ? "连接中"
																				: request::ACCOUNT_SYNCHRONIZING == info.state() ? "同步中"
																				: request::ACCOUNT_RECONNECTING == info.state()	 ? "重连中"
																																 : "未连接";
		rows.push_back({ m_rowIds.at(key), { info.broker_name(), info.site(), info.account(), info.name(), state } });
	}
	PublishTable(m_accountTable, *m_logs, rows);
	if (nullptr == SelectedAccount())
	{
		m_selectedIndex = {};
		ClearDetails();
	}
	RefreshChecks();
	RefreshSelection();
}

void CViewTradeAccounts::HandleTableOperation(const CTableOperation& operation)
{
	if (operation.m_generation != m_accountTable.GetView().GetSourceId())
	{
		return;
	}
	if (TableOperationKind::ChecksChanged == operation.m_kind)
	{
		RefreshChecks();
		return;
	}
	if (TableOperationKind::Selection == operation.m_kind)
	{
		RefreshSelection();
		return;
	}
	if (TableOperationKind::Action != operation.m_kind || 0 != m_operationId || !m_removeIds.empty())
	{
		return;
	}
	_TyAccountIndex key;
	for (const auto& info : m_accounts)
	{
		if (m_rowIds.at(AccountIndex(info)) == operation.m_rowId)
		{
			key = AccountIndex(info);
			break;
		}
	}
	if (!IsAccountValid(key))
	{
		return;
	}
	if ("disconnect" == operation.m_actionId)
	{
		if (QMessageBox::Yes != QMessageBox::question(this, "断开交易账户", "断开此交易账户可能影响其他正在使用该账户的用户。确定断开吗？"))
		{
			return;
		}
	}
	for (const auto& info : m_accounts)
	{
		if (key != AccountIndex(info))
		{
			continue;
		}
		if ("disconnect" == operation.m_actionId && request::ACCOUNT_CONNECTED == info.state())
		{
			Operate(_TyRequest::TRADE_ACCOUNT_DISCONNECT, key);
		}
		else if ("login" == operation.m_actionId && (request::ACCOUNT_DISCONNECTED == info.state() || request::ACCOUNT_ERROR == info.state()))
		{
			Operate(_TyRequest::TRADE_ACCOUNT_LOGIN, key);
		}
		return;
	}
}

const request::TradeAccountRecord*
CViewTradeAccounts::SelectedAccount() const
{
	for (const auto& account : m_accounts)
	{
		if (m_selectedIndex == AccountIndex(account))
		{
			return &account;
		}
	}
	return nullptr;
}

void CViewTradeAccounts::RefreshSelection()
{
	_TyDataRowId selected = m_logs->SelectedRowId();
	const request::TradeAccountRecord* account = nullptr;
	for (const auto& info : m_accounts)
	{
		if (m_rowIds.at(AccountIndex(info)) == selected)
		{
			account = &info;
			break;
		}
	}
	if (nullptr == account)
	{
		m_selectedIndex = {};
		m_snapshotId = 0;
		ClearDetails();
		RefreshButtons();
		return;
	}
	m_selectedIndex = AccountIndex(*account);
	m_heading->setText(QString::fromStdString(account->broker_name() + " / " + account->site() + " / " + account->account()));
	if (request::ACCOUNT_CONNECTED == account->state())
	{
		m_snapshotId = m_pServices->m_account.Send(_TyRequest::TRADE_FUNDS_QUERY, m_selectedIndex);
	}
	else
	{
		m_snapshotId = 0;
		for (QLabel* label : m_funds)
		{
			label->setText("--");
		}
	}
	RefreshStrategies();
	RefreshButtons();
}

void CViewTradeAccounts::ClearDetails()
{
	m_heading->setText("请选择账户");
	for (QLabel* label : m_funds)
	{
		label->setText("--");
	}
	PublishTable(m_strategyTable, *m_strategies, {});
}

void CViewTradeAccounts::RefreshButtons()
{
	CTablePresentation presentation = m_logs->DataModel()->GetPresentation();
	presentation.m_cells.clear();
	for (const auto& account : m_accounts)
	{
		_TyDataRowId id = m_rowIds.at(AccountIndex(account));
		bool connected = request::ACCOUNT_CONNECTED == account.state();
		bool idle = request::ACCOUNT_DISCONNECTED == account.state() || request::ACCOUNT_ERROR == account.state();
		if (connected)
		{
			presentation.m_cells[{ id, 5 }].m_foreground = QBrush(QColor("#199B61"));
		}
		_TyDataValue state;
		m_accountTable.GetView().GetValueById(id, 5, state);
		QString text = connected ? "断开" : idle ? "登录"
												 : QString::fromStdString(std::get<std::string>(state));
		presentation.m_cells[{ id, TableActionColumn }].m_actions = { { connected ? "disconnect" : "login", text, 0 == m_operationId && m_removeIds.empty() && (connected || idle), !connected } };
	}
	m_logs->DataModel()->SetPresentation(std::move(presentation));
	bool removable = !m_checkedAccounts.empty();
	for (const auto& account : m_accounts)
	{
		if (container::with(m_checkedAccounts, AccountIndex(account)) &&
			request::ACCOUNT_DISCONNECTED != account.state() && request::ACCOUNT_ERROR != account.state())
		{
			removable = false;
			break;
		}
	}
	m_add->setEnabled(0 == m_operationId && m_removeIds.empty());
	m_remove->setEnabled(0 == m_operationId && m_removeIds.empty() && removable);
	m_remove->setToolTip(removable ? "移除勾选的账户" : "请勾选已断开的账户；连接中的账户需先断开");
}

void CViewTradeAccounts::RefreshChecks()
{
	m_checkedAccounts.clear();
	QVector<qulonglong> checked = m_logs->CheckedRowIds();
	for (const auto& account : m_accounts)
	{
		if (checked.contains(m_rowIds.at(AccountIndex(account))))
		{
			m_checkedAccounts.insert(AccountIndex(account));
		}
	}
	m_checkedCount->setText(QString("已勾选 <span style='color: #2563EB;'>%1</span> 个账户").arg(static_cast<qulonglong>(m_checkedAccounts.size())));
	RefreshButtons();
}

void CViewTradeAccounts::HandleResult(
	const CAccountService::CResult& result)
{
	if (result.m_bSessionReset)
	{
		m_accounts.clear();
		m_rowIds.clear();
		m_selectedIndex = {};
		m_operationId = 0;
		m_removeIds.clear();
		RefreshRows();
		return;
	}
	if (0 != m_removeIds.erase(result.m_requestId))
	{
		if (result.m_error.empty())
		{
			m_checkedAccounts.erase(result.m_accountIndex);
		}
		if (m_removeIds.empty())
		{
			Refresh();
		}
	}
	if (result.m_requestId == m_operationId)
	{
		m_operationId = 0;
		Refresh();
	}
	if (_TyRequest::TRADE_ACCOUNT_LIST_QUERY == result.m_type &&
		result.m_error.empty())
	{
		m_accounts = m_pServices->m_account.GetAccounts();
		RefreshRows();
	}
	if (m_selectedIndex == result.m_accountIndex &&
		result.m_requestId == m_snapshotId && result.m_error.empty() &&
		result.m_data.has_counter_reply() &&
		result.m_data.counter_reply().has_funds())
	{
		const request::TradeAccountInfo& funds =
			result.m_data.counter_reply().funds();
		m_funds[0]->setText("余额 " +
							QLocale().toString(funds.balance() / 100.0, 'f', 2));
		m_funds[1]->setText("可用 " +
							(funds.extended_fields() && !funds.funds_valid() ? QString("失效") : QLocale().toString(funds.available() / 100.0, 'f', 2)));
		m_funds[2]->setText("冻结 " +
							(funds.extended_fields() && !funds.frozen_known() ? QString("未知") : QLocale().toString(funds.frozen() / 100.0, 'f', 2)));
		m_funds[3]->setText(funds.connected() ? "已连接" : "未连接");
	}
	m_notice->setText(QString::fromStdString(result.m_error));
	RefreshButtons();
}

void CViewTradeAccounts::RefreshStrategies()
{
	std::vector<CTableRow> rows;
	for (const auto& strategy : m_pServices->m_strategy.GetStrategies())
	{
		if (m_selectedIndex == AccountIndex(strategy))
		{
			rows.push_back({ strategy.strategy_id(), { strategy.strategy_name(), strategy.strategy_type() } });
		}
	}
	PublishTable(m_strategyTable, *m_strategies, rows);
}

void CViewTradeAccounts::RemoveCheckedAccounts()
{
	if (!m_remove->isEnabled())
	{
		return;
	}
	m_notice->clear();
	for (const auto& account : m_checkedAccounts)
	{
		std::uint64_t requestId = m_pServices->m_account.Send(_TyRequest::TRADE_ACCOUNT_REMOVE, account);
		if (0 != requestId)
		{
			m_removeIds.insert(requestId);
		}
		else
		{
			m_notice->setText("部分账户的移除请求未发送，请稍后重试");
		}
	}
	RefreshButtons();
}

void CViewTradeAccounts::Operate(_TyRequest type, const _TyAccountIndex& account)
{
	m_operationId =
		m_pServices->m_account.Send(type, account);
	RefreshButtons();
}

void CViewTradeAccounts::EditAccount(bool)
{
	CTradeAccountLoginDialog dialog(*m_pServices, this);
	if (QDialog::Accepted == dialog.exec())
	{
		Refresh();
	}
}
