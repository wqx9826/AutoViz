#pragma once

#include <QColor>
#include <QGraphicsView>
#include <QPoint>
#include <QRectF>
#include <QString>

class VisualizationView : public QGraphicsView
{
public:
    explicit VisualizationView(QWidget* parent = nullptr);

    void resetView();
    void fitToRegion(const QRectF& region);
    void enableAutoFit(bool enabled);
    bool autoFitEnabled() const;
    void setBackgroundColor(const QColor& color);
    void setGridVisible(bool visible);
    void setOverlayMessage(const QString& text);
    double minorGridSpacingMeters() const;
    double majorGridSpacingMeters() const;

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void setupScene();
    bool isPanButton(Qt::MouseButton button) const;

    QColor m_backgroundColor = QColor("#17212b");
    QColor m_minorGridColor = QColor("#223141");
    QColor m_majorGridColor = QColor("#35506a");
    bool m_gridVisible = true;
    bool m_isPanning = false;
    QPoint m_lastMousePosition;
    qreal m_zoomFactor = 1.0;
    QString m_overlayMessage;
    QRectF m_lastFitRegion;
    bool m_autoFitEnabled = true;
};
