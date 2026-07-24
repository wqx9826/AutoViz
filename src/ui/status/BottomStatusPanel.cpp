#include "ui/status/BottomStatusPanel.h"

#include <algorithm>
#include <QAbstractItemView>
#include <cmath>
#include <QColor>
#include <QDateTime>
#include <QFont>
#include <QFontDatabase>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPair>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTableWidget>
#include <QTabWidget>
#include <QTabBar>
#include <QStringList>
#include <QtMath>
#include <QVariant>
#include <QVector>
#include <QVBoxLayout>

#include "core/datacenter/DataManager.h"
#include "ui/theme/UiScaleManager.h"
#include "ui/theme/UiThemeManager.h"

namespace {
constexpr qint64 kStatusPanelRefreshMs = 200;

void repolish(QWidget* widget);

QString formatAge(qint64 ageMs)
{
    if (ageMs <= 0) {
        return QStringLiteral("--");
    }
    if (ageMs < 1000) {
        return QStringLiteral("%1 ms").arg(ageMs);
    }
    return QStringLiteral("%1 s").arg(QString::number(static_cast<double>(ageMs) / 1000.0, 'f', 1));
}

QString formatTime(qint64 timestampMs)
{
    if (timestampMs <= 0) {
        return QStringLiteral("--");
    }
    return QDateTime::fromMSecsSinceEpoch(timestampMs).toString(QStringLiteral("HH:mm:ss.zzz"));
}

QString formatFrequency(double frequencyHz)
{
    if (frequencyHz <= 0.0) {
        return QStringLiteral("--");
    }
    return QStringLiteral("%1 Hz").arg(QString::number(frequencyHz, 'f', 1));
}

QString formatNumber(double value, int precision = 3)
{
    return QString::number(value, 'f', precision);
}

QString displayNumber(bool valid, double value, int precision = 3)
{
    return valid ? formatNumber(value, precision) : QStringLiteral("--");
}

QString formatAngleDegrees(double radians, int precision = 1)
{
    return QStringLiteral("%1°").arg(formatNumber(qRadiansToDegrees(radians), precision));
}

QString formatAngularVelocityDegrees(double radiansPerSecond, int precision = 3)
{
    return QStringLiteral("%1°/s").arg(formatNumber(qRadiansToDegrees(radiansPerSecond), precision));
}

QString displayAngleDegrees(bool valid, double radians, int precision = 1)
{
    return valid ? formatAngleDegrees(radians, precision) : QStringLiteral("--");
}

QString displayAngularVelocityDegrees(bool valid, double radiansPerSecond, int precision = 3)
{
    return valid ? formatAngularVelocityDegrees(radiansPerSecond, precision) : QStringLiteral("--");
}

QString displayInt(bool valid, int value)
{
    return valid ? QString::number(value) : QStringLiteral("--");
}

QString displayInt64(bool valid, qint64 value)
{
    return valid ? QString::number(value) : QStringLiteral("--");
}

QString waterTankStatusText(bool valid, int status)
{
    if (!valid) {
        return QStringLiteral("--");
    }

    QString description;
    switch (status) {
    case 0:
        description = QObject::tr("空闲/停止");
        break;
    case 1:
        description = QObject::tr("注水中");
        break;
    case 2:
        description = QObject::tr("排水中");
        break;
    case 3:
        description = QObject::tr("重置中");
        break;
    case 4:
        description = QObject::tr("故障");
        break;
    case 5:
        description = QObject::tr("注水完成");
        break;
    case 6:
        description = QObject::tr("排水完成");
        break;
    default:
        return QObject::tr("未知状态（%1）").arg(status);
    }
    return QStringLiteral("%1（%2）").arg(description).arg(status);
}

QString waterTankLevelText(bool valid, int level, bool isRaw)
{
    if (!valid) {
        return QStringLiteral("--");
    }
    return isRaw ? QStringLiteral("原始电流 %1 mA").arg(level)
                : QStringLiteral("液位 %1%").arg(level);
}

QString displayBool(bool valid, bool value)
{
    if (!valid) {
        return QStringLiteral("--");
    }
    return value ? QObject::tr("是") : QObject::tr("否");
}

QString displayText(bool valid, const QString& text)
{
    if (!valid || text.isEmpty()) {
        return QStringLiteral("--");
    }
    return text;
}

QTableWidgetItem* makeItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    item->setToolTip(text);
    return item;
}

QTableWidgetItem* makeAlignedItem(const QString& text, Qt::Alignment alignment)
{
    auto* item = makeItem(text);
    item->setTextAlignment(alignment);
    return item;
}

QString validText(bool valid)
{
    return valid ? QObject::tr("有效") : QObject::tr("等待数据");
}

QString topicStatusText(bool timedOut, quint64 messageCount)
{
    return messageCount == 0 ? QStringLiteral("等待") : (timedOut ? QStringLiteral("超时") : QStringLiteral("在线"));
}

QString statusObjectName(const QString& text)
{
    if (text.contains(QStringLiteral("在线")) || text.contains(QStringLiteral("有效")) || text.contains(QStringLiteral("正常"))) {
        return QStringLiteral("status-normal");
    }
    if (text.contains(QStringLiteral("超时")) || text.contains(QStringLiteral("错误")) || text.contains(QStringLiteral("故障"))) {
        return QStringLiteral("status-warn");
    }
    return QStringLiteral("status-offline");
}

QWidget* createStatusBadgeWidget(const QString& text, QWidget* parent)
{
    auto* wrapper = new QWidget(parent);
    auto* layout = new QHBoxLayout(wrapper);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* badge = new QLabel(text, wrapper);
    badge->setProperty("class", QVariant(QStringLiteral("status-badge")));
    badge->setObjectName(statusObjectName(text));
    badge->setAlignment(Qt::AlignCenter);
    badge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    layout->addStretch(1);
    layout->addWidget(badge);
    layout->addStretch(1);
    repolish(badge);
    return wrapper;
}

const autoviz::model::TopicStatus* findTopicStatus(const autoviz::model::TopicStatusList& statuses,
                                                   const QString& topicName)
{
    for (const auto& status : statuses) {
        if (status.name == topicName) {
            return &status;
        }
    }
    return nullptr;
}

QString topicStateText(const autoviz::model::TopicStatus* status)
{
    if (status == nullptr || status->messageCount == 0) {
        return QStringLiteral("等待");
    }
    return status->timedOut ? QStringLiteral("超时/未更新") : QStringLiteral("在线");
}

QString compactTopicStateText(const autoviz::model::TopicStatus* status)
{
    if (status == nullptr || status->messageCount == 0) {
        return QStringLiteral("等待");
    }
    return status->timedOut ? QStringLiteral("超时") : QStringLiteral("在线");
}

QString dataSourceText(autoviz::datacenter::VisualizationInputSource inputSource)
{
    switch (inputSource) {
    case autoviz::datacenter::VisualizationInputSource::Ros1:
        return QStringLiteral("ROS1 实时数据");
    case autoviz::datacenter::VisualizationInputSource::Ros2:
        return QStringLiteral("ROS2 实时数据");
    case autoviz::datacenter::VisualizationInputSource::Mock:
    default:
        return QStringLiteral("内部 Mock 数据");
    }
}

bool isCrawlMode(const autoviz::model::ControlCommandStatus& control);
bool isSailingMode(const autoviz::model::ControlCommandStatus& control);
QString targetSpeedText(const autoviz::model::ControlCommandStatus& control);
QString gearText(bool valid, int gear);

QString actionStateText(bool valid, int state)
{
    if (!valid) {
        return QStringLiteral("--");
    }
    switch (state) {
    case 0:
        return QStringLiteral("空闲");
    case 1:
        return QStringLiteral("执行中");
    case 2:
        return QStringLiteral("取消中");
    case 3:
        return QStringLiteral("已完成");
    case 4:
        return QStringLiteral("已中止");
    default:
        return QStringLiteral("未知(%1)").arg(state);
    }
}

QString actionOwnerText(bool valid, int owner)
{
    if (!valid) {
        return QStringLiteral("--");
    }
    switch (owner) {
    case 0:
        return QStringLiteral("无");
    case 1:
        return QStringLiteral("Move");
    case 2:
        return QStringLiteral("Depth");
    default:
        return QStringLiteral("未知(%1)").arg(owner);
    }
}

double normalizeAngleRadians(double radians)
{
    while (radians > M_PI) {
        radians -= 2.0 * M_PI;
    }
    while (radians < -M_PI) {
        radians += 2.0 * M_PI;
    }
    return radians;
}

int explicitChassisFaultCount(const autoviz::model::ChassisRuntimeStatus& chassis)
{
    if (!chassis.valid) {
        return -1;
    }

    int count = 0;
    const bool waterActuatorFault = chassis.leftTailActuatorStatus != 0
                                    || chassis.rightTailActuatorStatus != 0
                                    || chassis.leftVerticalActuatorStatus != 0
                                    || chassis.rightVerticalActuatorStatus != 0
                                    || chassis.backVerticalActuatorStatus != 0;
    const bool crawlControllerFault = chassis.leftCrawlActuatorFaultCode != 0
                                      || chassis.rightCrawlActuatorFaultCode != 0;
    const bool crawlMotorFault = chassis.leftCrawlMotor.fault
                                 || chassis.rightCrawlMotor.fault
                                 || chassis.leftCrawlMotor.faultCode != 0
                                 || chassis.rightCrawlMotor.faultCode != 0;
    const bool bmsFault = chassis.highVoltageBmsStatus != 0
                          || (chassis.bms.valid && chassis.bms.selfCheckStatus != 0);
    const bool bmsWarning = chassis.bms.valid && std::any_of(chassis.bms.warningCodes.cbegin(),
                                                              chassis.bms.warningCodes.cend(),
                                                              [](int code) { return code != 0; });

    count += waterActuatorFault ? 1 : 0;
    count += crawlControllerFault ? 1 : 0;
    count += crawlMotorFault ? 1 : 0;
    count += bmsFault ? 1 : 0;
    count += bmsWarning ? 1 : 0;
    count += chassis.waterTankStatus == 4 ? 1 : 0;
    return count;
}

