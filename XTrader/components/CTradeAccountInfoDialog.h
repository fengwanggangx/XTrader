#pragma once

#include <QDialog>

namespace request
{
	class TradeAccountRecord;
}

class CTradeAccountInfoDialog final : public QDialog
{
public:
	explicit CTradeAccountInfoDialog(const request::TradeAccountRecord& account, QWidget* pParent = nullptr);

protected:
	bool eventFilter(QObject* pObject, QEvent* pEvent) override;
};
