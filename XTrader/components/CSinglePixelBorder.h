#pragma once
#include <QEvent>
#include <QPainter>
#include <QWidget>
#include <QtMath>
#include <memory>

// A mouse-transparent Qt-owned overlay draws in device pixels, independently of DPI.
class CSinglePixelBorder final : public QWidget
{
public:
	explicit CSinglePixelBorder(QWidget& widget, qreal fRadius = 0.0, Qt::Edges edges = Qt::TopEdge | Qt::BottomEdge | Qt::LeftEdge | Qt::RightEdge) : QWidget(&widget), m_fRadius(fRadius), m_edges(edges)
	{
		setObjectName("singlePixelBorder");
		setAttribute(Qt::WA_TransparentForMouseEvents);
		setAttribute(Qt::WA_NoSystemBackground);
		setFocusPolicy(Qt::NoFocus);
		setGeometry(widget.rect());
		widget.installEventFilter(this);
		raise();
		show();
	}
	static void Install(QWidget& widget, qreal fRadius = 0.0, Qt::Edges edges = Qt::TopEdge | Qt::BottomEdge | Qt::LeftEdge | Qt::RightEdge)
	{
		std::unique_ptr<CSinglePixelBorder> border = std::make_unique<CSinglePixelBorder>(widget, fRadius, edges);
		border.release(); // The QWidget parent owns and destroys the overlay.
	}

protected:
	bool eventFilter(QObject* pObject, QEvent* pEvent) override
	{
		if (parentWidget() == pObject)
		{
			if ((QEvent::Resize == pEvent->type()) || (QEvent::Show == pEvent->type()))
			{
				setGeometry(parentWidget()->rect());
				raise();
			}
			if ((QEvent::FocusIn == pEvent->type()) || (QEvent::FocusOut == pEvent->type()) || (QEvent::EnabledChange == pEvent->type()) || (QEvent::PaletteChange == pEvent->type()) || (QEvent::StyleChange == pEvent->type()))
			{
				update();
			}
		}
		return QWidget::eventFilter(pObject, pEvent);
	}
	void paintEvent(QPaintEvent*) override
	{
		QPainter painter(this);
		qreal fScale = devicePixelRatioF();
		// Use the actual backing-store offset: a child can start between device pixels.
		QRectF deviceRect = painter.deviceTransform().mapRect(QRectF(rect()));
		qreal fLeft = qCeil(deviceRect.left()) - deviceRect.left();
		qreal fTop = qCeil(deviceRect.top()) - deviceRect.top();
		int nWidth = qFloor(deviceRect.right()) - qCeil(deviceRect.left());
		int nHeight = qFloor(deviceRect.bottom()) - qCeil(deviceRect.top());
		painter.scale(1.0 / fScale, 1.0 / fScale);
		painter.translate(fLeft, fTop);
		QColor color = palette().color(parentWidget()->hasFocus() ? QPalette::Highlight : QPalette::WindowText);
		if (0.0 == m_fRadius)
		{
			if (m_edges.testFlag(Qt::TopEdge))
			{
				painter.fillRect(0, 0, nWidth, 1, color);
			}
			if (m_edges.testFlag(Qt::BottomEdge))
			{
				painter.fillRect(0, nHeight - 1, nWidth, 1, color);
			}
			if (m_edges.testFlag(Qt::LeftEdge))
			{
				painter.fillRect(0, 0, 1, nHeight, color);
			}
			if (m_edges.testFlag(Qt::RightEdge))
			{
				painter.fillRect(nWidth - 1, 0, 1, nHeight, color);
			}
			return;
		}
		QPen pen(color);
		pen.setWidth(1);
		pen.setCosmetic(true);
		painter.setPen(pen);
		painter.setBrush(Qt::NoBrush);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.drawRoundedRect(QRectF(0.5, 0.5, nWidth - 1.0, nHeight - 1.0), m_fRadius * fScale, m_fRadius * fScale);
	}

private:
	qreal m_fRadius{ 0.0 };
	Qt::Edges m_edges;
};