QString chassisHealthText(const autoviz::model::ChassisRuntimeStatus& chassis,
                          const autoviz::model::TopicStatus* topic)
{
    if (topic == nullptr || topic->messageCount == 0) {
        return QStringLiteral("等待底盘消息");
    }
    if (topic->timedOut) {
        return QStringLiteral("底盘消息超时");
    }
    if (!chassis.valid) {
        return QStringLiteral("等待底盘状态");
    }
    return QStringLiteral("消息在线 / 显式故障 %1").arg(explicitChassisFaultCount(chassis));
}

QString bmsSummaryText(const autoviz::model::ChassisRuntimeStatus& chassis)
{
    if (!chassis.valid) {
        return QStringLiteral("--");
    }

    const int selfCheckStatus = chassis.bms.valid ? chassis.bms.selfCheckStatus : chassis.highVoltageBmsStatus;
    const int soc = chassis.bms.valid ? chassis.bms.soc : chassis.highVoltageBmsSocStatus;
    int warningCount = 0;
    if (chassis.bms.valid) {
        for (const int code : chassis.bms.warningCodes) {
            warningCount += code != 0 ? 1 : 0;
        }
    }
    return QStringLiteral("自检 %1 / 告警 %2 / SOC %3%")
        .arg(selfCheckStatus == 0 ? QStringLiteral("正常") : QStringLiteral("故障"))
        .arg(warningCount)
        .arg(soc);
}

QString powerSummaryText(const autoviz::model::ChassisRuntimeStatus& chassis)
{
    if (!chassis.valid) {
        return QStringLiteral("--");
    }
    const QString channelText = chassis.powerSupplyStatuses.isEmpty()
                                    ? QStringLiteral("通道状态未映射")
                                    : QStringLiteral("%1 路状态码").arg(chassis.powerSupplyStatuses.size());
    return QStringLiteral("输入 %1 V / %2").arg(formatNumber(chassis.smartPowerInputVoltageStatus, 1), channelText);
}

QString motorFeedbackText(const autoviz::model::CrawlMotorRuntimeStatus& motor)
{
    if (!motor.valid) {
        return QStringLiteral("--");
    }
    return QStringLiteral("%1 rpm / %2 °C / %3 V / 故障码 %4")
        .arg(formatNumber(motor.speedRpm, 1))
        .arg(motor.temperature)
        .arg(formatNumber(motor.busVoltage, 1))
        .arg(motor.faultCode);
}

QString motorControllerText(const autoviz::model::CrawlMotorRuntimeStatus& left,
                            const autoviz::model::CrawlMotorRuntimeStatus& right)
{
    if (!left.valid && !right.valid) {
        return QStringLiteral("--");
    }
    return QStringLiteral("左 就绪/%1 / 输出/%2；右 就绪/%3 / 输出/%4")
        .arg(left.controllerReady ? QStringLiteral("是") : QStringLiteral("否"))
        .arg(left.outputEnabled ? QStringLiteral("是") : QStringLiteral("否"))
        .arg(right.controllerReady ? QStringLiteral("是") : QStringLiteral("否"))
        .arg(right.outputEnabled ? QStringLiteral("是") : QStringLiteral("否"));
}

QString motorCommandText(const autoviz::model::CrawlMotorRuntimeStatus& left,
                         const autoviz::model::CrawlMotorRuntimeStatus& right)
{
    if (!left.valid && !right.valid) {
        return QStringLiteral("--");
    }
    return QStringLiteral("左 %1 / %2 / %3 rpm；右 %4 / %5 / %6 rpm")
        .arg(left.commandEnable ? QStringLiteral("使能") : QStringLiteral("关闭"))
        .arg(left.commandSpeedMode ? QStringLiteral("速度") : QStringLiteral("转矩"))
        .arg(formatNumber(left.commandSpeedRpm, 1))
        .arg(right.commandEnable ? QStringLiteral("使能") : QStringLiteral("关闭"))
        .arg(right.commandSpeedMode ? QStringLiteral("速度") : QStringLiteral("转矩"))
        .arg(formatNumber(right.commandSpeedRpm, 1));
}

QString bmsPackText(const autoviz::model::ChassisRuntimeStatus& chassis)
{
    if (!chassis.valid || !chassis.bms.valid) {
        return QStringLiteral("--");
    }
    return QStringLiteral("%1 V / %2 A / 状态码 %3")
        .arg(formatNumber(chassis.bms.packVoltage, 2))
        .arg(formatNumber(chassis.bms.packCurrent, 2))
        .arg(chassis.bms.currentStatus);
}

QString bmsCellText(const autoviz::model::ChassisRuntimeStatus& chassis)
{
    if (!chassis.valid || !chassis.bms.valid) {
        return QStringLiteral("--");
    }
    return QStringLiteral("最高 %1 V (#%2) / 最低 %3 V (#%4)")
        .arg(formatNumber(chassis.bms.maxCellVoltage, 3))
        .arg(chassis.bms.maxCellVoltageIndex)
        .arg(formatNumber(chassis.bms.minCellVoltage, 3))
        .arg(chassis.bms.minCellVoltageIndex);
}

QString bmsTemperatureText(const autoviz::model::ChassisRuntimeStatus& chassis)
{
    if (!chassis.valid || !chassis.bms.valid) {
        return QStringLiteral("--");
    }
    return QStringLiteral("最高 %1 °C (#%2) / 最低 %3 °C (#%4)")
        .arg(chassis.bms.maxTemperature)
        .arg(chassis.bms.maxTemperatureIndex)
        .arg(chassis.bms.minTemperature)
        .arg(chassis.bms.minTemperatureIndex);
}

QString bmsWarningCodesText(const autoviz::model::ChassisRuntimeStatus& chassis)
{
    if (!chassis.valid || !chassis.bms.valid || chassis.bms.warningCodes.isEmpty()) {
        return QStringLiteral("--");
    }
    QStringList codes;
    for (const int code : chassis.bms.warningCodes) {
        codes.push_back(QString::number(code));
    }
    return QStringLiteral("等级 %1 / 原始码 [%2]").arg(chassis.bms.alarmLevel).arg(codes.join(QStringLiteral(", ")));
}

QString powerSupplyStatusText(const autoviz::model::ChassisRuntimeStatus& chassis)
{
    if (!chassis.valid || chassis.powerSupplyStatuses.isEmpty()) {
        return QStringLiteral("--");
    }
    QStringList codes;
    for (int index = 0; index < chassis.powerSupplyStatuses.size(); ++index) {
        codes.push_back(QStringLiteral("%1:%2").arg(index + 1).arg(chassis.powerSupplyStatuses.at(index)));
    }
    return codes.join(QStringLiteral(" / "));
}

QString pathStatusText(const autoviz::model::PathRuntimeStatus& path)
{
    return path.valid ? QStringLiteral("有效 / %1 点").arg(path.pointCount) : QStringLiteral("等待");
}

QString pathBindingText(const autoviz::model::PathRuntimeStatus& path,
                        const autoviz::model::ActionRuntimeStatus& action)
{
    if (!path.valid) {
        return QStringLiteral("无局部路径");
    }
    if (path.goalUuid.isEmpty() || action.goalUuid.isEmpty()) {
        return QStringLiteral("UUID 未提供");
    }
    return path.goalUuid == action.goalUuid ? QStringLiteral("匹配") : QStringLiteral("失配");
}

QString headingSummaryText(const autoviz::model::LocalizationStatus& localization,
                           const autoviz::model::ControlCommandStatus& control,
                           const autoviz::model::ActionRuntimeStatus& action)
{
    double targetHeading = 0.0;
    bool hasTarget = false;
    if (isSailingMode(control)) {
        targetHeading = control.heading;
        hasTarget = true;
    } else if (action.valid && autoviz::model::isSailingChassisMode(action.chassisMode)) {
        targetHeading = action.targetHeading;
        hasTarget = true;
    }

    const QString feedback = displayAngleDegrees(localization.valid, localization.heading);
    if (!hasTarget) {
        return feedback == QStringLiteral("--") ? QStringLiteral("--")
                                                 : QStringLiteral("反馈 %1").arg(feedback);
    }
    const QString command = formatAngleDegrees(targetHeading);
    if (!localization.valid) {
        return QStringLiteral("下发 %1 / 反馈 --").arg(command);
    }

    const double error = normalizeAngleRadians(targetHeading - localization.heading);
    return QStringLiteral("下发 %1 / 反馈 %2 / 误差 %3")
        .arg(command)
        .arg(feedback)
        .arg(formatAngleDegrees(error));
}

