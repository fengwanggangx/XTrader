#include "CUICurve.h"
#include "CUIStyle.h"
#include "ui_CUICurve.h"

#include <QwtPlot.h>

#include <QApplication>
#include <QDateTime>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QScopedValueRollback>
#include <QTime>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
	constexpr double InvalidValue = std::numeric_limits<double>::quiet_NaN();
	const QColor RisingColor("#f04455");
	const QColor FallingColor("#00b987");

	QColor FinancialColor(double fDifference)
	{
		if (0.0000001 < fDifference)
		{
			return RisingColor;
		}
		if (-0.0000001 > fDifference)
		{
			return FallingColor;
		}
		return qApp->palette().color(QPalette::Text);
	}

	double NiceStep(double fRawStep)
	{
		if (0.0 >= fRawStep)
		{
			return 1.0;
		}
		double fPower = std::pow(10.0, std::floor(std::log10(fRawStep)));
		double fFraction = fRawStep / fPower;
		double fNiceFraction = 10.0;
		if (1.0 >= fFraction)
		{
			fNiceFraction = 1.0;
		}
		else if (2.0 >= fFraction)
		{
			fNiceFraction = 2.0;
		}
		else if (2.5 >= fFraction)
		{
			fNiceFraction = 2.5;
		}
		else if (5.0 >= fFraction)
		{
			fNiceFraction = 5.0;
		}
		return fNiceFraction * fPower;
	}

	QDateTime BarDateTime(const CMarketBar& bar)
	{
		return QDateTime::fromMSecsSinceEpoch(bar.m_nBeginTime);
	}

	QDate BarDate(const CMarketBar& bar)
	{
		return BarDateTime(bar).date();
	}

	bool IsMinuteCurveMode(CurveMode mode)
	{
		return (CurveMode::Minute5 == mode) || (CurveMode::Minute15 == mode) || (CurveMode::Minute30 == mode) || (CurveMode::Minute60 == mode);
	}

	int MinutePeriod(CurveMode mode)
	{
		switch (mode)
		{
		case CurveMode::Minute5:
			return 5;
		case CurveMode::Minute15:
			return 15;
		case CurveMode::Minute30:
			return 30;
		case CurveMode::Minute60:
			return 60;
		default:
			return 0;
		}
	}

	int SessionMinute(const QTime& time)
	{
		if ((QTime(9, 30) <= time) && (QTime(11, 30) >= time))
		{
			return (std::min)(119, QTime(9, 30).secsTo(time) / 60);
		}
		if ((QTime(13, 0) <= time) && (QTime(15, 0) >= time))
		{
			return 120 + (std::min)(119, QTime(13, 0).secsTo(time) / 60);
		}
		return -1;
	}

	class CBarCandleSeriesData final : public QwtSeriesData<QwtOHLCSample>
	{
	public:
		explicit CBarCandleSeriesData(const std::shared_ptr<const std::vector<CMarketBar>>& bars) : m_bars(bars)
		{
		}

		std::size_t size() const override
		{
			return nullptr == m_bars ? 0 : m_bars->size();
		}

		QwtOHLCSample sample(std::size_t nIndex) const override
		{
			const CMarketBar& bar = (*m_bars)[nIndex];
			return QwtOHLCSample(static_cast<double>(nIndex), bar.m_fOpen, bar.m_fHigh, bar.m_fLow, bar.m_fClose);
		}
		QRectF boundingRect() const override
		{
			if ((nullptr == m_bars) || m_bars->empty())
			{
				return QRectF(1.0, 1.0, -2.0, -2.0);
			}
			double fLow = (std::numeric_limits<double>::max)();
			double fHigh = (std::numeric_limits<double>::lowest)();
			for (const CMarketBar& bar : *m_bars)
			{
				fLow = (std::min)(fLow, bar.m_fLow);
				fHigh = (std::max)(fHigh, bar.m_fHigh);
			}
			// QwtPlotTradingCurve exchanges the rectangle axes in vertical orientation.
			return QRectF(fLow, -0.5, fHigh - fLow, static_cast<double>(m_bars->size()));
		}

	private:
		std::shared_ptr<const std::vector<CMarketBar>> m_bars;
	};

	class CBarVolumeSeriesData final : public QwtSeriesData<QwtIntervalSample>
	{
	public:
		explicit CBarVolumeSeriesData(const std::shared_ptr<const std::vector<CMarketBar>>& bars) : m_bars(bars)
		{
		}

		std::size_t size() const override
		{
			return nullptr == m_bars ? 0 : m_bars->size();
		}

		QwtIntervalSample sample(std::size_t nIndex) const override
		{
			return QwtIntervalSample(static_cast<double>((*m_bars)[nIndex].m_nVolume), static_cast<double>(nIndex) - 0.38, static_cast<double>(nIndex) + 0.38);
		}
		QRectF boundingRect() const override
		{
			if ((nullptr == m_bars) || m_bars->empty())
			{
				return QRectF(1.0, 1.0, -2.0, -2.0);
			}
			std::int64_t nMaximumVolume = 0;
			for (const CMarketBar& bar : *m_bars)
			{
				nMaximumVolume = (std::max)(nMaximumVolume, bar.m_nVolume);
			}
			return QRectF(-0.38, 0.0, static_cast<double>(m_bars->size()), static_cast<double>(nMaximumVolume));
		}

	private:
		std::shared_ptr<const std::vector<CMarketBar>> m_bars;
	};

	class CFinancialTradingCurve final : public QwtPlotTradingCurve
	{
	public:
		explicit CFinancialTradingCurve(const QString& strTitle) : QwtPlotTradingCurve(strTitle)
		{
			setSymbolStyle(static_cast<SymbolStyle>(UserSymbol));
		}

		void SetColors(const QColor& rising, const QColor& falling)
		{
			m_rising = rising;
			m_falling = falling;
		}

	protected:
		void drawUserSymbol(QPainter* pPainter, SymbolStyle, const QwtOHLCSample& sample, Qt::Orientation orientation, bool, double fSymbolWidth) const override
		{
			bool bRising = sample.close < sample.open;
			pPainter->save();
			pPainter->setPen(QPen(bRising ? m_rising : m_falling, 1.0));
			pPainter->setBrush(bRising ? QBrush(Qt::NoBrush) : QBrush(m_falling));
			drawCandleStick(pPainter, sample, orientation, fSymbolWidth);
			pPainter->restore();
		}

	private:
		QColor m_rising{ RisingColor };
		QColor m_falling{ FallingColor };
	};

	class CColoredHistogram final : public QwtPlotHistogram
	{
	public:
		void SetColors(const QColor& rising, const QColor& falling)
		{
			m_risingColor = rising;
			m_fallingColor = falling;
		}

		void SetBars(const std::shared_ptr<const std::vector<CMarketBar>>& bars, bool bIntraday, double fReferencePrice)
		{
			m_rising.clear();
			if (nullptr == bars)
			{
				return;
			}
			m_rising.reserve(bars->size());
			QDate previousDate;
			QDate latestDate = bars->empty() ? QDate() : BarDate(bars->back());
			double fPreviousClose = 0.0;
			bool bRising = true;
			for (const CMarketBar& bar : *bars)
			{
				QDate date = BarDate(bar);
				if (date != previousDate)
				{
					if (bIntraday && (date == latestDate) && (0.0 < fReferencePrice))
					{
						fPreviousClose = fReferencePrice;
					}
					else if (!previousDate.isValid())
					{
						fPreviousClose = bar.m_fOpen;
					}
				}
				double fComparison = bIntraday ? fPreviousClose : bar.m_fOpen;
				if (bar.m_fClose > fComparison)
				{
					bRising = true;
				}
				else if (bar.m_fClose < fComparison)
				{
					bRising = false;
				}
				m_rising.emplace_back(bRising);
				fPreviousClose = bar.m_fClose;
				previousDate = date;
			}
		}

	protected:
		void drawColumn(QPainter* pPainter, const QwtColumnRect& rect, const QwtIntervalSample& sample) const override
		{
			int nIndex = qRound((sample.interval.minValue() + sample.interval.maxValue()) * 0.5);
			QColor color = m_fallingColor;
			if ((0 <= nIndex) && (static_cast<std::size_t>(nIndex) < m_rising.size()))
			{
				color = m_rising[static_cast<std::size_t>(nIndex)] ? m_risingColor : m_fallingColor;
			}
			pPainter->save();
			pPainter->setPen(Qt::NoPen);
			pPainter->setBrush(color);
			pPainter->drawRect(rect.toRect());
			pPainter->restore();
		}

	private:
		std::vector<bool> m_rising;
		QColor m_risingColor{ RisingColor };
		QColor m_fallingColor{ FallingColor };
	};

	class CTimeScaleDraw final : public QwtScaleDraw
	{
	public:
		void SetBars(const std::shared_ptr<const std::vector<CMarketBar>>& bars, CurveMode mode, int nDays)
		{
			m_bars = bars;
			m_mode = mode;
			m_nDays = nDays;
		}
		void SetFixedLabels(std::vector<std::pair<int, QString>> labels)
		{
			m_fixedLabels = std::move(labels);
		}
		QwtText label(double fValue) const override
		{
			for (const auto& value : m_fixedLabels)
			{
				if (0.01 > std::abs(fValue - static_cast<double>(value.first)))
				{
					return QwtText(value.second);
				}
			}
			if ((nullptr == m_bars) || m_bars->empty())
			{
				return QwtText();
			}
			int nIndex = qRound(fValue);
			if ((0 > nIndex) || (static_cast<std::size_t>(nIndex) >= m_bars->size()))
			{
				return QwtText();
			}
			QDateTime value = BarDateTime((*m_bars)[static_cast<std::size_t>(nIndex)]);
			if (CurveMode::Intraday == m_mode)
			{
				return QwtText(1 == m_nDays ? value.time().toString("HH:mm") : value.date().toString("MM-dd"));
			}
			return QwtText(IsMinuteCurveMode(m_mode) ? value.toString("MM-dd HH:mm") : value.date().toString("MM-dd"));
		}

	private:
		std::shared_ptr<const std::vector<CMarketBar>> m_bars;
		CurveMode m_mode{ CurveMode::Day };
		int m_nDays{ 1 };
		std::vector<std::pair<int, QString>> m_fixedLabels;
	};

	class CPercentScaleDraw final : public QwtScaleDraw
	{
	public:
		QwtText label(double fValue) const override
		{
			QwtText text(QString("%1%2%").arg(0.0 < fValue ? "+" : "").arg(fValue, 0, 'f', 2));
			text.setColor(FinancialColor(fValue));
			return text;
		}
	};

	class CPriceScaleDraw final : public QwtScaleDraw
	{
	public:
		void SetReferencePrice(double fReferencePrice, bool bColorize)
		{
			m_fReferencePrice = fReferencePrice;
			m_bColorize = bColorize;
		}

		QwtText label(double fValue) const override
		{
			QwtText text(QString::number(fValue, 'f', 2));
			text.setColor(m_bColorize ? FinancialColor(fValue - m_fReferencePrice) : qApp->palette().color(QPalette::Text));
			return text;
		}

	private:
		double m_fReferencePrice{ 0.0 };
		bool m_bColorize{ false };
	};

	class CCurvePicker final : public QwtPlotPicker
	{
	public:
		explicit CCurvePicker(QWidget* pCanvas) : QwtPlotPicker(QwtAxis::XBottom, QwtAxis::YLeft, CrossRubberBand, ActiveOnly, pCanvas)
		{
			setStateMachine(new QwtPickerTrackerMachine());
		}
		void SetBars(const std::shared_ptr<const std::vector<CMarketBar>>& bars, CurveMode mode)
		{
			m_bars = bars;
			m_mode = mode;
		}
		QwtText trackerTextF(const QPointF& point) const override
		{
			if ((nullptr == m_bars) || m_bars->empty())
			{
				return QwtText();
			}
			int nIndex = (std::clamp)(qRound(point.x()), 0, static_cast<int>(m_bars->size()) - 1);
			const CMarketBar& bar = (*m_bars)[static_cast<std::size_t>(nIndex)];
			QDateTime time = BarDateTime(bar);
			QString strText;
			if (CurveMode::Intraday == m_mode)
			{
				strText = QString("%1\n价格 %2\n成交量 %3").arg(time.toString("yyyy-MM-dd HH:mm")).arg(bar.m_fClose, 0, 'f', 2).arg(bar.m_nVolume);
			}
			else
			{
				strText = QString("%1\n开 %2  高 %3\n低 %4  收 %5\n成交量 %6")
							  .arg(time.toString(IsMinuteCurveMode(m_mode) ? "yyyy-MM-dd HH:mm" : "yyyy-MM-dd"))
							  .arg(bar.m_fOpen, 0, 'f', 2)
							  .arg(bar.m_fHigh, 0, 'f', 2)
							  .arg(bar.m_fLow, 0, 'f', 2)
							  .arg(bar.m_fClose, 0, 'f', 2)
							  .arg(bar.m_nVolume);
			}
			QPalette palette = qApp->palette();
			QColor background = palette.color(QPalette::Base);
			background.setAlpha(238);
			QwtText text(strText);
			text.setBackgroundBrush(background);
			text.setBorderPen(QPen(palette.color(QPalette::Mid)));
			text.setColor(palette.color(QPalette::Text));
			return text;
		}

	private:
		std::shared_ptr<const std::vector<CMarketBar>> m_bars;
		CurveMode m_mode{ CurveMode::Day };
	};

	QVector<QPointF> MovingAveragePoints(const std::shared_ptr<const std::vector<CMarketBar>>& fullBars, const std::shared_ptr<const std::vector<CMarketBar>>& visibleBars, std::size_t nPeriod)
	{
		QVector<QPointF> points;
		if ((nullptr == fullBars) || (nullptr == visibleBars) || visibleBars->empty() || (0 == nPeriod))
		{
			return points;
		}
		auto vIter = std::lower_bound(fullBars->begin(), fullBars->end(), visibleBars->front().m_nBeginTime, [](const CMarketBar& bar, std::int64_t nTime)
		{
			return bar.m_nBeginTime < nTime;
		});
		std::size_t nStart = static_cast<std::size_t>(std::distance(fullBars->begin(), vIter));
		std::vector<double> prefix(fullBars->size() + 1, 0.0);
		std::size_t nSize = fullBars->size();
		for (std::size_t nIndex = 0; nSize > nIndex; ++nIndex)
		{
			prefix[nIndex + 1] = prefix[nIndex] + (*fullBars)[nIndex].m_fClose;
		}
		std::size_t nVisibleSize = visibleBars->size();
		points.reserve(static_cast<int>(nVisibleSize));
		for (std::size_t nIndex = 0; nVisibleSize > nIndex; ++nIndex)
		{
			std::size_t nFullIndex = nStart + nIndex;
			double fValue = InvalidValue;
			if (nPeriod <= nFullIndex + 1)
			{
				fValue = (prefix[nFullIndex + 1] - prefix[nFullIndex + 1 - nPeriod]) / static_cast<double>(nPeriod);
			}
			points.emplace_back(static_cast<double>(nIndex), fValue);
		}
		return points;
	}
} // namespace

