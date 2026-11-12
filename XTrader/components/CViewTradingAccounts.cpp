#include "CViewTradingAccounts.h"
#include "../service/strategy/CStrategyService.h"
#include "CTradeAccountLoginDialog.h"
#include <QCheckBox>
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
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

CViewTradingAccounts::CViewTradingAccounts(CTraderServices& services, QWidget* parent) : QWidget(parent)
{
	m_pServices = &services;
	setObjectName("tradingAccountsPage");
	QVBoxLayout* layout = new QVBoxLayout(this);
	QHBoxLayout* actions = new QHBoxLayout();
	m_add = new QPushButton("新增账户", this);
	m_login = new QPushButton("登录账户", this);
	m_disconnect = new QPushButton("断开账户", this);
	m_remove = new QPushButton("移除账户", this);
	QPushButton* refresh = new QPushButton("刷新", this);
	actions->addWidget(m_add);
	actions->addWidget(m_login);
	actions->addWidget(m_disconnect);
	actions->addWidget(m_remove);
	actions->addWidget(refresh);
	actions->addStretch();
	layout->addLayout(actions);
	m_notice = new QLabel(this);
	layout->addWidget(m_notice);
	m_logs = new QTableWidget(this);
	m_logs->setColumnCount(8);
	m_logs->setHorizontalHeaderLabels(
		{ "", "序号", "券商", "站点", "账号", "账户名称", "状态", "操作" });
	m_logs->verticalHeader()->hide();
	m_logs->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_logs->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_logs->setSelectionMode(QAbstractItemView::SingleSelection);
	m_logs->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	m_logs->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
	m_logs->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
	m_logs->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Fixed);
	m_logs->setColumnWidth(0, 44);
	m_logs->setColumnWidth(1, 60);
	m_logs->setColumnWidth(7, 112);
	m_logs->verticalHeader()->setDefaultSectionSize(48);
	m_logs->setShowGrid(false);
	m_logs->setAlternatingRowColors(true);
	m_checkAll = new QCheckBox(m_logs->horizontalHeader()->viewport());
	m_checkAll->setToolTip("全选账户");
	connect(m_logs->horizontalHeader(), &QHeaderView::geometriesChanged, this, [this]()
	{
		QHeaderView* header = m_logs->horizontalHeader();
		QSize size = m_checkAll->sizeHint();
		m_checkAll->setGeometry(header->sectionViewportPosition(0) + (header->sectionSize(0) - size.width()) / 2,
								(header->height() - size.height()) / 2, size.width(), size.height());
	});
	connect(m_checkAll, &QCheckBox::clicked, this, [this](bool checked)
	{
		for (int row = 0; row < m_logs->rowCount(); ++row)
		{
			m_logs->item(row, 0)->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
		}
	});
	connect(m_logs, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item)
	{
		if (0 != item->column() || m_accounts.size() <= static_cast<std::size_t>(item->row()))
		{
			return;
		}
		_TyAccountIndex key = AccountIndex(m_accounts[item->row()]);
		if (Qt::Checked == item->checkState())
		{
			m_checkedAccounts.insert(key);
		}
		else
		{
			m_checkedAccounts.erase(key);
		}
		RefreshChecks();
	});
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
	m_strategies = new QTableWidget(this);
	m_strategies->setColumnCount(2);
	m_strategies->setHorizontalHeaderLabels({ "关联策略", "策略类型" });
	m_strategies->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	layout->addWidget(m_strategies);
	connect(m_add, &QPushButton::clicked, this, [this]()
	{
		EditAccount(true);
	});
	connect(m_login, &QPushButton::clicked, this, [this]()
	{
		Operate(_TyRequest::TRADE_ACCOUNT_LOGIN);
	});
	connect(m_disconnect, &QPushButton::clicked, this,
		[this]()
	{
		if (QMessageBox::Yes != QMessageBox::question(this, "断开交易账户", "断开此交易账户可能影响其他正在使用该账户的用户。确定断开吗？"))
		{
			return;
		}
		Operate(_TyRequest::TRADE_ACCOUNT_DISCONNECT);
	});
	connect(m_remove, &QPushButton::clicked, this, [this]()
	{
		Operate(_TyRequest::TRADE_ACCOUNT_REMOVE);
	});
	connect(refresh, &QPushButton::clicked, this, &CViewTradingAccounts::Refresh);
	connect(m_logs, &QTableWidget::itemSelectionChanged, this,
		&CViewTradingAccounts::RefreshSelection);
	connect(m_strategies, &QTableWidget::cellDoubleClicked, this,
		[this](int row, int)
	{
		if (nullptr != m_navigate &&
			nullptr != m_strategies->item(row, 0))
		{
			m_navigate(
				m_strategies->item(row, 0)->data(Qt::UserRole).toULongLong());
		}
	});
	QPointer<CViewTradingAccounts> self(this);
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
	timer->start(3000);
}

