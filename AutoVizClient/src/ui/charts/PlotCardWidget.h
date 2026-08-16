#pragma once

#include <QColor>
#include <QVector>
#include <QWidget>

#include "ui/charts/ControlDebugData.h"

namespace autoviz::ui::charts {

class PlotCardWidget : public QWidget {
public:
    enum class ValueRole {
        CmdSpeed,
        FeedbackSpeed,
        SpeedError,
        CmdYaw,
        FeedbackYaw,
        YawError,
        LateralError,
        PathYawError,
        CmdAngularVelocity,
        FeedbackAngularVelocity,
        AngularVelocityError
    };

    enum class AxisSide {
        Left,
        Right
    };

    struct SeriesConfig {
        QString label;
        QColor color;
        ValueRole role = ValueRole::CmdSpeed;
        AxisSide axis = AxisSide::Left;
        double width = 2.0;
    };

    explicit PlotCardWidget(QWidget* parent = nullptr);

    void configure(const QString& title,
                   const QString& leftAxisTitle,
                   const QString& rightAxisTitle,
                   const QVector<SeriesConfig>& series);
    void setSamples(const QVector<ControlDebugData>& samples,
                    const ControlDebugData& latest,
                    qint64 windowMs,
                    bool autoScale);
    void clearFrozenRange();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct AxisRange {
        bool valid = false;
        double min = 0.0;
        double max = 1.0;
    };

    struct PlotGeometry {
        QRectF card;
        QRectF plot;
    };

    static bool hasValue(const ControlDebugData& data, ValueRole role);
    static double valueOf(const ControlDebugData& data, ValueRole role);
    static QString valueSuffix(ValueRole role);
    static QString formatValue(double value, ValueRole role);
    static qint64 toRelativeTimeMs(qint64 elapsedMs, qint64 windowStartMs);

    PlotGeometry geometryFor(const QRect& bounds) const;
    AxisRange calculateRange(AxisSide axis) const;
    AxisRange expandedRange(AxisRange range) const;
    double relativeTimeToX(const QRectF& plot, qint64 relativeTimeMs) const;
    QPointF toPlotPoint(const QRectF& plot, const AxisRange& range, qint64 windowStartMs, qint64 elapsedMs, double value) const;
    QString currentValuesText() const;
    void drawCard(QPainter& painter, const PlotGeometry& geometry);
    void drawGridAndAxes(QPainter& painter, const PlotGeometry& geometry, const AxisRange& leftRange, const AxisRange& rightRange);
    void drawLegend(QPainter& painter, const QRectF& area);
    void drawSeries(QPainter& painter, const PlotGeometry& geometry, const AxisRange& leftRange, const AxisRange& rightRange);
    bool anySeriesHasData() const;

    QString m_title;
    QString m_leftAxisTitle;
    QString m_rightAxisTitle;
    QVector<SeriesConfig> m_series;
    QVector<ControlDebugData> m_samples;
    ControlDebugData m_latest;
    qint64 m_windowMs = 30000;
    bool m_autoScale = true;
    AxisRange m_frozenLeftRange;
    AxisRange m_frozenRightRange;
};

}  // namespace autoviz::ui::charts