CUICurve::CUICurve(QWidget* pParent) : QWidget(pParent), m_ui(std::make_unique<Ui::CUICurveClass>()), m_bars(std::make_shared<const std::vector<CMarketBar>>())
{
	m_ui->setupUi(this);
	InitializePlots();
}

CUICurve::~CUICurve() = default;

void CUICurve::SetMode(CurveMode mode)
{
	if (mode == m_mode)
	{
		return;
	}
	m_mode = mode;
	Refresh();
}

CurveMode CUICurve::GetMode() const noexcept
{
	return m_mode;
}

bool CUICurve::IsMinuteMode() const noexcept
{
	return IsMinuteCurveMode(m_mode);
}

void CUICurve::SetIntradayDays(int nDays)
{
	m_nIntradayDays = (std::clamp)(nDays, 1, 10);
	if (CurveMode::Intraday == m_mode)
	{
		Refresh();
	}
}

int CUICurve::AvailableTradingDays() const
{
	int nDays = 0;
	QDate previous;
	for (const CMarketBar& bar : *m_bars)
	{
		QDate date = BarDate(bar);
		if (date != previous)
		{
			previous = date;
			++nDays;
		}
	}
	return nDays;
}

void CUICurve::SetReferencePrice(double fPrice)
{
	m_fReferencePrice = fPrice;
	if (CurveMode::Intraday == m_mode)
	{
		Refresh();
	}
}

