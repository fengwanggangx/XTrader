#ifndef XTRADER_COMPONENTS_CSERVERSITEDIALOG_H
#define XTRADER_COMPONENTS_CSERVERSITEDIALOG_H

#include "../system/CHostMgr.h"

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui
{
	class CServerSiteDialogClass;
}
QT_END_NAMESPACE

class CServerSiteDialog final : public QDialog
{
public:
	explicit CServerSiteDialog(const CHostInfo& site, bool readOnly, QWidget* pParent = nullptr);
	~CServerSiteDialog() override;

	const CHostInfo& GetSite() const;

private:
	void AcceptSite();

	Ui::CServerSiteDialogClass* ui{ nullptr };
	CHostInfo m_site;
};

#endif
