#ifndef XTRADER_COMPONENTS_CSYSTEMSETTINGS_H
#define XTRADER_COMPONENTS_CSYSTEMSETTINGS_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui
{
	class CSystemSettingsClass;
}
QT_END_NAMESPACE

class CSystemSettings final : public QWidget
{
public:
	explicit CSystemSettings(QWidget* pParent = nullptr);
	~CSystemSettings() override;

private:
	Ui::CSystemSettingsClass* ui{ nullptr };
};

#endif
