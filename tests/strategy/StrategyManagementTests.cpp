// Replace only transport/account boundaries; exercise the production service and dialog.
namespace net
{
	class CTcpClient
	{
	};
} // namespace net
#include "system/CTraderCenter.h"
#include "service/strategy/CStrategyService.h"
#include "service/account/CAccountService.h"
#include "service/hqmarket/CHQService.h"
#include "components/CStrategyEditDialog.h"
#include "components/CViewStrategySettings.h"
#include "components/CViewTradeAccounts.h"
#include <QPushButton>
#include <QTabWidget>
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QTableWidget>
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
		strategy.set_strategy_type("moving_average");
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

	void RuntimeReply(const CRequest& query, int state, std::uint64_t activeOrders = 0)
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
	return (nullptr != pStrategyService) && pStrategyService->OnRequestReply(response);
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
CViewTradeAccounts::CViewTradeAccounts(CTraderServices& services, QWidget* parent) : QWidget(parent)
{
}
CViewTradeAccounts::~CViewTradeAccounts() = default;
void CViewTradeAccounts::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event);
}
void CViewTradeAccounts::SetStrategyNavigation(std::function<void(std::uint64_t)>&& handler)
{
	m_navigate = std::move(handler);
}
CLoginService::CLoginService() = default;

CLoginService::~CLoginService() = default;

CHQService::CHQService() = default;

CHQService::~CHQService() = default;

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

