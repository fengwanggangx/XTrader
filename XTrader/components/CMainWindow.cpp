#include "CMainWindow.h"
#include "CUIStyle.h"
#include "CKeyboardSprite.h"
#include "ui_CMainWindow.h"

#include "CRiskSettings.h"
#include "CSystemSettings.h"
#include "CViewHQMarket.h"
#include "CViewStrategySettings.h"
#include "../system/CSession.h"

#include <QActionGroup>
#include <QApplication>
#include <QDateTime>
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QAbstractItemView>
#include <QScreen>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QPixmap>
#include <QPalette>
#include <QStyle>
#include <QTimer>
#include <QTreeWidgetItem>
#include <QToolButton>

#include <cstddef>
#include <algorithm>
#include <vector>

namespace
{
	const QStringList NavigationIcons{
		":/navigation/hqmarket-%1.png",
		":/navigation/strategy-settings-%1.png",
		":/navigation/risk-setting-%1.png",
		":/navigation/ai-trade-%1.png",
		":/navigation/system-settings-%1.png"
	};

}

CMainWindow::CMainWindow(CTraderServices& services, QWidget* pParent) : QMainWindow(pParent), m_ui(std::make_unique<Ui::CMainWindowClass>())
{
	m_pServices = &services;
	m_ui->setupUi(this);
	m_ui->marketPage->Initialize(services);
	m_ui->strategyPage->Initialize(services);
	setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
	setWindowIcon(QIcon(":/branding/xtrader-app-icon.png"));
	UIInitialized();
	ConnectSlots();
	m_pKeyboardSprite = new CKeyboardSprite(this);
	m_pKeyboardSprite->setProperty("darkTheme", m_bDarkTheme);
	QPointer<CKeyboardSprite> safeSprite(m_pKeyboardSprite);
	m_nSecurityListToken = m_pServices->m_hq.AddSecurityListHandler([safeSprite](const CSecurityListEvent& ev)
	{
		if (!safeSprite.isNull())
		{
			QMetaObject::invokeMethod(safeSprite.data(), [safeSprite, securities = ev.m_securities]()
			{
				if (!safeSprite.isNull())
				{
					safeSprite->SetSecurities(securities);
				}
			}, Qt::QueuedConnection);
		}
	});
	m_pKeyboardSprite->SetSecurities(m_pServices->m_hq.GetSecurities());
	connect(m_pKeyboardSprite, &CKeyboardSprite::SecuritySelected, this, [this](const CSecurity& security)
	{
		m_ui->treeWidget->setCurrentItem(m_ui->treeWidget->topLevelItem(0));
		m_ui->stackedWidget->setCurrentWidget(m_ui->marketPage);
		m_ui->marketPage->OpenSecurityTab(security);
	});
	qApp->installEventFilter(this);
}

CMainWindow::~CMainWindow()
{
	qApp->removeEventFilter(this);
	m_pServices->m_hq.RemoveSecurityListHandler(m_nSecurityListToken);
	m_pServices->m_login.SetStateCallback({});
}

