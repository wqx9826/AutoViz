#pragma once

#include <QWidget>

#include "core/model/ControlTypes.h"
#include "core/model/VehicleState.h"

class QLabel;

class ControlStatusPanel : public QWidget
{
public:
    explicit ControlStatusPanel(QWidget* parent = nullptr);

    void setData(const autoviz::model::VehicleState& vehicleState, const autoviz::model::ControlCmd& controlCmd);

private:
    void setupUi();
    void setValue(QLabel* label, const QString& text);

    QLabel* m_timestampValue = nullptr;
    QLabel* m_gearValue = nullptr;
    QLabel* m_targetSpeedValue = nullptr;
    QLabel* m_actualSpeedValue = nullptr;
    QLabel* m_targetWheelAngleValue = nullptr;
    QLabel* m_actualWheelAngleValue = nullptr;
    QLabel* m_targetYawRateValue = nullptr;
    QLabel* m_actualAccelValue = nullptr;
};