QString angularVelocitySummaryText(const autoviz::model::LocalizationStatus& localization,
                                   const autoviz::model::ChassisRuntimeStatus& chassis,
                                   const autoviz::model::ControlCommandStatus& control,
                                   const autoviz::model::ActionRuntimeStatus& action)
{
    double target = 0.0;
    bool hasTarget = false;
    if (isCrawlMode(control)) {
        target = control.angularVelocity;
        hasTarget = true;
    } else if (action.valid && autoviz::model::isCrawlChassisMode(action.chassisMode)) {
        target = action.targetAngularVelocity;
        hasTarget = true;
    }
    const QString feedback = chassis.valid
                                 ? displayAngularVelocityDegrees(true, chassis.currentAngularVelocity)
                                 : displayAngularVelocityDegrees(localization.valid, localization.omegaZ);
    if (!hasTarget && feedback == QStringLiteral("--")) {
        return QStringLiteral("--");
    }
    return hasTarget ? QStringLiteral("下发 %1 / 反馈 %2").arg(formatAngularVelocityDegrees(target), feedback)
                     : QStringLiteral("反馈 %1").arg(feedback);
}

QString speedSummaryText(const autoviz::model::ControlCommandStatus& control,
                         const autoviz::model::LocalizationStatus& localization,
                         const autoviz::model::ChassisRuntimeStatus& chassis,
                         const autoviz::model::ActionRuntimeStatus& action)
{
    QString target = targetSpeedText(control);
    if (target == QStringLiteral("--") && action.valid) {
        target = formatNumber(action.targetSpeed, 2);
    }
    QString actual;
    if (isCrawlMode(control) || (action.valid && autoviz::model::isCrawlChassisMode(action.chassisMode))) {
        actual = displayNumber(chassis.valid, chassis.currentSpeed, 2);
    } else {
        actual = displayNumber(localization.valid, localization.velocity, 2);
    }
    if (target == QStringLiteral("--")) {
        return actual == QStringLiteral("--") ? QStringLiteral("--") : QStringLiteral("反馈 %1 m/s").arg(actual);
    }
    return QStringLiteral("下发 %1 / 反馈 %2 m/s").arg(target, actual);
}

QString gearSummaryText(const autoviz::model::ControlCommandStatus& control,
                        const autoviz::model::ChassisRuntimeStatus& chassis)
{
    const QString target = gearText(control.valid, control.expectedGear);
    const QString actual = gearText(chassis.valid, chassis.gearStatus);
    if (target == QStringLiteral("--") && actual == QStringLiteral("--")) {
        return QStringLiteral("--");
    }
    return QStringLiteral("下发 %1 / 反馈 %2").arg(target, actual);
}

QString linkSummaryText(const autoviz::datacenter::VisualizationSnapshot& snapshot)
{
    const auto* location = findTopicStatus(snapshot.topicStatuses, QStringLiteral("/location"));
    const auto* control = findTopicStatus(snapshot.topicStatuses, QStringLiteral("/chassis_command"));
    const auto* chassis = findTopicStatus(snapshot.topicStatuses, QStringLiteral("/chassis_states"));
    const auto* action = findTopicStatus(snapshot.topicStatuses, QStringLiteral("/system_run_states"));
    return QStringLiteral("定位 %1 / 控制 %2 / 底盘 %3 / 任务 %4")
        .arg(compactTopicStateText(location), compactTopicStateText(control), compactTopicStateText(chassis), compactTopicStateText(action));
}

QString overallStateText(const autoviz::datacenter::VisualizationSnapshot& snapshot)
{
    if (snapshot.runtimeStatus.inputSource == autoviz::datacenter::VisualizationInputSource::Mock) {
        return QStringLiteral("Mock 仿真在线");
    }

    const auto* location = findTopicStatus(snapshot.topicStatuses, QStringLiteral("/location"));
    if (location == nullptr || location->messageCount == 0) {
        return QStringLiteral("等待实时数据");
    }
    if (location->timedOut) {
        return QStringLiteral("定位超时");
    }

    if (snapshot.taskRuntimeStatus.valid && snapshot.taskRuntimeStatus.emergencyStop) {
        return QStringLiteral("急停");
    }

    const auto* chassis = findTopicStatus(snapshot.topicStatuses, QStringLiteral("/chassis_states"));
    if (chassis != nullptr && chassis->messageCount > 0 && chassis->timedOut) {
        return QStringLiteral("底盘反馈超时");
    }

    const auto* command = findTopicStatus(snapshot.topicStatuses, QStringLiteral("/chassis_command"));
    if (command != nullptr && command->messageCount > 0 && command->timedOut
        && snapshot.actionRuntimeStatus.valid && snapshot.actionRuntimeStatus.isEnable) {
        return QStringLiteral("控制命令超时");
    }

    if (snapshot.actionRuntimeStatus.valid && snapshot.actionRuntimeStatus.state == 4) {
        return QStringLiteral("任务已中止");
    }

    const bool crawlMotion = (snapshot.actionRuntimeStatus.valid
                              && autoviz::model::isCrawlChassisMode(snapshot.actionRuntimeStatus.chassisMode))
                             || (snapshot.controlCommandStatus.valid
                                 && autoviz::model::isCrawlChassisMode(snapshot.controlCommandStatus.mode));
    const auto* finalObjects = findTopicStatus(snapshot.topicStatuses, QStringLiteral("/targets/final_objects"));
    if (crawlMotion && finalObjects != nullptr && finalObjects->messageCount > 0 && finalObjects->timedOut) {
        return QStringLiteral("障碍物输入超时");
    }

    if (explicitChassisFaultCount(snapshot.chassisRuntimeStatus) > 0) {
        return QStringLiteral("底盘显式故障");
    }

    return QStringLiteral("在线");
}

QString chassisModeText(bool valid, int mode)
{
    if (!valid) {
        return QStringLiteral("--");
    }
    switch (mode) {
    case 0:
        return QStringLiteral("无效");
    case 1:
        return QStringLiteral("自主航行浮潜-深度");
    case 2:
        return QStringLiteral("自主航行浮潜-高度");
    case 3:
        return QStringLiteral("自主航行-浮力调节");
    case 4:
        return QStringLiteral("自主航行-定深");
    case 5:
        return QStringLiteral("自主航行-定高");
    case 6:
        return QStringLiteral("自主爬行");
    case 7:
        return QStringLiteral("遥控航行");
    case 8:
        return QStringLiteral("遥控爬行");
    case 9:
        return QStringLiteral("水推设备测试");
    case 10:
        return QStringLiteral("航行中心转向");
    case 11:
        return QStringLiteral("爬行中心转向");
    default:
        return QStringLiteral("未知(%1)").arg(mode);
    }
}

QString naviModeText(bool valid, int mode)
{
    if (!valid) {
        return QStringLiteral("--");
    }
    switch (mode) {
    case 0:
        return QStringLiteral("无效");
    case 1:
        return QStringLiteral("定深");
    case 2:
        return QStringLiteral("定高");
    default:
        return QStringLiteral("未知(%1)").arg(mode);
    }
}

QString taskTypeText(bool valid, int taskType)
{
    if (!valid) {
        return QStringLiteral("--");
    }
    switch (taskType) {
    case 0:
        return QStringLiteral("无效");
    case 1:
        return QStringLiteral("遥控模式");
    case 2:
        return QStringLiteral("自主模式");
    default:
        return QStringLiteral("未知(%1)").arg(taskType);
    }
}

QString taskTypeAndIdText(const autoviz::model::TaskRuntimeStatus& task)
{
    return task.valid ? QStringLiteral("%1 / %2").arg(taskTypeText(true, task.taskType)).arg(task.taskId)
                      : QStringLiteral("--");
}

QString gearText(bool valid, int gear)
{
    if (!valid) {
        return QStringLiteral("--");
    }
    switch (gear) {
    case 0:
        return QStringLiteral("N 档");
    case 1:
        return QStringLiteral("D 档");
    case 2:
        return QStringLiteral("R 档");
    case 3:
        return QStringLiteral("P 档");
    case 4:
        return QStringLiteral("中心转向");
    case 5:
        return QStringLiteral("档位紧急停止");
    default:
        return QStringLiteral("未知(%1)").arg(gear);
    }
}

bool isCrawlMode(const autoviz::model::ControlCommandStatus& control)
{
    return control.valid && autoviz::model::isCrawlChassisMode(control.mode);
}

bool isSailingMode(const autoviz::model::ControlCommandStatus& control)
{
    return control.valid && autoviz::model::isSailingChassisMode(control.mode);
}

QString targetSpeedText(const autoviz::model::ControlCommandStatus& control)
{
    if (isCrawlMode(control)) {
        return formatNumber(control.speed, 2);
    }
    if (isSailingMode(control)) {
        return formatNumber(control.speed, 2);
    }
    return QStringLiteral("--");
}

bool isOverviewBadgeKey(const QString& key)
{
    return key == QStringLiteral("system.state")
           || key == QStringLiteral("system.links")
           || key == QStringLiteral("system.action")
           || key == QStringLiteral("command.state")
           || key == QStringLiteral("path.global")
           || key == QStringLiteral("path.local")
           || key == QStringLiteral("path.binding")
           || key == QStringLiteral("obstacle.state")
           || key == QStringLiteral("hardware.feedback")
           || key == QStringLiteral("hardware.faults")
           || key == QStringLiteral("hardware.bms");
}

QFont overviewValueFont()
{
    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    static const QString family = []() {
        const QStringList candidates = {
            QStringLiteral("JetBrains Mono"),
            QStringLiteral("Roboto Mono"),
            QStringLiteral("Consolas"),
            QStringLiteral("DejaVu Sans Mono"),
            QStringLiteral("Monospace"),
        };
        const QStringList families = QFontDatabase().families();
        for (const auto& candidate : candidates) {
            if (families.contains(candidate, Qt::CaseInsensitive)) {
                return candidate;
            }
        }
        return QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    }();

    QFont font = scale.font(scale.fontSizeNormal(), QFont::DemiBold);
    font.setFamily(family);
    font.setStyleHint(QFont::Monospace);
    return font;
}

