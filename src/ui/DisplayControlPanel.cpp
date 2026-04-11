#include "ui/DisplayControlPanel.h"

#include <QCheckBox>
#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

DisplayControlPanel::DisplayControlPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void DisplayControlPanel::setLayerVisibility(const autoviz::render::LayerVisibility& visibility)
{
    m_vehicleCheck->setChecked(visibility.showVehicle);
    m_globalPathCheck->setChecked(visibility.showGlobalPath);
    m_referenceLineCheck->setChecked(visibility.showReferenceLine);
    m_localPathCheck->setChecked(visibility.showLocalPath);
    m_obstacleCheck->setChecked(visibility.showObstacles);
}

bool DisplayControlPanel::vehicleCenteredMode() const
{
    return m_vehicleCenteredModeCheck != nullptr && m_vehicleCenteredModeCheck->isChecked();
}

void DisplayControlPanel::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    setStyleSheet("background: #f4f6f8;");

    auto* title = new QLabel(tr("显示控制面板"), this);
    title->setStyleSheet("font-weight: 700; color: #27313b;");
    layout->addWidget(title);

    auto* groupTitle = new QLabel(tr("主视图图层"), this);
    groupTitle->setStyleSheet("color: #4b5a67;");
    layout->addWidget(groupTitle);

    m_vehicleCheck = new QCheckBox(tr("显示车辆"), this);
    m_globalPathCheck = new QCheckBox(tr("显示全局路径"), this);
    m_referenceLineCheck = new QCheckBox(tr("显示参考线"), this);
    m_localPathCheck = new QCheckBox(tr("显示局部路径"), this);
    m_obstacleCheck = new QCheckBox(tr("显示障碍物"), this);
    m_vehicleCenteredModeCheck = new QCheckBox(tr("车辆居中显示"), this);

    for (auto* checkBox : {m_vehicleCheck, m_globalPathCheck, m_referenceLineCheck, m_localPathCheck, m_obstacleCheck,
                           m_vehicleCenteredModeCheck}) {
        checkBox->setChecked(true);
        layout->addWidget(checkBox);
    }

    auto* divider = new QFrame(this);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet("color: #d6dde3;");
    layout->addWidget(divider);

    auto* hint = new QLabel(tr("后续可继续在这里增加车道线、轨迹点、预测框、调试标注等显示项。"), this);
    hint->setWordWrap(true);
    hint->setStyleSheet("color: #6b7a88;");
    layout->addWidget(hint);
    layout->addStretch(1);

    for (auto* checkBox : {m_vehicleCheck, m_globalPathCheck, m_referenceLineCheck, m_localPathCheck, m_obstacleCheck}) {
        connect(checkBox, &QCheckBox::toggled, this, [this](bool) { emitLayerVisibility(); });
    }
    connect(m_vehicleCenteredModeCheck, &QCheckBox::toggled, this, &DisplayControlPanel::vehicleCenteredModeChanged);
}

void DisplayControlPanel::emitLayerVisibility()
{
    autoviz::render::LayerVisibility visibility;
    visibility.showVehicle = m_vehicleCheck->isChecked();
    visibility.showGlobalPath = m_globalPathCheck->isChecked();
    visibility.showReferenceLine = m_referenceLineCheck->isChecked();
    visibility.showLocalPath = m_localPathCheck->isChecked();
    visibility.showObstacles = m_obstacleCheck->isChecked();
    emit layerVisibilityChanged(visibility);
}