void CUICurve::SetBars(const std::vector<CMarketBar>& bars)
{
	std::shared_ptr<std::vector<CMarketBar>> values = std::make_shared<std::vector<CMarketBar>>(bars);
	if ((CurveMode::Intraday == m_mode) || IsMinuteCurveMode(m_mode))
	{
		values->erase(std::remove_if(values->begin(), values->end(), [](const CMarketBar& bar)
		{
			return 0 > SessionMinute(BarDateTime(bar).time());
		}),
			values->end());
	}
	std::sort(values->begin(), values->end(), [](const CMarketBar& left, const CMarketBar& right)
	{
		return left.m_nBeginTime < right.m_nBeginTime;
	});
	m_bars = std::move(values);
	Refresh();
}

void CUICurve::Clear()
{
	m_bars = std::make_shared<const std::vector<CMarketBar>>();
	Refresh();
}

void CUICurve::InitializePlots()
{
	m_pPricePlot = m_ui->pricePlot;
	m_pVolumePlot = m_ui->volumePlot;
	m_pPricePlot->canvas()->setObjectName("curveCanvas");
	m_pVolumePlot->canvas()->setObjectName("curveCanvas");
	m_pVolumePlot->setAxisMaxMajor(QwtAxis::YLeft, 2);
	m_pVolumePlot->setAxisMaxMinor(QwtAxis::YLeft, 0);
	m_pTradingCurve = new CFinancialTradingCurve("K线");
	m_pTradingCurve->setMinSymbolWidth(3.0);
	m_pTradingCurve->setMaxSymbolWidth(9.0);
	m_pTradingCurve->attach(m_pPricePlot);
	m_pIntradayCurve = new QwtPlotCurve("价格");
	m_pIntradayCurve->setPen(QPen(QColor("#3187f5"), 1.35));
	m_pIntradayCurve->attach(m_pPricePlot);
	m_pAverageCurve = new QwtPlotCurve("均价");
	m_pAverageCurve->setPen(QPen(QColor("#e5a900"), 1.15));
	m_pAverageCurve->attach(m_pPricePlot);
	QwtPlotCurve** movingAverages[] = { &m_pMovingAverage5, &m_pMovingAverage10, &m_pMovingAverage20, &m_pMovingAverage60 };
	int periods[] = { 5, 10, 20, 60 };
	QColor colors[] = { QColor("#48a9e6"), QColor("#e5a900"), QColor("#ec49d8"), QColor("#16ad64") };
	for (int nIndex = 0; 4 > nIndex; ++nIndex)
	{
		*movingAverages[nIndex] = new QwtPlotCurve(QString("MA%1").arg(periods[nIndex]));
		(*movingAverages[nIndex])->setPen(QPen(colors[nIndex], 1.0));
		(*movingAverages[nIndex])->attach(m_pPricePlot);
	}
	CColoredHistogram* pVolume = new CColoredHistogram();
	pVolume->setTitle("成交量");
	pVolume->attach(m_pVolumePlot);
	m_pVolumeCurve = pVolume;
	m_pPriceGrid = new QwtPlotGrid();
	m_pPriceGrid->enableX(true);
	m_pPriceGrid->enableY(true);
	m_pPriceGrid->attach(m_pPricePlot);
	m_pVolumeGrid = new QwtPlotGrid();
	m_pVolumeGrid->enableX(true);
	m_pVolumeGrid->enableY(true);
	m_pVolumeGrid->attach(m_pVolumePlot);
	m_pPricePlot->setAxisScaleDraw(QwtAxis::XBottom, new CTimeScaleDraw());
	m_pVolumePlot->setAxisScaleDraw(QwtAxis::XBottom, new CTimeScaleDraw());
	m_pPricePlot->setAxisScaleDraw(QwtAxis::YLeft, new CPriceScaleDraw());
	m_pPricePlot->setAxisScaleDraw(QwtAxis::YRight, new CPercentScaleDraw());
	m_pPricePlot->enableAxis(QwtAxis::XBottom, true);
	m_pVolumePlot->enableAxis(QwtAxis::XBottom, false);
	m_pHighMarker = new QwtPlotMarker();
	m_pHighMarker->setZ(90.0);
	m_pHighMarker->setVisible(false);
	m_pHighMarker->attach(m_pPricePlot);
	m_pLowMarker = new QwtPlotMarker();
	m_pLowMarker->setZ(90.0);
	m_pLowMarker->setVisible(false);
	m_pLowMarker->attach(m_pPricePlot);
	m_pPicker = new CCurvePicker(m_pPricePlot->canvas());
	m_pPicker->setRubberBand(QwtPicker::NoRubberBand);
	m_pPriceVerticalMarker = new QwtPlotMarker();
	m_pPriceVerticalMarker->setLineStyle(QwtPlotMarker::VLine);
	m_pPriceVerticalMarker->setZ(100.0);
	m_pPriceVerticalMarker->setVisible(false);
	m_pPriceVerticalMarker->attach(m_pPricePlot);
	m_pPriceHorizontalMarker = new QwtPlotMarker();
	m_pPriceHorizontalMarker->setLineStyle(QwtPlotMarker::HLine);
	m_pPriceHorizontalMarker->setZ(100.0);
	m_pPriceHorizontalMarker->setVisible(false);
	m_pPriceHorizontalMarker->attach(m_pPricePlot);
	m_pVolumeMarker = new QwtPlotMarker();
	m_pVolumeMarker->setLineStyle(QwtPlotMarker::VLine);
	m_pVolumeMarker->setZ(100.0);
	m_pVolumeMarker->setVisible(false);
	m_pVolumeMarker->attach(m_pVolumePlot);
	m_pPricePlot->canvas()->setMouseTracking(true);
	m_pVolumePlot->canvas()->setMouseTracking(true);
	m_pPricePlot->canvas()->installEventFilter(this);
	m_pVolumePlot->canvas()->installEventFilter(this);
	QwtPlotPanner* pPanner = new QwtPlotPanner(m_pPricePlot->canvas());
	pPanner->setOrientations(Qt::Horizontal);
	QwtPlotMagnifier* pMagnifier = new QwtPlotMagnifier(m_pPricePlot->canvas());
	pMagnifier->setAxisEnabled(QwtAxis::YLeft, false);
	pMagnifier->setAxisEnabled(QwtAxis::YRight, false);
	pMagnifier->setAxisEnabled(QwtAxis::XTop, false);
	connect(pPanner, &QwtPlotPanner::panned, this, [this]()
	{
		if (m_bSynchronizingScale)
		{
			return;
		}
		QScopedValueRollback<bool> guard(m_bSynchronizingScale, true);
		m_pVolumePlot->setAxisScaleDiv(QwtAxis::XBottom, m_pPricePlot->axisScaleDiv(QwtAxis::XBottom));
		m_pVolumePlot->replot();
	});
	connect(m_pPricePlot->axisWidget(QwtAxis::XBottom), &QwtScaleWidget::scaleDivChanged, this, [this]()
	{
		if (m_bSynchronizingScale)
		{
			return;
		}
		QScopedValueRollback<bool> guard(m_bSynchronizingScale, true);
		m_pVolumePlot->setAxisScaleDiv(QwtAxis::XBottom, m_pPricePlot->axisScaleDiv(QwtAxis::XBottom));
		m_pVolumePlot->replot();
	});
	ApplyPalette();
}

