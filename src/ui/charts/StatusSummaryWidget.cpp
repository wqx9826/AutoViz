#include "ui/charts/StatusSummaryWidget.h"

#include <cmath>

#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "ui/charts/ControlPanelStyle.h"

namespace autoviz::ui::charts {

namespace {
QLabel* createCaption(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setFont(style::captionFont());
    label->setStyleSheet(style::captionStyleSheet());
    return label;
}

QLabel* createValue(QWidget* parent)
{
    auto* label = new QLabel(QStringLiteral("--"), parent);
    label->setFont(style::statusValueFont());
    label->setStyleSheet(style::statusValueStyleSheet(QColor("#111827")));
    return label;
}

void styleBadge(QLabel* label, const QColor& text, const QColor& background)
{
    label->setFont(style::controlFont());
    label->setStyleSheet(QStringLiteral(
        "color: %1; background: %2; border-radius: 10px; padding: 3px 9px; font-size: 12px; font-weight: 700;")
                             .arg(text.name(), background.name()));
}

QFrame* createMetricBlock(const QString& caption, QLabel* valueLabel, QWidget* parent)
{
    auto* block = new QFrame(parent);
    block->setStyleSheet(QStringLiteral("background: #F9FAFB; border-radius: 8px;"));
    auto* layout = new QVBoxLayout(block);
    layout->setContentsMargins(10, 7, 10, 7);
    layout->setSpacing(3);
    layout->addWidget(createCaption(caption, block));
    layout->addWidget(valueLabel);
    return block;
}
}  // namespace

StatusSummaryWidget::StatusSummaryWidget(QWidget* parent)
    : QFrame(parent)
{
    setupUi();
}

void StatusSummaryWidget::setData(const ControlDebugData& data)
{
    const auto mode = data.timedOut ? ControlDebugMode::Error : data.mode;
    m_modeLabel->setText(modeText(mode));
    if (mode == ControlDebugMode::Running) {
        styleBadge(m_modeLabel, QColor("#075985"), QColor("#E0F2FE"));
    } else if (mode == ControlDebugMode::Error) {
        styleBadge(m_modeLabel, QColor("#B91C1C"), QColor("#FEE2E2"));
    } else {
        styleBadge(m_modeLabel, QColor("#4B5563"), QColor("#F3F4F6"));
    }
    m_feedbackLabel->setText(data.feedbackSource.isEmpty() ? QStringLiteral("--") : data.feedbackSource);
    styleBadge(m_feedbackLabel, QColor("#374151"), QColor("#F3F4F6"));
    m_updateLabel->setText(data.sourceTimestampMs > 0
                               ? QDateTime::fromMSecsSinceEpoch(data.sourceTimestampMs).toString(QStringLiteral("HH:mm:ss.zzz"))
                               : QStringLiteral("--"));
    m_updateLabel->setFont(style::controlFont());
    m_updateLabel->setStyleSheet(QStringLiteral("color: #4B5563; font-size: 12px; font-weight: 400;"));
    m_timeoutLabel->setText(data.timedOut ? QStringLiteral("数据超时") : QStringLiteral("数据正常"));
    styleBadge(m_timeoutLabel,
               data.timedOut ? QColor("#B91C1C") : QColor("#047857"),
               data.timedOut ? QColor("#FEE2E2") : QColor("#D1FAE5"));

    m_speedErrorLabel->setText(QStringLiteral("%1 m/s").arg(data.hasSpeedError ? QString::number(data.speedError, 'f', 2) : QStringLiteral("--")));
    m_lateralErrorLabel->setText(QStringLiteral("%1 m").arg(data.hasLateralError ? QString::number(data.lateralError, 'f', 2) : QStringLiteral("--")));
    m_yawErrorLabel->setText(QStringLiteral("%1 rad").arg(data.hasYawError ? QString::number(data.yawError, 'f', 2) : QStringLiteral("--")));

    setValueStyle(m_speedErrorLabel, severityColor(std::abs(data.speedError), 0.5, 1.2));
    setValueStyle(m_lateralErrorLabel, severityColor(std::abs(data.lateralError), 0.5, 1.5));
    setValueStyle(m_yawErrorLabel, severityColor(std::abs(data.yawError), 0.25, 0.8));
}

QString StatusSummaryWidget::modeText(ControlDebugMode mode)
{
    switch (mode) {
    case ControlDebugMode::Running:
        return QStringLiteral("执行");
    case ControlDebugMode::Error:
        return QStringLiteral("错误");
    case ControlDebugMode::Standby:
    default:
        return QStringLiteral("待机");
    }
}

QColor StatusSummaryWidget::severityColor(double absValue, double warningThreshold, double errorThreshold)
{
    if (absValue >= errorThreshold) {
        return QColor("#DC2626");
    }
    if (absValue >= warningThreshold) {
        return QColor("#D97706");
    }
    return QColor("#047857");
}

void StatusSummaryWidget::setValueStyle(QLabel* label, const QColor& color)
{
    label->setFont(style::statusValueFont());
    label->setStyleSheet(style::statusValueStyleSheet(color));
}

void StatusSummaryWidget::setupUi()
{
    setObjectName(QStringLiteral("statusSummaryCard"));
    setFont(style::font());
    setStyleSheet(style::statusCardStyleSheet());

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 12, 14, 12);
    root->setSpacing(12);

    m_modeLabel = new QLabel(QStringLiteral("--"), this);
    m_feedbackLabel = new QLabel(QStringLiteral("--"), this);
    m_updateLabel = new QLabel(QStringLiteral("--"), this);
    m_timeoutLabel = new QLabel(QStringLiteral("--"), this);

    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(8);
    topRow->addWidget(createCaption(QStringLiteral("模式"), this));
    topRow->addWidget(m_modeLabel);
    topRow->addSpacing(8);
    topRow->addWidget(createCaption(QStringLiteral("反馈"), this));
    topRow->addWidget(m_feedbackLabel);
    topRow->addStretch(1);
    topRow->addWidget(createCaption(QStringLiteral("更新时间"), this));
    topRow->addWidget(m_updateLabel);
    topRow->addWidget(m_timeoutLabel);
    root->addLayout(topRow);

    auto* metricsRow = new QHBoxLayout();
    metricsRow->setSpacing(10);

    m_speedErrorLabel = createValue(this);
    m_lateralErrorLabel = createValue(this);
    m_yawErrorLabel = createValue(this);

    metricsRow->addWidget(createMetricBlock(QStringLiteral("速度误差"), m_speedErrorLabel, this), 1);
    metricsRow->addWidget(createMetricBlock(QStringLiteral("横向误差"), m_lateralErrorLabel, this), 1);
    metricsRow->addWidget(createMetricBlock(QStringLiteral("航向误差"), m_yawErrorLabel, this), 1);
    root->addLayout(metricsRow);
}

}  // namespace autoviz::ui::charts
