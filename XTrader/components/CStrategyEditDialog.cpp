#include "CStrategyEditDialog.h"
#include "../service/account/CTradeAccountService.h"
#include <QComboBox>
#include <QFormLayout>

#include "ui_CStrategyEditDialog.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QTableWidgetItem>

CStrategyEditDialog::CStrategyEditDialog(QWidget* pParent)
	: QDialog(pParent), ui(new Ui::CStrategyEditDialogClass())
{
	ui->setupUi(this);
	InitializeUI();
}

CStrategyEditDialog::~CStrategyEditDialog()
{
	delete ui;
}

void CStrategyEditDialog::InitializeUI()
{
	m_accounts = new QComboBox(this);
	for (const auto& account :
		 CTradeAccountService::InstanceRef().GetAccounts())
	{
		m_accounts->addItem(
			QString::fromStdString(account.name()),
			QString::fromStdString(EncodeAccount(AccountIndex(account))));
	}
	if (0 == m_accounts->count())
	{
		m_accounts->addItem(CTradeAccountService::InstanceRef().IsSupported()
								? "请先加载或新增交易账户"
								: "默认账户（旧版 XEngine）",
			QVariant::fromValue<qulonglong>(0));
	}
	ui->basicLayout->addRow("交易账户", m_accounts);
	ui->parameterTable->horizontalHeader()->setStretchLastSection(true);
	ui->subscriptionTable->horizontalHeader()->setStretchLastSection(true);
	connect(ui->addParameterButton, &QPushButton::clicked, this,
		&CStrategyEditDialog::OnAddParameter);
	connect(ui->removeParameterButton, &QPushButton::clicked, this,
		&CStrategyEditDialog::OnRemoveParameter);
	connect(ui->addSubscriptionButton, &QPushButton::clicked, this,
		&CStrategyEditDialog::OnAddSubscription);
	connect(ui->removeSubscriptionButton, &QPushButton::clicked, this,
		&CStrategyEditDialog::OnRemoveSubscription);
	connect(ui->buttonBox, &QDialogButtonBox::accepted, this,
		&CStrategyEditDialog::OnAccept);
	connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void CStrategyEditDialog::SetStrategy(const request::StrategyInfo& strategy)
{
	m_strategyId = strategy.strategy_id();
	_TyAccountIndex account = AccountIndex(strategy);
	int selectedIndex =
		m_accounts->findData(QString::fromStdString(EncodeAccount(account)));
	if ((0 > selectedIndex) && CTradeAccountService::InstanceRef().IsSupported())
	{
		m_accounts->addItem("账户 #" + QString::fromStdString(std::get<2>(account)),
							QString::fromStdString(EncodeAccount(account)));
		selectedIndex = m_accounts->count() - 1;
	}
	if (0 <= selectedIndex)
	{
		m_accounts->setCurrentIndex(selectedIndex);
	}
	ui->nameEdit->setText(QString::fromStdString(strategy.strategy_name()));
	ui->typeCombo->setCurrentText(
		QString::fromStdString(strategy.strategy_type()));
	ui->queueLimitSpin->setValue(static_cast<int>(strategy.event_queue_limit()));
	ui->enabledCheck->setChecked(strategy.enabled());
	ui->autoStartCheck->setChecked(strategy.auto_start());
	ui->parameterTable->setRowCount(0);
	for (const auto& [strKey, strValue] : strategy.parameters())
	{
		int row = ui->parameterTable->rowCount();
		ui->parameterTable->insertRow(row);
		ui->parameterTable->setItem(
			row, 0, new QTableWidgetItem(QString::fromStdString(strKey)));
		ui->parameterTable->setItem(
			row, 1, new QTableWidgetItem(QString::fromStdString(strValue)));
	}
	ui->subscriptionTable->setRowCount(0);
	for (const request::StrategySubscription& subscription :
		strategy.subscriptions())
	{
		int row = ui->subscriptionTable->rowCount();
		ui->subscriptionTable->insertRow(row);
		ui->subscriptionTable->setItem(
			row, 0,
			new QTableWidgetItem(QString::fromStdString(subscription.security())));
		ui->subscriptionTable->setItem(
			row, 1,
			new QTableWidgetItem(QString::fromStdString(subscription.exchange())));
		ui->subscriptionTable->setItem(
			row, 2,
			new QTableWidgetItem(QString::fromStdString(subscription.channel())));
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
	strategy.set_strategy_name(ui->nameEdit->text().trimmed().toStdString());
	strategy.set_strategy_type(
		ui->typeCombo->currentText().trimmed().toStdString());
	strategy.set_event_queue_limit(
		static_cast<std::uint32_t>(ui->queueLimitSpin->value()));
	strategy.set_enabled(ui->enabledCheck->isChecked());
	strategy.set_auto_start(ui->autoStartCheck->isChecked());
	for (int row = 0; row < ui->parameterTable->rowCount(); ++row)
	{
		QTableWidgetItem* pKey = ui->parameterTable->item(row, 0);
		QTableWidgetItem* pValue = ui->parameterTable->item(row, 1);
		if ((nullptr != pKey) && !pKey->text().trimmed().isEmpty())
		{
			(*strategy.mutable_parameters())[pKey->text().trimmed().toStdString()] =
				nullptr == pValue ? std::string()
								  : pValue->text().trimmed().toStdString();
		}
	}
	for (int row = 0; row < ui->subscriptionTable->rowCount(); ++row)
	{
		QTableWidgetItem* pSecurity = ui->subscriptionTable->item(row, 0);
		QTableWidgetItem* pExchange = ui->subscriptionTable->item(row, 1);
		QTableWidgetItem* pChannel = ui->subscriptionTable->item(row, 2);
		if ((nullptr == pSecurity) || pSecurity->text().trimmed().isEmpty())
		{
			continue;
		}
		request::StrategySubscription* pSubscription = strategy.add_subscriptions();
		pSubscription->set_security(pSecurity->text().trimmed().toStdString());
		pSubscription->set_exchange(
			nullptr == pExchange ? std::string()
								 : pExchange->text().trimmed().toStdString());
		pSubscription->set_channel(nullptr == pChannel
									   ? std::string()
									   : pChannel->text().trimmed().toStdString());
	}
	return strategy;
}

void CStrategyEditDialog::OnAddParameter()
{
	ui->parameterTable->insertRow(ui->parameterTable->rowCount());
}

void CStrategyEditDialog::OnRemoveParameter()
{
	ui->parameterTable->removeRow(ui->parameterTable->currentRow());
}

void CStrategyEditDialog::OnAddSubscription()
{
	int row = ui->subscriptionTable->rowCount();
	ui->subscriptionTable->insertRow(row);
	ui->subscriptionTable->setItem(row, 1, new QTableWidgetItem("sse"));
	ui->subscriptionTable->setItem(row, 2, new QTableWidgetItem("quote"));
}

void CStrategyEditDialog::OnRemoveSubscription()
{
	ui->subscriptionTable->removeRow(ui->subscriptionTable->currentRow());
}

void CStrategyEditDialog::OnAccept()
{
	request::StrategyInfo strategy = GetStrategy();
	if (CTradeAccountService::InstanceRef().IsSupported() &&
		(!IsAccountValid(AccountIndex(strategy))))
	{
		QMessageBox::warning(this, "缺少交易账户",
			"请先在交易账户页加载或新增账户");
		return;
	}
	if (strategy.strategy_name().empty() || strategy.strategy_type().empty() ||
		(0 == strategy.subscriptions_size()))
	{
		QMessageBox::warning(this, "参数校验",
			"请填写策略名称、策略类型，并至少添加一个订阅标的。");
		return;
	}
	accept();
}

void CStrategyEditDialog::SetAccountLocked(bool locked)
{
	m_accounts->setEnabled(!locked);
	m_accounts->setToolTip(locked ? "运行期间不能更换交易账户" : "");
}