void CUICurve::changeEvent(QEvent* pEvent)
{
	QWidget::changeEvent(pEvent);
	if ((QEvent::PaletteChange == pEvent->type()) || (QEvent::StyleChange == pEvent->type()) || (QEvent::FontChange == pEvent->type()))
	{
		SchedulePaletteUpdate();
	}
}

bool CUICurve::eventFilter(QObject* pObject, QEvent* pEvent)
{
	bool bPriceCanvas = pObject == m_pPricePlot->canvas();
	bool bVolumeCanvas = pObject == m_pVolumePlot->canvas();
	if (bPriceCanvas || bVolumeCanvas)
	{
		if (QEvent::Leave == pEvent->type())
		{
			HideCrosshair();
		}
		else if ((QEvent::MouseMove == pEvent->type()) && (nullptr != m_hoverBars) && !m_hoverBars->empty())
		{
			QMouseEvent* pMouse = static_cast<QMouseEvent*>(pEvent);
			QwtPlot* pPlot = bPriceCanvas ? m_pPricePlot : m_pVolumePlot;
			int nIndex = (std::clamp)(qRound(pPlot->invTransform(QwtAxis::XBottom, pMouse->position().x())), 0, static_cast<int>(m_hoverBars->size()) - 1);
			double fPrice = bPriceCanvas ? m_pPricePlot->invTransform(QwtAxis::YLeft, pMouse->position().y()) : (*m_hoverBars)[static_cast<std::size_t>(nIndex)].m_fClose;
			bool bIndexChanged = nIndex != m_nHoverIndex;
			if (bIndexChanged || (0.0001 < std::abs(fPrice - m_fHoverPrice)))
			{
				m_nHoverIndex = nIndex;
				m_fHoverPrice = fPrice;
				m_pPriceVerticalMarker->setXValue(static_cast<double>(nIndex));
				m_pPriceHorizontalMarker->setYValue(fPrice);
				m_pVolumeMarker->setXValue(static_cast<double>(nIndex));
				m_pPriceVerticalMarker->setVisible(true);
				m_pPriceHorizontalMarker->setVisible(true);
				m_pVolumeMarker->setVisible(true);
				m_pPricePlot->replot();
				if (bIndexChanged)
				{
					m_pVolumePlot->replot();
				}
			}
		}
	}
	return QWidget::eventFilter(pObject, pEvent);
}