QString overviewBadgeObjectName(const QString& text)
{
    const bool hasFault = text.contains(QStringLiteral("故障"))
                          && !text.contains(QStringLiteral("故障 0"))
                          && !text.contains(QStringLiteral("故障项 0"))
                          && !text.contains(QStringLiteral("显式故障 0"));
    const bool hasWarning = text.contains(QStringLiteral("告警")) && !text.contains(QStringLiteral("告警 0"));
    if (text.contains(QStringLiteral("急停")) ||
        text.contains(QStringLiteral("超时")) ||
        text.contains(QStringLiteral("失配")) ||
        text.contains(QStringLiteral("已中止")) ||
        text.contains(QStringLiteral("取消中")) ||
        hasFault ||
        hasWarning ||
        text == QStringLiteral("是")) {
        return QStringLiteral("status-warn");
    }
    if (text.contains(QStringLiteral("等待")) || text.contains(QStringLiteral("未提供")) || text == QStringLiteral("--")) {
        return QStringLiteral("status-offline");
    }
    if (text.contains(QStringLiteral("在线")) || text.contains(QStringLiteral("有效"))
        || text.contains(QStringLiteral("正常")) || text.contains(QStringLiteral("匹配"))
        || text.contains(QStringLiteral("执行中"))) {
        return QStringLiteral("status-normal");
    }
    return QString();
}

void repolish(QWidget* widget)
{
    if (widget == nullptr) {
        return;
    }
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}
}  // namespace

BottomStatusPanel::BottomStatusPanel(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setupUi();
}

QSize BottomStatusPanel::sizeHint() const
{
    const QSize base = QWidget::sizeHint();
    if (m_tabs == nullptr || m_overviewContent == nullptr) {
        return base;
    }

    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    const QMargins margins = contentsMargins();
    const int tabBarHeight = m_tabs->tabBar() != nullptr ? m_tabs->tabBar()->sizeHint().height() : scale.scaled(28);
    const int expandedOverviewHeight = m_overviewContent->layout() != nullptr
                                           ? m_overviewContent->layout()->sizeHint().height()
                                           : m_overviewContent->sizeHint().height();
    const int height = margins.top() + margins.bottom() + tabBarHeight + expandedOverviewHeight + scale.scaled(14);
    return QSize(qMax(base.width(), m_overviewContent->sizeHint().width()), height);
}

QSize BottomStatusPanel::minimumSizeHint() const
{
    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    const QSize base = QWidget::minimumSizeHint();
    return QSize(base.width(), scale.scaled(112));
}

void BottomStatusPanel::appendLog(const QString& message)
{
    if (m_logOutput == nullptr) {
        return;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"));
    m_logOutput->appendPlainText(QStringLiteral("[%1] %2").arg(timestamp, message));
}

void BottomStatusPanel::updateSnapshot(const autoviz::datacenter::VisualizationSnapshot& snapshot)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_lastStatusRefreshMs > 0 && nowMs - m_lastStatusRefreshMs < kStatusPanelRefreshMs) {
        return;
    }
    m_lastStatusRefreshMs = nowMs;
    updateOverview(snapshot);
    updateTopicTable(snapshot);
    updateStateTabs(snapshot);
}

void BottomStatusPanel::setupUi()
{
    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(scale.scaled(4), scale.scaled(4), scale.scaled(4), scale.scaled(4));
    layout->setSpacing(scale.scaled(2));

    m_tabs = new QTabWidget(this);
    m_tabs->setFont(scale.font(scale.fontSizeNormal()));
    m_tabs->setDocumentMode(true);
    m_tabs->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    layout->addWidget(m_tabs, 1);

    setupOverviewTab();
    setupDetailsTab();
    setupLogTab();
    m_tabs->setCurrentIndex(0);
}

void BottomStatusPanel::setupOverviewTab()
{
    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    m_overviewScrollArea = new QScrollArea(m_tabs);
    m_overviewScrollArea->setWidgetResizable(true);
    m_overviewScrollArea->setFrameShape(QFrame::NoFrame);
    m_overviewScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_overviewScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_overviewScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* tab = new QWidget(m_overviewScrollArea);
    m_overviewContent = tab;
    auto* layout = new QGridLayout(tab);
    layout->setContentsMargins(scale.scaled(4), scale.scaled(4), scale.scaled(4), scale.scaled(4));
    layout->setHorizontalSpacing(scale.scaled(4));
    layout->setVerticalSpacing(scale.scaled(2));

    layout->addWidget(createOverviewGroup(tab,
                                          tr("任务与链路"),
                                          {{QStringLiteral("system.source"), tr("数据源")},
                                           {QStringLiteral("system.state"), tr("当前状态")},
                                           {QStringLiteral("system.mode"), tr("运行模式")},
                                           {QStringLiteral("system.action"), tr("任务执行")},
                                           {QStringLiteral("system.owner"), tr("控制归属")},
                                           {QStringLiteral("system.task"), tr("任务类型/ID")},
                                           {QStringLiteral("system.links"), tr("关键链路")},
                                           {QStringLiteral("system.estop"), tr("急停状态")},
                                           {QStringLiteral("system.enable"), tr("动作使能")}}),
                      0,
                      0);
    layout->addWidget(createOverviewGroup(tab,
                                          tr("当前运动"),
                                          {{QStringLiteral("control.mode"), tr("底盘模式")},
                                           {QStringLiteral("control.speed"), tr("当前速度 (m/s)")},
                                           {QStringLiteral("control.heading"), tr("当前航向 (°)")},
                                           {QStringLiteral("control.angular"), tr("当前角速度 (°/s)")},
                                           {QStringLiteral("control.gear"), tr("当前档位")},
                                           {QStringLiteral("pose.age"), tr("定位延迟")}}),
                      0,
                      1);
    layout->addWidget(createOverviewGroup(tab,
                                          tr("控制指令"),
                                          {{QStringLiteral("command.speed"), tr("速度（下发/反馈）")},
                                           {QStringLiteral("command.heading"), tr("航向（下发/反馈）")},
                                           {QStringLiteral("command.angular"), tr("角速度（下发/反馈）")},
                                           {QStringLiteral("command.gear"), tr("档位（下发/反馈）")},
                                           {QStringLiteral("command.state"), tr("指令状态")}}),
                      0,
                      2);
    layout->addWidget(createOverviewGroup(tab,
                                          tr("垂向与浮力"),
                                          {{QStringLiteral("vertical.navi_mode"), tr("航行依赖")},
                                           {QStringLiteral("vertical.depth"), tr("深度 (m)")},
                                           {QStringLiteral("vertical.height"), tr("高度 (m)")},
                                           {QStringLiteral("vertical.tank_level"), tr("水箱液位")},
                                           {QStringLiteral("vertical.tank_state"), tr("水箱状态")}}),
                      1,
                      0);
    layout->addWidget(createOverviewGroup(tab,
                                          tr("路径与感知"),
                                          {{QStringLiteral("path.global"), tr("全局路径")},
                                           {QStringLiteral("path.local"), tr("局部路径")},
                                           {QStringLiteral("path.binding"), tr("任务/路径绑定")},
                                           {QStringLiteral("obstacle.state"), tr("Topic 状态")},
                                           {QStringLiteral("obstacle.count"), tr("障碍物数量")},
                                           {QStringLiteral("obstacle.age"), tr("Topic 延迟")}}),
                      1,
                      1);
    layout->addWidget(createOverviewGroup(tab,
                                          tr("硬件健康"),
                                          {{QStringLiteral("hardware.feedback"), tr("底盘反馈")},
                                           {QStringLiteral("hardware.faults"), tr("显式故障项")},
                                           {QStringLiteral("hardware.bms"), tr("BMS")},
                                           {QStringLiteral("hardware.dcdc"), tr("DCDC")},
                                           {QStringLiteral("hardware.power"), tr("配电输入")},
                                           {QStringLiteral("hardware.heartbeat"), tr("控制器心跳")}}),
                      1,
                      2);
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(2, 1);
    layout->setRowStretch(0, 1);
    layout->setRowStretch(1, 1);

    m_overviewScrollArea->setWidget(tab);
    m_tabs->addTab(m_overviewScrollArea, tr("运动总览"));
}

QGroupBox* BottomStatusPanel::createOverviewGroup(QWidget* parent,
                                                  const QString& title,
                                                  const QVector<QPair<QString, QString>>& fields)
{
    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    auto* group = new QGroupBox(title, parent);
    group->setFont(scale.font(scale.fontSizeNormal(), QFont::Bold));

    auto* layout = new QGridLayout(group);
    layout->setContentsMargins(scale.scaled(4), scale.scaled(4), scale.scaled(4), scale.scaled(4));
    layout->setHorizontalSpacing(scale.scaled(4));
    layout->setVerticalSpacing(scale.scaled(2));
    layout->setColumnStretch(0, 0);
    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(2, 0);
    layout->setColumnMinimumWidth(2, scale.scaled(132));

    int row = 0;
    for (const auto& field : fields) {
        auto* keyLabel = new QLabel(field.second, group);
        keyLabel->setFont(scale.font(scale.fontSizeNormal()));
        keyLabel->setProperty("class", QVariant(QStringLiteral("status-key")));
        keyLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        keyLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);

        auto* valueLabel = new QLabel(QStringLiteral("--"), group);
        valueLabel->setFont(overviewValueFont());
        valueLabel->setProperty("class", QVariant(isOverviewBadgeKey(field.first) ? QStringLiteral("status-badge") : QStringLiteral("status-value")));
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valueLabel->setTextInteractionFlags(Qt::NoTextInteraction);
        valueLabel->setMinimumWidth(scale.scaled(132));
        valueLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        layout->addWidget(keyLabel, row, 0, Qt::AlignLeft | Qt::AlignVCenter);
        layout->addWidget(valueLabel, row, 2, Qt::AlignRight | Qt::AlignVCenter);
        m_overviewValues.insert(field.first, valueLabel);
        ++row;
    }

    return group;
}

