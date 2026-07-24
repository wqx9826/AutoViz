#include "core/network/ProtocolModelConverter.h"

#include <algorithm>
#include <cmath>

#include <QDateTime>

namespace autoviz::network {

namespace v1 = autoviz::protocol::v1;
namespace model = autoviz::model;
namespace datacenter = autoviz::datacenter;

namespace {
qint64 timestampMs(const v1::Header& header)
{
    const auto ns = header.has_source_time_ns() ? header.source_time_ns()
                                                : header.server_receive_time_ns();
    return ns > 0 ? static_cast<qint64>(ns / 1000000ULL)
                  : QDateTime::currentMSecsSinceEpoch();
}

model::Header convertHeader(const v1::Header& source)
{
    model::Header target;
    target.timestamp = timestampMs(source);
    target.frameId = QString::fromStdString(source.frame_id());
    return target;
}

const v1::DiagnosticMetric* findMetric(
    const google::protobuf::RepeatedPtrField<v1::DiagnosticMetric>& metrics,
    const char* key)
{
    for (const auto& metric : metrics) {
        if (metric.key() == key) {
            return &metric;
        }
    }
    return nullptr;
}

double metricDouble(const google::protobuf::RepeatedPtrField<v1::DiagnosticMetric>& metrics,
                    const char* key,
                    double fallback = 0.0)
{
    const auto* metric = findMetric(metrics, key);
    if (metric == nullptr) {
        return fallback;
    }
    if (metric->has_double_value()) {
        return metric->double_value();
    }
    if (metric->has_int_value()) {
        return static_cast<double>(metric->int_value());
    }
    return fallback;
}

int metricInt(const google::protobuf::RepeatedPtrField<v1::DiagnosticMetric>& metrics,
              const std::string& key,
              int fallback = 0)
{
    const auto* metric = findMetric(metrics, key.c_str());
    if (metric == nullptr) {
        return fallback;
    }
    if (metric->has_int_value()) {
        return static_cast<int>(metric->int_value());
    }
    if (metric->has_double_value()) {
        return static_cast<int>(metric->double_value());
    }
    return fallback;
}

bool metricBool(const google::protobuf::RepeatedPtrField<v1::DiagnosticMetric>& metrics,
                const char* key,
                bool fallback = false)
{
    const auto* metric = findMetric(metrics, key);
    if (metric == nullptr) {
        return fallback;
    }
    if (metric->has_bool_value()) {
        return metric->bool_value();
    }
    if (metric->has_int_value()) {
        return metric->int_value() != 0;
    }
    return fallback;
}

model::VehicleLocation convertVehicle(const v1::VehicleState& source)
{
    model::VehicleLocation target;
    if (source.has_header()) {
        target.header = convertHeader(source.header());
    }
    if (source.has_position()) {
        target.position.x = source.position().x_m();
        target.position.y = source.position().y_m();
    }
    target.heading = source.heading_rad();
    target.pitch = source.pitch_rad();
    target.roll = source.roll_rad();
    target.speed = source.speed_mps();
    target.yawRate = source.yaw_rate_radps();
    target.acceleration = source.longitudinal_acceleration_mps2();
    if (source.has_linear_velocity_mps()) {
        target.Velocity.x = source.linear_velocity_mps().x();
        target.Velocity.y = source.linear_velocity_mps().y();
        target.Velocity.z = source.linear_velocity_mps().z();
    }
    if (source.has_linear_acceleration_mps2()) {
        target.Acceleration.x = source.linear_acceleration_mps2().x();
        target.Acceleration.y = source.linear_acceleration_mps2().y();
        target.Acceleration.z = source.linear_acceleration_mps2().z();
    }
    return target;
}

model::LocalizationStatus convertLocalization(const v1::VehicleState& source)
{
    model::LocalizationStatus target;
    target.valid = true;
    target.timestampMs = source.has_header() ? timestampMs(source.header())
                                             : QDateTime::currentMSecsSinceEpoch();
    target.gpsTime = static_cast<qint64>(source.gps_time());
    target.status = source.localization_status();
    target.error = source.localization_error();
    if (source.has_position()) {
        target.odomX = source.position().x_m();
        target.odomY = source.position().y_m();
        target.odomZ = source.position().z_m();
    }
    target.heading = source.heading_rad();
    target.pitch = source.pitch_rad();
    target.roll = source.roll_rad();
    if (source.has_linear_velocity_mps()) {
        target.velocityX = source.linear_velocity_mps().x();
        target.velocityY = source.linear_velocity_mps().y();
        target.velocityZ = source.linear_velocity_mps().z();
    }
    target.velocity = source.speed_mps();
    target.omegaZ = source.yaw_rate_radps();
    target.acc = source.longitudinal_acceleration_mps2();
    if (source.has_vertical()) {
        target.odomZ = source.vertical().odom_z_m();
        target.depth = source.vertical().depth_m();
        target.height = source.vertical().height_above_bottom_m();
    }
    return target;
}

model::CrawlMotorRuntimeStatus convertMotor(const v1::ActuatorState& source)
{
    model::CrawlMotorRuntimeStatus target;
    target.valid = true;
    target.speedRpm = metricDouble(source.metric(), "speed_rpm");
    target.torqueOrQAxisCurrent = metricDouble(source.metric(), "torque_or_q_axis_current");
    target.temperature = metricInt(source.metric(), "temperature");
    target.busVoltage = metricDouble(source.metric(), "bus_voltage");
    target.controllerReady = metricBool(source.metric(), "controller_ready");
    target.outputEnabled = metricBool(source.metric(), "output_enabled");
    target.controllerUTemperature = metricInt(source.metric(), "controller_u_temperature");
    target.controllerVTemperature = metricInt(source.metric(), "controller_v_temperature");
    target.fault = source.fault();
    target.faultCode = source.fault_code();
    target.commandEnable = metricBool(source.metric(), "command_enable");
    target.commandSpeedMode = metricBool(source.metric(), "command_speed_mode");
    target.commandReverse = metricBool(source.metric(), "command_reverse");
    target.commandSpeedRpm = metricDouble(source.metric(), "command_speed_rpm");
    target.commandTorqueOrQAxisCurrent =
        metricDouble(source.metric(), "command_torque_or_q_axis_current");
    return target;
}

model::VehicleChassisInfo convertChassisInfo(const v1::ChassisState& source)
{
    model::VehicleChassisInfo target;
    if (source.has_header()) {
        target.header = convertHeader(source.header());
    }
    target.currentSpeed = source.speed_mps();
    target.currentAngularVelocity = source.yaw_rate_radps();
    target.currentGearPosition = static_cast<uint8_t>(source.gear());
    target.throttleRatio = source.throttle_percent();
    target.brakeRatio = source.brake_percent();
    target.handBrake = source.hand_brake();
    target.energyRatio = source.energy_percent();
    return target;
}

model::ChassisRuntimeStatus convertChassisStatus(const v1::ChassisState& source)
{
    model::ChassisRuntimeStatus target;
    target.valid = true;
    target.timestampMs = source.has_header() ? timestampMs(source.header())
                                             : QDateTime::currentMSecsSinceEpoch();
    target.currentSpeed = source.speed_mps();
    target.currentAngularVelocity = source.yaw_rate_radps();
    target.gearStatus = source.gear();
    target.waterTankLevelStatus = metricInt(source.metric(), "water_tank_level");
    target.waterTankLevelIsRaw = metricBool(source.metric(), "water_tank_level_is_raw");
    target.waterTankStatus = metricInt(source.metric(), "water_tank_status");
    target.waterHeartbeat = metricInt(source.metric(), "water_heartbeat");
    target.crawlHeartbeat = metricInt(source.metric(), "crawl_heartbeat");
    target.dccdcStatus = metricBool(source.metric(), "dccdc_status");
    target.smartPowerInputVoltageStatus = metricDouble(source.metric(), "smart_power_input_voltage");

    for (const auto& actuator : source.actuator()) {
        if (actuator.id() == "left_tail_thruster") {
            target.leftTailActuatorStatus = actuator.fault_code();
        } else if (actuator.id() == "right_tail_thruster") {
            target.rightTailActuatorStatus = actuator.fault_code();
        } else if (actuator.id() == "left_vertical_thruster") {
            target.leftVerticalActuatorStatus = actuator.fault_code();
        } else if (actuator.id() == "right_vertical_thruster") {
            target.rightVerticalActuatorStatus = actuator.fault_code();
        } else if (actuator.id() == "back_vertical_thruster") {
            target.backVerticalActuatorStatus = actuator.fault_code();
        } else if (actuator.id() == "left_crawl_motor") {
            target.leftCrawlMotor = convertMotor(actuator);
            target.leftCrawlActuatorFaultCode = actuator.fault_code();
        } else if (actuator.id() == "right_crawl_motor") {
            target.rightCrawlMotor = convertMotor(actuator);
            target.rightCrawlActuatorFaultCode = actuator.fault_code();
        }
    }

    if (source.has_battery()) {
        const auto& battery = source.battery();
        target.highVoltageBmsStatus = metricInt(battery.metric(), "self_check_status");
        target.highVoltageBmsSocStatus = static_cast<int>(battery.state_of_charge_percent());
        auto& bms = target.bms;
        bms.valid = battery.valid();
        bms.selfCheckStatus = metricInt(battery.metric(), "self_check_status");
        bms.heartbeat = metricInt(battery.metric(), "heartbeat");
        bms.alarmLevel = battery.alarm_level();
        bms.currentStatus = metricInt(battery.metric(), "current_status");
        bms.packVoltage = battery.pack_voltage_v();
        bms.packCurrent = battery.pack_current_a();
        bms.maxCellVoltageIndex = metricInt(battery.metric(), "max_cell_voltage_index");
        bms.maxCellVoltage = metricDouble(battery.metric(), "max_cell_voltage");
        bms.minCellVoltageIndex = metricInt(battery.metric(), "min_cell_voltage_index");
        bms.minCellVoltage = metricDouble(battery.metric(), "min_cell_voltage");
        bms.maxTemperatureIndex = metricInt(battery.metric(), "max_temperature_index");
        bms.maxTemperature = metricInt(battery.metric(), "max_temperature");
        bms.minTemperatureIndex = metricInt(battery.metric(), "min_temperature_index");
        bms.minTemperature = metricInt(battery.metric(), "min_temperature");
        bms.soc = target.highVoltageBmsSocStatus;
        for (int index = 1; index <= 12; ++index) {
            bms.warningCodes.push_back(metricInt(battery.metric(),
                                                 "warning_" + std::to_string(index)));
        }
    }
    for (int index = 1; index <= 16; ++index) {
        target.powerSupplyStatuses.push_back(
            metricInt(source.metric(), "power_supply_" + std::to_string(index)));
    }
    return target;
}

model::TrajectoryPoint convertTrajectoryPoint(const v1::TrajectoryPoint& source)
{
    model::TrajectoryPoint target;
    if (source.has_path_point()) {
        const auto& path = source.path_point();
        if (path.has_position()) {
            target.position.x = path.position().x_m();
            target.position.y = path.position().y_m();
        }
        target.theta = path.heading_rad();
        target.kappa = path.curvature_per_m();
        target.dkappa = path.curvature_derivative_per_m2();
        target.s = path.accumulated_s_m();
        if (path.has_depth_m()) {
            target.depth = path.depth_m();
            target.hasDepth = true;
        }
        if (path.has_height_above_bottom_m()) {
            target.height = path.height_above_bottom_m();
            target.hasHeight = true;
        }
    }
    target.velocity = source.speed_mps();
    target.acceleration = source.acceleration_mps2();
    target.jerk = source.jerk_mps3();
    target.relativeTime = source.relative_time_s();
    target.absoluteTime = source.absolute_time_s();
    target.l = source.lateral_offset_m();
    target.dlDt = source.lateral_speed_mps();
    target.ddlDt = source.lateral_acceleration_mps2();
    return target;
}

model::Trajectory convertTrajectory(const v1::Trajectory& source)
{
    model::Trajectory target;
    if (source.has_header()) {
        target.header = convertHeader(source.header());
    }
    target.points.reserve(source.point_size());
    for (const auto& point : source.point()) {
        target.points.push_back(convertTrajectoryPoint(point));
    }
    return target;
}

model::ReferenceLine convertReferenceLine(const v1::ReferenceLine& source)
{
    model::ReferenceLine target;
    if (source.has_header()) {
        target.header = convertHeader(source.header());
    }
    target.points.reserve(source.point_size());
    for (const auto& point : source.point()) {
        model::ReferencePoint converted;
        if (point.has_position()) {
            converted.position.x = point.position().x_m();
            converted.position.y = point.position().y_m();
        }
        converted.theta = point.heading_rad();
        converted.kappa = point.curvature_per_m();
        converted.dkappa = point.curvature_derivative_per_m2();
        converted.s = point.accumulated_s_m();
        target.points.push_back(converted);
    }
    return target;
}

model::ObstacleList convertObstacles(const v1::ObstacleSet& source)
{
    model::ObstacleList target;
    target.reserve(source.obstacle_size());
    for (const auto& item : source.obstacle()) {
        model::Obstacle obstacle;
        bool idOk = false;
        obstacle.id = QString::fromStdString(item.id()).toInt(&idOk);
        if (!idOk) {
            obstacle.id = static_cast<int>(std::hash<std::string>{}(item.id()) & 0x7fffffffU);
        }
        obstacle.sourceClass = item.source_class();
        obstacle.classLabel = QString::fromStdString(item.class_label());
        obstacle.sourceTopic = QString::fromStdString(item.source());
        obstacle.shape = model::ObstacleShapeType::Box;
        if (item.has_header()) {
            obstacle.header = convertHeader(item.header());
        } else if (source.has_header()) {
            obstacle.header = convertHeader(source.header());
        }
        obstacle.isStatic = item.is_static();
        obstacle.isVirtual = item.is_virtual();
        if (item.has_center()) {
            obstacle.position.position.x = item.center().x_m();
            obstacle.position.position.y = item.center().y_m();
        }
        obstacle.position.theta = item.heading_rad();
        obstacle.length = item.length_m();
        obstacle.width = item.width_m();
        obstacle.boundingBox.center = obstacle.position.position;
        obstacle.boundingBox.heading = item.heading_rad();
        obstacle.boundingBox.length = item.length_m();
        obstacle.boundingBox.width = item.width_m();
        obstacle.anchorPoint = obstacle.position.position;
        for (const auto& point : item.predicted_point()) {
            obstacle.predictedTrajectory.push_back(convertTrajectoryPoint(point));
        }
        target.push_back(obstacle);
    }
    return target;
}

model::ControlMode convertControlMode(v1::ControlCommand::Mode mode)
{
    if (mode == v1::ControlCommand::MODE_CRAWL) {
        return model::ControlMode::Crawl;
    }
    if (mode == v1::ControlCommand::MODE_SAILING) {
        return model::ControlMode::Sailing;
    }
    return model::ControlMode::Unknown;
}

model::ControlCmd convertControl(const v1::ControlCommand& source)
{
    model::ControlCmd target;
    if (!source.enabled()) {
        return target;
    }
    if (source.has_header()) {
        target.header = convertHeader(source.header());
    }
    target.mode = convertControlMode(source.mode());
    target.desiredVelocity = source.target_speed_mps();
    target.desiredAngularVelocity = source.target_yaw_rate_radps();
    target.desiredHeading = source.target_heading_rad();
    target.desiredGear = source.target_gear();
    target.desiredThrottle = source.target_throttle_percent();
    target.desiredBrake = source.target_brake_percent();
    target.handBrake = source.hand_brake();
    return target;
}

model::ControlCommandStatus convertControlStatus(const v1::ControlCommand& source)
{
    model::ControlCommandStatus target;
    target.valid = true;
    target.timestampMs = source.has_header() ? timestampMs(source.header())
                                             : QDateTime::currentMSecsSinceEpoch();
    target.mode = source.mode() == v1::ControlCommand::MODE_CRAWL
                      ? 6
                      : (source.mode() == v1::ControlCommand::MODE_SAILING ? 4 : 0);
    target.isEnable = source.enabled();
    target.speed = source.target_speed_mps();
    target.angularVelocity = source.target_yaw_rate_radps();
    target.expectedGear = source.target_gear();
    target.naviMode = source.navigation_mode();
    target.heading = source.target_heading_rad();
    target.isOpenSonarPower = source.sonar_power_enabled();
    if (source.has_vertical()) {
        target.isUseWaterActuator = true;
        target.depth = source.vertical().target_depth_m();
        target.height = source.vertical().target_height_above_bottom_m();
        target.diveSpeed = source.vertical().dive_speed_mps();
        target.leftWaterActuatorSpeed = source.vertical().left_thruster_command();
        target.rightWaterActuatorSpeed = source.vertical().right_thruster_command();
        target.buoyancyAdjust = source.vertical().buoyancy_adjust();
    }
    return target;
}

model::ActionRuntimeStatus convertAction(const v1::ActionState& source)
{
    model::ActionRuntimeStatus target;
    target.valid = true;
    target.timestampMs = source.has_header() ? timestampMs(source.header())
                                             : QDateTime::currentMSecsSinceEpoch();
    target.owner = source.owner();
    target.state = source.state();
    target.goalUuid = QString::fromStdString(source.goal_id());
    target.chassisMode = source.chassis_mode();
    target.isEnable = source.enabled();
    target.naviMode = source.navigation_mode();
    target.targetSpeed = source.target_speed_mps();
    target.targetHeading = source.target_heading_rad();
    target.targetAngularVelocity = source.target_yaw_rate_radps();
    if (source.has_vertical()) {
        target.targetDepth = source.vertical().target_depth_m();
        target.targetHeight = source.vertical().target_height_above_bottom_m();
        target.buoyancyAdjust = source.vertical().buoyancy_adjust();
    }
    return target;
}

model::TaskRuntimeStatus convertTask(const v1::TaskState& source)
{
    model::TaskRuntimeStatus target;
    target.valid = true;
    target.timestampMs = source.has_header() ? timestampMs(source.header())
                                             : QDateTime::currentMSecsSinceEpoch();
    target.taskType = source.task_type();
    target.taskId = source.task_id();
    target.taskEnable = source.enabled();
    target.emergencyStop = source.emergency_stop();
    target.remoteMode = source.remote_mode();
    target.powerEnable = source.power_enable();
    return target;
}

model::TopicStatusList convertTopics(const v1::RuntimeState& source)
{
    model::TopicStatusList target;
    target.reserve(source.topic_size());
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (const auto& item : source.topic()) {
        model::TopicStatus status;
        status.name = QString::fromStdString(item.name());
        status.type = QString::fromStdString(item.type());
        status.lastUpdateMs = static_cast<qint64>(item.last_update_time_ns() / 1000000ULL);
        status.ageMs = status.lastUpdateMs > 0 ? std::max<qint64>(0, now - status.lastUpdateMs) : 0;
        status.timeoutMs = static_cast<qint64>(item.timeout_ns() / 1000000ULL);
        status.frequencyHz = item.frequency_hz();
        status.messageCount = item.message_count();
        status.timedOut = item.timed_out();
        target.push_back(status);
    }
    return target;
}

model::PathRuntimeStatus makePathStatus(const v1::Trajectory& source)
{
    model::PathRuntimeStatus status;
    status.valid = source.point_size() > 0;
    status.timestampMs = source.has_header() ? timestampMs(source.header())
                                             : QDateTime::currentMSecsSinceEpoch();
    status.frameId = source.has_header() ? QString::fromStdString(source.header().frame_id())
                                         : QString{};
    status.goalUuid = QString::fromStdString(source.goal_id());
    status.pointCount = source.point_size();
    status.length = source.total_length_m();
    return status;
}
}  // namespace

datacenter::VisualizationSnapshot ProtocolModelConverter::toModelSnapshot(
    const v1::VisualizationSnapshot& source)
{
    datacenter::VisualizationSnapshot target;
    target.runtimeStatus.inputSource = datacenter::VisualizationInputSource::Remote;
    if (source.has_vehicle_state()) {
        target.vehicleLocation = convertVehicle(source.vehicle_state());
        target.localizationStatus = convertLocalization(source.vehicle_state());
        target.runtimeStatus.hasVehicleLocationData = true;
    }
    if (source.has_chassis_state()) {
        target.vehicleChassisInfo = convertChassisInfo(source.chassis_state());
        target.chassisRuntimeStatus = convertChassisStatus(source.chassis_state());
        target.runtimeStatus.hasVehicleChassisData = true;
    }
    if (source.has_control_command()) {
        target.controlCmd = convertControl(source.control_command());
        target.controlCommandStatus = convertControlStatus(source.control_command());
        target.runtimeStatus.hasControlCmdData = source.control_command().enabled();
    }
    if (source.has_global_trajectory()) {
        target.globalPath = convertTrajectory(source.global_trajectory());
        target.globalPathStatus = makePathStatus(source.global_trajectory());
        target.runtimeStatus.hasGlobalPathData = !target.globalPath.points.isEmpty();
    }
    if (source.has_local_trajectory()) {
        target.localPath = convertTrajectory(source.local_trajectory());
        target.localPathStatus = makePathStatus(source.local_trajectory());
        target.runtimeStatus.hasLocalPathData = !target.localPath.points.isEmpty();
    }
    if (source.has_reference_line()) {
        target.referenceLine = convertReferenceLine(source.reference_line());
        target.runtimeStatus.hasReferenceLineData = !target.referenceLine.points.isEmpty();
    }
    if (source.has_obstacles()) {
        target.obstacles = convertObstacles(source.obstacles());
        target.runtimeStatus.hasObstacleData = !target.obstacles.isEmpty();
    }
    if (source.has_action_state()) {
        target.actionRuntimeStatus = convertAction(source.action_state());
    }
    if (source.has_task_state()) {
        target.taskRuntimeStatus = convertTask(source.task_state());
    }
    if (source.has_runtime_state()) {
        target.topicStatuses = convertTopics(source.runtime_state());
    }
    if (source.has_vehicle_parameters()) {
        target.vehicleConfig.vehicleLength = source.vehicle_parameters().length_m();
        target.vehicleConfig.vehicleWidth = source.vehicle_parameters().width_m();
        target.vehicleConfig.wheelBase = source.vehicle_parameters().wheel_base_m();
    }
    target.runVisualizationMode = model::inferRunVisualizationMode(target.actionRuntimeStatus,
                                                                    target.taskRuntimeStatus);
    return target;
}

void ProtocolModelConverter::applyUpdate(const v1::ChannelUpdate& update,
                                         datacenter::DataManager* dataManager)
{
    if (dataManager == nullptr) {
        return;
    }
    const bool clear = update.operation() == v1::ChannelUpdate::OPERATION_CLEAR;
    switch (update.channel()) {
    case v1::CHANNEL_VEHICLE_STATE:
        dataManager->setVehicleLocation(clear || !update.has_vehicle_state()
                                            ? model::VehicleLocation{}
                                            : convertVehicle(update.vehicle_state()));
        dataManager->setLocalizationStatus(clear || !update.has_vehicle_state()
                                               ? model::LocalizationStatus{}
                                               : convertLocalization(update.vehicle_state()));
        break;
    case v1::CHANNEL_CHASSIS_STATE:
        dataManager->setVehicleChassisInfo(clear || !update.has_chassis_state()
                                               ? model::VehicleChassisInfo{}
                                               : convertChassisInfo(update.chassis_state()));
        dataManager->setChassisRuntimeStatus(clear || !update.has_chassis_state()
                                                 ? model::ChassisRuntimeStatus{}
                                                 : convertChassisStatus(update.chassis_state()));
        break;
    case v1::CHANNEL_CONTROL_COMMAND:
        dataManager->setControlCmd(clear || !update.has_control_command()
                                       ? model::ControlCmd{}
                                       : convertControl(update.control_command()));
        dataManager->setControlCommandStatus(clear || !update.has_control_command()
                                                  ? model::ControlCommandStatus{}
                                                  : convertControlStatus(update.control_command()));
        break;
    case v1::CHANNEL_GLOBAL_TRAJECTORY:
        dataManager->setGlobalPath(clear || !update.has_trajectory()
                                       ? model::Trajectory{}
                                       : convertTrajectory(update.trajectory()));
        dataManager->setGlobalPathStatus(clear || !update.has_trajectory()
                                             ? model::PathRuntimeStatus{}
                                             : makePathStatus(update.trajectory()));
        break;
    case v1::CHANNEL_LOCAL_TRAJECTORY:
        dataManager->setLocalPath(clear || !update.has_trajectory()
                                      ? model::Trajectory{}
                                      : convertTrajectory(update.trajectory()));
        dataManager->setLocalPathStatus(clear || !update.has_trajectory()
                                            ? model::PathRuntimeStatus{}
                                            : makePathStatus(update.trajectory()));
        break;
    case v1::CHANNEL_REFERENCE_LINE:
        dataManager->setReferenceLine(clear || !update.has_reference_line()
                                          ? model::ReferenceLine{}
                                          : convertReferenceLine(update.reference_line()));
        break;
    case v1::CHANNEL_OBSTACLES:
        dataManager->setObstacles(clear || !update.has_obstacles()
                                      ? model::ObstacleList{}
                                      : convertObstacles(update.obstacles()));
        break;
    case v1::CHANNEL_ACTION_STATE:
        dataManager->setActionRuntimeStatus(clear || !update.has_action_state()
                                                ? model::ActionRuntimeStatus{}
                                                : convertAction(update.action_state()));
        break;
    case v1::CHANNEL_TASK_STATE:
        dataManager->setTaskRuntimeStatus(clear || !update.has_task_state()
                                              ? model::TaskRuntimeStatus{}
                                              : convertTask(update.task_state()));
        break;
    case v1::CHANNEL_RUNTIME_STATE:
        dataManager->setTopicStatuses(clear || !update.has_runtime_state()
                                          ? model::TopicStatusList{}
                                          : convertTopics(update.runtime_state()));
        break;
    case v1::CHANNEL_VEHICLE_PARAMETERS:
        if (!clear && update.has_vehicle_parameters()) {
            model::VehicleConfig config;
            config.vehicleLength = update.vehicle_parameters().length_m();
            config.vehicleWidth = update.vehicle_parameters().width_m();
            config.wheelBase = update.vehicle_parameters().wheel_base_m();
            dataManager->setVehicleConfig(config);
        }
        break;
    default:
        break;
    }
}

}  // namespace autoviz::network
