#include "CServerSettingsDialog.h"
#include "CUIStyle.h"
#include "CServerSiteDialog.h"
#include "ui_CServerSettingsDialog.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QPushButton>
#include <QTcpSocket>
#include <limits>
#include <exception>

namespace
{
	QString MakeSiteText(const CHostInfo& site)
	{
		QString siteText = QStringLiteral("%1 (%2:%3)").arg(QString::fromStdString(site.m_strName), QString::fromStdString(site.m_strHost)).arg(site.m_nPort);
		return siteText;
	}
} // namespace

CServerSettingsDialog::CServerSettingsDialog(QWidget* pParent) : QDialog(pParent), ui(new Ui::CServerSettingsDialogClass())
{
	ui->setupUi(this);
	UIStyle::Apply(*this, ":/styles/server-settings.qss");
	connect(ui->siteCombo, &QComboBox::currentIndexChanged, this, &CServerSettingsDialog::UpdateButtons);
	connect(ui->viewButton, &QPushButton::clicked, this, &CServerSettingsDialog::ViewSite);
	connect(ui->autoFastestCheck, &QCheckBox::toggled, this, &CServerSettingsDialog::OnAutoFastestToggled);
	connect(ui->addButton, &QPushButton::clicked, this, &CServerSettingsDialog::AddSite);
	connect(ui->removeButton, &QPushButton::clicked, this, &CServerSettingsDialog::RemoveSite);
	connect(ui->okButton, &QPushButton::clicked, this, &CServerSettingsDialog::AcceptSelection);
	connect(ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
	LoadSites();
}

CServerSettingsDialog::~CServerSettingsDialog()
{
	delete ui;
}

void CServerSettingsDialog::LoadSites()
{
	ui->siteCombo->clear();
	const auto& hosts = CHostMgr::InstanceRef().GetHosts();
	for (const auto& v : hosts)
	{
		const auto& site = v.second;
		ui->siteCombo->addItem(MakeSiteText(site), QString::fromStdString(site.m_strKey));
	}
	int activeIndex = 0;
	ui->siteCombo->setCurrentIndex(0 <= activeIndex ? activeIndex : 0);
	UpdateButtons();
}

void CServerSettingsDialog::UpdateButtons()
{
	bool hasSite = 0 <= ui->siteCombo->currentIndex();
	ui->viewButton->setEnabled(hasSite);
	ui->removeButton->setEnabled(hasSite);
	ui->autoFastestCheck->setEnabled(0 < ui->siteCombo->count());
	ui->okButton->setEnabled(hasSite);
}

void CServerSettingsDialog::ViewSite()
{
	const QString key = ui->siteCombo->currentData().toString();
	for (const auto& v : CHostMgr::InstanceRef().GetHosts())
	{
		const auto& site = v.second;
		if (key.toStdString() == site.m_strKey)
		{
			CServerSiteDialog dialog(site, true, this);
			dialog.exec();
			return;
		}
	}
}

void CServerSettingsDialog::OnAutoFastestToggled(bool checked)
{
	CHostMgr::InstanceRef().SetConnectFast(checked);
}

void CServerSettingsDialog::AddSite()
{
	CHostInfo site;
	site.m_strKey = CHostMgr::InstanceRef().Key();
	site.m_bEnabled = true;
	CServerSiteDialog dialog(site, false, this);
	if (QDialog::Accepted != dialog.exec())
	{
		return;
	}
	const CHostInfo& newSite = dialog.GetSite();
	try
	{
		CHostMgr::InstanceRef().Add(newSite);
	}
	catch (const std::exception&)
	{
		QMessageBox::warning(this, "新增失败", "无法写入 ini/system.ini。");
		return;
	}
	LoadSites();
	ui->siteCombo->setCurrentIndex(ui->siteCombo->findData(QString::fromStdString(newSite.m_strKey)));
}

void CServerSettingsDialog::RemoveSite()
{
	QString siteId = ui->siteCombo->currentData().toString();
	if (siteId.isEmpty())
	{
		return;
	}
	QMessageBox messageBox(QMessageBox::Question, "删除站点", "确定删除当前站点？", QMessageBox::Yes | QMessageBox::No, this);
	messageBox.button(QMessageBox::Yes)->setText("是");
	messageBox.button(QMessageBox::No)->setText("否");
	if (QMessageBox::Yes != messageBox.exec())
	{
		return;
	}
	try
	{
		CHostMgr::InstanceRef().Remove(siteId.toStdString());
	}
	catch (const std::exception&)
	{
		QMessageBox::warning(this, "删除失败", "无法删除当前站点。");
		return;
	}
	LoadSites();
}

void CServerSettingsDialog::AcceptSelection()
{
	if (ui->siteCombo->currentData().toString().isEmpty())
	{
		QMessageBox::warning(this, "保存失败", "请选择一个有效站点。");
		return;
	}
	accept();
}
