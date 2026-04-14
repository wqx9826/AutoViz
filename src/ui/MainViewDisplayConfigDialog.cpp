#include "ui/MainViewDisplayConfigDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QVBoxLayout>

#include "ui/ToggleSwitch.h"

MainViewDisplayConfigDialog::MainViewDisplayConfigDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
}

void MainViewDisplayConfigDialog::setLayerVisibility(const autoviz::render::LayerVisibility& visibility)
{
    m_vehicleCheck->setChecked(visibility.showVehicle);
    m_globalPathCheck->setChecked(visibility.showGlobalPath);
    m_referenceLineCheck->setChecked(visibility.showReferenceLine);
    m_localPathCheck->setChecked(visibility.showLocalPath);
    m_obstacleCheck->setChecked(visibility.showObstacles);
}

void MainViewDisplayConfigDialog::setVehicleCenteredMode(bool enabled)
{
    m_vehicleCenteredModeCheck->setChecked(enabled);
}

void MainViewDisplayConfigDialog::setDataAvailability(const MainViewDataAvailability& availability)
{
    updateAvailability(m_vehicleCheck, availability.hasVehicleData);
    updateAvailability(m_globalPathCheck, availability.hasGlobalPathData);
    updateAvailability(m_referenceLineCheck, availability.hasReferenceLineData);
    updateAvailability(m_localPathCheck, availability.hasLocalPathData);
    updateAvailability(m_obstacleCheck, availability.hasObstacleData);
}

void MainViewDisplayConfigDialog::setupUi()
{
    setWindowTitle(tr("主视图显示管理"));
    setModal(false);
    resize(420, 340);
    setStyleSheet("QDialog { background: #f8fafc; } QLabel { color: #1f2937; }");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto* title = new QLabel(tr("主视图显示管理"), this);
    title->setStyleSheet("font-size: 16px; font-weight: 700;");
    layout->addWidget(title);

    auto* hint = new QLabel(tr("绿色表示正在显示，灰色表示已关闭，红色表示当前未接收到数据。"), this);
    hint->setWordWrap(true);
    hint->setStyleSheet("color: #5f6b76;");
    layout->addWidget(hint);

    auto addRow = [this, layout](const QString& text, ToggleSwitch*& toggleSwitch) {
        auto* row = new QFrame(this);
        row->setStyleSheet("QFrame { background: white; border: 1px solid #e5e7eb; border-radius: 12px; }");
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(14, 12, 14, 12);
        rowLayout->setSpacing(12);

        auto* label = new QLabel(text, row);
        label->setStyleSheet("font-size: 14px;");
        toggleSwitch = new ToggleSwitch(row);

        rowLayout->addWidget(label);
        rowLayout->addStretch(1);
        rowLayout->addWidget(toggleSwitch, 0, Qt::AlignRight | Qt::AlignVCenter);
        layout->addWidget(row);
    };

    addRow(tr("车辆定位"), m_vehicleCheck);
    addRow(tr("全局路径"), m_globalPathCheck);
    addRow(tr("参考线"), m_referenceLineCheck);
    addRow(tr("局部路径"), m_localPathCheck);
    addRow(tr("障碍物"), m_obstacleCheck);

    auto* centerRow = new QFrame(this);
    centerRow->setStyleSheet("QFrame { background: white; border: 1px solid #e5e7eb; border-radius: 12px; }");
    auto* centerLayout = new QHBoxLayout(centerRow);
    centerLayout->setContentsMargins(14, 12, 14, 12);
    centerLayout->setSpacing(12);
    auto* centerLabel = new QLabel(tr("车辆居中显示"), centerRow);
    centerLabel->setStyleSheet("font-size: 14px;");
    m_vehicleCenteredModeCheck = new ToggleSwitch(centerRow);
    centerLayout->addWidget(centerLabel);
    centerLayout->addStretch(1);
    centerLayout->addWidget(m_vehicleCenteredModeCheck, 0, Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(centerRow);
    layout->addStretch(1);

    for (auto* toggleSwitch : {m_vehicleCheck, m_globalPathCheck, m_referenceLineCheck, m_localPathCheck, m_obstacleCheck}) {
        toggleSwitch->setChecked(true);
        connect(toggleSwitch, &ToggleSwitch::toggled, this, [this](bool) { emitLayerVisibility(); });
    }
    m_vehicleCenteredModeCheck->setChecked(true);
    connect(m_vehicleCenteredModeCheck, &ToggleSwitch::toggled, this, &MainViewDisplayConfigDialog::vehicleCenteredModeChanged);
}

void MainViewDisplayConfigDialog::emitLayerVisibility()
{
    autoviz::render::LayerVisibility visibility;
    visibility.showVehicle = m_vehicleCheck->isChecked();
    visibility.showGlobalPath = m_globalPathCheck->isChecked();
    visibility.showReferenceLine = m_referenceLineCheck->isChecked();
    visibility.showLocalPath = m_localPathCheck->isChecked();
    visibility.showObstacles = m_obstacleCheck->isChecked();
    emit layerVisibilityChanged(visibility);
}

void MainViewDisplayConfigDialog::updateAvailability(ToggleSwitch* toggleSwitch, bool hasData)
{
    if (toggleSwitch == nullptr) {
        return;
    }

    toggleSwitch->setHasData(hasData);
}
