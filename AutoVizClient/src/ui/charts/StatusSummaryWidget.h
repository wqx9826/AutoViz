#pragma once

#include <QFrame>

#include "ui/charts/ControlDebugData.h"

class QLabel;

namespace autoviz::ui::charts {

class StatusSummaryWidget : public QFrame {
public:
    explicit StatusSummaryWidget(QWidget* parent = nullptr);

    void setData(const ControlDebugData& data);

private:
    static QString modeText(ControlDebugMode mode);
    static QColor severityColor(double absValue, double warningThreshold, double errorThreshold);
    static void setValueStyle(QLabel* label, const QColor& color);
    void setupUi();

    QLabel* m_modeLabel = nullptr;
    QLabel* m_feedbackLabel = nullptr;
    QLabel* m_updateLabel = nullptr;
    QLabel* m_timeoutLabel = nullptr;
    QLabel* m_speedErrorLabel = nullptr;
    QLabel* m_lateralErrorLabel = nullptr;
    QLabel* m_yawErrorLabel = nullptr;
    QLabel* m_angularVelocityErrorLabel = nullptr;
};

}  // namespace autoviz::ui::charts
