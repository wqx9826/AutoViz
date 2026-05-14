#include "ui/charts/PlotCardWidget.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QStringList>
#include <QToolTip>

#include "ui/charts/ControlPanelStyle.h"

namespace autoviz::ui::charts {

namespace {
constexpr double kRangeMinSpan = 1.0e-6;
constexpr int kDataTimeoutGapMs = 1200;

QString secondsLabel(qint64 milliseconds)
{
    return QStringLiteral("%1s").arg(milliseconds / 1000);
}

QVector<qint64> xTickOffsets(qint64 windowMs)
{
    if (windowMs <= 10000) {
        return {0, windowMs / 2, windowMs};
    }
    if (windowMs <= 30000) {
        return {0, 10000, 20000, windowMs};
    }
    return {0, windowMs / 4, windowMs / 2, windowMs * 3 / 4, windowMs};
}

QPen linePen(const QColor& color, double width)
{
    QPen pen(color, width);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    return pen;
}
}  // namespace

PlotCardWidget::PlotCardWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFont(style::font());
    setMinimumHeight(290);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
}

void PlotCardWidget::configure(const QString& title,
                               const QString& leftAxisTitle,
                               const QString& rightAxisTitle,
                               const QVector<SeriesConfig>& series)
{
    m_title = title;
    m_leftAxisTitle = leftAxisTitle;
    m_rightAxisTitle = rightAxisTitle;
    m_series = series;
    clearFrozenRange();
    update();
}

void PlotCardWidget::setSamples(const QVector<ControlDebugData>& samples,
                                const ControlDebugData& latest,
                                qint64 windowMs,
                                bool autoScale)
{
    m_samples = samples;
    m_latest = latest;
    m_windowMs = windowMs;
    if (m_autoScale != autoScale) {
        clearFrozenRange();
    }
    m_autoScale = autoScale;
    update();
}

void PlotCardWidget::clearFrozenRange()
{
    m_frozenLeftRange = AxisRange{};
    m_frozenRightRange = AxisRange{};
}

void PlotCardWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const auto geometry = geometryFor(rect());
    auto leftRange = expandedRange(calculateRange(AxisSide::Left));
    auto rightRange = expandedRange(calculateRange(AxisSide::Right));
    if (!m_autoScale) {
        if (leftRange.valid && !m_frozenLeftRange.valid) {
            m_frozenLeftRange = leftRange;
        }
        if (rightRange.valid && !m_frozenRightRange.valid) {
            m_frozenRightRange = rightRange;
        }
        if (m_frozenLeftRange.valid) {
            leftRange = m_frozenLeftRange;
        }
        if (m_frozenRightRange.valid) {
            rightRange = m_frozenRightRange;
        }
    }

    drawCard(painter, geometry);
    drawGridAndAxes(painter, geometry, leftRange, rightRange);
    drawSeries(painter, geometry, leftRange, rightRange);

    if (!anySeriesHasData()) {
        painter.setPen(QColor("#6B7280"));
        painter.drawText(geometry.plot, Qt::AlignCenter, tr("等待数据..."));
    }
}

void PlotCardWidget::mouseMoveEvent(QMouseEvent* event)
{
    const auto geometry = geometryFor(rect());
    if (!geometry.plot.contains(event->pos())) {
        QToolTip::hideText();
        return;
    }

    const qint64 latestTimestamp = m_samples.isEmpty() ? m_latest.elapsedMs : m_samples.last().elapsedMs;
    const qint64 windowStartMs = std::max<qint64>(0, latestTimestamp - m_windowMs);
    const auto leftRange = expandedRange(calculateRange(AxisSide::Left));
    const auto rightRange = expandedRange(calculateRange(AxisSide::Right));

    double bestDistance = std::numeric_limits<double>::max();
    QString bestText;
    for (const auto& series : m_series) {
        const auto range = series.axis == AxisSide::Right ? rightRange : leftRange;
        if (!range.valid) {
            continue;
        }
        for (const auto& sample : m_samples) {
            if (latestTimestamp - sample.elapsedMs > m_windowMs || !hasValue(sample, series.role)) {
                continue;
            }
            const QPointF point = toPlotPoint(geometry.plot, range, windowStartMs, sample.elapsedMs, valueOf(sample, series.role));
            const double distance = std::hypot(point.x() - event->pos().x(), point.y() - event->pos().y());
            if (distance < bestDistance) {
                bestDistance = distance;
                const double ageSeconds = static_cast<double>(sample.elapsedMs) / 1000.0;
                bestText = QStringLiteral("%1\n时间: %2 s\n数值: %3")
                               .arg(series.label, QString::number(ageSeconds, 'f', 1), formatValue(valueOf(sample, series.role), series.role));
            }
        }
    }

    if (bestDistance <= 12.0 && !bestText.isEmpty()) {
        QToolTip::showText(event->globalPos(), bestText, this);
    } else {
        QToolTip::hideText();
    }
}

