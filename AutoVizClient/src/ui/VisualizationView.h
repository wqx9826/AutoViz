#pragma once

#include <QColor>
#include <QGraphicsView>
#include <QPoint>
#include <QRectF>
#include <QString>
#include <QVector>

class QLabel;
class QPainter;

namespace autoviz::ui::theme {
struct ThemePalette;
}

class VisualizationView : public QGraphicsView
{
    Q_OBJECT
public:
    struct VerticalProfileSample {
        double elapsedSec = 0.0;
        double depth = 0.0;
        bool hasDepth = false;
        double targetDepth = 0.0;
        bool hasTargetDepth = false;
        bool emergencyStop = false;
    };

    struct VerticalProfileFrame {
        bool visible = false;
        bool frozen = false;
        bool emergencyStop = false;
        QString modeText;
        QString quantityText = QStringLiteral("深度");
        double elapsedSec = 0.0;
        double currentDepth = 0.0;
        bool hasCurrentDepth = false;
        double targetDepth = 0.0;
        bool hasTargetDepth = false;
        double startDepth = 0.0;
        bool hasStartDepth = false;
        QVector<VerticalProfileSample> samples;
        QVector<double> emergencyEventTimes;
    };

    explicit VisualizationView(QWidget* parent = nullptr);

    void resetView();
    void fitToRegion(const QRectF& region);
    void enableAutoFit(bool enabled);
    bool autoFitEnabled() const;
    void setBackgroundColor(const QColor& color);
    void setGridVisible(bool visible);
    void applyTheme(const autoviz::ui::theme::ThemePalette& palette);
    void setOverlayMessage(const QString& text);
    void setVerticalStatusMessage(const QString& text);
    void setVerticalProfileFrame(const VerticalProfileFrame& frame);
    void clearVerticalProfileFrame();
    double minorGridSpacingMeters() const;
    double majorGridSpacingMeters() const;
    void setPlaybackSpeedVisible(bool visible);
    void setPlaybackRate(double rate);

signals:
    void playbackRateChanged(double rate);
    void manualNavigationStarted();

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
    double adaptiveMinorGridSpacingMeters() const;
    void updateOverlayGeometry();
    void drawVerticalProfileForeground(QPainter* painter) const;

    QColor m_backgroundColor = QColor("#17212b");
    QColor m_minorGridColor = QColor("#223141");
    QColor m_majorGridColor = QColor("#35506a");
    bool m_gridVisible = true;
    bool m_isPanning = false;
    QPoint m_lastMousePosition;
    qreal m_zoomFactor = 1.0;
    QLabel* m_overlayLabel = nullptr;
    QLabel* m_verticalStatusLabel = nullptr;
    QWidget* m_playbackSpeedWidget = nullptr;
    VerticalProfileFrame m_verticalProfileFrame;
    QRectF m_lastFitRegion;
    bool m_autoFitEnabled = true;
};
