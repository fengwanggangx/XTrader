#pragma once
#include "../system/CTraderServices.h"
#include "../service/account/CTradeAccountService.h"
#include <QWidget>
#include <array>
#include <functional>
class QLabel;
class QPushButton;
class QLineEdit;
class QComboBox;
class QTableView;
class QTableWidget;
class QTabWidget;
class QStandardItemModel;
class CTradingAccountModel;
class CViewTradingAccounts final : public QWidget
{
	friend class CTradingAccountUiTests;

public:
	explicit CViewTradingAccounts(CTraderServices& services, QWidget* parent = nullptr);
	~CViewTradingAccounts() override;
	void SetStrategyNavigation(std::function<void(std::uint64_t)>&& handler);

protected:
	void showEvent(QShowEvent* event) override;

private:
	void Refresh();
	void RefreshRows();
	void RefreshSelection();
	void ClearDetails();
	void RefreshButtons();
	void EditAccount(bool bNew);
	void Operate(_TyRequest type);
	void HandleResult(const CTradeAccountService::CResult& result);
	void RefreshStrategies();
	const request::TradeAccountRecord* SelectedAccount() const;
	QPushButton* m_add{ nullptr };
	QPushButton* m_disconnect{ nullptr };
	QLabel* m_heading{ nullptr };
	QLabel* m_notice{ nullptr };
	QTableWidget* m_strategies{ nullptr };
	QTableWidget* m_logs{ nullptr };
	std::array<QLabel*, 4> m_funds{};
	std::vector<request::TradeAccountRecord> m_accounts;
	_TyAccountIndex m_selectedIndex{};
	std::uint64_t m_operationId{ 0 };
	std::uint64_t m_snapshotId{ 0 };
	_TyCallbackId m_accountToken{ 0 };
	std::function<void(std::uint64_t)> m_navigate;

private:
	CTraderServices* m_pServices{ nullptr };
};
