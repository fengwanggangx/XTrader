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
#include "service/watchlist/CWatchlistService.h"
#include "components/CStrategyEditDialog.h"
#include "components/CViewStrategySettings.h"
#include "components/CViewTradeAccounts.h"
#include <QPushButton>
#include <QTabWidget>
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QMessageBox>
#include "components/CUITable.h"
#include "components/CKeyboardSprite.h"
#include <QKeyEvent>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
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

std::uint64_t CAccountService::Send(_TyRequest type, const _TyAccountIndex& account, const std::unordered_map<std::string, std::string>& extra)
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

class CTradeAccountUiTests
{
public:
	static void SavePreview(CViewTradeAccounts& page)
	{
		QString directory = qEnvironmentVariable("XTRADER_UI_ARTIFACTS");
		if (directory.isEmpty())
		{
			return;
		}
		QDir().mkpath(directory);
		QPalette original = QApplication::palette();
		page.resize(1400, 850);
		page.setAutoFillBackground(true);
		page.show();
		for (const QString& theme : { QString("light"), QString("dark") })
		{
			QFile style(QString(XTRADER_COMPONENTS_PATH) + "/theme-" + theme + ".qss");
			Check(style.open(QIODevice::ReadOnly), "Cannot load preview theme");
			QPalette palette = page.palette();
			palette.setColor(QPalette::Window, QColor("dark" == theme ? "#122130" : "#f4f7fb"));
			palette.setColor(QPalette::WindowText, QColor("dark" == theme ? "#dce8f6" : "#162033"));
			bool dark = "dark" == theme;
			palette.setColor(QPalette::Button, QColor(dark ? "#122130" : "#ffffff"));
			palette.setColor(QPalette::ButtonText, QColor(dark ? "#a9bdd0" : "#536176"));
			palette.setColor(QPalette::Text, QColor(dark ? "#c8d8e7" : "#344258"));
			palette.setColor(QPalette::Link, QColor(dark ? "#1683ff" : "#1677ff"));
			palette.setColor(QPalette::Mid, QColor(dark ? "#203b52" : "#e4eaf2"));
			QApplication::setPalette(palette);
			page.setPalette(palette);
			page.setAttribute(Qt::WA_StyledBackground, true);
			page.setStyleSheet(QString::fromUtf8(style.readAll()) + "\n#tradeAccountsPage { background: palette(window); }");
			QApplication::processEvents();
			Check(page.grab().save(directory + "/accounts-" + theme + ".png"), "Cannot save account preview");
		}
		page.hide();
		QApplication::setPalette(original);
	}

	static void Run(CTraderServices& services)
	{
		CViewTradeAccounts page(services);
		request::TradeAccountRecord first;
		first.set_broker_id(1);
		first.set_account("000123");
		first.set_broker_name("Broker A");
		first.set_site("primary");
		first.set_state(request::ACCOUNT_DISCONNECTED);
		request::TradeAccountRecord second = first;
		second.set_broker_id(2);
		second.set_broker_name("Broker B");
		page.m_accounts = { first, second };
		page.RefreshRows();
		_TyDataRowId firstId = page.m_rowIds.at(AccountIndex(first));
		_TyDataRowId secondId = page.m_rowIds.at(AccountIndex(second));
		Check(firstId != secondId, "Same account number at different brokers merged");
		page.m_logs->setSortingEnabled(true);
		page.m_logs->sortByColumn(2, Qt::DescendingOrder);
		page.m_logs->SelectRowId(secondId);
		page.m_logs->TriggerAction(page.m_logs->model()->index(page.m_logs->currentIndex().row(), 7), "login");
		_TyAccountIndex sent;
		Check(ReadAccountIdentity(Last(_TyRequest::TRADE_ACCOUNT_LOGIN), sent) && sent == AccountIndex(second), "Sorted account action used wrong broker");
		CAccountService::CResult result;
		result.m_requestId = page.m_operationId;
		result.m_type = _TyRequest::TRADE_ACCOUNT_LOGIN;
		page.HandleResult(result);
		page.m_logs->SetCheckedRowIds({ firstId, secondId });
		Check(page.m_remove->isEnabled(), "Disconnected checked accounts cannot be removed");
		std::size_t before = requests.size();
		page.m_remove->click();
		int removed = 0;
		for (std::size_t index = before; index < requests.size(); ++index)
		{
			if (_TyRequest::TRADE_ACCOUNT_REMOVE == requests[index].GetType())
			{
				++removed;
			}
		}
		Check(2 == removed, "Bulk remove did not target both checked accounts");
		page.m_removeIds.clear();
		page.m_accounts[1].set_state(request::ACCOUNT_CONNECTED);
		page.RefreshRows();
		Check(!page.m_remove->isEnabled(), "Connected account enabled bulk remove");
		SavePreview(page);
		page.m_logs->SelectRowId(secondId);
		before = requests.size();
		QTimer::singleShot(0, [&page]()
		{
			page.m_accounts.erase(page.m_accounts.begin() + 1);
			page.RefreshRows();
			for (QWidget* widget : QApplication::topLevelWidgets())
			{
				QMessageBox* question = qobject_cast<QMessageBox*>(widget);
				if (nullptr != question)
				{
					question->done(QMessageBox::Yes);
				}
			}
		});
		page.m_logs->TriggerAction(page.m_logs->model()->index(page.m_logs->currentIndex().row(), 7), "disconnect");
		for (std::size_t index = before; index < requests.size(); ++index)
		{
			Check(_TyRequest::TRADE_ACCOUNT_DISCONNECT != requests[index].GetType(), "Removed account disconnected after modal refresh");
		}
	}
};