void CMainWindow::UIInitialized()
{
	m_ui->titleBar->installEventFilter(this);
	m_ui->appIconLabel->setPixmap(QPixmap(":/branding/xtrader-app-icon.png").scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
	QActionGroup* pThemeGroup = new QActionGroup(m_ui->skinMenu);
	pThemeGroup->setExclusive(true);
	pThemeGroup->addAction(m_ui->lightThemeAction);
	pThemeGroup->addAction(m_ui->darkThemeAction);
	m_ui->skinButton->setMenu(m_ui->skinMenu);
	UpdateWindowButtonIcons();
	for (int nIndex = 0; m_ui->treeWidget->topLevelItemCount() > nIndex; ++nIndex)
	{
		m_ui->treeWidget->topLevelItem(nIndex)->setData(0, Qt::UserRole, nIndex);
	}
	QTreeWidgetItem* pFirstItem = m_ui->treeWidget->topLevelItem(0);
	if (nullptr != pFirstItem)
	{
		m_ui->treeWidget->setCurrentItem(pFirstItem);
		m_ui->stackedWidget->setCurrentIndex(0);
	}
	ApplyTheme(false);
	UpdateClock();
	QTimer* pTimer = new QTimer(this);
	connect(pTimer, &QTimer::timeout, this, &CMainWindow::UpdateClock);
	pTimer->start(1000);

	QPointer<CMainWindow> safeThis(this);
	m_pServices->m_login.SetStateCallback([safeThis](SessionState state, const std::string& strMessage)
	{
		QMetaObject::invokeMethod(safeThis.data(), [safeThis, state, strMessage]()
		{
			if (!safeThis.isNull())
			{
				safeThis->UpdateConnectionState(static_cast<int>(state), QString::fromStdString(strMessage));
			}
		}, Qt::QueuedConnection);
	});
	UpdateConnectionState(static_cast<int>(m_pServices->m_login.GetState()), QString());
}

void CMainWindow::ConnectSlots()
{
	connect(m_ui->treeWidget, &QTreeWidget::itemSelectionChanged, this, &CMainWindow::OnItemSelectChanged);
	connect(m_ui->lightThemeAction, &QAction::triggered, this, &CMainWindow::OnLightTheme);
	connect(m_ui->darkThemeAction, &QAction::triggered, this, &CMainWindow::OnDarkTheme);
	connect(m_ui->minimizeButton, &QPushButton::clicked, this, &QWidget::showMinimized);
	connect(m_ui->maximizeButton, &QPushButton::clicked, this, [this]()
	{
		isMaximized() ? showNormal() : showMaximized();
	});
	connect(m_ui->closeButton, &QPushButton::clicked, this, &QWidget::close);
}

void CMainWindow::OnItemSelectChanged()
{
	QTreeWidgetItem* pItem = m_ui->treeWidget->currentItem();
	if (nullptr == pItem)
	{
		return;
	}
	int nIndex = pItem->data(0, Qt::UserRole).toInt();
	if ((0 <= nIndex) && (m_ui->stackedWidget->count() > nIndex))
	{
		m_ui->stackedWidget->setCurrentIndex(nIndex);
	}
}

void CMainWindow::OnLightTheme()
{
	ApplyTheme(false);
}

void CMainWindow::OnDarkTheme()
{
	ApplyTheme(true);
}

void CMainWindow::ApplyTheme(bool bDark)
{
	if (m_bThemeInitialized && (bDark == m_bDarkTheme))
	{
		return;
	}
	setUpdatesEnabled(false);
	if (!m_bThemeInitialized)
	{
		qApp->setStyleSheet(UIStyle::Load(":/styles/theme.qss"));
	}
	m_bDarkTheme = bDark;
	m_bThemeInitialized = true;
	setProperty("darkTheme", bDark);
	if (nullptr != m_pKeyboardSprite)
	{
		m_pKeyboardSprite->setProperty("darkTheme", bDark);
		UIStyle::Refresh(*m_pKeyboardSprite);
	}
	m_ui->skinMenu->setProperty("darkTheme", bDark);
	QPalette palette = qApp->style()->standardPalette();
	if (bDark)
	{
		palette.setColor(QPalette::Window, QColor("#122130"));
		palette.setColor(QPalette::WindowText, QColor("#dce8f6"));
		palette.setColor(QPalette::Base, QColor("#0e192b"));
		palette.setColor(QPalette::AlternateBase, QColor("#122130"));
		palette.setColor(QPalette::Text, QColor("#c8d8e7"));
		palette.setColor(QPalette::Button, QColor("#122130"));
		palette.setColor(QPalette::ButtonText, QColor("#a9bdd0"));
		palette.setColor(QPalette::Highlight, QColor("#1e3e81"));
		palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
		palette.setColor(QPalette::ToolTipBase, QColor("#14283a"));
		palette.setColor(QPalette::ToolTipText, QColor("#dce8f6"));
		palette.setColor(QPalette::PlaceholderText, QColor("#718ba0"));
		palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#718ba0"));
		palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#718ba0"));
		palette.setColor(QPalette::Mid, QColor("#203b52"));
		palette.setColor(QPalette::Dark, QColor("#37546c"));
		palette.setColor(QPalette::Link, QColor("#1683ff"));
	}
	else
	{
		palette.setColor(QPalette::Window, QColor("#f4f7fb"));
		palette.setColor(QPalette::WindowText, QColor("#162033"));
		palette.setColor(QPalette::Base, QColor("#ffffff"));
		palette.setColor(QPalette::AlternateBase, QColor("#f8fafd"));
		palette.setColor(QPalette::Text, QColor("#344258"));
		palette.setColor(QPalette::Button, QColor("#ffffff"));
		palette.setColor(QPalette::ButtonText, QColor("#536176"));
		palette.setColor(QPalette::Highlight, QColor("#4d82f9"));
		palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
		palette.setColor(QPalette::ToolTipBase, QColor("#ffffff"));
		palette.setColor(QPalette::ToolTipText, QColor("#344258"));
		palette.setColor(QPalette::PlaceholderText, QColor("#68778c"));
		palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#8b98aa"));
		palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#8b98aa"));
		palette.setColor(QPalette::Mid, QColor("#e4eaf2"));
		palette.setColor(QPalette::Dark, QColor("#68778c"));
		palette.setColor(QPalette::Link, QColor("#1677ff"));
	}
	qApp->setPalette(palette);
	UIStyle::Refresh(*this);
	m_ui->lightThemeAction->setChecked(!bDark);
	m_ui->darkThemeAction->setChecked(bDark);
	UpdateWindowButtonIcons();
	QString strTheme = bDark ? "black" : "white";
	for (int nItem = 0; nItem < m_ui->treeWidget->topLevelItemCount(); ++nItem)
	{
		QTreeWidgetItem* pItem = m_ui->treeWidget->topLevelItem(nItem);
		int nIndex = pItem->data(0, Qt::UserRole).toInt();
		if ((0 <= nIndex) && (nIndex < NavigationIcons.size()))
		{
			pItem->setIcon(0, QIcon(NavigationIcons[nIndex].arg(strTheme)));
		}
	}
	setUpdatesEnabled(true);
	update();
}

void CMainWindow::UpdateClock()
{
	m_ui->clockLabel->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
}

void CMainWindow::UpdateWindowButtonIcons()
{
	bool bMaximized = isMaximized();
	QString strTheme = m_bDarkTheme ? "black" : "white";
	m_ui->skinButton->setIcon(QIcon(QString(":/navigation/skin-setting-%1.png").arg(strTheme)));
	m_ui->minimizeButton->setIcon(QIcon(QString(":/window/min-%1.png").arg(strTheme)));
	m_ui->maximizeButton->setIcon(QIcon(QString(bMaximized ? ":/window/restore-%1.png" : ":/window/max-%1.png").arg(strTheme)));
	m_ui->closeButton->setIcon(QIcon(QString(":/window/close-%1.png").arg(strTheme)));
	m_ui->maximizeButton->setToolTip(bMaximized ? "还原" : "最大化");
}

void CMainWindow::UpdateConnectionState(int nState, const QString& strMessage)
{
	SessionState state = static_cast<SessionState>(nState);
	bool bReady = SessionState::Ready == state;
	m_ui->connectionDot->setProperty("ready", bReady);
	UIStyle::Refresh(*m_ui->connectionDot);
	if (bReady)
	{
		m_ui->connectionLabel->setText("交易连接正常");
		return;
	}
	m_ui->connectionLabel->setText(strMessage.isEmpty() ? "交易连接中断" : strMessage);
}

void CMainWindow::changeEvent(QEvent* pEvent)
{
	QMainWindow::changeEvent(pEvent);
	if (QEvent::WindowStateChange == pEvent->type())
	{
		UpdateWindowButtonIcons();
	}
}

bool CMainWindow::eventFilter(QObject* pObject, QEvent* pEvent)
{
	QWidget* pEventWidget = qobject_cast<QWidget*>(pObject);
	if ((nullptr != m_pKeyboardSprite) && (QEvent::MouseButtonPress == pEvent->type()) && m_pKeyboardSprite->isVisible() && (nullptr != pEventWidget) && (m_pKeyboardSprite != pEventWidget) && !m_pKeyboardSprite->isAncestorOf(pEventWidget))
	{
		m_pKeyboardSprite->hide();
	}
	if ((nullptr != m_pKeyboardSprite) && (QEvent::KeyPress == pEvent->type()) && isActiveWindow())
	{
		QKeyEvent* pKey = static_cast<QKeyEvent*>(pEvent);
		if ((Qt::ControlModifier == pKey->modifiers()) && (Qt::Key_K == pKey->key()))
		{
			PositionKeyboardSprite();
			m_pKeyboardSprite->Open();
			return true;
		}
		QWidget* pFocus = qApp->focusWidget();
		bool bEditing = (nullptr != qobject_cast<QLineEdit*>(pFocus)) || (nullptr != qobject_cast<QTextEdit*>(pFocus)) || (nullptr != qobject_cast<QPlainTextEdit*>(pFocus)) || (nullptr != qobject_cast<QComboBox*>(pFocus)) || (nullptr != qobject_cast<QAbstractItemView*>(pFocus));
		QString strInput = pKey->text();
		if (!bEditing && !m_pKeyboardSprite->isVisible() && ((Qt::NoModifier == pKey->modifiers()) || (Qt::ShiftModifier == pKey->modifiers())) && (1 == strInput.size()) && strInput.at(0).isLetterOrNumber() && (strInput.at(0).unicode() < 128))
		{
			PositionKeyboardSprite();
			m_pKeyboardSprite->Open(strInput);
			return true;
		}
	}
	if (m_ui->titleBar != pObject)
	{
		return QMainWindow::eventFilter(pObject, pEvent);
	}
	if (QEvent::MouseButtonPress == pEvent->type())
	{
		QMouseEvent* pMouseEvent = static_cast<QMouseEvent*>(pEvent);
		if (Qt::LeftButton == pMouseEvent->button())
		{
			m_bDragging = true;
			m_dragPosition = pMouseEvent->globalPosition().toPoint() - frameGeometry().topLeft();
			return true;
		}
	}
	else if (QEvent::MouseMove == pEvent->type())
	{
		QMouseEvent* pMouseEvent = static_cast<QMouseEvent*>(pEvent);
		if (m_bDragging && !isMaximized())
		{
			move(pMouseEvent->globalPosition().toPoint() - m_dragPosition);
			return true;
		}
	}
	else if (QEvent::MouseButtonRelease == pEvent->type())
	{
		m_bDragging = false;
	}
	else if (QEvent::MouseButtonDblClick == pEvent->type())
	{
		isMaximized() ? showNormal() : showMaximized();
		return true;
	}
	return QMainWindow::eventFilter(pObject, pEvent);
}

void CMainWindow::PositionKeyboardSprite()
{
	QRect desired(mapToGlobal(QPoint(width() - m_pKeyboardSprite->width() - 8, height() - m_pKeyboardSprite->height() - 8)), m_pKeyboardSprite->size());
	QScreen* pScreen = QGuiApplication::screenAt(desired.center());
	if (nullptr == pScreen)
	{
		pScreen = screen();
	}
	if (nullptr != pScreen)
	{
		QRect available = pScreen->availableGeometry();
		desired.moveLeft((std::clamp)(desired.left(), available.left(), available.right() - desired.width() + 1));
		desired.moveTop((std::clamp)(desired.top(), available.top(), available.bottom() - desired.height() + 1));
	}
	m_pKeyboardSprite->move(desired.topLeft());
}

void CMainWindow::moveEvent(QMoveEvent* pEvent)
{
	QMainWindow::moveEvent(pEvent);
	if (nullptr != m_pKeyboardSprite)
	{
		PositionKeyboardSprite();
	}
}

void CMainWindow::resizeEvent(QResizeEvent* pEvent)
{
	QMainWindow::resizeEvent(pEvent);
	if (nullptr != m_pKeyboardSprite)
	{
		PositionKeyboardSprite();
	}
}