void BottomStatusPanel::setOverviewValue(const QString& key, const QString& value)
{
    auto* label = m_overviewValues.value(key, nullptr);
    if (label == nullptr) {
        return;
    }
    label->setText(value);
    if (isOverviewBadgeKey(key)) {
        label->setProperty("class", QVariant(QStringLiteral("status-badge")));
        label->setObjectName(overviewBadgeObjectName(value));
        label->setAlignment(Qt::AlignCenter);
    } else {
        label->setProperty("class", QVariant(QStringLiteral("status-value")));
        label->setObjectName(QString());
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    repolish(label);
}

QWidget* BottomStatusPanel::createDetailTab(const QString& title,
                                            const QVector<QPair<QString, QVector<QPair<QString, QString>>>>& groups,
                                            int columns)
{
    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    auto* scrollArea = new QScrollArea(m_detailTabs);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* content = new QWidget(scrollArea);
    auto* layout = new QGridLayout(content);
    layout->setContentsMargins(scale.marginNormal(), scale.marginNormal(), scale.marginNormal(), scale.marginNormal());
    layout->setHorizontalSpacing(scale.spacingNormal());
    layout->setVerticalSpacing(scale.spacingNormal());

    const int columnCount = qMax(1, columns);
    for (int index = 0; index < groups.size(); ++index) {
        const int row = index / columnCount;
        const int column = index % columnCount;
        layout->addWidget(createDetailGroup(content, groups.at(index).first, groups.at(index).second), row, column);
    }
    for (int column = 0; column < columnCount; ++column) {
        layout->setColumnStretch(column, 1);
    }
    layout->setRowStretch((groups.size() + columnCount - 1) / columnCount, 1);

    scrollArea->setWidget(content);
    m_detailTabs->addTab(scrollArea, title);
    return scrollArea;
}

QGroupBox* BottomStatusPanel::createDetailGroup(QWidget* parent,
                                                const QString& title,
                                                const QVector<QPair<QString, QString>>& fields)
{
    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    auto* group = new QGroupBox(title, parent);
    group->setFont(scale.font(scale.fontSizeNormal(), QFont::Bold));

    auto* layout = new QGridLayout(group);
    layout->setContentsMargins(scale.scaled(12), scale.scaled(8), scale.scaled(12), scale.scaled(8));
    layout->setHorizontalSpacing(scale.spacingNormal());
    layout->setVerticalSpacing(scale.scaled(4));
    layout->setColumnStretch(0, 0);
    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(2, 0);
    layout->setColumnMinimumWidth(2, scale.scaled(150));

    int row = 0;
    for (const auto& field : fields) {
        auto* keyLabel = new QLabel(field.second, group);
        keyLabel->setFont(scale.font(scale.fontSizeNormal()));
        keyLabel->setProperty("class", QVariant(QStringLiteral("status-key")));
        keyLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        keyLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);

        auto* valueLabel = new QLabel(QStringLiteral("--"), group);
        valueLabel->setFont(overviewValueFont());
        valueLabel->setProperty("class", QVariant(QStringLiteral("status-value")));
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        valueLabel->setMinimumWidth(scale.scaled(150));
        valueLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

        layout->addWidget(keyLabel, row, 0, Qt::AlignLeft | Qt::AlignVCenter);
        layout->addWidget(valueLabel, row, 2, Qt::AlignRight | Qt::AlignVCenter);
        m_detailValues.insert(field.first, valueLabel);
        ++row;
    }

    return group;
}

void BottomStatusPanel::setDetailValue(const QString& key, const QString& value)
{
    auto* label = m_detailValues.value(key, nullptr);
    if (label == nullptr) {
        return;
    }
    label->setText(value);
    label->setToolTip(value);
}

void BottomStatusPanel::setupLogTab()
{
    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    auto* tab = new QWidget(m_tabs);
    auto* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(scale.marginNormal(), scale.marginNormal(), scale.marginNormal(), scale.marginNormal());
    layout->setSpacing(scale.spacingNormal());

    m_logOutput = new QPlainTextEdit(tab);
    m_logOutput->setFont(scale.font(scale.fontSizeSmall()));
    m_logOutput->setReadOnly(true);
    m_logOutput->setMaximumBlockCount(2000);
    m_logOutput->setPlaceholderText(tr("运行日志将在此显示。"));
    layout->addWidget(m_logOutput, 1);

    m_tabs->addTab(tab, tr("日志"));
}

void BottomStatusPanel::setupDetailsTab()
{
    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    auto* tab = new QWidget(m_tabs);
    auto* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(scale.marginNormal(), scale.marginNormal(), scale.marginNormal(), scale.marginNormal());
    layout->setSpacing(scale.spacingNormal());

    m_detailTabs = new QTabWidget(tab);
    m_detailTabs->setFont(scale.font(scale.fontSizeNormal()));
    m_detailTabs->setDocumentMode(true);
    layout->addWidget(m_detailTabs, 1);

    setupTopicTab();
    setupStateTabs();

    m_tabs->addTab(tab, tr("详细信息"));
}

void BottomStatusPanel::setupTopicTab()
{
    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    auto* tab = new QWidget(m_detailTabs);
    auto* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(scale.marginNormal(), scale.marginNormal(), scale.marginNormal(), scale.marginNormal());
    layout->setSpacing(scale.spacingNormal());

    m_topicSummaryLabel = new QLabel(tr("等待订阅状态。"), tab);
    m_topicSummaryLabel->setFont(scale.font(scale.fontSizeNormal()));
    m_topicSummaryLabel->setProperty("class", QVariant(QStringLiteral("status-key")));
    layout->addWidget(m_topicSummaryLabel);

    m_topicTable = new QTableWidget(tab);
    m_topicTable->setObjectName(QStringLiteral("topicDashboardTable"));
    m_topicTable->setFont(scale.font(scale.fontSizeNormal()));
    m_topicTable->setColumnCount(7);
    m_topicTable->setHorizontalHeaderLabels({tr("Topic"),
                                             tr("类型"),
                                             tr("状态"),
                                             tr("最后更新"),
                                             tr("延迟"),
                                             tr("频率"),
                                             tr("计数")});
    m_topicTable->verticalHeader()->setVisible(false);
    m_topicTable->setShowGrid(false);
    m_topicTable->setAlternatingRowColors(false);
    m_topicTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_topicTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_topicTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_topicTable->setWordWrap(false);
    m_topicTable->setTextElideMode(Qt::ElideMiddle);
    m_topicTable->verticalHeader()->setDefaultSectionSize(scale.scaled(34));
    m_topicTable->verticalHeader()->setMinimumSectionSize(scale.scaled(32));
    m_topicTable->horizontalHeader()->setStretchLastSection(false);
    m_topicTable->horizontalHeader()->setHighlightSections(false);
    m_topicTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_topicTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_topicTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_topicTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_topicTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_topicTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_topicTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    m_topicTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
    m_topicTable->setColumnWidth(2, scale.scaled(92));
    m_topicTable->setColumnWidth(3, scale.scaled(118));
    m_topicTable->setColumnWidth(4, scale.scaled(82));
    m_topicTable->setColumnWidth(5, scale.scaled(82));
    m_topicTable->setColumnWidth(6, scale.scaled(70));
    layout->addWidget(m_topicTable, 1);

    m_detailTabs->addTab(tab, tr("ROS Topic"));
}