int main(int argc, char** argv)
{
	QApplication app(argc, argv);
	request::StrategyInfo strategy = Strategy();
	std::string error;
	Check(CStrategyService::ValidateStrategy(strategy, error), "Default moving average rejected");
	(*strategy.mutable_parameters())["short_window"] = "20";
	Check(!CStrategyService::ValidateStrategy(strategy, error), "Equal windows accepted");
	(*strategy.mutable_parameters())["short_window"] = "-1";
	Check(!CStrategyService::ValidateStrategy(strategy, error), "Negative window accepted");
	(*strategy.mutable_parameters())["short_window"] = "18446744073709551616";
	Check(!CStrategyService::ValidateStrategy(strategy, error), "Overflow window accepted");
	strategy.clear_parameters();
	(*strategy.mutable_parameters())["long_window"] = "100001";
	Check(!CStrategyService::ValidateStrategy(strategy, error),
		"Unsupported indicator window accepted");
	(*strategy.mutable_parameters())["long_window"] = "100000";
	Check(CStrategyService::ValidateStrategy(strategy, error),
		"Maximum indicator window rejected");
	strategy.clear_parameters();
	(*strategy.mutable_parameters())["typo"] = "5";
	Check(!CStrategyService::ValidateStrategy(strategy, error), "Unknown parameter accepted");
	strategy.clear_parameters();
	strategy.mutable_subscriptions(0)->set_exchange("sse");
	Check(!CStrategyService::ValidateStrategy(strategy, error), "Invalid exchange accepted");
	strategy = Strategy();
	*strategy.add_subscriptions() = strategy.subscriptions(0);
	Check(!CStrategyService::ValidateStrategy(strategy, error), "Duplicate subscription accepted");
	strategy = Strategy();

	request::TradeAccountRecord primary;
	primary.set_broker_id(1);
	primary.set_account("000123");
	primary.set_site("primary");
	request::TradeAccountRecord secondary = primary;
	secondary.set_site("secondary");
	Check(AccountIndex(primary) == AccountIndex(secondary), "Access node changed account identity");
	secondary.set_broker_id(2);
	Check(AccountIndex(primary) != AccountIndex(secondary), "Broker account domains merged");
	_TyAccountIndex identity;
	Check(DecodeAccount(EncodeAccount(AccountIndex(primary)), identity) && identity == AccountIndex(primary), "Binary account identity did not round trip");
	request::StrategyInfo binding;
	SetAccountIdentity(identity, binding);
	Check(binding.site().empty() && IsAccountValid(AccountIndex(binding)), "Strategy binding requires access node");
	request::TradeCounterReply fundsReply;
	SetAccountIdentity(identity, fundsReply);
	SetAccountIdentity(identity, *fundsReply.mutable_funds());
	Check(AccountIndex(primary) == AccountIndex(fundsReply) && AccountIndex(primary) == AccountIndex(fundsReply.funds()), "Funds identity depends on access node");
	CRequest identityRequest;
	WriteAccountIdentity(identity, identityRequest);
	_TyAccountIndex requestIdentity;
	Check(identityRequest.GetExtraData("site").empty() && ReadAccountIdentity(identityRequest, requestIdentity) && identity == requestIdentity, "Request identity requires access node");

	CLoginService loginService;
	CStrategyService service;
	CHQService marketService;
	CAccountService accountService;
	CTraderServices services{ loginService, service, marketService, accountService };
	pStrategyService = &service;
	service.SetTraderCenter(&center);
	service.Initialize();
	service.QueryStrategies();
	service.QueryRuntime();
	CRequest oldConfig = Last(_TyRequest::STRATEGY_QUERY);
	CRequest oldRuntime = Last(_TyRequest::STRATEGY_RUNTIME_QUERY);
	std::size_t count = requests.size();
	service.QueryStrategies();
	service.QueryRuntime();
	Check(count == requests.size(), "Polling did not deduplicate requests");
	CRequest mutation;
	mutation.SetType(_TyRequest::STRATEGY_ADD);
	mutation.SetId(999);
	Reply(mutation);
	ConfigReply(oldConfig, strategy);
	RuntimeReply(oldRuntime, 3);
	Check(!service.IsCacheValid() && !service.IsRuntimeValid(), "Old response made invalidated cache valid");
	Check(oldConfig.GetId() != Last(_TyRequest::STRATEGY_QUERY).GetId(), "Fresh config query lost");
	Check(oldRuntime.GetId() != Last(_TyRequest::STRATEGY_RUNTIME_QUERY).GetId(), "Fresh runtime query lost");
	ConfigReply(Last(_TyRequest::STRATEGY_QUERY), strategy);
	RuntimeReply(Last(_TyRequest::STRATEGY_RUNTIME_QUERY), 7);
	Check(service.IsCacheValid() && service.IsRuntimeValid(), "Fresh caches not committed");
	Check(service.CanModifyStrategy(42), "Stopped strategy cannot be edited");
	for (int state = 0; 8 >= state; ++state)
	{
		service.QueryRuntime();
		RuntimeReply(Last(_TyRequest::STRATEGY_RUNTIME_QUERY), state);
		Check(((1 == state) || (7 == state)) == service.CanModifyStrategy(42), "Unsafe runtime edit policy");
	}
	service.QueryRuntime();
	RuntimeReply(Last(_TyRequest::STRATEGY_RUNTIME_QUERY), 7, 1);
	Check(!service.CanModifyStrategy(42), "Active orders allowed modification");
	Check(0 == service.ModifyStrategy(strategy), "Service bypassed runtime guard");
	service.QueryStrategies();
	CRequest disconnectedQuery = Last(_TyRequest::STRATEGY_QUERY);
	stateHandlers.front()(SessionState::Disconnected, "disconnect");
	ConfigReply(disconnectedQuery, strategy);
	Check(!service.IsCacheValid(), "Disconnected response revived cache");
	sendSuccess = false;
	Check(!service.QueryStrategies(), "Transport failure not reported");
	sendSuccess = true;
	service.QueryStrategies();
	ConfigReply(Last(_TyRequest::STRATEGY_QUERY), strategy);
	Check(service.IsCacheValid(), "Failed send blocked retry");
	service.QueryRuntime();
	CRequest timeout = Last(_TyRequest::STRATEGY_RUNTIME_QUERY);
	timeout.SetErrorInfo(1, "timeout");
	Reply(timeout);
	Check(!service.IsRuntimeValid(), "Timeout kept runtime valid");
	service.QueryRuntime();
	RuntimeReply(Last(_TyRequest::STRATEGY_RUNTIME_QUERY), 7);
	Check(service.CanModifyStrategy(42), "Timeout blocked runtime retry");

	request::TradeAccountRecord account;
	account.set_broker_id(1);
	account.set_broker_name("Broker A");
	account.set_site("site");
	account.set_account("000123");
	account.set_name("same name");
	account.set_state(request::ACCOUNT_CONNECTED);
	accounts.emplace_back(account);
	account.set_broker_id(2);
	account.set_broker_name("Broker B");
	accounts.emplace_back(account);
	CStrategyEditDialog dialog(services);
	dialog.SetStrategy(strategy);
	Check("000123" == dialog.GetStrategy().account(), "Account leading zeros lost");
	Check(1 == dialog.GetStrategy().broker_id(), "Wrong broker selected");
	Check(dialog.GetStrategy().site().empty(), "Strategy persisted access node as identity");
	bool detailedLabel = false;
	for (QComboBox* combo : dialog.findChildren<QComboBox*>())
	{
		if (combo->currentText().contains("Broker A"))
		{
			detailedLabel = combo->currentText().contains("site") && combo->currentText().contains("000123");
		}
	}
	Check(detailedLabel, "Account identity not displayed");
	accounts.front().set_site("secondary");
	accountEvents.Notify({});
	QApplication::processEvents();
	Check(1 == dialog.GetStrategy().broker_id() && "000123" == dialog.GetStrategy().account(), "Access node change lost strategy account selection");

	accounts.erase(accounts.begin());
	accountEvents.Notify({});
	QApplication::processEvents();
	Check(1 == dialog.GetStrategy().broker_id(), "Missing account silently rebound");
	CloseWarning();
	QMetaObject::invokeMethod(&dialog, "OnAccept", Qt::DirectConnection);
	Check(QDialog::Accepted != dialog.result(), "Missing account accepted");
	accounts.insert(accounts.begin(), account);
	accounts.front().set_broker_id(1);
	QTableWidget* parameters = dialog.findChild<QTableWidget*>("parameterTable");
	parameters->setRowCount(2);
	for (int row = 0; 2 > row; ++row)
	{
		parameters->setItem(row, 0, new QTableWidgetItem("short_window"));
		parameters->setItem(row, 1, new QTableWidgetItem("5"));
	}
	CloseWarning();
	QMetaObject::invokeMethod(&dialog, "OnAccept", Qt::DirectConnection);
	Check(QDialog::Accepted != dialog.result(), "Duplicate parameter closed dialog");
	parameters->item(1, 0)->setText("long_window");
	parameters->item(1, 1)->setText("2");
	CloseWarning();
	QMetaObject::invokeMethod(&dialog, "OnAccept", Qt::DirectConnection);
	Check(QDialog::Accepted != dialog.result(), "Invalid windows closed dialog");
	parameters->item(1, 1)->setText("20");
	QMetaObject::invokeMethod(&dialog, "OnAccept", Qt::DirectConnection);
	Check(QDialog::Accepted == dialog.result(), "Valid dialog did not accept");
	CViewStrategySettings view;
	view.Initialize(services);
	view.show();
	view.findChild<QTabWidget*>("strategyTabs")->setCurrentIndex(1);
	ConfigReply(Last(_TyRequest::STRATEGY_QUERY), strategy);
	RuntimeReply(Last(_TyRequest::STRATEGY_RUNTIME_QUERY), 7);
	QApplication::processEvents();
	QPushButton* deleteButton = view.findChild<QPushButton*>("deleteButton");
	Check(deleteButton->isEnabled(), "Delete button unavailable after fresh snapshots");
	count = requests.size();
	QTimer::singleShot(0, [&service, &strategy]()
	{
		// A query refresh replaces the vector during the confirmation's nested event loop.
		request::StrategyInfo replacement = strategy;
		replacement.set_strategy_id(84);
		service.QueryStrategies();
		ConfigReply(Last(_TyRequest::STRATEGY_QUERY), replacement);
		QApplication::processEvents();
		for (QWidget* widget : QApplication::topLevelWidgets())
		{
			QMessageBox* question = qobject_cast<QMessageBox*>(widget);
			if (nullptr != question)
			{
				question->done(QMessageBox::Yes);
			}
		}
		CloseWarning();
	});
	deleteButton->click();
	for (std::size_t index = count; requests.size() > index; ++index)
	{
		Check(_TyRequest::STRATEGY_DELETE != requests[index].GetType(), "Deleted a replaced strategy after modal refresh");
	}
	// Queued notifications must render the latest cache, even when invalidated before delivery.
	service.QueryStrategies();
	ConfigReply(Last(_TyRequest::STRATEGY_QUERY), strategy);
	service.Invalidate();
	QApplication::processEvents();
	Check(!view.findChild<QPushButton*>("modifyButton")->isEnabled(), "Queued config notification revived stale edit button");
	std::cout << "Strategy management regression tests passed\n";
	return 0;
}
