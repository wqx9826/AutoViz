#include "ui/ControlStatusPanel.h"

#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {
QLabel* createValueLabel(QWidget* parent)
{
    auto* label = new QLabel(QStringLiteral("--"), parent);
    label->setStyleSheet("padding: 4px 6px; background: #24303d; color: white; border-radius: 4px;");
    return label;
}
}  // namespace

ControlStatusPanel::ControlStatusPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void ControlStatusPanel::setData(const autoviz::model::VehicleState& vehicleState, const autoviz::model::ControlCmd& controlCmd)
{
    setValue(m_timestampValue, QString::number(controlCmd.meta.timestamp));
    setValue(m_gearValue, autoviz::model::toDisplayString(vehicleState.chassis.gear));
    setValue(m_targetSpeedValue, QString::number(controlCmd.desiredVelocity, 'f', 2) + QStringLiteral(" m/s"));
    setValue(m_actualSpeedValue, QString::number(vehicleState.chassis.currentSpeed, 'f', 2) + QStringLiteral(" m/s"));
    setValue(m_targetWheelAngleValue, QString::number(controlCmd.desiredWheelAngle, 'f', 2) + QStringLiteral(" deg"));
    setValue(m_actualWheelAngleValue, QString::number(vehicleState.chassis.currentWheelAngle, 'f', 2) + QStringLiteral(" deg"));
    setValue(m_targetYawRateValue, QString::number(controlCmd.desiredAngularVelocity, 'f', 2) + QStringLiteral(" rad/s"));
    setValue(m_actualAccelValue, QString::number(vehicleState.location.acceleration, 'f', 2) + QStringLiteral(" m/s^2"));
}

void ControlStatusPanel::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* title = new QLabel(tr("控制状态面板"), this);
    title->setStyleSheet("font-weight: 600;");
    layout->addWidget(title);

    auto* grid = new QGridLayout();
    auto addField = [this, grid](int row, int column, const QString& name, QLabel*& valueLabel) {
        auto* nameLabel = new QLabel(name, this);
        valueLabel = createValueLabel(this);
        grid->addWidget(nameLabel, row, column * 2);
        grid->addWidget(valueLabel, row, column * 2 + 1);
    };

    addField(0, 0, tr("时间戳"), m_timestampValue);
    addField(0, 1, tr("档位"), m_gearValue);
    addField(0, 2, tr("目标速度"), m_targetSpeedValue);
    addField(1, 0, tr("实际速度"), m_actualSpeedValue);
    addField(1, 1, tr("目标前轮角"), m_targetWheelAngleValue);
    addField(1, 2, tr("实际前轮角"), m_actualWheelAngleValue);
    addField(2, 0, tr("目标角速度"), m_targetYawRateValue);
    addField(2, 1, tr("当前加速度"), m_actualAccelValue);

    layout->addLayout(grid);

    auto* hint = new QLabel(tr("后续可继续接入控制模式、横纵向误差、执行器反馈等字段。"), this);
    hint->setStyleSheet("color: #6c7a89;");
    layout->addWidget(hint);
    layout->addStretch(1);
}

void ControlStatusPanel::setValue(QLabel* label, const QString& text)
{
    if (label != nullptr) {
        label->setText(text);
    }
}
