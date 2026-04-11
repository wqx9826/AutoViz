#include "ui/VisualizationView.h"

#include <cmath>

#include <QGraphicsScene>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QWheelEvent>

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

void VisualizationView::setOverlayMessage(const QString& text)
{
    m_overlayMessage = text;
    viewport()->update();
}

double VisualizationView::minorGridSpacingMeters() const
{
    return kMinorGridSpacing;
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

void VisualizationView::drawForeground(QPainter* painter, const QRectF& rect)
{
    Q_UNUSED(rect);

    if (m_overlayMessage.isEmpty()) {
        return;
    }

    painter->save();
    painter->resetTransform();

    const QRect viewportRect = viewport()->rect();
    QRectF cardRect(viewportRect.width() - 320.0, 18.0, 300.0, 88.0);

    QLinearGradient gradient(cardRect.topLeft(), cardRect.bottomRight());
    gradient.setColorAt(0.0, QColor(18, 24, 31, 210));
    gradient.setColorAt(1.0, QColor(29, 40, 52, 220));
    painter->setPen(QPen(QColor("#5d738b"), 1.0));
    painter->setBrush(gradient);
    painter->drawRoundedRect(cardRect, 12.0, 12.0);

    QFont font = painter->font();
    font.setPointSize(10);
    painter->setFont(font);
    painter->setPen(QColor("#dde7ef"));
    painter->drawText(cardRect.adjusted(14, 12, -14, -12),
                      Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                      m_overlayMessage + QStringLiteral("\n网格：细格 1m / 粗格 5m"));

    painter->restore();
}

void VisualizationView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
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
}

bool VisualizationView::isPanButton(Qt::MouseButton button) const
{
    return button == Qt::LeftButton || button == Qt::MiddleButton || button == Qt::RightButton;
}
