#include "ui/playback/PlaybackSpeedWidget.h"

#include <array>
#include <cmath>
#include <limits>

#include <QEasingCurve>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QSignalBlocker>
#include <QVariantAnimation>

#include "ui/theme/UiScaleManager.h"
#include "ui/theme/UiThemeManager.h"

namespace autoviz::ui::playback {
namespace {
constexpr std::array<double, 6> kRates = {0.25, 0.5, 1.0, 2.0, 4.0, 8.0};

qreal scaled(qreal value)
{
    return autoviz::ui::theme::UiScaleManager::instance().scaled(static_cast<int>(value));
}

QString spinBoxStyle(const theme::ThemePalette& palette)
{
    const QColor background = palette.dark ? QColor(255, 255, 255, 18) : QColor(248, 250, 252, 222);
    const QColor border = palette.dark ? QColor(255, 255, 255, 30) : QColor(23, 32, 42, 30);
    return QStringLiteral(
               "QDoubleSpinBox#playbackRateSpin {"
               " color: %1; background-color: rgba(%2, %3, %4, %5);"
               " border: 1px solid rgba(%6, %7, %8, %9); border-radius: %10px;"
               " padding: 0px %11px; font-weight: 600; }"
               "QDoubleSpinBox#playbackRateSpin:focus { border-color: rgba(%12, %13, %14, %15); }")
        .arg(palette.text.name())
        .arg(background.red())
        .arg(background.green())
        .arg(background.blue())
        .arg(background.alpha())
        .arg(border.red())
        .arg(border.green())
        .arg(border.blue())
        .arg(border.alpha())
        .arg(qRound(scaled(10)))
        .arg(qRound(scaled(8)))
        .arg(palette.accent.red())
        .arg(palette.accent.green())
        .arg(palette.accent.blue())
        .arg(palette.dark ? 145 : 120);
}
}  // namespace

PillRateSlider::PillRateSlider(QWidget* parent)
    : QWidget(parent)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAccessibleName(tr("播放速度档位"));

    m_snapAnimation = new QVariantAnimation(this);
    m_snapAnimation->setDuration(140);
    m_snapAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_snapAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        setHandlePosition(value.toReal());
    });

    m_scaleAnimation = new QVariantAnimation(this);
    m_scaleAnimation->setDuration(110);
    m_scaleAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_scaleAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        setHandleScale(value.toReal());
    });
}

void PillRateSlider::setIndex(int index, bool animate)
{
    snapToIndex(index, animate);
}

int PillRateSlider::index() const
{
    return m_index;
}

QSize PillRateSlider::sizeHint() const
{
    // 29 px 手柄两侧各占约 14.5 px，胶囊有效轨道约为原来的 68%。
    return {qRound(scaled(233)), qRound(scaled(36))};
}

QSize PillRateSlider::minimumSizeHint() const
{
    return sizeHint();
}

void PillRateSlider::paintEvent(QPaintEvent*)
{
    const auto palette = theme::UiThemeManager::instance().effectivePalette();
    const qreal handleDiameter = scaled(29);
    const qreal trackHeight = scaled(18);
    const qreal sideInset = handleDiameter / 2.0;
    const QRectF track(sideInset, (height() - trackHeight) / 2.0,
                       qMax<qreal>(1.0, width() - handleDiameter), trackHeight);
    const qreal handleX = track.left() + m_handlePosition * track.width();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.setPen(Qt::NoPen);
    painter.setBrush(palette.dark ? QColor("#424247") : QColor("#E2E6EB"));
    painter.drawRoundedRect(track, trackHeight / 2.0, trackHeight / 2.0);

    QRectF selected = track;
    selected.setRight(qMax(track.left() + trackHeight / 2.0, handleX));
    QColor selectedColor = palette.accent;
    selectedColor.setAlpha(palette.dark ? 235 : 225);
    painter.setBrush(selectedColor);
    painter.drawRoundedRect(selected, trackHeight / 2.0, trackHeight / 2.0);

    const qreal dotRadius = scaled(2.0);
    for (int item = 0; item < static_cast<int>(kRates.size()); ++item) {
        const qreal x = track.left() + positionForIndex(item) * track.width();
        QColor dot = item <= m_index ? QColor(255, 255, 255, 155) : palette.mutedText;
        dot.setAlpha(item <= m_index ? 155 : 115);
        painter.setBrush(dot);
        painter.drawEllipse(QPointF(x, track.center().y()), dotRadius, dotRadius);
    }

    const qreal radius = handleDiameter * m_handleScale / 2.0;
    painter.setBrush(QColor(0, 0, 0, palette.dark ? 65 : 32));
    painter.drawEllipse(QPointF(handleX, track.center().y() + scaled(2)), radius + scaled(1.5), radius + scaled(1.5));
    painter.setPen(QPen(palette.dark ? QColor("#D7D9DF") : QColor("#B7C0CA"), scaled(1)));
    painter.setBrush(palette.dark ? QColor("#FAFAFC") : QColor("#FFFFFF"));
    painter.drawEllipse(QPointF(handleX, track.center().y()), radius, radius);
}

