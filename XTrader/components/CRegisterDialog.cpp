#include "CRegisterDialog.h"
#include "CUIStyle.h"

#include "../system/CHostMgr.h"

#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>

#include <optional>
#include <utility>

CRegisterDialog::CRegisterDialog(CTraderServices& services, QWidget* parent) : QDialog(parent)
{
	m_pServices = &services;
	m_ui.setupUi(this);
	UIStyle::Apply(*this, ":/styles/register.qss");
	connect(m_ui.registerButton, &QPushButton::clicked, this, &CRegisterDialog::Register);
	connect(m_ui.cancelButton, &QPushButton::clicked, this, &QDialog::reject);
	m_callbackId = m_pServices->m_login.Subscribe([this](const AuthEvent& event)
	{
		if (AuthOperation::Register != event.m_operation)
		{
			return;
		}
		QMetaObject::invokeMethod(this, [this, event]()
		{
			if (AuthState::Success == event.m_state)
			{
				m_registrationPending = false;
				QMessageBox::information(this, "注册成功", QString::fromStdString(event.m_message));
				accept();
			}
			else if (AuthState::Failed == event.m_state)
			{
				m_registrationPending = false;
				m_ui.registerButton->setEnabled(true);
				QMessageBox::warning(this, "注册失败", QString::fromStdString(event.m_message));
			}
		}, Qt::QueuedConnection);
	});
}

CRegisterDialog::~CRegisterDialog()
{
	m_pServices->m_login.Unsubscribe(m_callbackId);
	if (m_registrationPending)
	{
		m_pServices->m_login.Cancel();
	}
}

void CRegisterDialog::SetUser(const QString& user)
{
	m_ui.userEdit->setText(user);
}

QString CRegisterDialog::User() const
{
	return m_ui.userEdit->text().trimmed();
}

void CRegisterDialog::Register()
{
	QString user = User();
	QString password = m_ui.passwordEdit->text();
	if (user.isEmpty() || password.isEmpty())
	{
		QMessageBox::warning(this, "注册失败", "账号和密码不能为空。");
		return;
	}
	if (password != m_ui.confirmPasswordEdit->text())
	{
		QMessageBox::warning(this, "注册失败", "两次输入的密码不一致。");
		return;
	}

	std::optional<CHostInfo> site = CHostMgr::InstanceRef().GetActiveHost();
	if (!site.has_value())
	{
		QMessageBox::warning(this, "注册失败", "没有可用的服务器配置。");
		return;
	}

	m_ui.registerButton->setEnabled(false);
	CAuthParam param;
	param.m_operation = AuthOperation::Register;
	param.m_strUser = user.toStdString();
	param.m_strPassword = password.toStdString();
	param.m_host = std::move(*site);
	m_registrationPending = m_pServices->m_login.Authenticate(param);
}
