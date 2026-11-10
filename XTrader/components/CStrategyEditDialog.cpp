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
#include <QTableWidgetItem>

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
	m_ui->parameterTable->setRowCount(2);
	m_ui->parameterTable->setItem(0, 0, new QTableWidgetItem("short_window"));
	m_ui->parameterTable->setItem(0, 1, new QTableWidgetItem("5"));
	m_ui->parameterTable->setItem(1, 0, new QTableWidgetItem("long_window"));
	m_ui->parameterTable->setItem(1, 1, new QTableWidgetItem("20"));
	m_ui->basicLayout->addRow("交易账户", m_accounts);
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
	m_ui->parameterTable->setRowCount(0);
	for (const auto& [strKey, strValue] : strategy.parameters())
	{
		int row = m_ui->parameterTable->rowCount();
		m_ui->parameterTable->insertRow(row);
		m_ui->parameterTable->setItem(
			row, 0, new QTableWidgetItem(QString::fromStdString(strKey)));
		m_ui->parameterTable->setItem(
			row, 1, new QTableWidgetItem(QString::fromStdString(strValue)));
	}
	m_ui->subscriptionTable->setRowCount(0);
	for (const request::StrategySubscription& subscription :
		strategy.subscriptions())
	{
		int row = m_ui->subscriptionTable->rowCount();
		m_ui->subscriptionTable->insertRow(row);
		m_ui->subscriptionTable->setItem(
			row, 0,
			new QTableWidgetItem(QString::fromStdString(subscription.security())));
		SetSubscriptionChoices(row, QString::fromStdString(subscription.exchange()), QString::fromStdString(subscription.channel()));
	}
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
	for (int row = 0; row < m_ui->parameterTable->rowCount(); ++row)
	{
		QTableWidgetItem* pKey = m_ui->parameterTable->item(row, 0);
		QTableWidgetItem* pValue = m_ui->parameterTable->item(row, 1);
		if ((nullptr != pKey) && !pKey->text().trimmed().isEmpty())
		{
			(*strategy.mutable_parameters())[pKey->text().trimmed().toStdString()] =
				nullptr == pValue ? std::string()
								  : pValue->text().trimmed().toStdString();
		}
	}
	for (int row = 0; row < m_ui->subscriptionTable->rowCount(); ++row)
	{
		QTableWidgetItem* pSecurity = m_ui->subscriptionTable->item(row, 0);
		QComboBox* pExchange = qobject_cast<QComboBox*>(m_ui->subscriptionTable->cellWidget(row, 1));
		QComboBox* pChannel = qobject_cast<QComboBox*>(m_ui->subscriptionTable->cellWidget(row, 2));
		if ((nullptr == pSecurity) || pSecurity->text().trimmed().isEmpty())
		{
			continue;
		}
		request::StrategySubscription* pSubscription = strategy.add_subscriptions();
		pSubscription->set_security(pSecurity->text().trimmed().toStdString());
		pSubscription->set_exchange(
			nullptr == pExchange ? std::string()
								 : pExchange->currentText().trimmed().toStdString());
		pSubscription->set_channel(nullptr == pChannel
									   ? std::string()
									   : pChannel->currentText().trimmed().toStdString());
	}
	return strategy;
}

void CStrategyEditDialog::OnAddParameter()
{
	m_ui->parameterTable->insertRow(m_ui->parameterTable->rowCount());
}

void CStrategyEditDialog::OnRemoveParameter()
{
	m_ui->parameterTable->removeRow(m_ui->parameterTable->currentRow());
}

void CStrategyEditDialog::OnAddSubscription()
{
	int row = m_ui->subscriptionTable->rowCount();
	m_ui->subscriptionTable->insertRow(row);
	SetSubscriptionChoices(row, "SSE", "quote");
}

void CStrategyEditDialog::OnRemoveSubscription()
{
	m_ui->subscriptionTable->removeRow(m_ui->subscriptionTable->currentRow());
}

void CStrategyEditDialog::OnAccept()
{
	QSet<QString> keys;
	for (int row = 0; row < m_ui->parameterTable->rowCount(); ++row)
	{
		QTableWidgetItem* key = m_ui->parameterTable->item(row, 0);
		QTableWidgetItem* value = m_ui->parameterTable->item(row, 1);
		if ((nullptr == key) || key->text().trimmed().isEmpty() || (nullptr == value) || value->text().trimmed().isEmpty() || keys.contains(key->text().trimmed()))
		{
			m_ui->parameterTable->setCurrentCell(row, 0);
			QMessageBox::warning(this, "参数校验", QString("参数第 %1 行：名称和值不能为空，名称不能重复").arg(row + 1));
			return;
		}
		keys.insert(key->text().trimmed());
	}
	for (int row = 0; row < m_ui->subscriptionTable->rowCount(); ++row)
	{
		QTableWidgetItem* security = m_ui->subscriptionTable->item(row, 0);
		if ((nullptr == security) || security->text().trimmed().isEmpty())
		{
			m_ui->subscriptionTable->setCurrentCell(row, 0);
			QMessageBox::warning(this, "参数校验", QString("订阅第 %1 行：证券代码不能为空").arg(row + 1));
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

void CStrategyEditDialog::SetSubscriptionChoices(int row, const QString& exchange, const QString& channel)
{
	QComboBox* exchanges = new QComboBox(m_ui->subscriptionTable);
	exchanges->addItems({ "SSE", "SZSE", "BSE", "HKEX", "CFFEX", "SHFE", "DCE", "CZCE", "INE", "GFEX", "NASDAQ", "NYSE", "CRYPTO" });
	if (0 > exchanges->findText(exchange))
	{
		exchanges->addItem(exchange);
	}
	exchanges->setCurrentText(exchange);
	m_ui->subscriptionTable->setCellWidget(row, 1, exchanges);
	QComboBox* channels = new QComboBox(m_ui->subscriptionTable);
	channels->addItems({ "quote", "depth" });
	if (0 > channels->findText(channel))
	{
		channels->addItem(channel);
	}
	channels->setCurrentText(channel);
	m_ui->subscriptionTable->setCellWidget(row, 2, channels);
}
