// Replace only transport/account boundaries; exercise the production service
// and dialog.
namespace net
{
	class CTcpClient
	{
	};
} // namespace net
#include "components/CStrategyEditDialog.h"
#include "service/account/CAccountService.h"
#include "service/hqmarket/CHQService.h"
#include "service/strategy/CStrategyService.h"
#include "service/watchlist/CWatchlistService.h"
#include "system/CTraderCenter.h"

#include "components/CKeyboardSprite.h"
#include "components/CUITable.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <cstdlib>
#include <iostream>

namespace
{
	CTraderCenter center;
	CStrategyService* pStrategyService{ nullptr };
	std::vector<CRequest> requests;
	std::vector<CSession::_TyStateCallback> stateHandlers;
	std::vector<request::TradeAccountRecord> accounts;
	TMessagePump<CAccountService::CResult> accountEvents;
	bool sendSuccess = true;

	void Check(bool passed, const char* message)
	{
		if (!passed)
		{
			std::cerr << message << '\n';
			std::exit(1);
		}
	}

	void Reply(const CRequest& response)
	{
		center.OnRequestReply(response);
	}

	CRequest Last(_TyRequest type)
	{
		for (auto vIter = requests.rbegin(); requests.rend() != vIter; ++vIter)
		{
			if (type == vIter->GetType())
			{
				return *vIter;
			}
		}
		Check(false, "Expected request was not sent");
		return {};
	}

	request::StrategyInfo Strategy()
	{
		request::StrategyInfo strategy;
		strategy.set_strategy_id(42);
		strategy.set_strategy_name("MA");
		strategy.set_strategy_type("dual_ma_cross");
		strategy.set_broker_id(1);
		strategy.set_account("000123");
		strategy.set_enabled(true);
		request::StrategySubscription* subscription = strategy.add_subscriptions();
		subscription->set_security("600000");
		subscription->set_exchange("SSE");
		subscription->set_channel("quote");
		return strategy;
	}

	void ConfigReply(const CRequest& query, const request::StrategyInfo& strategy)
	{
		CRequest response = query;
		request::StrategyList list;
		*list.add_strategies() = strategy;
		response.SetData(list);
		Reply(response);
	}

	void RuntimeReply(const CRequest& query, int state,
		std::uint64_t activeOrders = 0)
	{
		CRequest response = query;
		request::StrategyRuntimeList list;
		request::StrategyRuntimeRecord* record = list.add_runtimes();
		record->set_strategy_id(42);
		record->set_state(state);
		record->set_active_orders(activeOrders);
		response.SetData(list);
		Reply(response);
	}

	void CloseWarning()
	{
		QTimer::singleShot(0, []()
		{
			for (QWidget* widget : QApplication::topLevelWidgets())
			{
				QMessageBox* message = qobject_cast<QMessageBox*>(widget);
				if (nullptr != message)
				{
					message->accept();
				}
			}
		});
	}
} // namespace

CTraderCenter::CTraderCenter() = default;

CTraderCenter::~CTraderCenter() = default;

bool CTraderCenter::OnRequestReply(const CRequest& response)
{
	return (nullptr != pStrategyService) &&
		   pStrategyService->OnRequestReply(response);
}
bool CTraderCenter::SendRequest(const CRequest& request)
{
	requests.emplace_back(request);
	return sendSuccess;
}
void CTraderCenter::RegisterStateHandler(CSession::_TyStateCallback&& handler)
{
	stateHandlers.emplace_back(std::move(handler));
}
bool CTraderCenter::IsAuthenticated() const noexcept
{
	return true;
}
CLoginService::CLoginService() = default;

CLoginService::~CLoginService() = default;

CHQService::CHQService() = default;

CHQService::~CHQService() = default;

std::vector<CSecurity> CHQService::GetSecurities() const
{
	return { CSecurity("600000", Exchange::sse) };
}

CAccountService::CAccountService() = default;

CAccountService::~CAccountService() = default;
std::vector<request::TradeAccountRecord> CAccountService::GetAccounts() const
{
	return accounts;
}
_TyCallbackId CAccountService::Subscribe(_TyHandler&& handler)
{
	return accountEvents.Subscribe(std::move(handler));
}
void CAccountService::Unsubscribe(_TyCallbackId token)
{
	accountEvents.Unsubscribe(token);
}

std::uint64_t CAccountService::Send(
	_TyRequest type, const _TyAccountIndex& account,
	const std::unordered_map<std::string, std::string>& extra)
{
	CRequest request;
	request.SetType(type);
	if (IsAccountValid(account))
	{
		WriteAccountIdentity(account, request);
	}
	for (const auto& [key, value] : extra)
	{
		request.SetExtraData(key, value);
	}
	requests.push_back(request);
	return request.GetId();
}

