#include "ui/charts/ControlPlotBuffer.h"

#include <algorithm>

namespace autoviz::ui::charts {

void ControlPlotBuffer::setWindowMs(qint64 windowMs)
{
    m_windowMs = windowMs;
    if (!m_samples.isEmpty()) {
        trim(m_samples.last().elapsedMs);
    }
}

qint64 ControlPlotBuffer::windowMs() const
{
    return m_windowMs;
}

void ControlPlotBuffer::pushData(const ControlDebugData& data)
{
    m_samples.push_back(data);
    std::sort(m_samples.begin(), m_samples.end(), [](const ControlDebugData& lhs, const ControlDebugData& rhs) {
        return lhs.elapsedMs < rhs.elapsedMs;
    });
    if (!m_samples.isEmpty()) {
        trim(m_samples.last().elapsedMs);
    }
}

void ControlPlotBuffer::clear()
{
    m_samples.clear();
}

QVector<ControlDebugData> ControlPlotBuffer::samples() const
{
    return m_samples;
}

void ControlPlotBuffer::trim(qint64 nowMs)
{
    while (!m_samples.isEmpty() && m_samples.first().elapsedMs < nowMs - m_windowMs) {
        m_samples.pop_front();
    }
}

}  // namespace autoviz::ui::charts