void CUICurve::HideCrosshair()
{
	if (-1 == m_nHoverIndex)
	{
		return;
	}
	m_nHoverIndex = -1;
	m_pPriceVerticalMarker->setVisible(false);
	m_pPriceHorizontalMarker->setVisible(false);
	m_pVolumeMarker->setVisible(false);
	m_pPricePlot->replot();
	m_pVolumePlot->replot();
}

void CUICurve::SchedulePaletteUpdate()
{
	if (m_bPaletteUpdatePending)
	{
		return;
	}
	m_bPaletteUpdatePending = true;
	QTimer::singleShot(0, this, [this]()
	{
		m_bPaletteUpdatePending = false;
		ApplyPalette();
	});
}

void CUICurve::ApplyPalette()
{
	if (m_bApplyingPalette || (nullptr == m_pPricePlot) || (nullptr == m_pVolumePlot))
	{
		return;
	}
	QScopedValueRollback<bool> paletteGuard(m_bApplyingPalette, true);
	QPalette palette = qApp->palette();
	QColor background = palette.color(QPalette::Base);
	QColor foreground = palette.color(QPalette::Text);
	bool bDark = 128 > background.lightness();
	QColor rising = bDark ? QColor("#ff4141") : RisingColor;
	QColor falling = bDark ? QColor("#00d9df") : QColor("#159b35");
	static_cast<CFinancialTradingCurve*>(m_pTradingCurve)->SetColors(rising, falling);
	static_cast<CColoredHistogram*>(m_pVolumeCurve)->SetColors(rising, falling);
	QColor priceColors[] = {
		bDark ? QColor("#f4f4f4") : QColor("#0088ad"),
		bDark ? QColor("#ffe900") : QColor("#a00980"),
		QColor("#ea42dc"),
		bDark ? QColor("#16dc38") : QColor("#157733")
	};
	QwtPlotCurve* priceAverages[] = { m_pMovingAverage5, m_pMovingAverage10, m_pMovingAverage20, m_pMovingAverage60 };
	for (int nIndex = 0; 4 > nIndex; ++nIndex)
	{
		priceAverages[nIndex]->setPen(QPen(priceColors[nIndex], 1.15));
	}
	QwtText highLabel = m_pHighMarker->label();
	QwtText lowLabel = m_pLowMarker->label();
	highLabel.setColor(foreground);
	lowLabel.setColor(foreground);
	m_pHighMarker->setLabel(highLabel);
	m_pLowMarker->setLabel(lowLabel);
	QColor grid = palette.color(QPalette::Mid);
	grid.setAlpha(95);
	QColor crosshair = foreground;
	crosshair.setAlpha(160);
	QPen crosshairPen(crosshair, 1.0, Qt::DashLine);
	m_pPriceVerticalMarker->setLinePen(crosshairPen);
	m_pPriceHorizontalMarker->setLinePen(crosshairPen);
	m_pVolumeMarker->setLinePen(crosshairPen);
	m_pTradingCurve->setSymbolPen(foreground, 1.0);
	m_pPriceGrid->setMajorPen(QPen(grid, 0.8, Qt::DashLine));
	m_pVolumeGrid->setMajorPen(QPen(grid, 0.8, Qt::DashLine));
	QString strStyle = UIStyle::Load(":/styles/curve.qss").arg(palette.color(QPalette::Window).name(), foreground.name(), background.name());
	if (styleSheet() != strStyle)
	{
		setStyleSheet(strStyle);
	}
	for (QwtPlot* pPlot : { m_pPricePlot, m_pVolumePlot })
	{
		pPlot->setPalette(palette);
		pPlot->setAutoFillBackground(true);
		pPlot->setCanvasBackground(background);
		for (int nAxis = 0; QwtAxis::AxisPositions > nAxis; ++nAxis)
		{
			QwtScaleWidget* pAxis = pPlot->axisWidget(nAxis);
			pAxis->setPalette(palette);
			pAxis->setAutoFillBackground(true);
		}
		pPlot->replot();
	}
	AlignLeftAxes();
}

