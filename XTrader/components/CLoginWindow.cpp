#include "CLoginWindow.h"
#include "../cryption/Cryption.h"
#include "../system/CHostMgr.h"
#include "CRegisterDialog.h"
#include "CServerSettingsDialog.h"
#include "CUIStyle.h"
#include "ui_CLoginWindow.h"
#include <QComboBox>
#include <QDateTime>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QSettings>
#include <QStyledItemDelegate>
#include <algorithm>
#include <functional>

namespace
{
	class CUserItemDelegate final : public QStyledItemDelegate
	{
	  public:
		using _TyDeleteCallback = std::function<void(int)>;

		CUserItemDelegate(_TyDeleteCallback&& callback, QObject* pParent) : QStyledItemDelegate(pParent), m_callback(std::move(callback))
		{
		}

		void paint(QPainter* pPainter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
		{
			QStyleOptionViewItem itemOption(option);
			itemOption.rect.adjust(0, 0, -24, 0);
			QStyledItemDelegate::paint(pPainter, itemOption, index);
			pPainter->save();
			pPainter->setPen(option.palette.color(QPalette::Text));
			pPainter->drawText(DeleteRect(option.rect), Qt::AlignCenter, QString::fromUtf8("×"));
			pPainter->restore();
		}

		bool editorEvent(QEvent* pEvent, QAbstractItemModel* pModel, const QStyleOptionViewItem& option, const QModelIndex& index) override
		{
			if ((QEvent::MouseButtonRelease == pEvent->type()) && DeleteRect(option.rect).contains(static_cast<QMouseEvent*>(pEvent)->position().toPoint()))
			{
				m_callback(index.row());
				return true;
			}
			return QStyledItemDelegate::editorEvent(pEvent, pModel, option, index);
		}

	  private:
		QRect DeleteRect(const QRect& rect) const
		{
			return QRect(rect.right() - 23, rect.top(), 24, rect.height());
		}

	  private:
		_TyDeleteCallback m_callback;
	};
} // namespace

LoginWindow::LoginWindow(CTraderServices& services, QWidget* pParent) : QDialog(pParent), m_ui(std::make_unique<Ui::LoginWindowClass>())
{
	m_pServices = &services;
	m_ui->setupUi(this);
	UIStyle::Apply(*this, ":/styles/login.qss");
	setWindowFlag(Qt::FramelessWindowHint);
	ConnectSlots();
	m_ui->checkBox_remember->setChecked(QSettings("Mary", "Mary").value("Login/RememberPassword", true).toBool());
	m_ui->comboBox_user->setItemDelegate(new CUserItemDelegate([this](int nIndex)
	{
		DeleteUser(nIndex);
	}, m_ui->comboBox_user));
	LoadUsers();
	QPointer<LoginWindow> safeThis(this);
	m_loginCallbackId = m_pServices->m_login.Subscribe([safeThis](const AuthEvent& event)
	{
		if (safeThis.isNull())
		{
			return;
		}
		QMetaObject::invokeMethod(safeThis.data(), [safeThis, event]()
		{
			if (safeThis.isNull() || (AuthOperation::Login != event.m_operation))
			{
				return;
			}
			if (AuthState::Success == event.m_state)
			{
				safeThis->SavePendingUser();
				safeThis->ClearPendingCredentials();
				safeThis->accept();
			}
			else if (AuthState::Failed == event.m_state)
			{
				safeThis->ClearPendingCredentials();
				safeThis->m_ui->pushButton_login->setEnabled(true);
				QMessageBox::information(safeThis.data(), "提示", QString::fromStdString(event.m_message));
			}
			else if (AuthState::Cancelled == event.m_state)
			{
				safeThis->ClearPendingCredentials();
				safeThis->m_ui->pushButton_login->setEnabled(true);
			}
		}, Qt::QueuedConnection);
	});
}

LoginWindow::~LoginWindow()
{
	ClearPendingCredentials();
	m_pServices->m_login.Unsubscribe(m_loginCallbackId);
}

void LoginWindow::ConnectSlots()
{
	QObject::connect(m_ui->pushButton_login, &QPushButton::clicked, this, &LoginWindow::OnLoginBtnClicked);
	QObject::connect(m_ui->pushButton_register, &QPushButton::clicked, this, &LoginWindow::OnRegisterBtnClicked);
	QObject::connect(m_ui->pushButton_close, &QPushButton::clicked, this, &LoginWindow::OnCloseBtnClicked);
	QObject::connect(m_ui->pushButton_settings, &QPushButton::clicked, this, &LoginWindow::OnSettingsBtnClicked);
	QObject::connect(m_ui->comboBox_user, &QComboBox::currentIndexChanged, this, &LoginWindow::OnUserChanged);
	QObject::connect(m_ui->comboBox_user->lineEdit(), &QLineEdit::textEdited, this, &LoginWindow::OnUserEdited);
	QObject::connect(m_ui->checkBox_remember, &QCheckBox::toggled, this, [](bool bChecked)
					 {
						 QSettings("Mary", "Mary").setValue("Login/RememberPassword", bChecked);
					 });
}

void LoginWindow::mousePressEvent(QMouseEvent* event)
{
	if (Qt::LeftButton == event->button())
	{
		QWidget* child = childAt(event->pos());
		if ((nullptr == child) || (this == child) || ((nullptr == qobject_cast<QPushButton*>(child)) && (nullptr == qobject_cast<QLineEdit*>(child)) && (nullptr == qobject_cast<QComboBox*>(child))))
		{
			m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
			m_isDragging = true;
			event->accept();
		}
	}
}
void LoginWindow::mouseMoveEvent(QMouseEvent* event)
{
	if (m_isDragging && (0 != (event->buttons() & Qt::LeftButton)))
	{
		move(event->globalPosition().toPoint() - m_dragPosition);
		event->accept();
	}
	QDialog::mouseMoveEvent(event);
}

void LoginWindow::mouseReleaseEvent(QMouseEvent* event)
{
	m_isDragging = false;
	QDialog::mouseReleaseEvent(event);
}

bool bTest = false;
void LoginWindow::OnLoginBtnClicked()
{
	if (bTest)
	{
		accept();
		return;
	}
	std::optional<CHostInfo> site = CHostMgr::InstanceRef().GetActiveHost();
	if (!site.has_value())
	{
		QMessageBox::warning(this, "登录失败", "没有可用的服务器配置。");
		return;
	}

	m_ui->pushButton_login->setEnabled(false);

	CAuthParam param;
	param.m_operation = AuthOperation::Login;
	param.m_strUser = m_ui->comboBox_user->currentText().toStdString();
	param.m_strPassword = m_ui->lineEdit_passwd->text().toStdString();
	param.m_host = std::move(*site);
	m_strPendingUser = param.m_strUser;
	m_strPendingPassword = param.m_strPassword;
	m_pServices->m_login.Authenticate(param);
}

void LoginWindow::OnRegisterBtnClicked()
{
	CRegisterDialog dialog(*m_pServices, this);
	dialog.SetUser(m_ui->comboBox_user->currentText());
	if (QDialog::Accepted == dialog.exec())
	{
		m_ui->comboBox_user->setCurrentText(dialog.User());
		m_ui->lineEdit_passwd->clear();
		m_ui->lineEdit_passwd->setFocus();
	}
}

void LoginWindow::OnCloseBtnClicked()
{
	reject();
	// QApplication::quit();
}

void LoginWindow::OnSettingsBtnClicked()
{
	CServerSettingsDialog dialog(this);
	dialog.exec();
}

void LoginWindow::LoadUsers()
{
	m_users = m_userRepository.LoadUsers();
	m_ui->lineEdit_passwd->clear();
	m_ui->comboBox_user->blockSignals(true);
	m_ui->comboBox_user->clear();
	for (const auto& user : m_users)
	{
		m_ui->comboBox_user->addItem(QString::fromStdString(user.m_strUser));
	}
	m_ui->comboBox_user->blockSignals(false);
	if (!m_users.empty())
	{
		m_ui->comboBox_user->setCurrentIndex(0);
		FillPassword(0);
	}
}

void LoginWindow::FillPassword(int nIndex)
{
	m_ui->lineEdit_passwd->clear();
	if ((0 > nIndex) || (static_cast<std::size_t>(nIndex) >= m_users.size()))
	{
		return;
	}
	std::vector<std::uint8_t> password;
	std::string strError;
	if (crypto::CCryptor::InstanceRef().UnprotectCurrentUser(m_users[static_cast<std::size_t>(nIndex)].m_password, password, strError))
	{
		m_ui->lineEdit_passwd->setText(QString::fromUtf8(reinterpret_cast<const char*>(password.data()), static_cast<qsizetype>(password.size())));
	}
}

void LoginWindow::OnUserChanged(int nIndex)
{
	FillPassword(nIndex);
}

void LoginWindow::OnUserEdited(const QString& strUser)
{
	int nIndex = m_ui->comboBox_user->findText(strUser, Qt::MatchExactly);
	if (0 > nIndex)
	{
		m_ui->lineEdit_passwd->clear();
	}
}

void LoginWindow::SavePendingUser()
{
	if (!m_ui->checkBox_remember->isChecked() || m_strPendingUser.empty() || m_strPendingPassword.empty())
	{
		return;
	}
	std::vector<std::uint8_t> data(m_strPendingPassword.begin(), m_strPendingPassword.end());
	std::vector<std::uint8_t> protectedData;
	std::string strError;
	bool bProtected = crypto::CCryptor::InstanceRef().ProtectCurrentUser(data, protectedData, strError);
	std::fill(data.begin(), data.end(), 0);
	if (!bProtected)
	{
		return;
	}
	std::int64_t nTimestamp = QDateTime::currentMSecsSinceEpoch();
	if (m_userRepository.SaveUser(m_strPendingUser, protectedData, nTimestamp))
	{
		LoadUsers();
	}
}

void LoginWindow::DeleteUser(int nIndex)
{
	if ((0 > nIndex) || (static_cast<std::size_t>(nIndex) >= m_users.size()))
	{
		return;
	}
	QString strUser = QString::fromStdString(m_users[static_cast<std::size_t>(nIndex)].m_strUser);
	if (QMessageBox::Yes != QMessageBox::question(this, "删除账号", QString("确定删除已保存账号“%1”吗？").arg(strUser), QMessageBox::Yes | QMessageBox::No, QMessageBox::No))
	{
		return;
	}
	if (m_userRepository.DeleteUser(strUser.toStdString()))
	{
		LoadUsers();
	}
}

void LoginWindow::ClearPendingCredentials()
{
	std::fill(m_strPendingPassword.begin(), m_strPendingPassword.end(), '\0');
	m_strPendingPassword.clear();
	m_strPendingUser.clear();
}
