#include "ui/VisualizationView.h"

#include <cmath>

#include <QGraphicsScene>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScrollBar>
#include <QWheelEvent>

#include "ui/theme/UiScaleManager.h"
#include "ui/theme/UiThemeManager.h"

namespace {
constexpr double kMinorGridSpacing = 1.0;
constexpr double kMajorGridSpacing = 5.0;
constexpr qreal kZoomStep = 1.15;
constexpr qreal kMinZoom = 0.02;
constexpr qreal kMaxZoom = 500.0;
}

VisualizationView::VisualizationView(QWidget* parent)
    : QGraphicsView(parent)
{
    setupScene();
}

void VisualizationView::resetView()
{
    m_autoFitEnabled = true;
    resetTransform();
    m_zoomFactor = 1.0;
    centerOn(0.0, 0.0);
    viewport()->update();
}

void VisualizationView::fitToRegion(const QRectF& region)
{
    if (!region.isValid() || region.isEmpty()) {
        return;
    }

    m_lastFitRegion = region;
    if (!m_autoFitEnabled) {
        return;
    }
    resetTransform();
    fitInView(region, Qt::KeepAspectRatio);
    m_zoomFactor = transform().m11();
    centerOn(region.center());
}

void VisualizationView::enableAutoFit(bool enabled)
{
    m_autoFitEnabled = enabled;
    if (enabled && m_lastFitRegion.isValid() && !m_lastFitRegion.isEmpty()) {
        fitToRegion(m_lastFitRegion);
    }
}

bool VisualizationView::autoFitEnabled() const
{
    return m_autoFitEnabled;
}

void VisualizationView::setBackgroundColor(const QColor& color)
{
    m_backgroundColor = color;
    viewport()->update();
}

void VisualizationView::setGridVisible(bool visible)
{
    m_gridVisible = visible;
    viewport()->update();
}

void VisualizationView::applyTheme(const autoviz::ui::theme::ThemePalette& palette)
{
    m_backgroundColor = palette.dark ? QColor("#17212B") : QColor("#F2F6FA");
    m_minorGridColor = palette.dark ? QColor("#223141") : QColor("#DDE6EF");
    m_majorGridColor = palette.dark ? QColor("#35506A") : QColor("#B8C7D6");

    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    if (m_overlayLabel != nullptr) {
        m_overlayLabel->setStyleSheet(
            QStringLiteral("QLabel {"
                           "padding: %1px %2px;"
                           "border: 1px solid %3;"
                           "border-radius: %4px;"
                           "background-color: rgba(%5, %6, %7, %8);"
                           "color: %9;"
                           "}")
                .arg(scale.scaled(4))
                .arg(scale.scaled(7))
                .arg(palette.overlayBorder.name())
                .arg(scale.scaled(5))
                .arg(palette.overlayBackground.red())
                .arg(palette.overlayBackground.green())
                .arg(palette.overlayBackground.blue())
                .arg(palette.overlayBackground.alpha())
                .arg(palette.overlayText.name()));
    }
    if (m_verticalStatusLabel != nullptr) {
        m_verticalStatusLabel->setStyleSheet(
            QStringLiteral("QLabel {"
                           "padding: %1px %2px;"
                           "border: 1px solid %3;"
                           "border-radius: %4px;"
                           "background-color: %5;"
                           "color: %6;"
                           "}")
                .arg(scale.scaled(5))
                .arg(scale.scaled(7))
                .arg(palette.accent.name())
                .arg(scale.scaled(5))
                .arg(palette.dark ? QStringLiteral("rgba(8, 47, 73, 225)") : QStringLiteral("rgba(232, 247, 255, 235)"))
                .arg(palette.dark ? QStringLiteral("#E0F2FE") : QStringLiteral("#075985")));
    }
    viewport()->update();
}

void VisualizationView::setOverlayMessage(const QString& text)
{
    if (m_overlayLabel == nullptr) {
        return;
    }

    m_overlayLabel->setVisible(!text.trimmed().isEmpty());
    if (!text.trimmed().isEmpty()) {
        m_overlayLabel->setText(text);
        updateOverlayGeometry();
        m_overlayLabel->raise();
    }
}

