#include "ui/ToggleSwitch.h"

#include <QMouseEvent>
#include <QPainter>

namespace {
constexpr int kSwitchWidth = 52;
constexpr int kSwitchHeight = 30;
constexpr int kMargin = 3;
}

ToggleSwitch::ToggleSwitch(QWidget* parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    connect(this, &QAbstractButton::toggled, this, [this](bool) { updateToolTip(); });
    updateToolTip();
}

QSize ToggleSwitch::sizeHint() const
{
    return QSize(kSwitchWidth, kSwitchHeight);
}

void ToggleSwitch::setHasData(bool hasData)
{
    m_hasData = hasData;
    setEnabled(hasData);
    if (!hasData) {
        setCursor(Qt::ArrowCursor);
    } else {
        setCursor(Qt::PointingHandCursor);
    }
    updateToolTip();
    update();
}

bool ToggleSwitch::hasData() const
{
    return m_hasData;
}

void ToggleSwitch::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QColor trackColor("#8b1e1e");
    if (m_hasData) {
        trackColor = isChecked() ? QColor("#1f9d55") : QColor("#6b7280");
    }

    const QRectF trackRect(1.0, 1.0, width() - 2.0, height() - 2.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(trackColor);
    painter.drawRoundedRect(trackRect, trackRect.height() * 0.5, trackRect.height() * 0.5);

    const qreal knobDiameter = trackRect.height() - 2.0 * kMargin;
    const qreal knobX = isChecked() && m_hasData
                            ? trackRect.right() - kMargin - knobDiameter
                            : trackRect.left() + kMargin;
    const QRectF knobRect(knobX, trackRect.top() + kMargin, knobDiameter, knobDiameter);

    painter.setBrush(QColor("#f8fafc"));
    painter.drawEllipse(knobRect);
}

void ToggleSwitch::mouseReleaseEvent(QMouseEvent* event)
{
    if (!m_hasData) {
        event->accept();
        return;
    }

    QAbstractButton::mouseReleaseEvent(event);
    updateToolTip();
}

void ToggleSwitch::updateToolTip()
{
    if (!m_hasData) {
        setToolTip(QStringLiteral("当前未接收到数据"));
        return;
    }

    setToolTip(isChecked() ? QStringLiteral("正在显示") : QStringLiteral("已关闭显示"));
}