void CUICurve::AlignLeftAxes()
{
	QwtScaleDraw* pPriceScale = m_pPricePlot->axisScaleDraw(QwtAxis::YLeft);
	QwtScaleDraw* pVolumeScale = m_pVolumePlot->axisScaleDraw(QwtAxis::YLeft);
	m_pPricePlot->updateAxes();
	m_pVolumePlot->updateAxes();
	pPriceScale->setMinimumExtent(0.0);
	pVolumeScale->setMinimumExtent(0.0);
	double fExtent = (std::max)(pPriceScale->extent(m_pPricePlot->axisWidget(QwtAxis::YLeft)->font()), pVolumeScale->extent(m_pVolumePlot->axisWidget(QwtAxis::YLeft)->font()));
	pPriceScale->setMinimumExtent(fExtent);
	pVolumeScale->setMinimumExtent(fExtent);
	m_pPricePlot->updateLayout();
	m_pVolumePlot->updateLayout();
	m_pPricePlot->replot();
	m_pVolumePlot->replot();
}

void CUICurve::Refresh()
{
	std::shared_ptr<const std::vector<CMarketBar>> fullBars = ModeBars();
	std::shared_ptr<const std::vector<CMarketBar>> bars = DisplayBars();
	m_hoverBars.reset();
	HideCrosshair();
	m_hoverBars = bars;
	bool bIntraday = CurveMode::Intraday == m_mode;
	m_pTradingCurve->setVisible(!bIntraday);
	m_pIntradayCurve->setVisible(bIntraday);
	m_pAverageCurve->setVisible(bIntraday);
	for (QwtPlotCurve* pCurve : { m_pMovingAverage5, m_pMovingAverage10, m_pMovingAverage20, m_pMovingAverage60 })
	{
		pCurve->setVisible(!bIntraday);
	}
	m_pTradingCurve->setSamples(new CBarCandleSeriesData(bars));
	QVector<QPointF> pricePoints;
	QVector<QPointF> averagePoints;
	double fWeightedValue = 0.0;
	double fTotalVolume = 0.0;
	QDate previousDate;
	std::int64_t nPreviousTime = 0;
	std::size_t nSize = bars->size();
	for (std::size_t nIndex = 0; nSize > nIndex; ++nIndex)
	{
		const CMarketBar& bar = (*bars)[nIndex];
		QDate date = BarDate(bar);
		bool bGap = date != previousDate || ((0 != nPreviousTime) && (5 * 60 * 1000 < bar.m_nBeginTime - nPreviousTime));
		if ((0 != nIndex) && bGap)
		{
			pricePoints.emplace_back(static_cast<double>(nIndex) - 0.5, InvalidValue);
			averagePoints.emplace_back(static_cast<double>(nIndex) - 0.5, InvalidValue);
		}
		if (date != previousDate)
		{
			fWeightedValue = 0.0;
			fTotalVolume = 0.0;
		}
		fWeightedValue += bar.m_fClose * static_cast<double>(bar.m_nVolume);
		fTotalVolume += static_cast<double>(bar.m_nVolume);
		pricePoints.emplace_back(static_cast<double>(nIndex), bar.m_fClose);
		averagePoints.emplace_back(static_cast<double>(nIndex), 0.0 < fTotalVolume ? fWeightedValue / fTotalVolume : bar.m_fClose);
		previousDate = date;
		nPreviousTime = bar.m_nBeginTime;
	}
	m_pIntradayCurve->setSamples(pricePoints);
	m_pAverageCurve->setSamples(averagePoints);
	m_pMovingAverage5->setSamples(MovingAveragePoints(fullBars, bars, 5));
	m_pMovingAverage10->setSamples(MovingAveragePoints(fullBars, bars, 10));
	m_pMovingAverage20->setSamples(MovingAveragePoints(fullBars, bars, 20));
	m_pMovingAverage60->setSamples(MovingAveragePoints(fullBars, bars, 60));
	static_cast<CColoredHistogram*>(m_pVolumeCurve)->SetBars(bars, bIntraday, m_fReferencePrice);
	m_pVolumeCurve->setSamples(new CBarVolumeSeriesData(bars));
	static_cast<CTimeScaleDraw*>(m_pPricePlot->axisScaleDraw(QwtAxis::XBottom))->SetBars(bars, m_mode, m_nIntradayDays);
	static_cast<CTimeScaleDraw*>(m_pVolumePlot->axisScaleDraw(QwtAxis::XBottom))->SetBars(bars, m_mode, m_nIntradayDays);
	std::vector<std::pair<int, QString>> intradayLabels;
	if (bIntraday && (1 == m_nIntradayDays) && !bars->empty())
	{
		QTime times[] = { QTime(9, 30), QTime(10, 30), QTime(13, 0), QTime(14, 0), QTime(15, 0) };
		QString labels[] = { "09:30", "10:30", "11:30", "14:00", "15:00" };
		for (int nTarget = 0; 5 > nTarget; ++nTarget)
		{
			int nBestIndex = -1;
			int nBestDistance = 6 * 60;
			std::size_t nBarCount = bars->size();
			for (std::size_t nIndex = 0; nBarCount > nIndex; ++nIndex)
			{
				int nDistance = std::abs(times[nTarget].secsTo(BarDateTime((*bars)[nIndex]).time()));
				if (nDistance < nBestDistance)
				{
					nBestDistance = nDistance;
					nBestIndex = static_cast<int>(nIndex);
				}
			}
			if ((0 <= nBestIndex) && (intradayLabels.empty() || (intradayLabels.back().first != nBestIndex)))
			{
				intradayLabels.emplace_back(nBestIndex, labels[nTarget]);
			}
		}
	}
	static_cast<CTimeScaleDraw*>(m_pPricePlot->axisScaleDraw(QwtAxis::XBottom))->SetFixedLabels(intradayLabels);
	static_cast<CTimeScaleDraw*>(m_pVolumePlot->axisScaleDraw(QwtAxis::XBottom))->SetFixedLabels(intradayLabels);
	static_cast<CPriceScaleDraw*>(m_pPricePlot->axisScaleDraw(QwtAxis::YLeft))->SetReferencePrice(m_fReferencePrice, bIntraday);
	static_cast<CCurvePicker*>(m_pPicker)->SetBars(bars, m_mode);
	m_pHighMarker->setVisible(false);
	m_pLowMarker->setVisible(false);
	m_pPricePlot->enableAxis(QwtAxis::YLeft, true);
	m_pPricePlot->enableAxis(QwtAxis::YRight, bIntraday);
	m_pVolumePlot->enableAxis(QwtAxis::YLeft, true);
	m_pVolumePlot->enableAxis(QwtAxis::YRight, false);
	if (bars->empty())
	{
		m_pPricePlot->setAxisAutoScale(QwtAxis::YLeft);
		m_pVolumePlot->setAxisAutoScale(QwtAxis::YLeft);
	}
	else
	{
		m_pPricePlot->setAxisAutoScale(QwtAxis::YLeft, false);
		m_pVolumePlot->setAxisAutoScale(QwtAxis::YLeft, false);
		double fMaximumX = static_cast<double>(bars->size()) - 0.5;
		double fMinimumX = -0.5;
		std::size_t nVisibleStart = 0;
		if (!bIntraday && (60 < bars->size()))
		{
			nVisibleStart = bars->size() - 60;
			fMinimumX = static_cast<double>(nVisibleStart) - 0.5;
		}
		m_pPricePlot->setAxisScale(QwtAxis::XBottom, fMinimumX, fMaximumX);
		m_pVolumePlot->setAxisScale(QwtAxis::XBottom, fMinimumX, fMaximumX);
		if (1U < intradayLabels.size())
		{
			QList<double> majorTicks;
			for (const auto& value : intradayLabels)
			{
				majorTicks.append(static_cast<double>(value.first));
			}
			QwtScaleDiv scale(fMinimumX, fMaximumX, QList<double>{}, QList<double>{}, majorTicks);
			m_pPricePlot->setAxisScaleDiv(QwtAxis::XBottom, scale);
			m_pVolumePlot->setAxisScaleDiv(QwtAxis::XBottom, scale);
		}
		std::int64_t nMaximumVolume = 0;
		std::size_t nSize = bars->size();
		for (std::size_t nIndex = nVisibleStart; nSize > nIndex; ++nIndex)
		{
			nMaximumVolume = (std::max)(nMaximumVolume, (*bars)[nIndex].m_nVolume);
		}
		double fVolumeStep = NiceStep(static_cast<double>(nMaximumVolume) / 4.0);
		double fMaximumVolume = (std::max)(fVolumeStep, std::ceil(static_cast<double>(nMaximumVolume) / fVolumeStep) * fVolumeStep);
		m_pVolumePlot->setAxisScale(QwtAxis::YLeft, 0.0, fMaximumVolume, fVolumeStep);
		if (bIntraday)
		{
			double fReference = 0.0 < m_fReferencePrice ? m_fReferencePrice : bars->front().m_fOpen;
			static_cast<CPriceScaleDraw*>(m_pPricePlot->axisScaleDraw(QwtAxis::YLeft))->SetReferencePrice(fReference, true);
			double fDifference = 0.01;
			for (const CMarketBar& bar : *bars)
			{
				fDifference = (std::max)(fDifference, std::abs(bar.m_fHigh - fReference));
				fDifference = (std::max)(fDifference, std::abs(bar.m_fLow - fReference));
			}
			fDifference *= 1.08;
			m_pPricePlot->setAxisScale(QwtAxis::YLeft, fReference - fDifference, fReference + fDifference);
			double fPercent = 0.0 < fReference ? 100.0 * fDifference / fReference : 0.0;
			m_pPricePlot->setAxisScale(QwtAxis::YRight, -fPercent, fPercent);
		}
		else
		{
			double fMinimumPrice = (std::numeric_limits<double>::max)();
			double fMaximumPrice = (std::numeric_limits<double>::lowest)();
			std::size_t nLowIndex = nVisibleStart;
			std::size_t nHighIndex = nVisibleStart;
			for (std::size_t nIndex = nVisibleStart; nSize > nIndex; ++nIndex)
			{
				const CMarketBar& bar = (*bars)[nIndex];
				if (0.0 < bar.m_fLow)
				{
					if (bar.m_fLow < fMinimumPrice)
					{
						fMinimumPrice = bar.m_fLow;
						nLowIndex = nIndex;
					}
					if (bar.m_fHigh > fMaximumPrice)
					{
						fMaximumPrice = bar.m_fHigh;
						nHighIndex = nIndex;
					}
				}
			}
			if (fMinimumPrice <= fMaximumPrice)
			{
				double fPadding = (std::max)(0.01, (fMaximumPrice - fMinimumPrice) * 0.13);
				m_pPricePlot->setAxisScale(QwtAxis::YLeft, fMinimumPrice - fPadding, fMaximumPrice + fPadding);
				QwtText highLabel(QString("← %1").arg(fMaximumPrice, 0, 'f', 2));
				QwtText lowLabel(QString("← %1").arg(fMinimumPrice, 0, 'f', 2));
				highLabel.setColor(qApp->palette().color(QPalette::Text));
				lowLabel.setColor(qApp->palette().color(QPalette::Text));
				m_pHighMarker->setValue(static_cast<double>(nHighIndex), fMaximumPrice);
				m_pHighMarker->setLabel(highLabel);
				m_pHighMarker->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
				m_pHighMarker->setVisible(true);
				m_pLowMarker->setValue(static_cast<double>(nLowIndex), fMinimumPrice);
				m_pLowMarker->setLabel(lowLabel);
				m_pLowMarker->setLabelAlignment(Qt::AlignRight | Qt::AlignBottom);
				m_pLowMarker->setVisible(true);
			}
		}
	}
	AlignLeftAxes();
}

