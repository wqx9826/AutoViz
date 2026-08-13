#pragma once

#include <QFrame>
#include <QWidget>

class QDoubleSpinBox;
class QVariantAnimation;

namespace autoviz::ui::playback {

class PillRateSlider final : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal handlePosition READ handlePosition WRITE setHandlePosition)
    Q_PROPERTY(qreal handleScale READ handleScale WRITE setHandleScale)

public:
    explicit PillRateSlider(QWidget* parent = nullptr);

    void setIndex(int index, bool animate = false);
    int index() const;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void indexChanged(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    qreal handlePosition() const;
    void setHandlePosition(qreal position);
    qreal handleScale() const;
    void setHandleScale(qreal scale);
    qreal positionForIndex(int index) const;
    int indexForPosition(qreal position) const;
    void setVisualScale(qreal target);
    void snapToIndex(int index, bool animate);

    int m_index = 2;
    qreal m_handlePosition = 0.4;
    qreal m_handleScale = 1.0;
    bool m_pressed = false;
    QVariantAnimation* m_snapAnimation = nullptr;
    QVariantAnimation* m_scaleAnimation = nullptr;
};

class PlaybackSpeedWidget final : public QFrame {
    Q_OBJECT
public:
    explicit PlaybackSpeedWidget(QWidget* parent=nullptr);
    void setRate(double rate);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void rateChanged(double rate);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int nearestStep(double rate) const;
    PillRateSlider* m_slider = nullptr;
    QDoubleSpinBox* m_spin = nullptr;
};

}  // namespace autoviz::ui::playback
