#include "ui/VisualizationView.h"

#include <cmath>

#include <QGraphicsScene>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

namespace {
constexpr int kMinorGridSpacing = 20;
constexpr int kMajorGridSpacing = 100;
constexpr qreal kZoomStep = 1.15;
constexpr qreal kMinZoom = 0.2;
constexpr qreal kMaxZoom = 10.0;
}

VisualizationView::VisualizationView(QWidget* parent)
    : QGraphicsView(parent)
{
    setupScene();
}

void VisualizationView::resetView()
{
    resetTransform();
    m_zoomFactor = 1.0;
    centerOn(0.0, 0.0);
    viewport()->update();
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

void VisualizationView::setWelcomeState(int loadedPackageCount, int compiledPackageCount)
{
    m_loadedPackageCount = loadedPackageCount;
    m_compiledPackageCount = compiledPackageCount;
    viewport()->update();
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

    for (int x = left - (left % kMinorGridSpacing); x <= right; x += kMinorGridSpacing) {
        painter->setPen((x % kMajorGridSpacing == 0) ? majorPen : minorPen);
        painter->drawLine(QLineF(x, top, x, bottom));
    }

    for (int y = top - (top % kMinorGridSpacing); y <= bottom; y += kMinorGridSpacing) {
        painter->setPen((y % kMajorGridSpacing == 0) ? majorPen : minorPen);
        painter->drawLine(QLineF(left, y, right, y));
    }

    painter->setPen(QPen(QColor("#8aa2b6"), 0.0));
    painter->drawLine(QLineF(left, 0, right, 0));
    painter->drawLine(QLineF(0, top, 0, bottom));
}

void VisualizationView::drawForeground(QPainter* painter, const QRectF& rect)
{
    Q_UNUSED(rect);

    painter->save();
    painter->resetTransform();

    const QRect viewportRect = viewport()->rect();
    QRectF cardRect(
        viewportRect.width() * 0.5 - 230.0,
        viewportRect.height() * 0.5 - 80.0,
        460.0,
        160.0);

    QLinearGradient gradient(cardRect.topLeft(), cardRect.bottomRight());
    gradient.setColorAt(0.0, QColor(18, 25, 34, 210));
    gradient.setColorAt(1.0, QColor(28, 39, 51, 220));
    painter->setPen(QPen(QColor("#4f6377"), 1.0));
    painter->setBrush(gradient);
    painter->drawRoundedRect(cardRect, 12.0, 12.0);

    QFont titleFont = painter->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    painter->setFont(titleFont);
    painter->setPen(QColor("#e8f1f7"));
    painter->drawText(cardRect.adjusted(24, 20, -24, 0), Qt::AlignTop | Qt::AlignHCenter, QStringLiteral("AutoViz 二维视图"));

    QFont bodyFont = painter->font();
    bodyFont.setPointSize(11);
    bodyFont.setBold(false);
    painter->setFont(bodyFont);
    painter->setPen(QColor("#c9d6e2"));

    QString bodyText;
    if (m_loadedPackageCount <= 0) {
        bodyText = QStringLiteral(
            "当前未加载 ROS 消息包\n请通过“文件 -> 加载 ROS 消息包”导入标准 ROS 消息包目录");
    } else if (m_compiledPackageCount <= 0) {
        bodyText = QStringLiteral("当前已加载消息包：%1 个\n请点击“编译消息包”完成类型构建")
                       .arg(m_loadedPackageCount);
    } else {
        bodyText = QStringLiteral("当前已编译消息包：%1 个\n可进入下一阶段的话题绑定与可视化配置")
                       .arg(m_compiledPackageCount);
    }
    painter->drawText(cardRect.adjusted(24, 58, -24, -20), Qt::AlignTop | Qt::AlignHCenter | Qt::TextWordWrap, bodyText);

    painter->restore();
}

void VisualizationView::wheelEvent(QWheelEvent* event)
{
    const qreal step = (event->angleDelta().y() > 0) ? kZoomStep : (1.0 / kZoomStep);
    const qreal nextZoom = m_zoomFactor * step;
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
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
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
    if (m_isPanning && (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton)) {
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
    graphicsScene->setSceneRect(-5000, -5000, 10000, 10000);
    setScene(graphicsScene);

    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::TextAntialiasing, true);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
}