void BottomStatusPanel::setupStateTabs()
{
    createDetailTab(tr("定位"),
                    {{tr("基础状态"),
                      {{QStringLiteral("loc.state"), tr("数据状态")},
                       {QStringLiteral("loc.time"), tr("更新时间")},
                       {QStringLiteral("loc.gps_time"), tr("GPS 时间")},
                       {QStringLiteral("loc.status_error"), tr("定位状态/错误码")}}},
                     {tr("空间姿态"),
                      {{QStringLiteral("loc.odom"), tr("里程计 X/Y/Z")},
                       {QStringLiteral("loc.attitude"), tr("航向/俯仰/横滚 (°)")},
                       {QStringLiteral("loc.depth_height"), tr("深度/高度")}}},
                     {tr("运动反馈"),
                      {{QStringLiteral("loc.velocity_xyz"), tr("速度 X/Y/Z")},
                       {QStringLiteral("loc.velocity"), tr("合速度")},
                       {QStringLiteral("loc.omega_z"), tr("Z 轴角速度 (°/s)")},
                       {QStringLiteral("loc.acc"), tr("加速度")}}}});

    createDetailTab(tr("底盘"),
                    {{tr("运动状态"),
                      {{QStringLiteral("chassis.state"), tr("数据状态")},
                       {QStringLiteral("chassis.time"), tr("更新时间")},
                       {QStringLiteral("chassis.speed"), tr("当前前向速度")},
                       {QStringLiteral("chassis.angular"), tr("当前角速度 (°/s)")},
                       {QStringLiteral("chassis.gear"), tr("挡位状态")}}},
                     {tr("压载与电源"),
                      {{QStringLiteral("chassis.tank_level"), tr("水箱液位状态")},
                       {QStringLiteral("chassis.tank_state"), tr("压载水箱状态")},
                       {QStringLiteral("chassis.bms_dcdc"), tr("高压 BMS / DCDC")},
                       {QStringLiteral("chassis.bms_soc"), tr("高压 BMS SOC")},
                       {QStringLiteral("chassis.bms_pack"), tr("BMS 电池组 V/A")},
                       {QStringLiteral("chassis.bms_cell"), tr("BMS 单体电压")},
                       {QStringLiteral("chassis.bms_temperature"), tr("BMS 温度")},
                       {QStringLiteral("chassis.bms_warning"), tr("BMS 告警码")},
                       {QStringLiteral("chassis.power_channels"), tr("配电通道状态码")},
                       {QStringLiteral("chassis.input_voltage"), tr("智能电源输入电压")}}},
                     {tr("履带驱动回采"),
                      {{QStringLiteral("chassis.left_motor"), tr("左履带电机")},
                       {QStringLiteral("chassis.right_motor"), tr("右履带电机")},
                       {QStringLiteral("chassis.motor_controller"), tr("控制器就绪/输出")},
                       {QStringLiteral("chassis.motor_command"), tr("指令回采")}}},
                     {tr("执行器与故障"),
                      {{QStringLiteral("chassis.heartbeat"), tr("水面/爬行心跳")},
                       {QStringLiteral("chassis.tail_actuator"), tr("尾部执行器 L/R")},
                       {QStringLiteral("chassis.vertical_actuator"), tr("垂向执行器 L/R/B")},
                       {QStringLiteral("chassis.crawl_fault"), tr("爬行故障 L/R")}}}});

    createDetailTab(tr("控制"),
                    {{tr("控制使能"),
                      {{QStringLiteral("control.state"), tr("数据状态")},
                       {QStringLiteral("control.time"), tr("更新时间")},
                       {QStringLiteral("control.mode_enable"), tr("控制模式/使能")},
                       {QStringLiteral("control.navi_mode"), tr("航行依赖模式")},
                       {QStringLiteral("control.expected_gear"), tr("期望挡位")},
                       {QStringLiteral("control.water_enabled"), tr("水面执行器使能")}}},
                     {tr("运动指令"),
                      {{QStringLiteral("control.crawl_velocity"), tr("爬行线速度/角速度 (m/s, °/s)")},
                       {QStringLiteral("control.speed_heading"), tr("期望速度/航向 (m/s, °)")},
                       {QStringLiteral("control.depth_height"), tr("期望深度/高度")},
                       {QStringLiteral("control.dive_speed"), tr("下潜速度")}}},
                     {tr("执行器指令"),
                      {{QStringLiteral("control.water_actuator"), tr("水面执行器 L/R")},
                       {QStringLiteral("control.buoyancy"), tr("浮力调节步长")},
                       {QStringLiteral("control.sonar"), tr("声呐电源")}}}});

    createDetailTab(tr("路径"),
                     {{tr("全局路径"),
                      {{QStringLiteral("path.global_state"), tr("数据状态")},
                       {QStringLiteral("path.global_time"), tr("更新时间")},
                       {QStringLiteral("path.global_frame"), tr("坐标系")},
                       {QStringLiteral("path.global_count"), tr("路径点数")},
                       {QStringLiteral("path.global_length"), tr("路径长度")}}},
                     {tr("局部路径"),
                      {{QStringLiteral("path.local_state"), tr("数据状态")},
                       {QStringLiteral("path.local_time"), tr("更新时间")},
                       {QStringLiteral("path.local_frame"), tr("坐标系")},
                       {QStringLiteral("path.local_goal_uuid"), tr("Goal UUID")},
                       {QStringLiteral("path.local_count"), tr("路径点数")},
                       {QStringLiteral("path.local_length"), tr("路径长度")}}},
                     {tr("终点信息"),
                      {{QStringLiteral("path.endpoint_note"), tr("终点说明")},
                       {QStringLiteral("path.endpoint_xy"), tr("终点 X/Y")}}}});

    createDetailTab(tr("任务状态"),
                    {{tr("运行状态"),
                      {{QStringLiteral("action.source"), tr("数据来源")},
                       {QStringLiteral("action.mode"), tr("运行模式")},
                       {QStringLiteral("action.state"), tr("数据状态")},
                       {QStringLiteral("action.time"), tr("更新时间")},
                       {QStringLiteral("action.owner"), tr("控制归属")},
                       {QStringLiteral("action.goal_uuid"), tr("Goal UUID")},
                       {QStringLiteral("action.run_state"), tr("运行状态码")}}},
                     {tr("目标指令"),
                      {{QStringLiteral("action.chassis_mode"), tr("底盘控制模式")},
                       {QStringLiteral("action.enable"), tr("使能状态")},
                       {QStringLiteral("action.navi_mode"), tr("航行依赖模式")},
                       {QStringLiteral("action.target_speed"), tr("期望前向速度")},
                       {QStringLiteral("action.target_attitude"), tr("目标航向/角速度 (°, °/s)")},
                       {QStringLiteral("action.target_vertical"), tr("目标深度/高度")},
                       {QStringLiteral("action.buoyancy"), tr("浮力调节步长")}}},
                     {tr("任务参数"),
                      {{QStringLiteral("task.state"), tr("任务数据状态")},
                       {QStringLiteral("task.type_id"), tr("任务类型/ID")},
                       {QStringLiteral("task.enable_estop"), tr("任务使能/急停")},
                       {QStringLiteral("task.remote_power"), tr("遥控模式/电源使能")}}}});

    createDetailTab(tr("垂向"),
                    {{tr("垂向反馈"),
                      {{QStringLiteral("vertical.source"), tr("数据来源")},
                       {QStringLiteral("vertical.mode"), tr("运行模式")},
                       {QStringLiteral("vertical.depth_height"), tr("当前绝对深度/高度")},
                       {QStringLiteral("vertical.target"), tr("目标深度/高度")}}},
                     {tr("压载控制"),
                      {{QStringLiteral("vertical.tank_level"), tr("水箱液位状态")},
                       {QStringLiteral("vertical.tank_state"), tr("压载水箱状态")},
                       {QStringLiteral("vertical.buoyancy"), tr("浮力调节步长")}}}});
}

