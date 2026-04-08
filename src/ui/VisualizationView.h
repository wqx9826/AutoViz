#pragma once

#include <QColor>
#include <QGraphicsView>
#include <QPoint>

// Basic 2D visualization viewport backed by QGraphicsView.
// It currently provides a background, grid, zooming and panning.
class VisualizationView : public QGraphicsView
{
public:
    explicit VisualizationView(QWidget* parent = nullptr);

    void resetView();
    void setBackgroundColor(const QColor& color);
    void setGridVisible(bool visible);
    void setWelcomeState(int loadedPackageCount, int compiledPackageCount);

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void setupScene();

    QColor m_backgroundColor = QColor("#1f2630");
    QColor m_minorGridColor = QColor("#2b3644");
    QColor m_majorGridColor = QColor("#3d4d60");
    bool m_gridVisible = true;
    bool m_isPanning = false;
    QPoint m_lastMousePosition;
    qreal m_zoomFactor = 1.0;
    int m_loadedPackageCount = 0;
    int m_compiledPackageCount = 0;
};
