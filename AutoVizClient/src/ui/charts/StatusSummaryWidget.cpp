#include "ui/charts/StatusSummaryWidget.h"

#include <cmath>

#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QtMath>
#include <QVBoxLayout>

#include "ui/charts/ControlPanelStyle.h"
#include "ui/theme/UiScaleManager.h"
#include "ui/theme/UiThemeManager.h"

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
    label->setStyleSheet(style::statusValueStyleSheet(autoviz::ui::theme::UiThemeManager::instance().effectivePalette().text));
    return label;
}

void styleBadge(QLabel* label, const QColor& text, const QColor& background)
{
    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    label->setFont(style::controlFont());
    label->setStyleSheet(QStringLiteral(
        "color: %1; background: %2; border-radius: %3px; padding: %4px %5px; font-weight: 700;")
                             .arg(text.name())
                             .arg(background.name())
                             .arg(scale.scaled(10))
                             .arg(scale.scaled(3))
                             .arg(scale.scaled(9)));
}

QFrame* createMetricBlock(const QString& caption, QLabel* valueLabel, QWidget* parent)
{
    auto* block = new QFrame(parent);
    block->setObjectName(QStringLiteral("metricBlock"));
    auto* layout = new QVBoxLayout(block);
    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    layout->setContentsMargins(scale.scaled(8), scale.scaled(5), scale.scaled(8), scale.scaled(5));
    layout->setSpacing(scale.spacingSmall() / 2);
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
    const auto p = autoviz::ui::theme::UiThemeManager::instance().effectivePalette();
    m_modeLabel->setText(modeText(mode));
    if (mode == ControlDebugMode::Running) {
        styleBadge(m_modeLabel, p.normalText, p.normalBackground);
    } else if (mode == ControlDebugMode::Error) {
        styleBadge(m_modeLabel, p.warnText, p.warnBackground);
    } else {
        styleBadge(m_modeLabel, p.offlineText, p.offlineBackground);
    }
    m_feedbackLabel->setText(data.feedbackSource.isEmpty() ? QStringLiteral("--") : data.feedbackSource);
    styleBadge(m_feedbackLabel, p.offlineText, p.offlineBackground);
    m_updateLabel->setText(data.sourceTimestampMs > 0
                               ? QDateTime::fromMSecsSinceEpoch(data.sourceTimestampMs).toString(QStringLiteral("HH:mm:ss.zzz"))
                               : QStringLiteral("--"));
    m_updateLabel->setFont(style::controlFont());
    m_updateLabel->setStyleSheet(QStringLiteral("color: %1; font-weight: 400;").arg(p.mutedText.name()));
    m_timeoutLabel->setText(data.timedOut ? QStringLiteral("数据超时") : QStringLiteral("数据正常"));
    styleBadge(m_timeoutLabel,
               data.timedOut ? p.warnText : p.normalText,
               data.timedOut ? p.warnBackground : p.normalBackground);

    m_speedErrorLabel->setText(QStringLiteral("%1 m/s").arg(data.hasSpeedError ? QString::number(data.speedError, 'f', 2) : QStringLiteral("--")));
    m_lateralErrorLabel->setText(QStringLiteral("%1 m").arg(data.hasLateralError ? QString::number(data.lateralError, 'f', 2) : QStringLiteral("--")));
    m_yawErrorLabel->setText(QStringLiteral("%1°").arg(data.hasYawError ? QString::number(data.yawError, 'f', 2) : QStringLiteral("--")));

    setValueStyle(m_speedErrorLabel, severityColor(std::abs(data.speedError), 0.5, 1.2));
    setValueStyle(m_lateralErrorLabel, severityColor(std::abs(data.lateralError), 0.5, 1.5));
    setValueStyle(m_yawErrorLabel,
                  severityColor(std::abs(data.yawError), qRadiansToDegrees(0.25), qRadiansToDegrees(0.8)));
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
        return QColor("#FF6B6B");
    }
    if (absValue >= warningThreshold) {
        return QColor("#FFB45C");
    }
    return QColor("#6EF2A0");
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

    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(scale.scaled(10), scale.scaled(8), scale.scaled(10), scale.scaled(8));
    root->setSpacing(scale.spacingSmall());

    m_modeLabel = new QLabel(QStringLiteral("--"), this);
    m_feedbackLabel = new QLabel(QStringLiteral("--"), this);
    m_updateLabel = new QLabel(QStringLiteral("--"), this);
    m_timeoutLabel = new QLabel(QStringLiteral("--"), this);

    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(scale.spacingSmall());
    topRow->addWidget(createCaption(QStringLiteral("模式"), this));
    topRow->addWidget(m_modeLabel);
    topRow->addSpacing(scale.spacingSmall());
    topRow->addWidget(createCaption(QStringLiteral("反馈"), this));
    topRow->addWidget(m_feedbackLabel);
    topRow->addStretch(1);
    topRow->addWidget(createCaption(QStringLiteral("更新时间"), this));
    topRow->addWidget(m_updateLabel);
    topRow->addWidget(m_timeoutLabel);
    root->addLayout(topRow);

    auto* metricsRow = new QHBoxLayout();
    metricsRow->setSpacing(scale.spacingSmall());

    m_speedErrorLabel = createValue(this);
    m_lateralErrorLabel = createValue(this);
    m_yawErrorLabel = createValue(this);

    metricsRow->addWidget(createMetricBlock(QStringLiteral("速度误差"), m_speedErrorLabel, this), 1);
    metricsRow->addWidget(createMetricBlock(QStringLiteral("横向误差"), m_lateralErrorLabel, this), 1);
    metricsRow->addWidget(createMetricBlock(QStringLiteral("航向误差"), m_yawErrorLabel, this), 1);
    root->addLayout(metricsRow);
}

}  // namespace autoviz::ui::charts
