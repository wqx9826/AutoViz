#pragma once

#include <QVector>

#include "ui/charts/ControlDebugData.h"

namespace autoviz::ui::charts {

class ControlPlotBuffer {
public:
    void setWindowMs(qint64 windowMs);
    qint64 windowMs() const;
    void pushData(const ControlDebugData& data);
    void clear();
    QVector<ControlDebugData> samples() const;

private:
    void trim(qint64 nowMs);

    QVector<ControlDebugData> m_samples;
    qint64 m_windowMs = 30000;
};

}  // namespace autoviz::ui::charts
