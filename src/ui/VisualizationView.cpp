#include "ui/VisualizationView.h"

#include <cmath>

#include <QGraphicsScene>
#include <QLabel>
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

    m_overlayLabel = new QLabel(viewport());
    m_overlayLabel->setWordWrap(true);
    m_overlayLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_overlayLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_overlayLabel->setStyleSheet(
        "QLabel {"
        "padding: 12px 14px;"
        "border: 1px solid #5d738b;"
        "border-radius: 12px;"
        "background-color: rgba(18, 24, 31, 220);"
        "color: #dde7ef;"
        "font-size: 13px;"
        "}");
    m_overlayLabel->hide();
}

void VisualizationView::updateOverlayGeometry()
{
    if (m_overlayLabel == nullptr) {
        return;
    }

    const int maxWidth = qMin(360, qMax(220, viewport()->width() - 36));
    m_overlayLabel->setFixedWidth(maxWidth);
    m_overlayLabel->adjustSize();
    m_overlayLabel->move(18, 18);
}

bool VisualizationView::isPanButton(Qt::MouseButton button) const
{
    return button == Qt::LeftButton || button == Qt::MiddleButton || button == Qt::RightButton;
}
