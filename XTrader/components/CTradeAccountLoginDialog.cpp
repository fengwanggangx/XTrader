#include "CTradeAccountLoginDialog.h"
#include "CSinglePixelBorder.h"
#include "CUIStyle.h"
#include "ui_CTradeAccountLoginDialog.h"
#include <QAbstractItemView>
#include <QAction>
#include <QCoreApplication>
#include <QIcon>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPointer>
#include <QSettings>
#include <QStyledItemDelegate>
#include <QWindow>

namespace
{
	class CComboItemDelegate final : public QStyledItemDelegate
	{
	public:
		explicit CComboItemDelegate(QComboBox& combo) : QStyledItemDelegate(combo.view()), m_combo(combo)
		{
		}

		QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
		{
			QSize size = QStyledItemDelegate::sizeHint(option, index);
			size.setHeight(m_combo.height());
			return size;
		}

	private:
		QComboBox& m_combo;
	};
}

CTradeAccountLoginDialog::CTradeAccountLoginDialog(CTraderServices& services, QWidget* pParent)
	: QDialog(pParent), m_ui(std::make_unique<Ui::CTradeAccountLoginDialog>())
{
	m_pServices = &services;
	setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint |
				   Qt::NoDropShadowWindowHint);
	m_ui->setupUi(this);
	setFixedSize(272, 272);
	m_ui->titleFrame->installEventFilter(this);
	m_ui->titleLabel->installEventFilter(this);
	connect(m_ui->closeButton, &QPushButton::clicked, this, &QDialog::reject);
	UIStyle::Apply(*this, ":/styles/trade-account-login.qss");
	// Only the device-pixel overlay draws borders; disable native frames.
	m_ui->accountEdit->setFrame(false);
	m_ui->passwordEdit->setFrame(false);
	for (QComboBox* pCombo : { m_ui->brokerCombo, m_ui->siteCombo })
	{
		pCombo->setFrame(false);
		pCombo->setItemDelegate(new CComboItemDelegate(*pCombo));
		pCombo->view()->setFrameShape(QFrame::NoFrame);
		QFrame* pPopup = qobject_cast<QFrame*>(pCombo->view()->parentWidget());
		if (nullptr != pPopup)
		{
			pPopup->setFrameShape(QFrame::NoFrame);
		}
		CSinglePixelBorder::Install(*pCombo->view());
	}
	for (QWidget* pControl :
		{ static_cast<QWidget*>(this), static_cast<QWidget*>(m_ui->accountEdit),
			static_cast<QWidget*>(m_ui->passwordEdit),
			static_cast<QWidget*>(m_ui->brokerCombo),
			static_cast<QWidget*>(m_ui->siteCombo) })
	{
		CSinglePixelBorder::Install(*pControl);
	}
	CSinglePixelBorder::Install(*m_ui->titleFrame, 0.0, Qt::BottomEdge);
	CSinglePixelBorder::Install(*m_ui->divider, 0.0, Qt::TopEdge);
	CSinglePixelBorder::Install(*m_ui->loginButton, 5.0);
	CSinglePixelBorder::Install(*m_ui->cancelButton, 5.0);
	connect(m_ui->brokerCombo, &QComboBox::currentIndexChanged, this,
		[this](int index)
	{
		m_ui->siteCombo->clear();
		if (0 <= index &&
			static_cast<std::size_t>(index) < m_brokers.size())
		{
			for (const auto& site : m_brokers[index].sites())
			{
				m_ui->siteCombo->addItem(QString::fromStdString(site.name()),
					QString::fromStdString(site.site()));
			}
		}
	});
	QAction* pReveal = m_ui->passwordEdit->addAction(
		QIcon(":/trade/password-eye.svg"), QLineEdit::TrailingPosition);
	pReveal->setCheckable(true);
	pReveal->setText("显示密码");
	pReveal->setToolTip("显示密码");
	connect(pReveal, &QAction::toggled, this, [this, pReveal](bool bVisible)
	{
		m_ui->passwordEdit->setEchoMode(bVisible ? QLineEdit::Normal
												 : QLineEdit::Password);
		pReveal->setText(bVisible ? "隐藏密码" : "显示密码");
		pReveal->setToolTip(pReveal->text());
	});
	connect(m_ui->loginButton, &QPushButton::clicked, this,
		&CTradeAccountLoginDialog::AddAccount);
	connect(m_ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
	connect(m_ui->passwordEdit, &QLineEdit::returnPressed, this,
		&CTradeAccountLoginDialog::AddAccount);
	QPointer<CTradeAccountLoginDialog> self(this);
	m_nToken = m_pServices->m_account.Subscribe(
		[self](const CAccountService::CResult& result)
	{
		if (nullptr != self)
		{
			QMetaObject::invokeMethod(
				self,
				[self, result]()
			{
				if (nullptr != self)
				{
					self->HandleResult(result);
				}
			},
				Qt::QueuedConnection);
		}
	});
	m_directoryId =
		m_pServices->m_account.Send(_TyRequest::SUPPORTED_BROKER_QUERY);
	if (0 == m_directoryId)
	{
		ShowError("无法加载券商目录，请重新打开登录窗口");
	}
	m_ui->accountEdit->setFocus();
}