int main(int argc, char** argv)
{
	QApplication app(argc, argv);
	CLoginService login;
	CStrategyService service;
	CHQService market;
	CAccountService account;
	CWatchlistService watchlist;
	CTraderServices services{ login, service, market, account, watchlist };
	pStrategyService = &service;
	service.SetTraderCenter(&center);
	request::TradeAccountRecord record;
	record.set_broker_id(1);
	record.set_account("000123");
	record.set_state(request::ACCOUNT_CONNECTED);
	accounts.push_back(record);
	CStrategyEditDialog dialog(services);
	QComboBox* types = dialog.findChild<QComboBox*>("typeCombo");
	Check(nullptr != types && 4 == types->count(),
		"all four strategy choices are present");
	dialog.findChild<QLineEdit*>("nameEdit")->setText("typed strategy");
	dialog.show();
	QApplication::processEvents();
	CUITable* subscriptions = dialog.findChild<CUITable*>("subscriptionTable");
	QPushButton* addSubscription = dialog.findChild<QPushButton*>("addSubscriptionButton");
	QPushButton* removeSubscription = dialog.findChild<QPushButton*>("removeSubscriptionButton");
	addSubscription->click();
	Check(1 == subscriptions->model()->rowCount(), "plus inserts a draft row directly");
	QLineEdit* securityEditor = subscriptions->findChild<QLineEdit*>();
	Check(nullptr != securityEditor, "new row immediately opens its code editor");
	securityEditor->setText("600000");
	QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
	QApplication::sendEvent(securityEditor, &enter);
	QApplication::processEvents();
	Check("SSE" == dialog.GetStrategy().subscriptions(0).exchange(), "code editor fills exchange");
	Check(0 == (subscriptions->model()->flags(subscriptions->model()->index(0, 1)) & Qt::ItemIsEditable),
		"inferred exchange is read only");
	for (const QString& code : { QString("000001"), QString("430047"), QString("920001") })
	{
		addSubscription->click();
		QModelIndex index = subscriptions->currentIndex();
		Check(subscriptions->model()->setData(index, code), "draft code can be edited");
		request::StrategyInfo draft = dialog.GetStrategy();
		Check((code.startsWith('0') ? "SZSE" : "BSE") == draft.subscriptions(2).exchange(),
			"exchange inference works without cached security data");
		removeSubscription->click();
		QApplication::processEvents();
		Check(1 == subscriptions->model()->rowCount(), "minus removes selected draft");
	}
	Check(nullptr == dialog.findChild<QLineEdit*>("strategy_security"), "standalone code input is removed");
	std::string error;
	Check(!dialog.GetStrategy().enabled() && !dialog.GetStrategy().auto_start(),
		"new strategies are inactive");
	Check(CStrategyService::ValidateStrategy(dialog.GetStrategy(), error),
		"default SMA form validates");
	Check(2 == dialog.GetStrategy().subscriptions_size(),
		"MA form supplies quotes and bars automatically");
	QComboBox* period = dialog.findChild<QComboBox*>("strategy_bar_period");
	period->setCurrentIndex(period->findData("1d"));
	Check("bar_1d" == dialog.GetStrategy().subscriptions(1).channel(),
		"bar subscription follows selected period");
	types->setCurrentIndex(types->findData("take_profit_stop_loss"));
	Check(!CStrategyService::ValidateStrategy(dialog.GetStrategy(), error),
		"stop threshold requires input");
	dialog.findChild<QLineEdit*>("strategy_stop_loss_value")->setText("5");
	Check(CStrategyService::ValidateStrategy(dialog.GetStrategy(), error),
		"stop loss form validates");
	types->setCurrentIndex(types->findData("trailing_take_profit"));
	dialog.findChild<QLineEdit*>("strategy_activation_value")->setText("5");
	dialog.findChild<QLineEdit*>("strategy_drawdown_value")->setText("2");
	Check(CStrategyService::ValidateStrategy(dialog.GetStrategy(), error),
		"trailing form validates");
	types->setCurrentIndex(types->findData("price_breakout"));
	dialog.findChild<QLineEdit*>("strategy_trigger_price")->setText("10.25");
	Check(CStrategyService::ValidateStrategy(dialog.GetStrategy(), error),
		"breakout form validates");
	request::StrategyInfo saved = dialog.GetStrategy();
	saved.set_strategy_id(42);
	saved.set_config_revision(3);
	CStrategyEditDialog edit(services);
	edit.SetStrategy(saved);
	Check(edit.GetStrategy().parameters().at("trigger_price") == "10.25" &&
			  3 == edit.GetStrategy().config_revision(),
		"form round-trips configuration and revision");
	Check(!edit.findChild<QComboBox*>("typeCombo")->isEnabled() &&
			  QAbstractItemView::NoEditTriggers == edit.findChild<CUITable*>("subscriptionTable")->editTriggers(),
		"existing immutable binding is locked");
	request::StrategyInfo multiple = saved;
	multiple.add_subscriptions()->CopyFrom(multiple.subscriptions(0));
	multiple.mutable_subscriptions(1)->set_security("600001");
	Check(CStrategyService::ValidateStrategy(multiple, error),
		"multiple distinct securities are supported");
	multiple.mutable_subscriptions(1)->set_security("600000");
	Check(!CStrategyService::ValidateStrategy(multiple, error),
		"duplicate security subscriptions are rejected");
	bool actionDelivered = false;
	service.AddOperationHandler(
		[&actionDelivered](const CStrategyService::COperationResult& result)
	{
		if (_TyRequest::STRATEGY_ACTION == result.m_type)
		{
			actionDelivered = result.m_success;
		}
	});
	std::uint64_t action = service.ActionStrategy(42, "rearm");
	Check(0 != action &&
			  "rearm" == Last(_TyRequest::STRATEGY_ACTION).GetExtraData("action"),
		"strategy actions use explicit request type");
	CRequest response = Last(_TyRequest::STRATEGY_ACTION);
	Reply(response);
	Check(actionDelivered,
		"strategy action response reaches operation observers");
	std::cout << "Four-strategy UI and service tests passed\n";
	return 0;
}
