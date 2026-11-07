#ifndef XTRADER_COMPONENTS_CVIEWASHARE_H
#define XTRADER_COMPONENTS_CVIEWASHARE_H

#include "../system/CTraderServices.h"

#include <QWidget>
#include <memory>

namespace Ui
{
	class CViewAShareClass;
}
class CMarketPageController;

class CViewAShare final : public QWidget
{
public:
	void Initialize(CTraderServices& services);
	explicit CViewAShare(QWidget* pParent = nullptr);
	~CViewAShare() override;

private:
	void showEvent(QShowEvent* pEvent) override;

	std::unique_ptr<Ui::CViewAShareClass> m_ui;
	std::unique_ptr<CMarketPageController> m_controller;
	bool m_bLayoutInitialized{ false };

private:
	CTraderServices* m_pServices{ nullptr };
};

#endif