CViewTradingAccounts::~CViewTradingAccounts()
{
	m_pServices->m_account.Unsubscribe(m_accountToken);
}

void CViewTradingAccounts::SetStrategyNavigation(
	std::function<void(std::uint64_t)>&& handler)
{
	m_navigate = std::move(handler);
}

void CViewTradingAccounts::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event);
	Refresh();
}

void CViewTradingAccounts::Refresh()
{
	m_pServices->m_account.Send(_TyRequest::TRADE_ACCOUNT_LIST_QUERY);
}

void CViewTradingAccounts::RefreshRows()
{
	m_logs->blockSignals(true);
	m_logs->setRowCount(static_cast<int>(m_accounts.size()));
	int selected = -1;
	for (std::size_t index = 0; index < m_accounts.size(); ++index)
	{
		const request::TradeAccountRecord& info = m_accounts[index];
		QTableWidgetItem* check = new QTableWidgetItem();
		check->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
		check->setCheckState(m_checkedAccounts.contains(AccountIndex(info)) ? Qt::Checked : Qt::Unchecked);
		m_logs->setItem(static_cast<int>(index), 0, check);
		QStringList values{ QString::number(index + 1),
			QString::fromStdString(info.broker_name()),
			QString::fromStdString(info.site()),
			QString::fromStdString(info.account()),
			QString::fromStdString(info.name()),
			request::ACCOUNT_SYNCHRONIZING == info.state() ? "同步中" : request::ACCOUNT_RECONNECTING == info.state() ? "重连中"
																	: request::ACCOUNT_CONNECTING == info.state()	  ? "连接中"
																	: request::ACCOUNT_ERROR == info.state()		  ? "错误"
																	: request::ACCOUNT_CONNECTED == info.state()	  ? "已连接"
																													  : "未连接" };
		for (int column = 0; column < values.size(); ++column)
		{
			QTableWidgetItem* item = new QTableWidgetItem(values[column]);
			if (0 == column)
			{
				item->setTextAlignment(Qt::AlignCenter);
			}
			if (5 == column)
			{
				item->setForeground(QColor(request::ACCOUNT_CONNECTED == info.state() ? "#199B61" : "#6B7280"));
			}
			m_logs->setItem(static_cast<int>(index), column + 1, item);
		}
		QWidget* operation = new QWidget(m_logs);
		QHBoxLayout* operationLayout = new QHBoxLayout(operation);
		operationLayout->setContentsMargins(10, 7, 10, 7);
		QPushButton* button = new QPushButton(operation);
		button->setObjectName("accountStateAction");
		operationLayout->addWidget(button);
		m_logs->setCellWidget(static_cast<int>(index), 7, operation);
		_TyAccountIndex key = AccountIndex(info);
		connect(button, &QPushButton::clicked, this, [this, key]()
		{
			if (0 != m_operationId)
			{
				return;
			}
			for (std::size_t row = 0; row < m_accounts.size(); ++row)
			{
				if (key == AccountIndex(m_accounts[row]))
				{
					m_logs->selectRow(static_cast<int>(row));
					RefreshSelection();
					if (request::ACCOUNT_CONNECTED == m_accounts[row].state())
					{
						m_disconnect->click();
					}
					else
					{
						m_login->click();
					}
					return;
				}
			}
		});
		if (m_selectedIndex == AccountIndex(info))
		{
			selected = static_cast<int>(index);
		}
	}
	m_logs->blockSignals(false);
	RefreshChecks();
	if (0 <= selected)
	{
		m_logs->selectRow(selected);
	}
	else
	{
		m_selectedIndex = {};
		ClearDetails();
	}
	RefreshButtons();
}

const request::TradeAccountRecord*
CViewTradingAccounts::SelectedAccount() const
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

