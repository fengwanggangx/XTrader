#ifndef XTRADER_COMPONENTS_CSTRATEGYEDITDIALOG_H
#define XTRADER_COMPONENTS_CSTRATEGYEDITDIALOG_H

#include "../system/CTraderServices.h"

#include "../request/request.pb.h"

#include <QDialog>
#include <memory>
#include "CTableData.h"
#include <map>
#include <tuple>
#include "../request/request.h"
#include "../basic/TMessagePump.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
	class CStrategyEditDialogClass;
}
QT_END_NAMESPACE

class QComboBox;

class CStrategyEditDialog final : public QDialog
{
	Q_OBJECT

public:
	explicit CStrategyEditDialog(CTraderServices& services, QWidget* pParent = nullptr);
	~CStrategyEditDialog() override;
	void SetStrategy(const request::StrategyInfo& strategy);
	void SetAccountLocked(bool locked);
	request::StrategyInfo GetStrategy() const;

private slots:
	void OnAddParameter();
	void OnRemoveParameter();
	void OnAddSubscription();
	void OnRemoveSubscription();
	void OnAccept();

private:
	void InitializeUI();
	void RefreshAccounts();
	void ApplyEdit(const CTableOperation& operation, CDataTable& table, std::vector<CTableRow>& rows, CUITable& widget);

private:
	std::unique_ptr<Ui::CStrategyEditDialogClass> m_ui;
	CDataTable m_parameterData;
	CDataTable m_subscriptionData;
	std::vector<CTableRow> m_parameters;
	std::vector<CTableRow> m_subscriptions;
	_TyDataRowId m_nextDraftId{ 1 };
	std::map<std::tuple<std::uint64_t, _TyDataRowId, _TyTableColumnId>, std::uint64_t> m_editRequests;
	_TyCallbackId m_accountToken{ 0 };
	_TyAccountIndex m_originalAccount{};
	std::uint64_t m_strategyId{ 0 };
	QComboBox* m_accounts{ nullptr };

private:
	CTraderServices* m_pServices{ nullptr };
};

#endif
