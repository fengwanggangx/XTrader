#include "CStrategyEditDialog.h"
#include "../service/account/CAccountService.h"
#include <QComboBox>
#include <QFormLayout>
#include <QPointer>
#include <QSet>
#include "../service/strategy/CStrategyService.h"

#include "ui_CStrategyEditDialog.h"

#include <QHeaderView>
#include <QMessageBox>
#include "CTableData.h"

CStrategyEditDialog::CStrategyEditDialog(CTraderServices& services, QWidget* pParent)
	: QDialog(pParent), m_ui(std::make_unique<Ui::CStrategyEditDialogClass>())
{
	m_pServices = &services;
	m_ui->setupUi(this);
	InitializeUI();
}

CStrategyEditDialog::~CStrategyEditDialog()
{
	m_pServices->m_account.Unsubscribe(m_accountToken);
}

void CStrategyEditDialog::InitializeUI()
{
	m_accounts = new QComboBox(this);
	RefreshAccounts();
	QPointer<CStrategyEditDialog> safeThis(this);
	m_accountToken = m_pServices->m_account.Subscribe([safeThis](const CAccountService::CResult&)
	{
		if (!safeThis.isNull())
		{
			QMetaObject::invokeMethod(safeThis.data(), [safeThis]()
			{
				if (!safeThis.isNull())
				{
					safeThis->RefreshAccounts();
				}
			}, Qt::QueuedConnection);
		}
	});
	m_ui->typeCombo->setEditable(false);
	m_ui->queueLimitSpin->setMinimum(1);
	DefineTable(m_parameterData, { { 1, "参数名", DataType::String }, { 2, "参数值", DataType::String } });
	DefineTable(m_subscriptionData, { { 1, "证券代码", DataType::String }, { 2, "交易所", DataType::String }, { 3, "行情通道", DataType::String } });
	m_parameters = { { m_nextDraftId++, { std::string("short_window"), std::string("5") } }, { m_nextDraftId++, { std::string("long_window"), std::string("20") } } };
	PublishTable(m_parameterData, *m_ui->parameterTable, m_parameters);
	PublishTable(m_subscriptionData, *m_ui->subscriptionTable, m_subscriptions);
	CTablePresentation parameters;
	parameters.m_columns = { { 1, "参数名" }, { 2, "参数值" } };
	for (auto& column : parameters.m_columns)
	{
		column.m_editable = true;
	}
	m_ui->parameterTable->DataModel()->SetPresentation(std::move(parameters));
	CTablePresentation subscriptions;
	subscriptions.m_columns = { { 1, "证券代码" }, { 2, "交易所" }, { 3, "行情通道" } };
	for (auto& column : subscriptions.m_columns)
	{
		column.m_editable = true;
	}
	subscriptions.m_columns[1].m_choices = { "SSE", "SZSE", "BSE", "HKEX", "CFFEX", "SHFE", "DCE", "CZCE", "INE", "GFEX", "NASDAQ", "NYSE", "CRYPTO" };
	subscriptions.m_columns[2].m_choices = { "quote", "depth" };
	m_ui->subscriptionTable->DataModel()->SetPresentation(std::move(subscriptions));
	connect(m_ui->parameterTable, &CUITable::OperationRequested, this, [this](const CTableOperation& operation)
	{
		ApplyEdit(operation, m_parameterData, m_parameters, *m_ui->parameterTable);
	});
	connect(m_ui->subscriptionTable, &CUITable::OperationRequested, this, [this](const CTableOperation& operation)
	{
		ApplyEdit(operation, m_subscriptionData, m_subscriptions, *m_ui->subscriptionTable);
	});
	m_ui->basicLayout->addRow("交易账户", m_accounts);
	m_ui->parameterTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
	m_ui->subscriptionTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
	m_ui->parameterTable->horizontalHeader()->setStretchLastSection(true);
	m_ui->subscriptionTable->horizontalHeader()->setStretchLastSection(true);
	connect(m_ui->addParameterButton, &QPushButton::clicked, this,
		&CStrategyEditDialog::OnAddParameter);
	connect(m_ui->removeParameterButton, &QPushButton::clicked, this,
		&CStrategyEditDialog::OnRemoveParameter);
	connect(m_ui->addSubscriptionButton, &QPushButton::clicked, this,
		&CStrategyEditDialog::OnAddSubscription);
	connect(m_ui->removeSubscriptionButton, &QPushButton::clicked, this,
		&CStrategyEditDialog::OnRemoveSubscription);
	connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this,
		&CStrategyEditDialog::OnAccept);
	connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void CStrategyEditDialog::SetStrategy(const request::StrategyInfo& strategy)
{
	m_strategyId = strategy.strategy_id();
	m_originalAccount = AccountIndex(strategy);
	m_accounts->setCurrentIndex(-1);
	RefreshAccounts();
	m_accounts->setCurrentIndex(m_accounts->findData(QString::fromStdString(EncodeAccount(m_originalAccount))));
	m_ui->nameEdit->setText(QString::fromStdString(strategy.strategy_name()));
	if (0 > m_ui->typeCombo->findText(QString::fromStdString(strategy.strategy_type())))
	{
		m_ui->typeCombo->addItem(QString::fromStdString(strategy.strategy_type()));
	}
	m_ui->typeCombo->setCurrentText(
		QString::fromStdString(strategy.strategy_type()));
	m_ui->queueLimitSpin->setValue(static_cast<int>(0 == strategy.event_queue_limit() ? 4096 : strategy.event_queue_limit()));
	m_ui->enabledCheck->setChecked(strategy.enabled());
	m_ui->autoStartCheck->setChecked(strategy.auto_start());
	m_parameters.clear();
	m_subscriptions.clear();
	for (const auto& [name, value] : strategy.parameters())
	{
		m_parameters.push_back({ m_nextDraftId++, { name, value } });
	}
	for (const auto& subscription : strategy.subscriptions())
	{
		m_subscriptions.push_back({ m_nextDraftId++, { subscription.security(), subscription.exchange(), subscription.channel() } });
	}
	PublishTable(m_parameterData, *m_ui->parameterTable, m_parameters);
	PublishTable(m_subscriptionData, *m_ui->subscriptionTable, m_subscriptions);
}

