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
    void configureYawPlot(YawMetric metric);
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
    quint64 m_lastCommandSequence = 0;
    quint64 m_lastLocationSequence = 0;
    qint64 m_lastCommandTimestampMs = 0;
    qint64 m_lastLocationTimestampMs = 0;
    YawMetric m_yawMetric = YawMetric::Unknown;
    quint64 m_lastSnapshotSequence = 0;
    int m_lastInputSource = -1;
    QString m_lastSessionId;
    bool m_sourceInitialized = false;
    bool m_paused = false;
    bool m_renderingSuspended = false;
};

}  // namespace autoviz::ui::charts
