#ifndef XTRADER_COMPONENTS_CSERVERSETTINGSDIALOG_H
#define XTRADER_COMPONENTS_CSERVERSETTINGSDIALOG_H

#include "../system/CHostMgr.h"

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui
{
	class CServerSettingsDialogClass;
}
QT_END_NAMESPACE

class CServerSettingsDialog final : public QDialog
{
public:
	explicit CServerSettingsDialog(QWidget* pParent = nullptr);
	~CServerSettingsDialog() override;

private:
	void LoadSites();
	void UpdateButtons();
	void ViewSite();
	void OnAutoFastestToggled(bool checked);
	void AddSite();
	void RemoveSite();
	void AcceptSelection();

	Ui::CServerSettingsDialogClass* ui{ nullptr };
};

#endif