request::StrategyInfo CStrategyEditDialog::GetStrategy() const
{
	request::StrategyInfo strategy;
	strategy.set_strategy_id(m_strategyId);
	_TyAccountIndex account;
	if (DecodeAccount(m_accounts->currentData().toString().toStdString(),
			account))
	{
		SetAccountIdentity(account, strategy);
	}
	strategy.set_strategy_name(m_ui->nameEdit->text().trimmed().toStdString());
	strategy.set_strategy_type(
		m_ui->typeCombo->currentText().trimmed().toStdString());
	strategy.set_event_queue_limit(
		static_cast<std::uint32_t>(m_ui->queueLimitSpin->value()));
	strategy.set_enabled(m_ui->enabledCheck->isChecked());
	strategy.set_auto_start(m_ui->autoStartCheck->isChecked());
	for (const auto& row : m_parameters)
	{
		QString key = QString::fromStdString(std::get<std::string>(row.m_values[0])).trimmed();
		if (!key.isEmpty())
		{
			(*strategy.mutable_parameters())[key.toStdString()] = QString::fromStdString(std::get<std::string>(row.m_values[1])).trimmed().toStdString();
		}
	}
	for (const auto& row : m_subscriptions)
	{
		QString security = QString::fromStdString(std::get<std::string>(row.m_values[0])).trimmed();
		if (security.isEmpty())
		{
			continue;
		}
		request::StrategySubscription* subscription = strategy.add_subscriptions();
		subscription->set_security(security.toStdString());
		subscription->set_exchange(std::get<std::string>(row.m_values[1]));
		subscription->set_channel(std::get<std::string>(row.m_values[2]));
	}
	return strategy;
}

void CStrategyEditDialog::ApplyEdit(const CTableOperation& operation, CDataTable& table, std::vector<CTableRow>& rows, CUITable& widget)
{
	if (TableOperationKind::Edit != operation.m_kind || table.GetView().GetSourceId() != operation.m_generation || 0 == operation.m_columnId)
	{
		return;
	}
	std::uint64_t& latest = m_editRequests[{ operation.m_generation, operation.m_rowId, operation.m_columnId }];
	if (latest >= operation.m_requestId)
	{
		return;
	}
	latest = operation.m_requestId;
	for (auto& row : rows)
	{
		if (row.m_id != operation.m_rowId || row.m_values.size() < operation.m_columnId)
		{
			continue;
		}
		std::string& value = std::get<std::string>(row.m_values[operation.m_columnId - 1]);
		if (QString::fromStdString(value) != operation.m_originalValue.toString())
		{
			return;
		}
		value = operation.m_value.toString().toStdString();
		PublishTable(table, widget, rows);
		return;
	}
}

void CStrategyEditDialog::OnAddParameter()
{
	m_parameters.push_back({ m_nextDraftId++, { std::string(), std::string() } });
	PublishTable(m_parameterData, *m_ui->parameterTable, m_parameters);
	m_ui->parameterTable->SelectRowId(m_parameters.back().m_id);
}

void CStrategyEditDialog::OnRemoveParameter()
{
	_TyDataRowId id = m_ui->parameterTable->SelectedRowId();
	std::erase_if(m_parameters, [id](const CTableRow& row)
	{
		return row.m_id == id;
	});
	PublishTable(m_parameterData, *m_ui->parameterTable, m_parameters);
}