std::shared_ptr<const std::vector<CMarketBar>> CUICurve::ModeBars() const
{
	if ((CurveMode::Intraday == m_mode) || (CurveMode::Day == m_mode))
	{
		return m_bars;
	}
	if (CurveMode::Week == m_mode)
	{
		return AggregateBars(false);
	}
	if (CurveMode::Month == m_mode)
	{
		return AggregateBars(true);
	}
	return AggregateMinuteBars(MinutePeriod(m_mode));
}

std::shared_ptr<const std::vector<CMarketBar>> CUICurve::DisplayBars() const
{
	std::shared_ptr<const std::vector<CMarketBar>> bars = ModeBars();
	if (bars->empty())
	{
		return bars;
	}
	if (CurveMode::Intraday == m_mode)
	{
		int nDays = 0;
		QDate date;
		auto vIter = bars->rbegin();
		for (; bars->rend() != vIter; ++vIter)
		{
			QDate current = BarDate(*vIter);
			if (current != date)
			{
				date = current;
				++nDays;
				if (m_nIntradayDays < nDays)
				{
					break;
				}
			}
		}
		return std::make_shared<const std::vector<CMarketBar>>(vIter.base(), bars->end());
	}
	return bars;
}

std::shared_ptr<const std::vector<CMarketBar>> CUICurve::AggregateBars(bool bMonthly) const
{
	std::shared_ptr<std::vector<CMarketBar>> values = std::make_shared<std::vector<CMarketBar>>();
	for (const CMarketBar& bar : *m_bars)
	{
		QDate date = BarDate(bar);
		int nYear = date.year();
		int nPeriod = bMonthly ? date.month() : date.weekNumber(&nYear);
		bool bNewPeriod = values->empty();
		if (!bNewPeriod)
		{
			QDate previousDate = BarDate(values->back());
			int nPreviousYear = previousDate.year();
			int nPreviousPeriod = bMonthly ? previousDate.month() : previousDate.weekNumber(&nPreviousYear);
			bNewPeriod = (nYear != nPreviousYear) || (nPeriod != nPreviousPeriod);
		}
		if (bNewPeriod)
		{
			values->emplace_back(bar);
			continue;
		}
		CMarketBar& value = values->back();
		value.m_fHigh = (std::max)(value.m_fHigh, bar.m_fHigh);
		value.m_fLow = (std::min)(value.m_fLow, bar.m_fLow);
		value.m_fClose = bar.m_fClose;
		value.m_nVolume += bar.m_nVolume;
		value.m_nTurnover += bar.m_nTurnover;
	}
	return values;
}