void VisualizationView::setVerticalStatusMessage(const QString& text)
{
    if (m_verticalStatusLabel == nullptr) {
        return;
    }

    const bool hasText = !text.trimmed().isEmpty();
    m_verticalStatusLabel->setVisible(hasText);
    if (hasText) {
        m_verticalStatusLabel->setText(text);
        updateOverlayGeometry();
        m_verticalStatusLabel->raise();
    }
}

void VisualizationView::setVerticalProfileFrame(const VerticalProfileFrame& frame)
{
    m_verticalProfileFrame = frame;
    viewport()->update();
}

void VisualizationView::clearVerticalProfileFrame()
{
    if (!m_verticalProfileFrame.visible) {
        return;
    }
    m_verticalProfileFrame = VerticalProfileFrame{};
    viewport()->update();
}

double VisualizationView::minorGridSpacingMeters() const
{
    return kMinorGridSpacing;
}

void VisualizationView::drawForeground(QPainter* painter, const QRectF& rect)
{
    QGraphicsView::drawForeground(painter, rect);
    if (!m_verticalProfileFrame.visible) {
        return;
    }

    painter->save();
    painter->resetTransform();
    drawVerticalProfileForeground(painter);
    painter->restore();
}

double VisualizationView::majorGridSpacingMeters() const
{
    return kMajorGridSpacing;
}

void VisualizationView::drawBackground(QPainter* painter, const QRectF& rect)
{
    painter->fillRect(rect, m_backgroundColor);

    if (!m_gridVisible) {
        return;
    }

    QPen minorPen(m_minorGridColor);
    QPen majorPen(m_majorGridColor);
    minorPen.setWidthF(0.0);
    majorPen.setWidthF(0.0);

    const int left = static_cast<int>(std::floor(rect.left()));
    const int right = static_cast<int>(std::ceil(rect.right()));
    const int top = static_cast<int>(std::floor(rect.top()));
    const int bottom = static_cast<int>(std::ceil(rect.bottom()));

    const int minorSpacing = static_cast<int>(kMinorGridSpacing);
    const int majorSpacing = static_cast<int>(kMajorGridSpacing);

    for (int x = left - (left % minorSpacing); x <= right; x += minorSpacing) {
        painter->setPen((x % majorSpacing == 0) ? majorPen : minorPen);
        painter->drawLine(QLineF(x, top, x, bottom));
    }

    for (int y = top - (top % minorSpacing); y <= bottom; y += minorSpacing) {
        painter->setPen((y % majorSpacing == 0) ? majorPen : minorPen);
        painter->drawLine(QLineF(left, y, right, y));
    }

    painter->setPen(QPen(QColor("#78909c"), 0.0));
    painter->drawLine(QLineF(left, 0, right, 0));
    painter->drawLine(QLineF(0, top, 0, bottom));
}

void VisualizationView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    updateOverlayGeometry();
    if (m_autoFitEnabled && m_lastFitRegion.isValid() && !m_lastFitRegion.isEmpty()) {
        fitToRegion(m_lastFitRegion);
    }
}

void VisualizationView::wheelEvent(QWheelEvent* event)
{
    m_autoFitEnabled = false;
    const QPoint angleDelta = event->angleDelta();
    const QPoint pixelDelta = event->pixelDelta();
    const bool zoomIn = angleDelta.y() > 0 || pixelDelta.y() > 0;
    const bool zoomOut = angleDelta.y() < 0 || pixelDelta.y() < 0;
    if (!zoomIn && !zoomOut) {
        event->ignore();
        return;
    }

    const qreal currentScale = std::abs(transform().m11());
    const qreal step = zoomIn ? kZoomStep : (1.0 / kZoomStep);
    const qreal nextZoom = currentScale * step;
    if (nextZoom < kMinZoom || nextZoom > kMaxZoom) {
        event->accept();
        return;
    }

    m_zoomFactor = nextZoom;
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    scale(step, step);
    event->accept();
}

