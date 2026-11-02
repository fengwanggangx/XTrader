#pragma once

#include "../database/CUserStorager.h"
#include "../system/CLoginService.h"
#include <QDialog>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui
{
	class LoginWindowClass;
};
QT_END_NAMESPACE

class LoginWindow : public QDialog
{
	Q_OBJECT

  public:
	LoginWindow(QWidget* pParent = nullptr);
	~LoginWindow();

  protected:
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;

  private slots:
	void OnLoginBtnClicked();
	void OnRegisterBtnClicked();
	void OnCloseBtnClicked();
	void OnSettingsBtnClicked();
	void OnUserChanged(int nIndex);
	void OnUserEdited(const QString& strUser);

private:
	void ConnectSlots();
	void LoadUsers();
	void FillPassword(int nIndex);
	void SavePendingUser();
	void DeleteUser(int nIndex);
	void ClearPendingCredentials();

  private:
	std::unique_ptr<Ui::LoginWindowClass> m_ui;

  private:
	QPoint m_dragPosition;
	bool m_isDragging{ false };
	_TyCallbackId m_loginCallbackId{ 0 };
	CUserStorager m_userRepository;
	std::vector<CUserInfo> m_users;
	std::string m_strPendingUser;
	std::string m_strPendingPassword;
};