void PillRateSlider::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    setFocus(Qt::MouseFocusReason);
    m_pressed = true;
    m_snapAnimation->stop();
    setVisualScale(0.94);
    event->accept();
}

void PillRateSlider::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_pressed || !(event->buttons() & Qt::LeftButton)) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    const qreal handleInset = (width() - scaled(29)) / 2.0;
    const qreal position = qBound<qreal>(0.0, (
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                                                  event->position().x()
#else
                                                  event->localPos().x()
#endif
                                                  - handleInset)
                                                  / qMax<qreal>(1.0, width() - scaled(29)), 1.0);
    setHandlePosition(position);
    const int nextIndex = indexForPosition(position);
    if (nextIndex != m_index) {
        m_index = nextIndex;
        emit indexChanged(m_index);
    }
    event->accept();
}

void PillRateSlider::mouseReleaseEvent(QMouseEvent* event)
{
    if (!m_pressed || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    m_pressed = false;
    setVisualScale(underMouse() ? 1.06 : 1.0);
    const qreal handleInset = (width() - scaled(29)) / 2.0;
    const qreal position = qBound<qreal>(0.0, (
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                                                  event->position().x()
#else
                                                  event->localPos().x()
#endif
                                                  - handleInset)
                                                  / qMax<qreal>(1.0, width() - scaled(29)), 1.0);
    snapToIndex(indexForPosition(position), true);
    event->accept();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void PillRateSlider::enterEvent(QEnterEvent* event)
#else
void PillRateSlider::enterEvent(QEvent* event)
#endif
{
    if (!m_pressed) {
        setVisualScale(1.06);
    }
    QWidget::enterEvent(event);
}

void PillRateSlider::leaveEvent(QEvent* event)
{
    if (!m_pressed) {
        setVisualScale(1.0);
    }
    QWidget::leaveEvent(event);
}

void PillRateSlider::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Down) {
        snapToIndex(m_index - 1, true);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Right || event->key() == Qt::Key_Up) {
        snapToIndex(m_index + 1, true);
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

qreal PillRateSlider::handlePosition() const
{
    return m_handlePosition;
}

void PillRateSlider::setHandlePosition(qreal position)
{
    m_handlePosition = qBound<qreal>(0.0, position, 1.0);
    update();
}

qreal PillRateSlider::handleScale() const
{
    return m_handleScale;
}

void PillRateSlider::setHandleScale(qreal scale)
{
    m_handleScale = scale;
    update();
}

qreal PillRateSlider::positionForIndex(int index) const
{
    return static_cast<qreal>(qBound(0, index, static_cast<int>(kRates.size()) - 1))
           / static_cast<qreal>(kRates.size() - 1);
}

int PillRateSlider::indexForPosition(qreal position) const
{
    return qBound(0, qRound(position * static_cast<qreal>(kRates.size() - 1)), static_cast<int>(kRates.size()) - 1);
}

void PillRateSlider::setVisualScale(qreal target)
{
    m_scaleAnimation->stop();
    m_scaleAnimation->setStartValue(m_handleScale);
    m_scaleAnimation->setEndValue(target);
    m_scaleAnimation->start();
}

void PillRateSlider::snapToIndex(int index, bool animate)
{
    const int boundedIndex = qBound(0, index, static_cast<int>(kRates.size()) - 1);
    const qreal target = positionForIndex(boundedIndex);
    m_snapAnimation->stop();
    if (animate) {
        m_snapAnimation->setStartValue(m_handlePosition);
        m_snapAnimation->setEndValue(target);
        m_snapAnimation->start();
    } else {
        setHandlePosition(target);
    }
    if (m_index != boundedIndex) {
        m_index = boundedIndex;
        emit indexChanged(m_index);
    }
}

PlaybackSpeedWidget::PlaybackSpeedWidget(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("playbackSpeedOverlay"));
    setAccessibleName(tr("播放速度"));
    setToolTip(tr("播放速度"));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFrameShape(QFrame::NoFrame);
    setFixedSize(sizeHint());
    setMaximumSize(sizeHint());

    const auto& scale = theme::UiScaleManager::instance();
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(scale.scaled(11), scale.scaled(6), scale.scaled(12), scale.scaled(6));
    row->setSpacing(scale.scaled(11));

    m_slider = new PillRateSlider(this);
    m_slider->setFixedSize(m_slider->sizeHint());

    m_spin = new QDoubleSpinBox(this);
    m_spin->setObjectName(QStringLiteral("playbackRateSpin"));
    m_spin->setRange(0.1, 8.0);
    m_spin->setDecimals(2);
    m_spin->setSingleStep(0.05);
    m_spin->setValue(1.0);
    m_spin->setSuffix(QStringLiteral("×"));
    m_spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_spin->setAlignment(Qt::AlignCenter);
    m_spin->setFixedSize(scale.scaled(62), scale.scaled(28));
    m_spin->setFont(scale.font(scale.fontSizeNormal(), QFont::DemiBold));
    m_spin->setStyleSheet(spinBoxStyle(theme::UiThemeManager::instance().effectivePalette()));
    m_spin->setToolTip(tr("可输入 0.10× 到 8.00×"));

    row->addWidget(m_slider);
    row->addWidget(m_spin);

    connect(m_slider, &PillRateSlider::indexChanged, this, [this](int index) {
        const double rate = kRates[static_cast<std::size_t>(index)];
        const QSignalBlocker blocker(m_spin);
        m_spin->setValue(rate);
        emit rateChanged(rate);
    });
    connect(m_spin,
            qOverload<double>(&QDoubleSpinBox::valueChanged),
            this,
            [this](double rate) {
                const QSignalBlocker blocker(m_slider);
                m_slider->setIndex(nearestStep(rate));
                emit rateChanged(rate);
            });
}

QSize PlaybackSpeedWidget::sizeHint() const
{
    return {theme::UiScaleManager::instance().scaled(329),
            theme::UiScaleManager::instance().scaled(48)};
}

QSize PlaybackSpeedWidget::minimumSizeHint() const
{
    return sizeHint();
}

void PlaybackSpeedWidget::paintEvent(QPaintEvent* event)
{
    QFrame::paintEvent(event);

    const auto palette = theme::UiThemeManager::instance().effectivePalette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal borderWidth = qMax<qreal>(1.0, scaled(1));
    const QRectF bounds = QRectF(rect()).adjusted(borderWidth / 2.0,
                                                   borderWidth / 2.0,
                                                   -borderWidth / 2.0,
                                                   -borderWidth / 2.0);
    const QColor background = palette.dark ? QColor(30, 30, 34, 224) : QColor(255, 255, 255, 226);
    const QColor border = palette.dark ? QColor(255, 255, 255, 28) : QColor(23, 32, 42, 24);
    painter.setPen(QPen(border, borderWidth));
    painter.setBrush(background);
    const qreal radius = scaled(14);
    painter.drawRoundedRect(bounds, radius, radius);
}

int PlaybackSpeedWidget::nearestStep(double rate) const
{
    int best = 0;
    double smallestDelta = std::numeric_limits<double>::max();
    for (int index = 0; index < static_cast<int>(kRates.size()); ++index) {
        const double delta = std::abs(rate - kRates[static_cast<std::size_t>(index)]);
        if (delta < smallestDelta) {
            smallestDelta = delta;
            best = index;
        }
    }
    return best;
}

void PlaybackSpeedWidget::setRate(double rate)
{
    const QSignalBlocker sliderBlocker(m_slider);
    const QSignalBlocker spinBlocker(m_spin);
    m_slider->setIndex(nearestStep(rate));
    m_spin->setValue(rate);
}

}  // namespace autoviz::ui::playback
