#ifndef XTRADER_COMPONENTS_CDISTRIBUTIONCHART_H
#define XTRADER_COMPONENTS_CDISTRIBUTIONCHART_H

#include <QColor>
#include <QVector>
#include <QWidget>

class CDistributionChart final : public QWidget
{
	Q_OBJECT
	Q_PROPERTY(QColor risingColor READ GetRisingColor WRITE SetRisingColor)
	Q_PROPERTY(QColor fallingColor READ GetFallingColor WRITE SetFallingColor)
public:
	explicit CDistributionChart(QWidget* pParent = nullptr);
	void SetValues(const QVector<int>& values);
	const QColor& GetRisingColor() const;
	const QColor& GetFallingColor() const;
	void SetRisingColor(const QColor& color);
	void SetFallingColor(const QColor& color);

protected:
	void paintEvent(QPaintEvent* pEvent) override;

private:
	QVector<int> m_values;
	QColor m_risingColor{ Qt::red };
	QColor m_fallingColor{ Qt::green };
};

#endif
