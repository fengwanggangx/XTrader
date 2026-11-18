#include "CStrategyEditDialog.h"
#include "../service/account/CAccountService.h"
#include "../service/strategy/CStrategyService.h"
#include "CSinglePixelBorder.h"
#include "CUIStyle.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPointer>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QWindow>

#include "ui_CStrategyEditDialog.h"

#include <QHeaderView>
#include <QMessageBox>
#include "CTableData.h"

namespace
{
	class CComboItemDelegate final : public QStyledItemDelegate
	{
	public:
		explicit CComboItemDelegate(QComboBox& combo) : QStyledItemDelegate(combo.view()), m_combo(combo)
		{
		}

		QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
		{
			QSize size = QStyledItemDelegate::sizeHint(option, index);
			size.setHeight(m_combo.height());
			return size;
		}

	private:
		QComboBox& m_combo;
	};

	void PrepareCombo(QComboBox& combo)
	{
		combo.setFrame(false);
		combo.setItemDelegate(new CComboItemDelegate(combo));
		combo.view()->setFrameShape(QFrame::NoFrame);
		QFrame* pPopup = qobject_cast<QFrame*>(combo.view()->parentWidget());
		if (nullptr != pPopup)
		{
			pPopup->setFrameShape(QFrame::NoFrame);
		}
		CSinglePixelBorder::Install(*combo.view());
	}
}