void TestKeyboardSprite()
{
	std::vector<CSecurity> selected;
	CKeyboardSprite sprite;
	sprite.SetSecurities({ CSecurity("600000", "Alpha", Exchange::sse), CSecurity("600000", "Beta", Exchange::szse) });
	QObject::connect(&sprite, &CKeyboardSprite::SecuritySelected, &sprite, [&selected](const CSecurity& security)
	{
		selected.push_back(security);
	});
	sprite.Open("600");
	CUITable* table = sprite.findChild<CUITable*>("keyboardSpriteResults");
	Check(nullptr != table && 2 == table->ResultCount(), "Keyboard search lost matching securities");
	_TyDataRowId betaId = 0;
	for (int row = 0; row < table->ResultCount(); ++row)
	{
		if ("Beta" == table->model()->index(row, 1).data().toString())
		{
			betaId = table->model()->index(row, 1).data(TableRowIdRole).toULongLong();
		}
	}
	Check(0 != betaId, "Keyboard result has no stable identity");
	table->setSortingEnabled(true);
	table->sortByColumn(1, Qt::DescendingOrder);
	table->SelectRowId(betaId);
	QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
	QLineEdit* search = sprite.findChild<QLineEdit*>("keyboardSpriteSearch");
	QApplication::sendEvent(search, &enter);
	Check(1 == selected.size() && Exchange::szse == selected.back().m_hq, "Keyboard confirmation used displayed row number");
	sprite.Open("600");
	CTableOperation activate;
	activate.m_kind = TableOperationKind::Activate;
	activate.m_generation = table->DataModel()->GetView().GetSourceId();
	activate.m_rowId = betaId;
	table->selectRow(1);
	table->OperationRequested(activate);
	Check(2 == selected.size() && Exchange::szse == selected.back().m_hq, "Activation ignored message row identity");
	sprite.Open("no-match");
	table->OperationRequested(activate);
	Check(2 == selected.size(), "Hidden stale search result was activated");
}

int main(int argc, char** argv)
{
	qInstallMessageHandler([](QtMsgType type, const QMessageLogContext&, const QString& message)
	{
		std::cerr << message.toStdString() << '\n';
		if (QtFatalMsg == type)
		{
			std::_Exit(2);
		}
	});
	QApplication app(argc, argv);
	int fontId = QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
	if (0 <= fontId)
	{
		app.setFont(QFont(QFontDatabase::applicationFontFamilies(fontId).front(), 10));
	}
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
	CWatchlistService watchlistService;
	CTraderServices services{ loginService, service, marketService, accountService, watchlistService };
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
	CUITable* parameters = dialog.findChild<CUITable*>("parameterTable");
	QMetaObject::invokeMethod(&dialog, "OnAddParameter", Qt::DirectConnection);
	QMetaObject::invokeMethod(&dialog, "OnAddParameter", Qt::DirectConnection);
	for (int row = 0; row < 2; ++row)
	{
		parameters->model()->setData(parameters->model()->index(row, 0), "short_window");
		parameters->model()->setData(parameters->model()->index(row, 1), "5");
	}
	CloseWarning();
	QMetaObject::invokeMethod(&dialog, "OnAccept", Qt::DirectConnection);
	Check(QDialog::Accepted != dialog.result(), "Duplicate parameter closed dialog");
	parameters->model()->setData(parameters->model()->index(1, 0), "long_window");
	parameters->model()->setData(parameters->model()->index(1, 1), "2");
	CloseWarning();
	QMetaObject::invokeMethod(&dialog, "OnAccept", Qt::DirectConnection);
	Check(QDialog::Accepted != dialog.result(), "Invalid windows closed dialog");
	CTableOperation lastEdit;
	QMetaObject::Connection editConnection = QObject::connect(parameters, &CUITable::OperationRequested, parameters, [&lastEdit](const CTableOperation& operation)
	{
		if (TableOperationKind::Edit == operation.m_kind)
		{
			lastEdit = operation;
		}
	});
	parameters->model()->setData(parameters->model()->index(1, 1), "20");
	lastEdit.m_value = "1";
	parameters->OperationRequested(lastEdit);
	Check(dialog.GetStrategy().parameters().at("long_window") == "20", "Stale edit overwrote accepted draft");
	lastEdit.m_requestId += 100;
	parameters->OperationRequested(lastEdit);
	Check(dialog.GetStrategy().parameters().at("long_window") == "20", "Edit with stale original value overwrote draft");
	lastEdit.m_generation += 1;
	parameters->OperationRequested(lastEdit);
	Check(dialog.GetStrategy().parameters().at("long_window") == "20", "Foreign data source edit overwrote draft");
	QObject::disconnect(editConnection);
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
	QLineEdit* strategySearch = view.findChild<QLineEdit*>("searchEdit");
	CUITable* instances = view.findChild<CUITable*>("instancesTable");
	strategySearch->setText("no matching strategy");
	Check(0 == instances->ResultCount() && !deleteButton->isEnabled(), "Strategy filter left an actionable hidden selection");
	strategySearch->clear();
	instances->SelectRowId(42);
	Check(deleteButton->isEnabled(), "Strategy selection did not recover after filtering");
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
	CTradeAccountUiTests::Run(services);
	TestKeyboardSprite();
	std::cout << "Strategy management regression tests passed\n";
	return 0;
}