void CViewTradingAccounts::RefreshSelection()
{
	int row = m_logs->currentRow();
	if (0 > row || m_accounts.size() <= static_cast<std::size_t>(row))
	{
		return;
	}
	m_selectedIndex = AccountIndex(m_accounts[row]);
	m_heading->setText(QString::fromStdString(m_accounts[row].broker_name() +
											  " / " + m_accounts[row].site() +
											  " / " + m_accounts[row].account()));
	if (request::ACCOUNT_CONNECTED == m_accounts[row].state())
	{
		m_snapshotId = m_pServices->m_account.Send(
			_TyRequest::TRADE_FUNDS_QUERY, m_selectedIndex);
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

void CViewTradingAccounts::ClearDetails()
{
	m_heading->setText("请选择账户");
	for (QLabel* label : m_funds)
	{
		label->setText("--");
	}
	m_strategies->setRowCount(0);
}

void CViewTradingAccounts::RefreshButtons()
{
	for (int row = 0; row < m_logs->rowCount(); ++row)
	{
		QPushButton* button = m_logs->cellWidget(row, 7)->findChild<QPushButton*>("accountStateAction");
		request::TradingAccountState state = m_accounts[row].state();
		bool connected = request::ACCOUNT_CONNECTED == state;
		bool idle = request::ACCOUNT_DISCONNECTED == state || request::ACCOUNT_ERROR == state;
		button->setText(connected ? "断开" : idle ? "登录"
												  : m_logs->item(row, 6)->text());
		button->setEnabled(0 == m_operationId && (connected || idle));
		button->setStyleSheet(connected
								  ? "QPushButton { color: #475569; background: white; border: 1px solid #CBD5E1; border-radius: 4px; padding: 3px 12px; } QPushButton:disabled { color: #94A3B8; }"
								  : "QPushButton { color: white; background: #2563EB; border: 1px solid #2563EB; border-radius: 4px; padding: 3px 12px; } QPushButton:disabled { color: #94A3B8; background: #F1F5F9; border-color: #E2E8F0; }");
	}
	const request::TradeAccountRecord* account = SelectedAccount();
	m_login->setEnabled(0 == m_operationId && nullptr != account &&
		(request::ACCOUNT_DISCONNECTED == account->state() || request::ACCOUNT_ERROR == account->state()));
	m_disconnect->setEnabled(0 == m_operationId && nullptr != account &&
							 request::ACCOUNT_CONNECTED == account->state());
	m_remove->setEnabled(0 == m_operationId && nullptr != account &&
		(request::ACCOUNT_DISCONNECTED == account->state() || request::ACCOUNT_ERROR == account->state()));
}

void CViewTradingAccounts::RefreshChecks()
{
	std::set<_TyAccountIndex> checked;
	for (const auto& account : m_accounts)
	{
		_TyAccountIndex key = AccountIndex(account);
		if (m_checkedAccounts.contains(key))
		{
			checked.insert(std::move(key));
		}
	}
	m_checkedAccounts = std::move(checked);
	QSignalBlocker blocker(m_checkAll);
	m_checkAll->setTristate(!m_checkedAccounts.empty() && m_checkedAccounts.size() < m_accounts.size());
	m_checkAll->setCheckState(m_checkedAccounts.empty() ? Qt::Unchecked : m_checkedAccounts.size() == m_accounts.size() ? Qt::Checked
																														: Qt::PartiallyChecked);
	m_checkAll->setEnabled(!m_accounts.empty());
}

void CViewTradingAccounts::HandleResult(
	const CAccountService::CResult& result)
{
	if (result.m_bSessionReset)
	{
		m_accounts.clear();
		m_selectedIndex = {};
		m_operationId = 0;
		RefreshRows();
		return;
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

void CViewTradingAccounts::RefreshStrategies()
{
	m_strategies->setRowCount(0);
	for (const auto& strategy : m_pServices->m_strategy.GetStrategies())
	{
		if (m_selectedIndex != AccountIndex(strategy))
		{
			continue;
		}
		int row = m_strategies->rowCount();
		m_strategies->insertRow(row);
		QTableWidgetItem* name =
			new QTableWidgetItem(QString::fromStdString(strategy.strategy_name()));
		name->setData(Qt::UserRole,
			QVariant::fromValue<qulonglong>(strategy.strategy_id()));
		m_strategies->setItem(row, 0, name);
		m_strategies->setItem(
			row, 1,
			new QTableWidgetItem(QString::fromStdString(strategy.strategy_type())));
	}
}

void CViewTradingAccounts::Operate(_TyRequest type)
{
	m_operationId =
		m_pServices->m_account.Send(type, m_selectedIndex);
	RefreshButtons();
}

void CViewTradingAccounts::EditAccount(bool)
{
	CTradeAccountLoginDialog dialog(*m_pServices, this);
	if (QDialog::Accepted == dialog.exec())
	{
		Refresh();
	}
}