CTradeAccountLoginDialog::~CTradeAccountLoginDialog()
{
	m_pServices->m_account.Unsubscribe(m_nToken);
	m_ui->passwordEdit->clear();
}

void CTradeAccountLoginDialog::SetBusy(bool bBusy)
{
	m_ui->loginButton->setEnabled(!bBusy);
	m_ui->brokerCombo->setEnabled(!bBusy);
	m_ui->siteCombo->setEnabled(!bBusy);
	m_ui->accountEdit->setEnabled(!bBusy);
	m_ui->passwordEdit->setEnabled(!bBusy);
	m_ui->loginButton->setText(bBusy ? "保存中…" : "保存");
}

void CTradeAccountLoginDialog::AddAccount()
{
	if (0 != m_nRequestId)
	{
		return;
	}
	if ((0 > m_ui->brokerCombo->currentIndex()) ||
		(0 > m_ui->siteCombo->currentIndex()) ||
		m_ui->accountEdit->text().isEmpty() ||
		m_ui->passwordEdit->text().isEmpty())
	{
		ShowError("请选择券商、站点，并输入账户和密码");
		return;
	}
	m_nRequestId = m_pServices->m_account.Send(
		_TyRequest::TRADE_ACCOUNT_ADD, {},
		{ { "broker_id", m_ui->brokerCombo->currentData().toString().toStdString() },
			{ "broker_name", m_ui->brokerCombo->currentText().toStdString() },
			{ "site", m_ui->siteCombo->currentData().toString().toStdString() },
			{ "account", m_ui->accountEdit->text().toStdString() },
			{ "password", m_ui->passwordEdit->text().toStdString() } });
	if (0 == m_nRequestId)
	{
		ShowError("请求未发送，请检查 XEngine 登录状态或等待当前请求完成");
		return;
	}
	ShowError(QString());
	SetBusy(true);
}

void CTradeAccountLoginDialog::HandleResult(
	const CAccountService::CResult& result)
{
	if (0 != m_directoryId && result.m_requestId == m_directoryId)
	{
		m_directoryId = 0;
		if (!result.m_error.empty() || !result.m_data.has_broker_list())
		{
			ShowError("无法加载券商目录");
			return;
		}
		m_brokers.assign(result.m_data.broker_list().brokers().begin(),
			result.m_data.broker_list().brokers().end());
		m_ui->brokerCombo->clear();
		for (const auto& broker : m_brokers)
		{
			m_ui->brokerCombo->addItem(QString::fromStdString(broker.name()),
				QString::number(broker.broker_id()));
		}
		return;
	}

	if ((0 == m_nRequestId) ||
		((m_nRequestId != result.m_requestId) && (!result.m_bSessionReset)))
	{
		return;
	}
	m_nRequestId = 0;
	SetBusy(false);
	m_ui->passwordEdit->clear();
	if (!result.m_error.empty() || (result.m_bSessionReset))
	{
		ShowError(result.m_error.empty() ? "连接已断开，请重新登录 XEngine"
										 : QString::fromStdString(result.m_error));
		return;
	}
	accept();
}

bool CTradeAccountLoginDialog::eventFilter(QObject* pObject, QEvent* pEvent)
{
	if (((m_ui->titleFrame == pObject) || (m_ui->titleLabel == pObject)) &&
		(QEvent::MouseButtonPress == pEvent->type()))
	{
		QMouseEvent* pMouse = static_cast<QMouseEvent*>(pEvent);
		if ((Qt::LeftButton == pMouse->button()) && (nullptr != windowHandle()))
		{
			return windowHandle()->startSystemMove();
		}
	}
	return QDialog::eventFilter(pObject, pEvent);
}

void CTradeAccountLoginDialog::ShowError(const QString& strMessage)
{
	m_ui->errorLabel->setText(strMessage);
	m_ui->errorLabel->setVisible(!strMessage.isEmpty());
	int nExtraHeight = 0;
	if (!strMessage.isEmpty())
	{
		nExtraHeight =
			qMax(m_ui->errorLabel->minimumHeight(),
				m_ui->errorLabel->heightForWidth(m_ui->errorLabel->width())) +
			m_ui->bodyLayout->spacing();
	}
	setFixedSize(272, 272 + nExtraHeight);
}