void VisualizationView::mousePressEvent(QMouseEvent* event)
{
    if (isPanButton(event->button())) {
        m_autoFitEnabled = false;
        m_isPanning = true;
        m_lastMousePosition = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void VisualizationView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isPanning) {
        const QPoint delta = event->pos() - m_lastMousePosition;
        m_lastMousePosition = event->pos();

        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
}

void VisualizationView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_isPanning && isPanButton(event->button())) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void VisualizationView::drawVerticalProfileForeground(QPainter* painter) const
{
    const auto palette = autoviz::ui::theme::UiThemeManager::instance().effectivePalette();
    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    const QRect viewportRect = viewport()->rect();
    if (viewportRect.width() <= 0 || viewportRect.height() <= 0) {
        return;
    }

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    painter->fillRect(viewportRect, palette.window);

    const int margin = scale.scaled(18);
    const int topReserve = qMax(scale.scaled(128), viewportRect.height() / 5);
    const int leftReserve = scale.scaled(74);
    const int rightReserve = scale.scaled(38);
    const int bottomReserve = scale.scaled(70);
    QRectF plotRect(viewportRect.left() + margin + leftReserve,
                    viewportRect.top() + topReserve,
                    viewportRect.width() - margin * 2 - leftReserve - rightReserve,
                    viewportRect.height() - topReserve - bottomReserve - margin);
    if (plotRect.width() < scale.scaled(260) || plotRect.height() < scale.scaled(160)) {
        plotRect = QRectF(viewportRect.left() + scale.scaled(72),
                          viewportRect.top() + scale.scaled(110),
                          qMax(scale.scaled(260), viewportRect.width() - scale.scaled(120)),
                          qMax(scale.scaled(160), viewportRect.height() - scale.scaled(180)));
    }

    double xMax = 10.0;
    bool hasDepthRange = false;
    double depthMin = 0.0;
    double depthMax = 5.0;
    for (const auto& sample : m_verticalProfileFrame.samples) {
        xMax = std::max(xMax, sample.elapsedSec);
        if (sample.hasDepth) {
            depthMin = hasDepthRange ? std::min(depthMin, sample.depth) : sample.depth;
            depthMax = hasDepthRange ? std::max(depthMax, sample.depth) : sample.depth;
            hasDepthRange = true;
        }
        if (sample.hasTargetDepth) {
            depthMin = hasDepthRange ? std::min(depthMin, sample.targetDepth) : sample.targetDepth;
            depthMax = hasDepthRange ? std::max(depthMax, sample.targetDepth) : sample.targetDepth;
            hasDepthRange = true;
        }
    }
    if (m_verticalProfileFrame.hasStartDepth) {
        depthMin = hasDepthRange ? std::min(depthMin, m_verticalProfileFrame.startDepth) : m_verticalProfileFrame.startDepth;
        depthMax = hasDepthRange ? std::max(depthMax, m_verticalProfileFrame.startDepth) : m_verticalProfileFrame.startDepth;
        hasDepthRange = true;
    }
    if (!hasDepthRange) {
        depthMin = 0.0;
        depthMax = 5.0;
    }

    const double depthSpan = std::max(1.0, depthMax - depthMin);
    const double depthMargin = std::max(0.6, depthSpan * 0.15);
    const double topDepth = std::max(0.0, depthMin - depthMargin);
    const double bottomDepth = depthMax + depthMargin;
    const auto timeToX = [&plotRect, xMax](double elapsedSec) {
        const double ratio = xMax > 0.0 ? std::clamp(elapsedSec / xMax, 0.0, 1.0) : 0.0;
        return plotRect.left() + ratio * plotRect.width();
    };
    const auto depthToY = [&plotRect, topDepth, bottomDepth](double depth) {
        const double ratio = bottomDepth > topDepth ? std::clamp((depth - topDepth) / (bottomDepth - topDepth), 0.0, 1.0) : 0.0;
        return plotRect.top() + ratio * plotRect.height();
    };

    const QColor depthColor = palette.dark ? QColor("#4CC3FF") : QColor("#0B63CE");
    const QColor targetColor = palette.dark ? QColor("#FFB45C") : QColor("#C45A0A");
    const QColor startColor = palette.dark ? QColor("#A78BFA") : QColor("#7C3AED");
    const QColor emergencyColor = palette.dark ? QColor("#FF5C7A") : QColor("#DC2626");
    const QColor gridColor = palette.dark ? QColor(60, 68, 78, 130) : QColor(209, 220, 232, 170);
    const QColor majorGridColor = palette.dark ? QColor(82, 94, 108, 150) : QColor(174, 190, 208, 190);

    const QFont titleFont = scale.font(scale.fontSizeTitle() + 5, QFont::DemiBold);
    const QFont summaryFont = scale.font(scale.fontSizeNormal(), QFont::Normal);
    QFont axisFont = scale.font(scale.fontSizeSmall(), QFont::Normal);
    QFont valueFont = scale.font(scale.fontSizeSmall(), QFont::DemiBold);
    valueFont.setFamily(QStringLiteral("JetBrains Mono"));
    valueFont.setStyleHint(QFont::Monospace);

    QRectF titleRect(viewportRect.left() + scale.scaled(260),
                     viewportRect.top() + scale.scaled(24),
                     qMax(0, viewportRect.width() - scale.scaled(520)),
                     scale.scaled(34));
    painter->setFont(titleFont);
    painter->setPen(palette.text);
    painter->drawText(titleRect, Qt::AlignCenter, tr("垂向动作段深度趋势图"));

    const QString summary = tr("模式：%1    已执行：%2 s    深度：%3 m -> 目标 %4 m")
                                .arg(m_verticalProfileFrame.modeText.isEmpty() ? tr("--") : m_verticalProfileFrame.modeText,
                                     QString::number(m_verticalProfileFrame.elapsedSec, 'f', 1),
                                     m_verticalProfileFrame.hasCurrentDepth ? QString::number(m_verticalProfileFrame.currentDepth, 'f', 2) : QStringLiteral("--"),
                                     m_verticalProfileFrame.hasTargetDepth ? QString::number(m_verticalProfileFrame.targetDepth, 'f', 2) : QStringLiteral("--"));
    QRectF summaryRect(titleRect.left(),
                       titleRect.bottom() + scale.scaled(8),
                       titleRect.width(),
                       scale.scaled(24));
    painter->setFont(summaryFont);
    painter->setPen(palette.mutedText);
    painter->drawText(summaryRect, Qt::AlignCenter, summary);

    painter->setPen(QPen(palette.border, 1.0));
    painter->setBrush(palette.plotBackground);
    painter->drawRect(plotRect);

    painter->setFont(axisFont);
    const double xStep = xMax <= 20.0 ? 2.0 : (xMax <= 60.0 ? 10.0 : 30.0);
    for (double t = 0.0; t <= xMax + 1.0e-6; t += xStep) {
        const double x = timeToX(t);
        painter->setPen(QPen(qFuzzyIsNull(t) ? majorGridColor : gridColor, 1.0, Qt::DashLine));
        painter->drawLine(QPointF(x, plotRect.top()), QPointF(x, plotRect.bottom()));
        painter->setPen(palette.mutedText);
        painter->drawText(QRectF(x - scale.scaled(26), plotRect.bottom() + scale.scaled(6), scale.scaled(52), scale.scaled(20)),
                          Qt::AlignCenter,
                          QString::number(t, 'f', 0));
    }

    const double yStep = depthSpan > 20.0 ? 5.0 : (depthSpan > 8.0 ? 2.0 : 1.0);
    const int firstDepthTick = static_cast<int>(std::floor(topDepth / yStep));
    for (double depth = static_cast<double>(firstDepthTick) * yStep; depth <= bottomDepth + 1.0e-6; depth += yStep) {
        if (depth < topDepth - 1.0e-6) {
            continue;
        }
        const double y = depthToY(depth);
        painter->setPen(QPen(std::abs(std::fmod(depth, 5.0)) < 1.0e-6 ? majorGridColor : gridColor, 1.0, Qt::DashLine));
        painter->drawLine(QPointF(plotRect.left(), y), QPointF(plotRect.right(), y));
        painter->setPen(palette.mutedText);
        painter->drawText(QRectF(plotRect.left() - scale.scaled(60), y - scale.scaled(10), scale.scaled(52), scale.scaled(20)),
                          Qt::AlignRight | Qt::AlignVCenter,
                          QStringLiteral("%1m").arg(QString::number(depth, 'f', 0)));
    }

    painter->setPen(QPen(palette.axis, 1.2));
    painter->drawLine(plotRect.bottomLeft(), plotRect.topLeft());
    painter->drawLine(plotRect.bottomLeft(), plotRect.bottomRight());

    painter->setPen(palette.text);
    painter->drawText(QRectF(plotRect.left(), plotRect.bottom() + scale.scaled(28), plotRect.width(), scale.scaled(24)),
                      Qt::AlignCenter,
                      tr("elapsed time / s"));
    painter->save();
    painter->translate(plotRect.left() - scale.scaled(42), plotRect.center().y());
    painter->rotate(-90);
    painter->drawText(QRectF(-plotRect.height() * 0.5, -scale.scaled(12), plotRect.height(), scale.scaled(24)),
                      Qt::AlignCenter,
                      tr("depth / m"));
    painter->restore();

    const VerticalProfileSample* latestDepthSample = nullptr;
    const VerticalProfileSample* latestTargetSample = nullptr;
    for (int index = m_verticalProfileFrame.samples.size() - 1; index >= 0; --index) {
        const auto& sample = m_verticalProfileFrame.samples.at(index);
        if (latestDepthSample == nullptr && sample.hasDepth) {
            latestDepthSample = &sample;
        }
        if (latestTargetSample == nullptr && sample.hasTargetDepth) {
            latestTargetSample = &sample;
        }
        if (latestDepthSample != nullptr && latestTargetSample != nullptr) {
            break;
        }
    }

    if (latestTargetSample != nullptr) {
        QPen targetPen(targetColor, 1.3, Qt::DashLine);
        targetPen.setDashPattern({6.0, 4.0});
        painter->setPen(targetPen);
        const double y = depthToY(latestTargetSample->targetDepth);
        painter->drawLine(QPointF(plotRect.left(), y), QPointF(plotRect.right(), y));
    }
    if (m_verticalProfileFrame.hasStartDepth) {
        QPen startPen(startColor, 1.3, Qt::DashLine);
        startPen.setDashPattern({5.0, 4.0});
        painter->setPen(startPen);
        const double y = depthToY(m_verticalProfileFrame.startDepth);
        painter->drawLine(QPointF(plotRect.left(), y), QPointF(plotRect.right(), y));
    }

    for (const double eventTime : m_verticalProfileFrame.emergencyEventTimes) {
        const double x = timeToX(eventTime);
        painter->setPen(QPen(emergencyColor, 1.2, Qt::DashLine));
        painter->drawLine(QPointF(x, plotRect.top()), QPointF(x, plotRect.bottom()));
        painter->setPen(emergencyColor);
        painter->drawText(QPointF(x + scale.scaled(4), plotRect.top() + scale.scaled(18)), tr("急停"));
    }

    QPainterPath path;
    bool hasPath = false;
    for (const auto& sample : m_verticalProfileFrame.samples) {
        if (!sample.hasDepth) {
            continue;
        }
        const QPointF point(timeToX(sample.elapsedSec), depthToY(sample.depth));
        if (!hasPath) {
            path.moveTo(point);
            hasPath = true;
        } else {
            path.lineTo(point);
        }
    }
    if (hasPath) {
        painter->setPen(QPen(depthColor, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path);
    } else {
        painter->setFont(scale.font(scale.fontSizeTitle(), QFont::DemiBold));
        painter->setPen(palette.warnText);
        painter->drawText(plotRect, Qt::AlignCenter, tr("等待 depth 数据"));
        painter->setFont(axisFont);
    }

    if (latestDepthSample != nullptr) {
        const QPointF point(timeToX(latestDepthSample->elapsedSec), depthToY(latestDepthSample->depth));
        painter->setPen(QPen(depthColor, 2.0));
        painter->setBrush(palette.plotBackground);
        painter->drawEllipse(point, scale.scaled(6), scale.scaled(6));
        painter->setBrush(depthColor);
        painter->drawEllipse(point, scale.scaled(3), scale.scaled(3));
    }

    const QSize legendSize(scale.scaled(150), scale.scaled(72));
    QRectF legendRect(plotRect.right() - legendSize.width() - scale.scaled(10),
                      plotRect.top() - legendSize.height() - scale.scaled(18),
                      legendSize.width(),
                      legendSize.height());
    if (legendRect.top() < summaryRect.bottom() + scale.scaled(8)) {
        legendRect.moveTop(plotRect.top() + scale.scaled(10));
    }
    painter->setPen(QPen(palette.border, 1.0));
    painter->setBrush(palette.panel);
    painter->drawRect(legendRect);

    const auto drawLegendRow = [&](int row, const QColor& color, const QString& text, Qt::PenStyle style) {
        const qreal y = legendRect.top() + scale.scaled(18 + row * 18);
        painter->setPen(QPen(color, 1.6, style));
        painter->drawLine(QPointF(legendRect.left() + scale.scaled(14), y),
                          QPointF(legendRect.left() + scale.scaled(44), y));
        painter->setPen(palette.text);
        painter->drawText(QRectF(legendRect.left() + scale.scaled(54),
                                 y - scale.scaled(9),
                                 legendRect.width() - scale.scaled(62),
                                 scale.scaled(18)),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          text);
    };
    drawLegendRow(0, depthColor, tr("当前深度"), Qt::SolidLine);
    drawLegendRow(1, targetColor, tr("目标深度"), Qt::DashLine);
    drawLegendRow(2, startColor, tr("起始深度"), Qt::DashLine);

    painter->setPen(palette.mutedText);
    painter->drawText(QRectF(viewportRect.left() + margin,
                             viewportRect.bottom() - scale.scaled(32),
                             viewportRect.width() - margin * 2,
                             scale.scaled(20)),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      tr("Depth 向下为正，t=0 为动作开始时刻"));
}

void VisualizationView::setupScene()
{
    auto* graphicsScene = new QGraphicsScene(this);
    graphicsScene->setSceneRect(-120.0, -120.0, 240.0, 240.0);
    setScene(graphicsScene);

    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::TextAntialiasing, true);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setInteractive(false);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();

    m_overlayLabel = new QLabel(viewport());
    m_overlayLabel->setWordWrap(true);
    m_overlayLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_overlayLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_overlayLabel->setFont(scale.font(scale.fontSizeSmall()));
    m_overlayLabel->hide();

    m_verticalStatusLabel = new QLabel(viewport());
    m_verticalStatusLabel->setWordWrap(true);
    m_verticalStatusLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_verticalStatusLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_verticalStatusLabel->setFont(scale.font(scale.fontSizeSmall()));
    m_verticalStatusLabel->hide();

    applyTheme(autoviz::ui::theme::UiThemeManager::instance().effectivePalette());
}

void VisualizationView::updateOverlayGeometry()
{
    if (m_overlayLabel == nullptr) {
        return;
    }

    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    const int maxWidth = qMin(scale.scaled(280), qMax(scale.scaled(190), viewport()->width() / 4));
    m_overlayLabel->setFixedWidth(maxWidth);
    m_overlayLabel->adjustSize();
    m_overlayLabel->move(scale.scaled(8), scale.scaled(8));

    if (m_verticalStatusLabel != nullptr) {
        const int verticalMaxWidth = qMin(qMax(scale.scaled(210), viewport()->width() / 4), scale.scaled(300));
        m_verticalStatusLabel->setFixedWidth(verticalMaxWidth);
        m_verticalStatusLabel->adjustSize();
        m_verticalStatusLabel->move(qMax(scale.scaled(8), viewport()->width() - verticalMaxWidth - scale.scaled(8)), scale.scaled(8));
    }
}

bool VisualizationView::isPanButton(Qt::MouseButton button) const
{
    return button == Qt::LeftButton || button == Qt::MiddleButton || button == Qt::RightButton;
}