void BottomStatusPanel::updateOverview(const autoviz::datacenter::VisualizationSnapshot& snapshot)
{
    const auto& loc = snapshot.localizationStatus;
    const auto& chassis = snapshot.chassisRuntimeStatus;
    const auto& control = snapshot.controlCommandStatus;
    const auto& action = snapshot.actionRuntimeStatus;
    const auto& task = snapshot.taskRuntimeStatus;
    const auto& globalPath = snapshot.globalPathStatus;
    const auto& localPath = snapshot.localPathStatus;
    const auto* locationTopic = findTopicStatus(snapshot.topicStatuses, QStringLiteral("/location"));
    const auto* controlTopic = findTopicStatus(snapshot.topicStatuses, QStringLiteral("/chassis_command"));
    const auto* chassisTopic = findTopicStatus(snapshot.topicStatuses, QStringLiteral("/chassis_states"));
    const auto* obstacleTopic = findTopicStatus(snapshot.topicStatuses, QStringLiteral("/targets/final_objects"));

    setOverviewValue(QStringLiteral("system.source"), dataSourceText(snapshot.runtimeStatus.inputSource));
    setOverviewValue(QStringLiteral("system.state"), overallStateText(snapshot));
    setOverviewValue(QStringLiteral("system.mode"), autoviz::model::toDisplayString(snapshot.runVisualizationMode));
    setOverviewValue(QStringLiteral("system.action"), actionStateText(action.valid, action.state));
    setOverviewValue(QStringLiteral("system.owner"), actionOwnerText(action.valid, action.owner));
    setOverviewValue(QStringLiteral("system.estop"), displayBool(task.valid, task.emergencyStop));
    setOverviewValue(QStringLiteral("system.task"), taskTypeAndIdText(task));
    setOverviewValue(QStringLiteral("system.links"), linkSummaryText(snapshot));
    setOverviewValue(QStringLiteral("system.enable"), control.valid ? displayBool(true, control.isEnable)
                                                                      : displayBool(action.valid, action.isEnable));

    setOverviewValue(QStringLiteral("pose.age"), locationTopic != nullptr ? formatAge(locationTopic->ageMs) : QStringLiteral("--"));

    const int controlMode = control.valid ? control.mode : (action.valid ? action.chassisMode : 0);
    setOverviewValue(QStringLiteral("control.mode"), chassisModeText(control.valid || action.valid, controlMode));
    const bool useCrawlFeedback = isCrawlMode(control)
                                   || (action.valid && autoviz::model::isCrawlChassisMode(action.chassisMode));
    const QString feedbackSpeed = useCrawlFeedback
                                      ? displayNumber(chassis.valid, chassis.currentSpeed, 2)
                                      : displayNumber(loc.valid, loc.velocity, 2);
    setOverviewValue(QStringLiteral("control.speed"), feedbackSpeed == QStringLiteral("--")
                                                            ? QStringLiteral("--")
                                                            : QStringLiteral("%1 m/s").arg(feedbackSpeed));
    setOverviewValue(QStringLiteral("control.heading"), displayAngleDegrees(loc.valid, loc.heading));
    const QString feedbackAngular = chassis.valid
                                        ? displayAngularVelocityDegrees(true, chassis.currentAngularVelocity)
                                        : displayAngularVelocityDegrees(loc.valid, loc.omegaZ);
    setOverviewValue(QStringLiteral("control.angular"), feedbackAngular);
    setOverviewValue(QStringLiteral("control.gear"), gearText(chassis.valid, chassis.gearStatus));

    setOverviewValue(QStringLiteral("command.speed"), speedSummaryText(control, loc, chassis, action));
    setOverviewValue(QStringLiteral("command.heading"), headingSummaryText(loc, control, action));
    setOverviewValue(QStringLiteral("command.angular"), angularVelocitySummaryText(loc, chassis, control, action));
    setOverviewValue(QStringLiteral("command.gear"), gearSummaryText(control, chassis));
    const QString commandState = control.valid
                                     ? QStringLiteral("%1 / %2")
                                           .arg(chassisModeText(true, control.mode),
                                                control.isEnable ? QStringLiteral("已使能") : QStringLiteral("未使能"))
                                     : (controlTopic == nullptr ? QStringLiteral("等待控制消息") : topicStateText(controlTopic));
    setOverviewValue(QStringLiteral("command.state"), commandState);

    setOverviewValue(QStringLiteral("hardware.feedback"), chassisHealthText(chassis, chassisTopic));
    const int faultCount = explicitChassisFaultCount(chassis);
    setOverviewValue(QStringLiteral("hardware.faults"), faultCount < 0 ? QStringLiteral("--") : QStringLiteral("故障项 %1").arg(faultCount));
    setOverviewValue(QStringLiteral("hardware.bms"), bmsSummaryText(chassis));
    setOverviewValue(QStringLiteral("hardware.dcdc"), displayBool(chassis.valid, chassis.dccdcStatus));
    setOverviewValue(QStringLiteral("hardware.power"), powerSummaryText(chassis));
    setOverviewValue(QStringLiteral("hardware.heartbeat"), chassis.valid
                                                               ? QStringLiteral("水推 %1 / 履带 %2").arg(chassis.waterHeartbeat).arg(chassis.crawlHeartbeat)
                                                               : QStringLiteral("--"));

    setOverviewValue(QStringLiteral("vertical.navi_mode"), naviModeText(action.valid, action.naviMode));
    const QString currentDepth = displayNumber(loc.valid, loc.depth, 2);
    const QString targetDepth = displayNumber(action.valid, action.targetDepth, 2);
    const QString currentHeight = displayNumber(loc.valid, loc.height, 2);
    const QString targetHeight = displayNumber(action.valid, action.targetHeight, 2);
    setOverviewValue(QStringLiteral("vertical.depth"), currentDepth == QStringLiteral("--") && targetDepth == QStringLiteral("--")
                                                            ? QStringLiteral("--")
                                                            : QStringLiteral("当前 %1 / 目标 %2").arg(currentDepth, targetDepth));
    setOverviewValue(QStringLiteral("vertical.height"), currentHeight == QStringLiteral("--") && targetHeight == QStringLiteral("--")
                                                             ? QStringLiteral("--")
                                                             : QStringLiteral("当前 %1 / 目标 %2").arg(currentHeight, targetHeight));
    setOverviewValue(QStringLiteral("vertical.tank_level"), waterTankLevelText(chassis.valid, chassis.waterTankLevelStatus, chassis.waterTankLevelIsRaw));
    setOverviewValue(QStringLiteral("vertical.tank_state"), waterTankStatusText(chassis.valid, chassis.waterTankStatus));

    setOverviewValue(QStringLiteral("path.global"), pathStatusText(globalPath));
    setOverviewValue(QStringLiteral("path.local"), pathStatusText(localPath));
    setOverviewValue(QStringLiteral("path.binding"), pathBindingText(localPath, action));
    setOverviewValue(QStringLiteral("obstacle.count"), obstacleTopic != nullptr && obstacleTopic->messageCount > 0 ? QString::number(snapshot.obstacles.size()) : QStringLiteral("--"));
    setOverviewValue(QStringLiteral("obstacle.state"), topicStateText(obstacleTopic));
    setOverviewValue(QStringLiteral("obstacle.age"), obstacleTopic != nullptr ? formatAge(obstacleTopic->ageMs) : QStringLiteral("--"));
}

void BottomStatusPanel::updateTopicTable(const autoviz::datacenter::VisualizationSnapshot& snapshot)
{
    if (m_topicTable == nullptr || m_topicSummaryLabel == nullptr) {
        return;
    }

    const auto& statuses = snapshot.topicStatuses;
    int onlineCount = 0;
    int timeoutCount = 0;
    for (const auto& status : statuses) {
        if (status.messageCount > 0 && !status.timedOut) {
            ++onlineCount;
        } else {
            ++timeoutCount;
        }
    }

    m_topicSummaryLabel->setText(tr("Topic 总数 %1，在线 %2，等待或超时/未更新 %3，频率为相邻两帧估算")
                                     .arg(statuses.size())
                                     .arg(onlineCount)
                                     .arg(timeoutCount));

    m_topicTable->setRowCount(statuses.size());
    for (int row = 0; row < statuses.size(); ++row) {
        const auto& status = statuses.at(row);
        m_topicTable->setItem(row, 0, makeItem(status.name));
        m_topicTable->setItem(row, 1, makeItem(status.type));
        m_topicTable->setItem(row, 2, makeAlignedItem(QString(), Qt::AlignCenter));
        m_topicTable->setCellWidget(row, 2, createStatusBadgeWidget(topicStatusText(status.timedOut, status.messageCount), m_topicTable));
        m_topicTable->setItem(row, 3, makeAlignedItem(formatTime(status.lastUpdateMs), Qt::AlignCenter));
        m_topicTable->setItem(row, 4, makeAlignedItem(formatAge(status.ageMs), Qt::AlignCenter));
        m_topicTable->setItem(row, 5, makeAlignedItem(formatFrequency(status.frequencyHz), Qt::AlignCenter));
        m_topicTable->setItem(row, 6, makeAlignedItem(QString::number(status.messageCount), Qt::AlignCenter));
    }
}

