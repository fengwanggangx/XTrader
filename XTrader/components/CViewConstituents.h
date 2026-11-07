#ifndef XTRADER_COMPONENTS_CVIEWCONSTITUENTS_H
#define XTRADER_COMPONENTS_CVIEWCONSTITUENTS_H

#include "../system/CTraderServices.h"

#include <QWidget>
#include <memory>
#include <vector>

struct CSectorInfo;
struct CSecurity;

namespace Ui
{
	class CViewConstituentsClass;
}
class CMarketPageController;

class CViewConstituents final : public QWidget
{
public:
	void Initialize(CTraderServices& services);
	explicit CViewConstituents(QWidget* pParent = nullptr);
	~CViewConstituents() override;
	void SetSector(const CSectorInfo& sector, const std::vector<CSecurity>& securities);

private:
	std::unique_ptr<Ui::CViewConstituentsClass> m_ui;
	QString m_strSector;
	std::unique_ptr<CMarketPageController> m_controller;

private:
	CTraderServices* m_pServices{ nullptr };
};

#endif
