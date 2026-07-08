#include "ui/VisualizationView.h"

#include <cmath>

#include <QGraphicsScene>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
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
