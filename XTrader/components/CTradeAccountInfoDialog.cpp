#include "CTradeAccountInfoDialog.h"
#include "CSinglePixelBorder.h"
#include "CUIStyle.h"
#include "../request/request.pb.h"
#include "ui_CTradeAccountInfoDialog.h"
#include <QMouseEvent>
#include <QWindow>

CTradeAccountInfoDialog::CTradeAccountInfoDialog(const request::TradeAccountRecord& account, QWidget* pParent)
	: QDialog(pParent)
{
	setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
	Ui::CTradeAccountInfoDialog ui;
	ui.setupUi(this);
	QString style = UIStyle::Load(":/styles/trade-account-login.qss");
	setStyleSheet(style.replace(QStringLiteral("#CTradeAccountLoginDialog"), QStringLiteral("#CTradeAccountInfoDialog")));
	ui.titleFrame->installEventFilter(this);
	ui.titleLabel->installEventFilter(this);
	connect(ui.closeButton, &QPushButton::clicked, this, &QDialog::reject);
	connect(ui.loginButton, &QPushButton::clicked, this, &QDialog::accept);
	ui.brokerEdit->setText(QString::fromStdString(account.broker_name()));
	ui.siteEdit->setText(QString::fromStdString(account.site()));
	ui.accountEdit->setText(QString::fromStdString(account.account()));
	for (QLineEdit* edit : { ui.brokerEdit, ui.siteEdit, ui.accountEdit })
	{
		edit->setReadOnly(true);
		edit->setFrame(false);
		CSinglePixelBorder::Install(*edit);
	}
	CSinglePixelBorder::Install(*this);
	CSinglePixelBorder::Install(*ui.titleFrame, 0.0, Qt::BottomEdge);
	CSinglePixelBorder::Install(*ui.divider, 0.0, Qt::TopEdge);
	ui.loginButton->setFocus();
}

bool CTradeAccountInfoDialog::eventFilter(QObject* pObject, QEvent* pEvent)
{
	if (QEvent::MouseButtonPress == pEvent->type())
	{
		QMouseEvent* mouse = static_cast<QMouseEvent*>(pEvent);
		QWindow* window = windowHandle();
		if (Qt::LeftButton == mouse->button() && nullptr != window)
		{
			return window->startSystemMove();
		}
	}
	return QDialog::eventFilter(pObject, pEvent);
}
