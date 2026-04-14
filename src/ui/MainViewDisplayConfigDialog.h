#pragma once

#include <QDialog>

#include "core/render/SceneManager.h"

class QLabel;
class ToggleSwitch;

struct MainViewDataAvailability {
    bool hasVehicleData = false;
    bool hasGlobalPathData = false;
    bool hasReferenceLineData = false;
    bool hasLocalPathData = false;
    bool hasObstacleData = false;
};

class MainViewDisplayConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MainViewDisplayConfigDialog(QWidget* parent = nullptr);

    void setLayerVisibility(const autoviz::render::LayerVisibility& visibility);
    void setVehicleCenteredMode(bool enabled);
    void setDataAvailability(const MainViewDataAvailability& availability);

signals:
    void layerVisibilityChanged(const autoviz::render::LayerVisibility& visibility);
    void vehicleCenteredModeChanged(bool enabled);

private:
    void setupUi();
    void emitLayerVisibility();
    void updateAvailability(ToggleSwitch* toggleSwitch, bool hasData);

    ToggleSwitch* m_vehicleCheck = nullptr;
    ToggleSwitch* m_globalPathCheck = nullptr;
    ToggleSwitch* m_referenceLineCheck = nullptr;
    ToggleSwitch* m_localPathCheck = nullptr;
    ToggleSwitch* m_obstacleCheck = nullptr;
    ToggleSwitch* m_vehicleCenteredModeCheck = nullptr;
};
