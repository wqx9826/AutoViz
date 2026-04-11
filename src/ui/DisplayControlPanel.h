#pragma once

#include <QWidget>

#include "core/render/SceneManager.h"

class QCheckBox;

class DisplayControlPanel : public QWidget
{
    Q_OBJECT

public:
    explicit DisplayControlPanel(QWidget* parent = nullptr);

    void setLayerVisibility(const autoviz::render::LayerVisibility& visibility);
    bool vehicleCenteredMode() const;

signals:
    void layerVisibilityChanged(const autoviz::render::LayerVisibility& visibility);
    void vehicleCenteredModeChanged(bool enabled);

private:
    void setupUi();
    void emitLayerVisibility();

    QCheckBox* m_vehicleCheck = nullptr;
    QCheckBox* m_globalPathCheck = nullptr;
    QCheckBox* m_referenceLineCheck = nullptr;
    QCheckBox* m_localPathCheck = nullptr;
    QCheckBox* m_obstacleCheck = nullptr;
    QCheckBox* m_vehicleCenteredModeCheck = nullptr;
};
