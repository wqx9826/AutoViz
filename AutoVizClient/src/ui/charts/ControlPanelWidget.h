#pragma once

#include <QString>
#include <QWidget>

#include "ui/charts/ControlDebugData.h"
#include "ui/charts/ControlPlotBuffer.h"

class QCheckBox;
class QComboBox;
class QPushButton;
class QTimer;

namespace autoviz::datacenter {
struct VisualizationSnapshot;
}

namespace autoviz::ui::charts {

class PlotCardWidget;
class StatusSummaryWidget;

class ControlPanelWidget : public QWidget {
public:
    explicit ControlPanelWidget(QWidget* parent = nullptr);

    void updateSnapshot(const autoviz::datacenter::VisualizationSnapshot& snapshot);
    void clearHistory();
    void setRenderingSuspended(bool suspended);

private:
    void setupUi();
    void configurePlots();
    void refreshPlots();
    void setWindowFromCombo();
    ControlDebugData buildDebugData(const autoviz::datacenter::VisualizationSnapshot& snapshot);

    StatusSummaryWidget* m_statusSummary = nullptr;
    PlotCardWidget* m_speedPlot = nullptr;
    PlotCardWidget* m_yawPlot = nullptr;
    PlotCardWidget* m_pathErrorPlot = nullptr;
    QPushButton* m_pauseButton = nullptr;
    QPushButton* m_clearButton = nullptr;
    QComboBox* m_windowCombo = nullptr;
    QCheckBox* m_autoScaleCheck = nullptr;
    QTimer* m_repaintTimer = nullptr;
    ControlPlotBuffer m_buffer;
    ControlDebugData m_latestData;
    qint64 m_firstSampleTimestampMs = 0;
    qint64 m_lastBufferedTimestampMs = -1;
    QString m_activeActionGoalId;
    int m_activeActionMode = 0;
    bool m_actionWasActive = false;
    bool m_paused = false;
    bool m_renderingSuspended = false;
};

}  // namespace autoviz::ui::charts
