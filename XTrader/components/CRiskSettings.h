#ifndef XTRADER_COMPONENTS_CRISKSETTINGS_H
#define XTRADER_COMPONENTS_CRISKSETTINGS_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui
{
	class CRiskSettingsClass;
}
QT_END_NAMESPACE

class CRiskSettings final : public QWidget
{
public:
	explicit CRiskSettings(QWidget* pParent = nullptr);
	~CRiskSettings() override;

private:
	Ui::CRiskSettingsClass* ui{ nullptr };
};

#endif