void BottomStatusPanel::updateStateTabs(const autoviz::datacenter::VisualizationSnapshot& snapshot)
{
    const auto& loc = snapshot.localizationStatus;
    setDetailValue(QStringLiteral("loc.state"), validText(loc.valid));
    setDetailValue(QStringLiteral("loc.time"), formatTime(loc.timestampMs));
    setDetailValue(QStringLiteral("loc.gps_time"), displayInt64(loc.valid, loc.gpsTime));
    setDetailValue(QStringLiteral("loc.status_error"), loc.valid ? QStringLiteral("%1 / %2").arg(loc.status).arg(loc.error) : QStringLiteral("--"));
    setDetailValue(QStringLiteral("loc.odom"), loc.valid ? QStringLiteral("%1 / %2 / %3").arg(formatNumber(loc.odomX), formatNumber(loc.odomY), formatNumber(loc.odomZ)) : QStringLiteral("--"));
    setDetailValue(QStringLiteral("loc.attitude"), loc.valid ? QStringLiteral("%1 / %2 / %3").arg(formatAngleDegrees(loc.heading), formatAngleDegrees(loc.pitch), formatAngleDegrees(loc.roll)) : QStringLiteral("--"));
    setDetailValue(QStringLiteral("loc.depth_height"), loc.valid ? QStringLiteral("%1 / %2").arg(formatNumber(loc.depth), formatNumber(loc.height)) : QStringLiteral("--"));
    setDetailValue(QStringLiteral("loc.velocity_xyz"), loc.valid ? QStringLiteral("%1 / %2 / %3").arg(formatNumber(loc.velocityX), formatNumber(loc.velocityY), formatNumber(loc.velocityZ)) : QStringLiteral("--"));
    setDetailValue(QStringLiteral("loc.velocity"), displayNumber(loc.valid, loc.velocity));
    setDetailValue(QStringLiteral("loc.omega_z"), displayAngularVelocityDegrees(loc.valid, loc.omegaZ));
    setDetailValue(QStringLiteral("loc.acc"), displayNumber(loc.valid, loc.acc));

    const auto& chassis = snapshot.chassisRuntimeStatus;
    setDetailValue(QStringLiteral("chassis.state"), validText(chassis.valid));
    setDetailValue(QStringLiteral("chassis.time"), formatTime(chassis.timestampMs));
    setDetailValue(QStringLiteral("chassis.speed"), displayNumber(chassis.valid, chassis.currentSpeed));
    setDetailValue(QStringLiteral("chassis.angular"), displayAngularVelocityDegrees(chassis.valid, chassis.currentAngularVelocity));
    setDetailValue(QStringLiteral("chassis.gear"), displayInt(chassis.valid, chassis.gearStatus));
    setDetailValue(QStringLiteral("chassis.tank_level"), waterTankLevelText(chassis.valid, chassis.waterTankLevelStatus, chassis.waterTankLevelIsRaw));
    setDetailValue(QStringLiteral("chassis.tank_state"), waterTankStatusText(chassis.valid, chassis.waterTankStatus));
    setDetailValue(QStringLiteral("chassis.bms_dcdc"), chassis.valid ? QStringLiteral("自检码 %1 / DCDC %2")
                                                                     .arg(chassis.bms.valid ? chassis.bms.selfCheckStatus : chassis.highVoltageBmsStatus)
                                                                     .arg(chassis.dccdcStatus ? tr("开启") : tr("关闭"))
                                                               : QStringLiteral("--"));
    setDetailValue(QStringLiteral("chassis.bms_soc"), displayInt(chassis.valid, chassis.highVoltageBmsSocStatus));
    setDetailValue(QStringLiteral("chassis.bms_pack"), bmsPackText(chassis));
    setDetailValue(QStringLiteral("chassis.bms_cell"), bmsCellText(chassis));
    setDetailValue(QStringLiteral("chassis.bms_temperature"), bmsTemperatureText(chassis));
    setDetailValue(QStringLiteral("chassis.bms_warning"), bmsWarningCodesText(chassis));
    setDetailValue(QStringLiteral("chassis.power_channels"), powerSupplyStatusText(chassis));
    setDetailValue(QStringLiteral("chassis.input_voltage"), displayNumber(chassis.valid, chassis.smartPowerInputVoltageStatus));
    setDetailValue(QStringLiteral("chassis.heartbeat"), chassis.valid ? QStringLiteral("%1 / %2").arg(chassis.waterHeartbeat).arg(chassis.crawlHeartbeat) : QStringLiteral("--"));
    setDetailValue(QStringLiteral("chassis.tail_actuator"), chassis.valid ? QStringLiteral("%1 / %2").arg(chassis.leftTailActuatorStatus).arg(chassis.rightTailActuatorStatus) : QStringLiteral("--"));
    setDetailValue(QStringLiteral("chassis.vertical_actuator"), chassis.valid ? QStringLiteral("%1 / %2 / %3").arg(chassis.leftVerticalActuatorStatus).arg(chassis.rightVerticalActuatorStatus).arg(chassis.backVerticalActuatorStatus) : QStringLiteral("--"));
    setDetailValue(QStringLiteral("chassis.crawl_fault"), chassis.valid ? QStringLiteral("%1 / %2").arg(chassis.leftCrawlActuatorFaultCode).arg(chassis.rightCrawlActuatorFaultCode) : QStringLiteral("--"));
    setDetailValue(QStringLiteral("chassis.left_motor"), motorFeedbackText(chassis.leftCrawlMotor));
    setDetailValue(QStringLiteral("chassis.right_motor"), motorFeedbackText(chassis.rightCrawlMotor));
    setDetailValue(QStringLiteral("chassis.motor_controller"), motorControllerText(chassis.leftCrawlMotor, chassis.rightCrawlMotor));
    setDetailValue(QStringLiteral("chassis.motor_command"), motorCommandText(chassis.leftCrawlMotor, chassis.rightCrawlMotor));

    const auto& control = snapshot.controlCommandStatus;
    setDetailValue(QStringLiteral("control.state"), validText(control.valid));
    setDetailValue(QStringLiteral("control.time"), formatTime(control.timestampMs));
    setDetailValue(QStringLiteral("control.mode_enable"), control.valid ? QStringLiteral("%1 / %2").arg(chassisModeText(true, control.mode), displayBool(true, control.isEnable)) : QStringLiteral("--"));
    setDetailValue(QStringLiteral("control.navi_mode"), naviModeText(control.valid, control.naviMode));
    setDetailValue(QStringLiteral("control.expected_gear"), gearText(control.valid, control.expectedGear));
    setDetailValue(QStringLiteral("control.water_enabled"), displayBool(control.valid, control.isUseWaterActuator));
    setDetailValue(QStringLiteral("control.crawl_velocity"), control.valid ? QStringLiteral("%1 / %2").arg(formatNumber(control.speed), formatAngularVelocityDegrees(control.angularVelocity)) : QStringLiteral("--"));
    setDetailValue(QStringLiteral("control.speed_heading"), control.valid ? QStringLiteral("%1 / %2").arg(formatNumber(control.speed), formatAngleDegrees(control.heading)) : QStringLiteral("--"));
    setDetailValue(QStringLiteral("control.depth_height"), control.valid ? QStringLiteral("%1 / %2").arg(formatNumber(control.depth), formatNumber(control.height)) : QStringLiteral("--"));
    setDetailValue(QStringLiteral("control.dive_speed"), displayNumber(control.valid, control.diveSpeed));
    setDetailValue(QStringLiteral("control.water_actuator"), control.valid ? QStringLiteral("%1 / %2").arg(control.leftWaterActuatorSpeed).arg(control.rightWaterActuatorSpeed) : QStringLiteral("--"));
    setDetailValue(QStringLiteral("control.buoyancy"), displayInt(control.valid, control.buoyancyAdjust));
    setDetailValue(QStringLiteral("control.sonar"), control.valid ? (control.isOpenSonarPower ? tr("开启") : tr("关闭")) : QStringLiteral("--"));

    const auto& globalPath = snapshot.globalPathStatus;
    const auto& localPath = snapshot.localPathStatus;
    const auto& endpoint = snapshot.pathEndpointStatus;
    setDetailValue(QStringLiteral("path.global_state"), validText(globalPath.valid));
    setDetailValue(QStringLiteral("path.global_time"), formatTime(globalPath.timestampMs));
    setDetailValue(QStringLiteral("path.global_frame"), displayText(globalPath.valid, globalPath.frameId));
    setDetailValue(QStringLiteral("path.global_count"), displayInt(globalPath.valid, globalPath.pointCount));
    setDetailValue(QStringLiteral("path.global_length"), displayNumber(globalPath.valid, globalPath.length));
    setDetailValue(QStringLiteral("path.local_state"), validText(localPath.valid));
    setDetailValue(QStringLiteral("path.local_time"), formatTime(localPath.timestampMs));
    setDetailValue(QStringLiteral("path.local_frame"), displayText(localPath.valid, localPath.frameId));
    setDetailValue(QStringLiteral("path.local_goal_uuid"), displayText(localPath.valid, localPath.goalUuid));
    setDetailValue(QStringLiteral("path.local_count"), displayInt(localPath.valid, localPath.pointCount));
    setDetailValue(QStringLiteral("path.local_length"), displayNumber(localPath.valid, localPath.length));
    setDetailValue(QStringLiteral("path.endpoint_note"), endpoint.valid ? endpoint.label : QStringLiteral("--"));
    setDetailValue(QStringLiteral("path.endpoint_xy"), endpoint.valid ? QStringLiteral("%1 / %2").arg(formatNumber(endpoint.x), formatNumber(endpoint.y)) : QStringLiteral("--"));

    const auto& action = snapshot.actionRuntimeStatus;
    const auto& task = snapshot.taskRuntimeStatus;
    setDetailValue(QStringLiteral("action.source"), tr("/system_run_states + /task_params"));
    setDetailValue(QStringLiteral("action.mode"), autoviz::model::toDisplayString(snapshot.runVisualizationMode));
    setDetailValue(QStringLiteral("action.state"), validText(action.valid));
    setDetailValue(QStringLiteral("action.time"), formatTime(action.timestampMs));
    setDetailValue(QStringLiteral("action.owner"), displayInt(action.valid, action.owner));
    setDetailValue(QStringLiteral("action.goal_uuid"), displayText(action.valid, action.goalUuid));
    setDetailValue(QStringLiteral("action.run_state"), displayInt(action.valid, action.state));
    setDetailValue(QStringLiteral("action.chassis_mode"), chassisModeText(action.valid, action.chassisMode));
    setDetailValue(QStringLiteral("action.enable"), displayBool(action.valid, action.isEnable));
    setDetailValue(QStringLiteral("action.navi_mode"), naviModeText(action.valid, action.naviMode));
    setDetailValue(QStringLiteral("action.target_speed"), displayNumber(action.valid, action.targetSpeed));
    setDetailValue(QStringLiteral("action.target_attitude"), action.valid ? QStringLiteral("%1 / %2").arg(formatAngleDegrees(action.targetHeading), formatAngularVelocityDegrees(action.targetAngularVelocity)) : QStringLiteral("--"));
    setDetailValue(QStringLiteral("action.target_vertical"), action.valid ? QStringLiteral("%1 / %2").arg(formatNumber(action.targetDepth), formatNumber(action.targetHeight)) : QStringLiteral("--"));
    setDetailValue(QStringLiteral("action.buoyancy"), displayInt(action.valid, action.buoyancyAdjust));
    setDetailValue(QStringLiteral("task.state"), validText(task.valid));
    setDetailValue(QStringLiteral("task.type_id"), taskTypeAndIdText(task));
    setDetailValue(QStringLiteral("task.enable_estop"), task.valid ? QStringLiteral("%1 / %2").arg(displayBool(true, task.taskEnable), displayBool(true, task.emergencyStop)) : QStringLiteral("--"));
    setDetailValue(QStringLiteral("task.remote_power"), task.valid ? QStringLiteral("%1 / %2").arg(task.remoteMode).arg(task.powerEnable) : QStringLiteral("--"));

    setDetailValue(QStringLiteral("vertical.source"), tr("定位/底盘/任务状态融合"));
    setDetailValue(QStringLiteral("vertical.mode"), autoviz::model::toDisplayString(snapshot.runVisualizationMode));
    setDetailValue(QStringLiteral("vertical.depth_height"), loc.valid ? QStringLiteral("%1 / %2").arg(formatNumber(loc.depth), formatNumber(loc.height)) : QStringLiteral("--"));
    setDetailValue(QStringLiteral("vertical.target"), action.valid ? QStringLiteral("%1 / %2").arg(formatNumber(action.targetDepth), formatNumber(action.targetHeight)) : QStringLiteral("--"));
    setDetailValue(QStringLiteral("vertical.tank_level"), waterTankLevelText(chassis.valid, chassis.waterTankLevelStatus, chassis.waterTankLevelIsRaw));
    setDetailValue(QStringLiteral("vertical.tank_state"), waterTankStatusText(chassis.valid, chassis.waterTankStatus));
    setDetailValue(QStringLiteral("vertical.buoyancy"), displayInt(action.valid, action.buoyancyAdjust));
}
