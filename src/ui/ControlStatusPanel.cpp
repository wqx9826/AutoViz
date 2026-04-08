#include "ui/ControlStatusPanel.h"

#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "utils/Logger.h"

namespace {
QLabel* createValueLabel(QWidget* parent)
{
    auto* label = new QLabel(QStringLiteral("--"), parent);
    label->setStyleSheet("padding: 4px 6px; background: #24303d; border-radius: 4px;");
    return label;
}
}  // namespace

ControlStatusPanel::ControlStatusPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void ControlStatusPanel::setControlStatus(const ControlStatusData& data)
{
    setValue(m_timestampValue, data.timestamp);
    setValue(m_expectedSteeringValue, data.expectedSteering);
    setValue(m_actualSteeringValue, data.actualSteering);
    setValue(m_expectedSpeedValue, data.expectedSpeed);
    setValue(m_actualSpeedValue, data.actualSpeed);
    setValue(m_expectedAccelerationValue, data.expectedAcceleration);
    setValue(m_actualAccelerationValue, data.actualAcceleration);
    setValue(m_controlModeValue, data.controlMode);
    setValue(m_lateralErrorValue, data.lateralError);
    setValue(m_longitudinalErrorValue, data.longitudinalError);
}

void ControlStatusPanel::setPlaceholderData()
{
    ControlStatusData data;
    data.timestamp = QStringLiteral("2026-04-08 10:12:45.230");
    data.expectedSteering = QStringLiteral("2.50 deg");
    data.actualSteering = QStringLiteral("2.34 deg");
    data.expectedSpeed = QStringLiteral("8.20 m/s");
    data.actualSpeed = QStringLiteral("8.05 m/s");
    data.expectedAcceleration = QStringLiteral("0.60 m/s^2");
    data.actualAcceleration = QStringLiteral("0.52 m/s^2");
    data.controlMode = QStringLiteral("自动驾驶");
    data.lateralError = QStringLiteral("0.08 m");
    data.longitudinalError = QStringLiteral("-0.12 m");

    setControlStatus(data);
    Logger::instance().info("已设置控制状态占位数据。");
}

void ControlStatusPanel::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* title = new QLabel(tr("控制状态面板"), this);
    title->setStyleSheet("font-weight: 600;");
    layout->addWidget(title);

    auto* gridLayout = new QGridLayout();
    gridLayout->setHorizontalSpacing(16);
    gridLayout->setVerticalSpacing(8);

    auto addField = [this, gridLayout](int row, int column, const QString& name, QLabel*& valueLabel) {
        auto* nameLabel = new QLabel(name, this);
        valueLabel = createValueLabel(this);
        gridLayout->addWidget(nameLabel, row, column * 2);
        gridLayout->addWidget(valueLabel, row, column * 2 + 1);
    };

    addField(0, 0, tr("当前时间戳"), m_timestampValue);
    addField(0, 1, tr("期望转角"), m_expectedSteeringValue);
    addField(0, 2, tr("实际转角"), m_actualSteeringValue);
    addField(1, 0, tr("期望速度"), m_expectedSpeedValue);
    addField(1, 1, tr("实际速度"), m_actualSpeedValue);
    addField(1, 2, tr("期望加速度"), m_expectedAccelerationValue);
    addField(2, 0, tr("实际加速度"), m_actualAccelerationValue);
    addField(2, 1, tr("控制模式"), m_controlModeValue);
    addField(2, 2, tr("横向误差"), m_lateralErrorValue);
    addField(3, 0, tr("纵向误差"), m_longitudinalErrorValue);

    layout->addLayout(gridLayout);

    auto* hintLabel = new QLabel(tr("后续将在此接入控制消息解析与实时状态刷新。"), this);
    hintLabel->setStyleSheet("color: #6c7a89;");
    layout->addWidget(hintLabel);
    layout->addStretch(1);
}

void ControlStatusPanel::setValue(QLabel*& label, const QString& text)
{
    if (label != nullptr) {
        label->setText(text);
    }
}