void PlotCardWidget::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    QToolTip::hideText();
}

bool PlotCardWidget::hasValue(const ControlDebugData& data, ValueRole role)
{
    switch (role) {
    case ValueRole::CmdSpeed:
        return data.hasCmdSpeed;
    case ValueRole::FeedbackSpeed:
        return data.hasFeedbackSpeed;
    case ValueRole::SpeedError:
        return data.hasSpeedError;
    case ValueRole::CmdYaw:
        return data.hasCmdYaw;
    case ValueRole::FeedbackYaw:
        return data.hasFeedbackYaw;
    case ValueRole::YawError:
        return data.hasYawError;
    case ValueRole::LateralError:
        return data.hasLateralError;
    case ValueRole::PathYawError:
        return data.hasPathYawError;
    }
    return false;
}

double PlotCardWidget::valueOf(const ControlDebugData& data, ValueRole role)
{
    switch (role) {
    case ValueRole::CmdSpeed:
        return data.cmdSpeed;
    case ValueRole::FeedbackSpeed:
        return data.feedbackSpeed;
    case ValueRole::SpeedError:
        return data.speedError;
    case ValueRole::CmdYaw:
        return data.cmdYaw;
    case ValueRole::FeedbackYaw:
        return data.feedbackYaw;
    case ValueRole::YawError:
        return data.yawError;
    case ValueRole::LateralError:
        return data.lateralError;
    case ValueRole::PathYawError:
        return data.pathYawError;
    }
    return 0.0;
}

QString PlotCardWidget::valueSuffix(ValueRole role)
{
    switch (role) {
    case ValueRole::CmdSpeed:
    case ValueRole::FeedbackSpeed:
    case ValueRole::SpeedError:
        return QStringLiteral(" m/s");
    case ValueRole::LateralError:
        return QStringLiteral(" m");
    case ValueRole::CmdYaw:
    case ValueRole::FeedbackYaw:
    case ValueRole::YawError:
    case ValueRole::PathYawError:
        return QStringLiteral(" rad");
    }
    return {};
}

QString PlotCardWidget::formatValue(double value, ValueRole role)
{
    return QStringLiteral("%1%2").arg(QString::number(value, 'f', 3), valueSuffix(role));
}

qint64 PlotCardWidget::toRelativeTimeMs(qint64 elapsedMs, qint64 windowStartMs)
{
    return elapsedMs - windowStartMs;
}

PlotCardWidget::PlotGeometry PlotCardWidget::geometryFor(const QRect& bounds) const
{
    PlotGeometry geometry;
    geometry.card = bounds.adjusted(0, 0, -1, -1);
    const bool hasRightAxis = std::any_of(m_series.begin(), m_series.end(), [](const SeriesConfig& series) {
        return series.axis == AxisSide::Right;
    });
    geometry.plot = geometry.card.adjusted(62, 88, hasRightAxis ? -72 : -18, -34);
    return geometry;
}

PlotCardWidget::AxisRange PlotCardWidget::calculateRange(AxisSide axis) const
{
    AxisRange range;
    const qint64 latestTimestamp = m_samples.isEmpty() ? m_latest.elapsedMs : m_samples.last().elapsedMs;
    for (const auto& sample : m_samples) {
        if (latestTimestamp - sample.elapsedMs > m_windowMs) {
            continue;
        }
        for (const auto& series : m_series) {
            if (series.axis != axis || !hasValue(sample, series.role)) {
                continue;
            }
            const double value = valueOf(sample, series.role);
            if (!std::isfinite(value)) {
                continue;
            }
            if (range.valid) {
                range.min = std::min(range.min, value);
                range.max = std::max(range.max, value);
            } else {
                range.min = value;
                range.max = value;
                range.valid = true;
            }
        }
    }
    return range;
}

PlotCardWidget::AxisRange PlotCardWidget::expandedRange(AxisRange range) const
{
    if (!range.valid) {
        return range;
    }

    const double span = range.max - range.min;
    if (span < kRangeMinSpan) {
        const double center = range.min;
        const double halfSpan = std::max(0.5, std::abs(center) * 0.15);
        range.min = center - halfSpan;
        range.max = center + halfSpan;
    } else {
        const double margin = span * 0.1;
        range.min -= margin;
        range.max += margin;
    }
    return range;
}

double PlotCardWidget::relativeTimeToX(const QRectF& plot, qint64 relativeTimeMs) const
{
    const qint64 clamped = std::clamp(relativeTimeMs, qint64{0}, m_windowMs);
    const double xRatio = static_cast<double>(clamped) / static_cast<double>(m_windowMs);
    return plot.left() + xRatio * plot.width();
}

