#pragma once

#include "../system/CLoginService.h"
#include "ui_CRegisterDialog.h"

#include <QDialog>

class CRegisterDialog final : public QDialog
{
	Q_OBJECT

public:
	explicit CRegisterDialog(QWidget* parent = nullptr);
	~CRegisterDialog() override;

	void SetUser(const QString& user);
	QString User() const;

private slots:
	void Register();

private:
	Ui::CRegisterDialogClass m_ui;
	_TyCallbackId m_callbackId{ 0 };
	bool m_registrationPending{ false };
};
