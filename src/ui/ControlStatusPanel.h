#pragma once

#include <QString>
#include <QWidget>

class QLabel;

struct ControlStatusData {
    QString timestamp;
    QString expectedSteering;
    QString actualSteering;
    QString expectedSpeed;
    QString actualSpeed;
    QString expectedAcceleration;
    QString actualAcceleration;
    QString controlMode;
    QString lateralError;
    QString longitudinalError;
};

// ControlStatusPanel is a placeholder inspector for control command and
// feedback topics. It focuses on presenting normalized control fields that
// will later be filled by parsed ROS messages.
class ControlStatusPanel : public QWidget
{
public:
    explicit ControlStatusPanel(QWidget* parent = nullptr);

    void setControlStatus(const ControlStatusData& data);
    void setPlaceholderData();

private:
    void setupUi();
    void setValue(QLabel*& label, const QString& text);

    QLabel* m_timestampValue = nullptr;
    QLabel* m_expectedSteeringValue = nullptr;
    QLabel* m_actualSteeringValue = nullptr;
    QLabel* m_expectedSpeedValue = nullptr;
    QLabel* m_actualSpeedValue = nullptr;
    QLabel* m_expectedAccelerationValue = nullptr;
    QLabel* m_actualAccelerationValue = nullptr;
    QLabel* m_controlModeValue = nullptr;
    QLabel* m_lateralErrorValue = nullptr;
    QLabel* m_longitudinalErrorValue = nullptr;
};
