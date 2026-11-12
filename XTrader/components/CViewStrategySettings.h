#ifndef XTRADER_COMPONENTS_CVIEWSTRATEGYSETTINGS_H
#define XTRADER_COMPONENTS_CVIEWSTRATEGYSETTINGS_H

#include "../system/CTraderServices.h"
#include "../request/request.h"

#include "../service/strategy/CStrategyService.h"
#include <QString>
#include "CTableData.h"
#include <QWidget>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace Ui
{
	class CViewStrategySettingsClass;
}

class CStrategyFilterProxyModel;
class QLabel;
class QLineEdit;
class QComboBox;

class CViewStrategySettings final : public QWidget
{
	Q_OBJECT
public:
	void Initialize(CTraderServices& services);
	explicit CViewStrategySettings(QWidget* pParent = nullptr);
	~CViewStrategySettings() override;

private:
	std::unique_ptr<Ui::CViewStrategySettingsClass> m_ui;
	void RefreshFilter();
	int SelectedStrategyRow() const;
	void RefreshDetails();
	void RefreshSnapshot();
	void RefreshRows();
	void HandleOperation(const CStrategyService::COperationResult& result);
	void EditStrategy(bool bNew);
	void DeleteStrategy();
	void ControlStrategy(_TyRequest type);
	void RefreshConnection();
	CDataTable m_instanceTable;
	CDataTable m_parameterTable;
	std::map<std::pair<std::uint64_t, std::string>, _TyDataRowId> m_parameterIds;
	_TyDataRowId m_nextParameterId{ 1 };
	CDataTable m_logTable;
	CStrategyService::_TyStrategyList m_strategies;
	CStrategyService::_TyRuntimeMap m_runtimes;
	std::unordered_map<std::uint64_t, QString> m_pendingOperations;
	_TyCallbackId m_nAccountToken{ 0 };
	_TyCallbackId m_nQueryToken{ 0 };
	_TyCallbackId m_nRuntimeToken{ 0 };
	_TyCallbackId m_nOperationToken{ 0 };
	bool m_bConfigStale{ true };
	bool m_bRuntimeStale{ true };
	bool m_bWasConnected{ false };
	int m_refreshTicks{ 0 };
	std::uint64_t m_selectedStrategyId{ 0 };
	std::uint64_t m_snapshotGeneration{ 0 };
	CUITable* m_pInstances{ nullptr };
	CStrategyFilterProxyModel* m_instanceFilter{ nullptr };
	QLabel* m_pDetails{ nullptr };
	QLineEdit* m_pSearch{ nullptr };
	QComboBox* m_pStatus{ nullptr };

private:
	CTraderServices* m_pServices{ nullptr };
};
#endif