QPointF PlotCardWidget::toPlotPoint(const QRectF& plot,
                                    const AxisRange& range,
                                    qint64 windowStartMs,
                                    qint64 elapsedMs,
                                    double value) const
{
    const double x = relativeTimeToX(plot, toRelativeTimeMs(elapsedMs, windowStartMs));
    const double yRatio = (value - range.min) / (range.max - range.min);
    const double y = plot.bottom() - yRatio * plot.height();
    return QPointF(x, y);
}

QString PlotCardWidget::currentValuesText() const
{
    QStringList values;
    for (const auto& series : m_series) {
        if (hasValue(m_latest, series.role)) {
            values << QStringLiteral("%1 %2").arg(series.label, formatValue(valueOf(m_latest, series.role), series.role));
        }
        if (values.size() >= 3) {
            break;
        }
    }
    return values.join(QStringLiteral("    "));
}

void PlotCardWidget::drawCard(QPainter& painter, const PlotGeometry& geometry)
{
    painter.setPen(QPen(QColor("#E5E7EB"), 1.0));
    painter.setBrush(QColor("#FFFFFF"));
    painter.drawRoundedRect(geometry.card, 10, 10);

    painter.setFont(style::cardTitleFont());
    painter.setPen(QColor("#111827"));
    const QRectF titleRect(geometry.card.left() + 14, geometry.card.top() + 10, geometry.card.width() * 0.36, 20);
    painter.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, m_title);

    painter.setFont(style::currentValueFont());
    painter.setPen(QColor("#374151"));
    const QRectF valuesRect(geometry.card.left() + geometry.card.width() * 0.38,
                            geometry.card.top() + 8,
                            geometry.card.width() * 0.60 - 14,
                            34);
    painter.drawText(valuesRect, Qt::AlignRight | Qt::AlignTop | Qt::TextWordWrap, currentValuesText());

    drawLegend(painter, QRectF(geometry.card.left() + 14, geometry.card.top() + 44, geometry.card.width() - 28, 34));
}

void PlotCardWidget::drawGridAndAxes(QPainter& painter,
                                     const PlotGeometry& geometry,
                                     const AxisRange& leftRange,
                                     const AxisRange& rightRange)
{
    const auto plot = geometry.plot;
    const qint64 latestElapsedMs = m_samples.isEmpty() ? m_latest.elapsedMs : m_samples.last().elapsedMs;
    const qint64 windowStartMs = std::max<qint64>(0, latestElapsedMs - m_windowMs);

    painter.setBrush(QColor("#FAFAFA"));
    painter.setPen(Qt::NoPen);
    painter.drawRect(plot);

    painter.setFont(style::axisFont());

    painter.setPen(QPen(QColor("#E5E7EB"), 1.0));
    const auto xTicks = xTickOffsets(m_windowMs);
    for (const qint64 offsetMs : xTicks) {
        const double x = plot.left() + (static_cast<double>(offsetMs) / static_cast<double>(m_windowMs)) * plot.width();
        painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
    }

    constexpr int kYTicks = 5;
    if (leftRange.valid) {
        for (int index = 0; index < kYTicks; ++index) {
            const double ratio = static_cast<double>(index) / static_cast<double>(kYTicks - 1);
            const double value = leftRange.max - ratio * (leftRange.max - leftRange.min);
            const double y = plot.top() + ratio * plot.height();
            painter.setPen(QPen(QColor("#E5E7EB"), 1.0));
            painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
            painter.setPen(QColor("#374151"));
            painter.drawText(QRectF(geometry.card.left() + 8, y - 8, plot.left() - geometry.card.left() - 12, 16),
                             Qt::AlignRight | Qt::AlignVCenter,
                             QString::number(value, 'f', 2));
        }
    }

    if (rightRange.valid) {
        for (int index = 0; index < kYTicks; ++index) {
            const double ratio = static_cast<double>(index) / static_cast<double>(kYTicks - 1);
            const double value = rightRange.max - ratio * (rightRange.max - rightRange.min);
            const double y = plot.top() + ratio * plot.height();
            painter.setPen(QColor("#4B5563"));
            painter.drawText(QRectF(plot.right() + 6, y - 8, geometry.card.right() - plot.right() - 10, 16),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             QString::number(value, 'f', 2));
        }
    }

    painter.setPen(QPen(QColor("#9CA3AF"), 1.2));
    painter.drawLine(plot.bottomLeft(), plot.topLeft());
    painter.drawLine(plot.bottomLeft(), plot.bottomRight());
    if (rightRange.valid) {
        painter.drawLine(plot.bottomRight(), plot.topRight());
    }

    painter.setPen(QColor("#4B5563"));
    painter.drawText(QRectF(plot.left() - 52, plot.top() - 20, 50, 16), Qt::AlignRight | Qt::AlignVCenter, m_leftAxisTitle);
    if (rightRange.valid) {
        painter.drawText(QRectF(plot.right() + 6, plot.top() - 20, 66, 16), Qt::AlignLeft | Qt::AlignVCenter, m_rightAxisTitle);
    }

    painter.setPen(QColor("#6B7280"));
    for (const qint64 offsetMs : xTicks) {
        const double x = plot.left() + (static_cast<double>(offsetMs) / static_cast<double>(m_windowMs)) * plot.width();
        const QString label = secondsLabel(windowStartMs + offsetMs);
        QRectF labelRect(x - 28, plot.bottom() + 7, 56, 16);
        Qt::Alignment alignment = Qt::AlignCenter;
        if (offsetMs == 0) {
            labelRect.moveLeft(plot.left());
            alignment = Qt::AlignLeft | Qt::AlignVCenter;
        } else if (offsetMs == m_windowMs) {
            labelRect.moveRight(plot.right());
            alignment = Qt::AlignRight | Qt::AlignVCenter;
        }
        painter.drawText(labelRect, alignment, label);
    }
}

