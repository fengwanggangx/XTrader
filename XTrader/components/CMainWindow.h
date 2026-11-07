#pragma once
#include "../system/CTraderServices.h"

#include <QMainWindow>
#include <QPoint>
#include <cstdint>
#include <memory>

class QAction;
class QMoveEvent;
class QResizeEvent;
class CKeyboardSprite;
namespace Ui
{
	class CMainWindowClass;
}

class CMainWindow final : public QMainWindow
{
	Q_OBJECT

public:
	explicit CMainWindow(CTraderServices& services, QWidget* pParent = nullptr);
	~CMainWindow() override;

private slots:
	void OnItemSelectChanged();
	void OnLightTheme();
	void OnDarkTheme();
	void UpdateClock();

private:
	void ConnectSlots();
	void UIInitialized();
	void ApplyTheme(bool bDark);
	void UpdateWindowButtonIcons();
	void UpdateConnectionState(int nState, const QString& strMessage);
	void PositionKeyboardSprite();
	void changeEvent(QEvent* pEvent) override;
	bool eventFilter(QObject* pObject, QEvent* pEvent) override;
	void moveEvent(QMoveEvent* pEvent) override;
	void resizeEvent(QResizeEvent* pEvent) override;

	std::unique_ptr<Ui::CMainWindowClass> m_ui;
	QPoint m_dragPosition;
	bool m_bDarkTheme{ false };
	bool m_bThemeInitialized{ false };
	bool m_bDragging{ false };
	CKeyboardSprite* m_pKeyboardSprite{ nullptr };
	std::uint64_t m_nSecurityListToken{ 0 };

private:
	CTraderServices* m_pServices{ nullptr };
};
