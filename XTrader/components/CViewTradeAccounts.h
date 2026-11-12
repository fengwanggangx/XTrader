#pragma once
#include "../system/CTraderServices.h"
#include "../service/account/CAccountService.h"
#include <QWidget>
#include <array>
#include <functional>
#include <set>
#include <map>
#include "CTableData.h"

class QLabel;
class QPushButton;

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
	void Operate(_TyRequest type, const _TyAccountIndex& account);
	void HandleTableOperation(const CTableOperation& operation);
	void HandleResult(const CAccountService::CResult& result);
	void RefreshStrategies();
	const request::TradeAccountRecord* SelectedAccount() const;
	QPushButton* m_add{ nullptr };
	QPushButton* m_remove{ nullptr };
	QLabel* m_heading{ nullptr };
	QLabel* m_notice{ nullptr };
	QLabel* m_checkedCount{ nullptr };
	CUITable* m_strategies{ nullptr };
	CUITable* m_logs{ nullptr };
	CDataTable m_accountTable;
	CDataTable m_strategyTable;
	std::map<_TyAccountIndex, _TyDataRowId> m_rowIds;
	_TyDataRowId m_nextRowId{ 1 };
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
