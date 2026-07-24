#include "ui/MainViewDisplayConfigDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QSignalBlocker>
#include <QVariant>
#include <QVBoxLayout>

#include "ui/ToggleSwitch.h"
#include "ui/theme/UiScaleManager.h"

MainViewDisplayConfigDialog::MainViewDisplayConfigDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
}

void MainViewDisplayConfigDialog::setLayerVisibility(const autoviz::render::LayerVisibility& visibility)
{
    const QSignalBlocker vehicleBlocker(m_vehicleCheck);
    const QSignalBlocker historyTrailBlocker(m_historyTrailCheck);
    const QSignalBlocker globalPathBlocker(m_globalPathCheck);
    const QSignalBlocker referenceLineBlocker(m_referenceLineCheck);
    const QSignalBlocker localPathBlocker(m_localPathCheck);
    const QSignalBlocker obstacleBlocker(m_obstacleCheck);

    m_vehicleCheck->setChecked(visibility.showVehicle);
    m_historyTrailCheck->setChecked(visibility.showHistoryTrail);
    m_globalPathCheck->setChecked(visibility.showGlobalPath);
    m_referenceLineCheck->setChecked(visibility.showReferenceLine);
    m_localPathCheck->setChecked(visibility.showLocalPath);
    m_obstacleCheck->setChecked(visibility.showObstacles);
}

void MainViewDisplayConfigDialog::setVehicleCenteredMode(bool enabled)
{
    const QSignalBlocker blocker(m_vehicleCenteredModeCheck);
    m_vehicleCenteredModeCheck->setChecked(enabled);
}

void MainViewDisplayConfigDialog::setDataAvailability(const MainViewDataAvailability& availability)
{
    updateAvailability(m_vehicleCheck, availability.hasVehicleData);
    updateAvailability(m_historyTrailCheck, availability.hasHistoryTrailData);
    updateAvailability(m_globalPathCheck, availability.hasGlobalPathData);
    updateAvailability(m_referenceLineCheck, availability.hasReferenceLineData);
    updateAvailability(m_localPathCheck, availability.hasLocalPathData);
    updateAvailability(m_obstacleCheck, availability.hasObstacleData);
}

void MainViewDisplayConfigDialog::setupUi()
{
    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    setWindowTitle(tr("主视图显示管理"));
    setModal(false);
    resize(scale.scaled(420), scale.scaled(340));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(scale.scaled(18), scale.scaled(18), scale.scaled(18), scale.scaled(18));
    layout->setSpacing(scale.spacingNormal());

    auto* title = new QLabel(tr("主视图显示管理"), this);
    title->setFont(scale.font(scale.fontSizeTitle(), QFont::Bold));
    title->setStyleSheet(QStringLiteral("font-weight: 700;"));
    layout->addWidget(title);

    auto* hint = new QLabel(tr("绿色表示正在显示，灰色表示已关闭，红色表示当前未接收到数据。"), this);
    hint->setWordWrap(true);
    hint->setProperty("class", QVariant(QStringLiteral("status-key")));
    layout->addWidget(hint);

    auto addRow = [this, layout, &scale](const QString& text, ToggleSwitch*& toggleSwitch) {
        auto* row = new QFrame(this);
        row->setObjectName(QStringLiteral("dialogRow"));
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(scale.scaled(14), scale.scaled(12), scale.scaled(14), scale.scaled(12));
        rowLayout->setSpacing(scale.spacingNormal());

        auto* label = new QLabel(text, row);
        label->setFont(scale.font(scale.fontSizeNormal()));
        toggleSwitch = new ToggleSwitch(row);

        rowLayout->addWidget(label);
        rowLayout->addStretch(1);
        rowLayout->addWidget(toggleSwitch, 0, Qt::AlignRight | Qt::AlignVCenter);
        layout->addWidget(row);
    };

    addRow(tr("车辆定位"), m_vehicleCheck);
    addRow(tr("历史轨迹"), m_historyTrailCheck);
    addRow(tr("全局路径"), m_globalPathCheck);
    addRow(tr("参考线"), m_referenceLineCheck);
    addRow(tr("局部路径"), m_localPathCheck);
    addRow(tr("障碍物"), m_obstacleCheck);

    auto* centerRow = new QFrame(this);
    centerRow->setObjectName(QStringLiteral("dialogRow"));
    auto* centerLayout = new QHBoxLayout(centerRow);
    centerLayout->setContentsMargins(scale.scaled(14), scale.scaled(12), scale.scaled(14), scale.scaled(12));
    centerLayout->setSpacing(scale.spacingNormal());
    auto* centerLabel = new QLabel(tr("车辆居中显示"), centerRow);
    centerLabel->setFont(scale.font(scale.fontSizeNormal()));
    m_vehicleCenteredModeCheck = new ToggleSwitch(centerRow);
    centerLayout->addWidget(centerLabel);
    centerLayout->addStretch(1);
    centerLayout->addWidget(m_vehicleCenteredModeCheck, 0, Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(centerRow);
    layout->addStretch(1);

    for (auto* toggleSwitch : {m_vehicleCheck, m_historyTrailCheck, m_globalPathCheck, m_referenceLineCheck, m_localPathCheck, m_obstacleCheck}) {
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
    visibility.showHistoryTrail = m_historyTrailCheck->isChecked();
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