void PlotCardWidget::drawLegend(QPainter& painter, const QRectF& area)
{
    painter.setFont(style::legendFont());

    double x = area.left();
    double y = area.top();
    for (const auto& series : m_series) {
        const double itemWidth = painter.fontMetrics().horizontalAdvance(series.label) + 38;
        if (x > area.left() && x + itemWidth > area.right()) {
            x = area.left();
            y += 16;
        }
        painter.setPen(linePen(series.color, series.width));
        painter.drawLine(QPointF(x, y + 8), QPointF(x + 18, y + 8));
        painter.setPen(QColor("#374151"));
        painter.drawText(QRectF(x + 24, y, itemWidth, 16), Qt::AlignLeft | Qt::AlignVCenter, series.label);
        x += itemWidth;
    }
}

void PlotCardWidget::drawSeries(QPainter& painter,
                                const PlotGeometry& geometry,
                                const AxisRange& leftRange,
                                const AxisRange& rightRange)
{
    if (m_samples.isEmpty()) {
        return;
    }

    const qint64 latestElapsedMs = m_samples.last().elapsedMs;
    const qint64 windowStartMs = std::max<qint64>(0, latestElapsedMs - m_windowMs);
    painter.save();
    painter.setClipRect(geometry.plot.adjusted(0, -2, 1, 2));
    painter.setBrush(Qt::NoBrush);

    for (const auto& series : m_series) {
        const AxisRange range = series.axis == AxisSide::Right ? rightRange : leftRange;
        if (!range.valid) {
            continue;
        }

        QVector<QPolygonF> segments;
        QPolygonF current;
        qint64 previousElapsedMs = 0;
        int pointCount = 0;
        const int step = std::max(1, static_cast<int>(m_samples.size() / std::max(1.0, geometry.plot.width() * 2.0)));

        for (int index = 0; index < m_samples.size(); ++index) {
            const auto& sample = m_samples.at(index);
            if (latestElapsedMs - sample.elapsedMs > m_windowMs || !hasValue(sample, series.role)) {
                if (!current.isEmpty()) {
                    segments.push_back(current);
                    current.clear();
                }
                previousElapsedMs = 0;
                continue;
            }
            if (index % step != 0 && index + 1 < m_samples.size()) {
                continue;
            }
            if (previousElapsedMs != 0 && sample.elapsedMs - previousElapsedMs > kDataTimeoutGapMs && !current.isEmpty()) {
                segments.push_back(current);
                current.clear();
            }
            previousElapsedMs = sample.elapsedMs;
            current.append(toPlotPoint(geometry.plot, range, windowStartMs, sample.elapsedMs, valueOf(sample, series.role)));
            ++pointCount;
        }
        if (!current.isEmpty()) {
            segments.push_back(current);
        }

        painter.setPen(linePen(series.color, series.width));
        for (const auto& segment : segments) {
            if (segment.size() == 1) {
                painter.drawPoint(segment.first());
            } else {
                painter.drawPolyline(segment);
            }
        }
        if (pointCount > 0) {
            for (int index = m_samples.size() - 1; index >= 0; --index) {
                const auto& sample = m_samples.at(index);
                if (hasValue(sample, series.role)) {
                    const QPointF latestPoint = toPlotPoint(geometry.plot, range, windowStartMs, sample.elapsedMs, valueOf(sample, series.role));
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(series.color);
                    painter.drawEllipse(latestPoint, 3.0, 3.0);
                    painter.setBrush(Qt::NoBrush);
                    break;
                }
            }
        }
    }

    painter.restore();
}

bool PlotCardWidget::anySeriesHasData() const
{
    for (const auto& sample : m_samples) {
        for (const auto& series : m_series) {
            if (hasValue(sample, series.role)) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace autoviz::ui::charts