void CStrategyEditDialog::OnAddSubscription()
{
	m_subscriptions.push_back({ m_nextDraftId++, { std::string(), std::string("SSE"), std::string("quote") } });
	PublishTable(m_subscriptionData, *m_ui->subscriptionTable, m_subscriptions);
	m_ui->subscriptionTable->SelectRowId(m_subscriptions.back().m_id);
}

void CStrategyEditDialog::OnRemoveSubscription()
{
	_TyDataRowId id = m_ui->subscriptionTable->SelectedRowId();
	std::erase_if(m_subscriptions, [id](const CTableRow& row)
	{
		return row.m_id == id;
	});
	PublishTable(m_subscriptionData, *m_ui->subscriptionTable, m_subscriptions);
}

void CStrategyEditDialog::OnAccept()
{
	QSet<QString> keys;
	for (const auto& row : m_parameters)
	{
		QString key = QString::fromStdString(std::get<std::string>(row.m_values[0])).trimmed();
		QString value = QString::fromStdString(std::get<std::string>(row.m_values[1])).trimmed();
		if (key.isEmpty() || value.isEmpty() || keys.contains(key))
		{
			m_ui->parameterTable->SelectRowId(row.m_id);
			QMessageBox::warning(this, "参数校验", "参数名称和值不能为空，名称不能重复");
			return;
		}
		keys.insert(key);
	}
	for (const auto& row : m_subscriptions)
	{
		if (QString::fromStdString(std::get<std::string>(row.m_values[0])).trimmed().isEmpty())
		{
			m_ui->subscriptionTable->SelectRowId(row.m_id);
			QMessageBox::warning(this, "参数校验", "订阅证券代码不能为空");
			return;
		}
	}
	request::StrategyInfo strategy = GetStrategy();
	bool accountAvailable = false;
	for (const auto& account : m_pServices->m_account.GetAccounts())
	{
		if (AccountIndex(account) == AccountIndex(strategy))
		{
			accountAvailable = (request::ACCOUNT_CONNECTED == account.state()) || ((0 != m_strategyId) && (m_originalAccount == AccountIndex(strategy)));
			break;
		}
	}
	if (!accountAvailable)
	{
		QMessageBox::warning(this, "交易账户不可用", "请在交易账户页登录账户；修改时可保留已有的离线账户绑定");
		return;
	}
	std::string error;
	if (!CStrategyService::ValidateStrategy(strategy, error))
	{
		QMessageBox::warning(this, "参数校验", QString::fromStdString(error));
		return;
	}
	accept();
}

void CStrategyEditDialog::SetAccountLocked(bool locked)
{
	m_accounts->setEnabled(!locked);
	m_accounts->setToolTip(locked ? "运行期间不能更换交易账户" : "");
}

void CStrategyEditDialog::RefreshAccounts()
{
	QString selected = m_accounts->currentData().toString();
	if (selected.isEmpty() && IsAccountValid(m_originalAccount))
	{
		selected = QString::fromStdString(EncodeAccount(m_originalAccount));
	}
	m_accounts->clear();
	for (const auto& account : m_pServices->m_account.GetAccounts())
	{
		QString state;
		switch (account.state())
		{
		case request::ACCOUNT_CONNECTED:
			state = "已连接";
			break;
		case request::ACCOUNT_CONNECTING:
			state = "连接中";
			break;
		case request::ACCOUNT_SYNCHRONIZING:
			state = "同步中";
			break;
		case request::ACCOUNT_RECONNECTING:
			state = "重连中";
			break;
		case request::ACCOUNT_ERROR:
			state = "连接失败";
			break;
		default:
			state = "未连接";
			break;
		}
		QString label = QString("%1 (#%2) / %3 / %4（%5） [%6]").arg(QString::fromStdString(account.broker_name())).arg(account.broker_id()).arg(QString::fromStdString(account.site()), QString::fromStdString(account.account()), QString::fromStdString(account.name()), state);
		m_accounts->addItem(label, QString::fromStdString(EncodeAccount(AccountIndex(account))));
	}
	int index = m_accounts->findData(selected);
	if (!selected.isEmpty() && (0 > index))
	{
		_TyAccountIndex identity;
		DecodeAccount(selected.toStdString(), identity);
		m_accounts->addItem(QString("账户 %1 / %2 [不可用]").arg(identity.first).arg(QString::fromStdString(identity.second)), selected);
		index = m_accounts->count() - 1;
	}
	if (0 == m_accounts->count())
	{
		m_accounts->addItem("请先在交易账户页登录账户");
	}
	if (0 <= index)
	{
		m_accounts->setCurrentIndex(index);
	}
}