std::shared_ptr<const std::vector<CMarketBar>> CUICurve::AggregateMinuteBars(int nMinutes) const
{
	std::shared_ptr<std::vector<CMarketBar>> values = std::make_shared<std::vector<CMarketBar>>();
	if (0 >= nMinutes)
	{
		return values;
	}
	QDate currentDate;
	int nCurrentBucket = -1;
	for (const CMarketBar& bar : *m_bars)
	{
		QDateTime dateTime = BarDateTime(bar);
		int nMinute = SessionMinute(dateTime.time());
		if (0 > nMinute)
		{
			continue;
		}
		int nBucket = nMinute / nMinutes;
		bool bNewBucket = values->empty() || (dateTime.date() != currentDate) || (nBucket != nCurrentBucket);
		if (bNewBucket)
		{
			values->emplace_back(bar);
			currentDate = dateTime.date();
			nCurrentBucket = nBucket;
			continue;
		}
		CMarketBar& value = values->back();
		value.m_fHigh = (std::max)(value.m_fHigh, bar.m_fHigh);
		value.m_fLow = (std::min)(value.m_fLow, bar.m_fLow);
		value.m_fClose = bar.m_fClose;
		value.m_nVolume += bar.m_nVolume;
		value.m_nTurnover += bar.m_nTurnover;
	}
	return values;
}