CStrategyEditDialog::CStrategyEditDialog(CTraderServices& services, QWidget* pParent)
	: QDialog(pParent), m_ui(std::make_unique<Ui::CStrategyEditDialogClass>())
{
	m_pServices = &services;
	setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
	m_ui->setupUi(this);
	UIStyle::Apply(*this, ":/styles/strategy-edit.qss");
	m_ui->titleFrame->installEventFilter(this);
	m_ui->titleLabel->installEventFilter(this);
	connect(m_ui->closeButton, &QPushButton::clicked, this, &QDialog::reject);
	connect(m_ui->saveButton, &QPushButton::clicked, this, &CStrategyEditDialog::OnAccept);
	connect(m_ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
	InitializeUI();
}

CStrategyEditDialog::~CStrategyEditDialog()
{
	m_pServices->m_account.Unsubscribe(m_accountToken);
}

void CStrategyEditDialog::InitializeUI()
{
	m_ui->bodyLayout->setStretch(1, 1);
	m_ui->bodyLayout->setStretch(2, 1);
	m_accounts = new QComboBox(this);
	m_accounts->setFixedHeight(27);
	m_accounts->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	m_accounts->setMinimumContentsLength(8);
	m_accounts->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
	PrepareCombo(*m_accounts);
	CSinglePixelBorder::Install(*m_accounts);
	connect(m_accounts, &QComboBox::currentTextChanged, m_accounts, [this](const QString& text)
	{
		m_accounts->setToolTip(text);
	});
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
	m_ui->typeCombo->clear();
	m_ui->typeCombo->addItem("双均线交叉", "dual_ma_cross");
	m_ui->typeCombo->addItem("止盈止损", "take_profit_stop_loss");
	m_ui->typeCombo->addItem("移动止盈", "trailing_take_profit");
	m_ui->typeCombo->addItem("价格突破", "price_breakout");
	m_ui->enabledCheck->setChecked(false);
	m_ui->autoStartCheck->setChecked(false);
	QScrollArea* area = new QScrollArea(m_ui->parameterGroup);
	area->setWidgetResizable(true);
	QWidget* form = new QWidget(area);
	m_parameterForm = new QFormLayout(form);
	area->setWidget(form);
	m_ui->parameterLayout->addWidget(area);
	m_ui->parameterTable->hide();
	m_ui->addParameterButton->hide();
	m_ui->removeParameterButton->hide();
	m_ui->subscriptionTable->hide();
	m_ui->addSubscriptionButton->hide();
	m_ui->removeSubscriptionButton->hide();
	QWidget* instrument = new QWidget(m_ui->subscriptionGroup);
	QFormLayout* instrumentForm = new QFormLayout(instrument);
	m_securityEdit = new QLineEdit(instrument);
	m_securityEdit->setObjectName("strategy_security");
	m_securityEdit->setPlaceholderText("六位股票代码");
	m_exchange = new QComboBox(instrument);
	m_exchange->setObjectName("strategy_exchange");
	m_exchange->addItems({ "SSE", "SZSE", "BSE" });
	instrumentForm->addRow("股票代码", m_securityEdit);
	instrumentForm->addRow("交易所", m_exchange);
	m_ui->subscriptionLayout->addWidget(instrument);
	connect(m_ui->typeCombo, &QComboBox::currentIndexChanged, this,
		[this]()
	{
		BuildFields(
			m_ui->typeCombo->currentData().toString().toStdString());
	});
	m_ui->queueLimitSpin->setMinimum(1);
	DefineTable(m_parameterData, { { 1, "参数名", DataType::String }, { 2, "参数值", DataType::String } });
	DefineTable(m_subscriptionData, { { 1, "证券代码", DataType::String }, { 2, "交易所", DataType::String }, { 3, "行情通道", DataType::String } });
	m_parameters.clear();
	BuildFields("dual_ma_cross");
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
	m_ui->parameterTable->SetHeaderTable(QStringLiteral("strategy_edit_parameters"));
	m_ui->subscriptionTable->SetHeaderTable(QStringLiteral("strategy_edit_subscriptions"));
	connect(m_ui->addParameterButton, &QPushButton::clicked, this,
		&CStrategyEditDialog::OnAddParameter);
	connect(m_ui->removeParameterButton, &QPushButton::clicked, this,
		&CStrategyEditDialog::OnRemoveParameter);
	connect(m_ui->addSubscriptionButton, &QPushButton::clicked, this,
		&CStrategyEditDialog::OnAddSubscription);
	connect(m_ui->removeSubscriptionButton, &QPushButton::clicked, this,
		&CStrategyEditDialog::OnRemoveSubscription);
	m_ui->nameEdit->setFrame(false);
	m_ui->queueLimitSpin->setFrame(false);
	PrepareCombo(*m_ui->typeCombo);
	for (QWidget* pControl :
		{ static_cast<QWidget*>(m_ui->nameEdit),
			static_cast<QWidget*>(m_ui->typeCombo),
			static_cast<QWidget*>(m_ui->queueLimitSpin) })
	{
		CSinglePixelBorder::Install(*pControl);
	}
	CSinglePixelBorder::Install(*this);
	CSinglePixelBorder::Install(*m_ui->titleFrame, 0.0, Qt::BottomEdge);
	CSinglePixelBorder::Install(*m_ui->divider, 0.0, Qt::TopEdge);
	CSinglePixelBorder::Install(*m_ui->cancelButton, 4.0);
	for (QPushButton* pButton :
		{ m_ui->addParameterButton, m_ui->removeParameterButton,
			m_ui->addSubscriptionButton, m_ui->removeSubscriptionButton })
	{
		CSinglePixelBorder::Install(*pButton, 4.0);
	}
}

void CStrategyEditDialog::SetStrategy(const request::StrategyInfo& strategy)
{
	m_ui->titleLabel->setText("修改策略");
	setWindowTitle("修改策略");
	m_strategyId = strategy.strategy_id();
	m_originalAccount = AccountIndex(strategy);
	m_accounts->setCurrentIndex(-1);
	RefreshAccounts();
	m_accounts->setCurrentIndex(m_accounts->findData(QString::fromStdString(EncodeAccount(m_originalAccount))));
	m_ui->nameEdit->setText(QString::fromStdString(strategy.strategy_name()));
	m_ui->typeCombo->setCurrentIndex(m_ui->typeCombo->findData(
		QString::fromStdString(strategy.strategy_type())));
	m_ui->typeCombo->setEnabled(0 == m_strategyId);
	m_accounts->setEnabled(0 == m_strategyId);
	m_securityEdit->setEnabled(0 == m_strategyId);
	m_exchange->setEnabled(0 == m_strategyId);
	m_configRevision = strategy.config_revision();
	std::map<std::string, std::string> values;
	for (const auto& [key, value] : strategy.parameters())
	{
		values.emplace(key, value);
	}
	BuildFields(strategy.strategy_type(), values);
	if (0 < strategy.subscriptions_size())
	{
		m_securityEdit->setText(
			QString::fromStdString(strategy.subscriptions(0).security()));
		m_exchange->setCurrentText(
			QString::fromStdString(strategy.subscriptions(0).exchange()));
	}
	m_ui->queueLimitSpin->setValue(static_cast<int>(
		0 == strategy.event_queue_limit() ? 4096
										  : strategy.event_queue_limit()));
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
	request::StrategyInfo result;
	result.set_strategy_id(m_strategyId);
	result.set_config_revision(m_configRevision);
	_TyAccountIndex account;
	if (DecodeAccount(m_accounts->currentData().toString().toStdString(),
			account))
	{
		SetAccountIdentity(account, result);
	}
	result.set_strategy_name(m_ui->nameEdit->text().trimmed().toStdString());
	result.set_strategy_type(
		m_ui->typeCombo->currentData().toString().toStdString());
	result.set_event_queue_limit(m_ui->queueLimitSpin->value());
	result.set_enabled(m_ui->enabledCheck->isChecked());
	result.set_auto_start(m_ui->autoStartCheck->isChecked());
	for (const auto& [key, field] : m_fields)
	{
		std::string value;
		if (QCheckBox* box = qobject_cast<QCheckBox*>(field); nullptr != box)
		{
			value = box->isChecked() ? "1" : "0";
		}
		else if (QComboBox* combo = qobject_cast<QComboBox*>(field);
			nullptr != combo)
		{
			value = combo->currentData().toString().toStdString();
		}
		else if (QLineEdit* edit = qobject_cast<QLineEdit*>(field);
			nullptr != edit)
		{
			value = edit->text().trimmed().toStdString();
		}
		if (!value.empty())
		{
			(*result.mutable_parameters())[key] = value;
		}
	}
	request::StrategySubscription* quote = result.add_subscriptions();
	quote->set_security(m_securityEdit->text().trimmed().toStdString());
	quote->set_exchange(m_exchange->currentText().toStdString());
	quote->set_channel("quote");
	if ("dual_ma_cross" == result.strategy_type())
	{
		request::StrategySubscription* bar = result.add_subscriptions();
		bar->CopyFrom(*quote);
		bar->set_channel("1d" == result.parameters().at("bar_period") ? "bar_1d"
																	  : "bar_1m");
	}
	return result;
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
	m_accounts->setEnabled(!locked && (0 == m_strategyId));
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

bool CStrategyEditDialog::eventFilter(QObject* pObject, QEvent* pEvent)
{
	if (((m_ui->titleFrame == pObject) || (m_ui->titleLabel == pObject)) &&
		(QEvent::MouseButtonPress == pEvent->type()))
	{
		QMouseEvent* pMouse = static_cast<QMouseEvent*>(pEvent);
		if ((Qt::LeftButton == pMouse->button()) && (nullptr != windowHandle()))
		{
			return windowHandle()->startSystemMove();
		}
	}
	return QDialog::eventFilter(pObject, pEvent);
}

void CStrategyEditDialog::BuildFields(
	const std::string& type, const std::map<std::string, std::string>& values)
{
	while (0 < m_parameterForm->rowCount())
	{
		m_parameterForm->removeRow(0);
	}
	m_fields.clear();
	m_parameters.clear();
	auto text = [this, &values](const std::string& key, const QString& label,
					const std::string& fallback = {})
	{
		QLineEdit* edit = new QLineEdit(this);
		edit->setObjectName(QString::fromStdString("strategy_" + key));
		const auto mIter = values.find(key);
		edit->setText(QString::fromStdString(
			values.end() == mIter ? fallback : mIter->second));
		edit->setPlaceholderText("请填写数值");
		m_fields.emplace(key, edit);
		m_parameterForm->addRow(label, edit);
	};
	auto choice =
		[this, &values](const std::string& key, const QString& label,
			const std::vector<std::pair<QString, QString>>& options)
	{
		QComboBox* combo = new QComboBox(this);
		combo->setObjectName(QString::fromStdString("strategy_" + key));
		for (const auto& option : options)
		{
			combo->addItem(option.first, option.second);
		}
		const auto mIter = values.find(key);
		if (values.end() != mIter)
		{
			combo->setCurrentIndex(
				combo->findData(QString::fromStdString(mIter->second)));
		}
		m_fields.emplace(key, combo);
		m_parameterForm->addRow(label, combo);
	};
	auto flag = [this, &values](const std::string& key, const QString& label,
					bool fallback)
	{
		QCheckBox* box = new QCheckBox(this);
		box->setObjectName(QString::fromStdString("strategy_" + key));
		const auto mIter = values.find(key);
		box->setChecked(values.end() == mIter ? fallback : "1" == mIter->second);
		m_fields.emplace(key, box);
		m_parameterForm->addRow(label, box);
	};
	if ("dual_ma_cross" == type)
	{
		choice("ma_type", "均线算法",
			{ { "简单均线 SMA", "sma" }, { "指数均线 EMA", "ema" } });
		choice("bar_period", "K 线周期", { { "一分钟", "1m" }, { "日线", "1d" } });
		text("short_window", "短窗口", "5");
		text("long_window", "长窗口", "20");
	}
	if ("take_profit_stop_loss" == type)
	{
		flag("take_profit_enabled", "启用止盈", false);
		choice("take_profit_mode", "止盈模式",
			{ { "盈利比例 (%)", "percent" }, { "指定价格", "price" } });
		text("take_profit_value", "止盈阈值");
		flag("stop_loss_enabled", "启用止损", true);
		choice("stop_loss_mode", "止损模式",
			{ { "亏损比例 (%)", "percent" }, { "指定价格", "price" } });
		text("stop_loss_value", "止损阈值");
	}
	if ("trailing_take_profit" == type)
	{
		choice("activation_mode", "启动模式",
			{ { "盈利比例 (%)", "percent" }, { "指定价格", "price" } });
		text("activation_value", "启动阈值");
		choice("drawdown_mode", "回撤模式",
			{ { "回撤比例 (%)", "percent" }, { "价格差", "amount" } });
		text("drawdown_value", "回撤阈值");
	}
	if ("price_breakout" == type)
	{
		text("trigger_price", "突破价格");
		choice("direction", "穿越方向", { { "向上", "up" }, { "向下", "down" } });
		choice("action", "交易方向", { { "买入", "buy" }, { "卖出", "sell" } });
	}
	if (("dual_ma_cross" == type) || ("price_breakout" == type))
	{
		choice("buy_mode", "买入模式",
			{ { "固定数量", "quantity" },
				{ "固定金额 (元)", "amount" },
				{ "可用资金比例 (%)", "funds_ratio" } });
		text("buy_value", "买入数值", "100");
	}
	choice("sell_mode", "卖出模式",
		{ { "全部持仓", "all" },
			{ "固定数量", "quantity" },
			{ "总持仓比例 (%)", "position_ratio" } });
	text("sell_value", "卖出数值（非全仓时必填）");
	if ("dual_ma_cross" != type)
	{
		flag("auto_rearm", "自动重新布防", false);
	}
}
