#pragma once
#include "../system/CTraderServices.h"
#include "../service/account/CAccountService.h"
#include <QWidget>
#include <array>
#include <functional>
#include <set>
class QCheckBox;
class QLabel;
class QPushButton;
class QLineEdit;
class QComboBox;
class QTableView;
class QTableWidget;
class QTabWidget;
class QStandardItemModel;
class CTradeAccountModel;
class CViewTradeAccounts final : public QWidget
{
	friend class CTradeAccountUiTests;

public:
	explicit CViewTradeAccounts(CTraderServices& services, QWidget* parent = nullptr);
	~CViewTradeAccounts() override;
	void SetStrategyNavigation(std::function<void(std::uint64_t)>&& handler);

protected:
	void showEvent(QShowEvent* event) override;

private:
	void Refresh();
	void RefreshRows();
	void RefreshSelection();
	void ClearDetails();
	void RefreshButtons();
	void RefreshChecks();
	void RemoveCheckedAccounts();
	void EditAccount(bool bNew);
	void Operate(_TyRequest type);
	void HandleResult(const CAccountService::CResult& result);
	void RefreshStrategies();
	const request::TradeAccountRecord* SelectedAccount() const;
	QPushButton* m_add{ nullptr };
	QPushButton* m_remove{ nullptr };
	QLabel* m_heading{ nullptr };
	QLabel* m_notice{ nullptr };
	QLabel* m_checkedCount{ nullptr };
	QTableWidget* m_strategies{ nullptr };
	QTableWidget* m_logs{ nullptr };
	QCheckBox* m_checkAll{ nullptr };
	std::set<_TyAccountIndex> m_checkedAccounts;
	std::set<std::uint64_t> m_removeIds;
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
