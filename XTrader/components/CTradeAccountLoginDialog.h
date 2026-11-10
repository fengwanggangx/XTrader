#pragma once
#include "../system/CTraderServices.h"
#include "../service/account/CAccountService.h"
#include <QDialog>
#include <memory>
namespace Ui
{
	class CTradeAccountLoginDialog;
}
class CTradeAccountLoginDialog final : public QDialog
{
public:
	explicit CTradeAccountLoginDialog(CTraderServices& services, QWidget* pParent = nullptr);
	~CTradeAccountLoginDialog() override;

protected:
	bool eventFilter(QObject* pObject, QEvent* pEvent) override;

private:
	std::vector<request::BrokerRecord> m_brokers;
	std::uint64_t m_directoryId{ 0 };
	void Login();
	void HandleResult(const CAccountService::CResult& result);
	void SetBusy(bool bBusy);
	void ShowError(const QString& strMessage);
	std::unique_ptr<Ui::CTradeAccountLoginDialog> m_ui;
	_TyCallbackId m_nToken{ 0 };
	std::uint64_t m_nRequestId{ 0 };

private:
	CTraderServices* m_pServices{ nullptr };
};
