#include "core/network/ProtocolModelConverter.h"

#include <algorithm>
#include <cmath>

#include <QDateTime>

namespace autoviz::network {

namespace wire = ::autoviz;
namespace model = autoviz::model;
namespace datacenter = autoviz::datacenter;

namespace {
qint64 timestampMs(const wire::Header& header)
{
    const auto ns = header.has_source_time_ns() ? header.source_time_ns()
                                                : header.server_receive_time_ns();
    return ns > 0 ? static_cast<qint64>(ns / 1000000ULL)
                  : QDateTime::currentMSecsSinceEpoch();
}

model::Header convertHeader(const wire::Header& source)
{
    model::Header target;
    target.timestamp = timestampMs(source);
    target.frameId = QString::fromStdString(source.frame_id());
    return target;
}

model::VisualizationChannel convertDataKind(wire::DataKind kind)
{
    switch (kind) {
    case wire::DATA_KIND_VEHICLE_STATE:
        return model::VisualizationChannel::VehicleState;
    case wire::DATA_KIND_CHASSIS_STATE:
        return model::VisualizationChannel::ChassisState;
    case wire::DATA_KIND_CONTROL_COMMAND:
        return model::VisualizationChannel::ControlCommand;
    case wire::DATA_KIND_GLOBAL_TRAJECTORY:
        return model::VisualizationChannel::GlobalTrajectory;
    case wire::DATA_KIND_LOCAL_TRAJECTORY:
        return model::VisualizationChannel::LocalTrajectory;
    case wire::DATA_KIND_REFERENCE_LINE:
        return model::VisualizationChannel::ReferenceLine;
    case wire::DATA_KIND_OBSTACLES:
        return model::VisualizationChannel::Obstacles;
    case wire::DATA_KIND_ACTION_STATE:
        return model::VisualizationChannel::ActionState;
    case wire::DATA_KIND_TASK_STATE:
        return model::VisualizationChannel::TaskState;
    case wire::DATA_KIND_RUNTIME_STATE:
        return model::VisualizationChannel::RuntimeState;
    case wire::DATA_KIND_VEHICLE_PARAMETERS:
        return model::VisualizationChannel::VehicleParameters;
    case wire::DATA_KIND_UNKNOWN:
    default:
        return model::VisualizationChannel::Unknown;
    }
}

model::WaterTankState convertWaterTankState(wire::WaterTankState state)
{
    switch (state) {
    case wire::WATER_TANK_STATE_IDLE:
        return model::WaterTankState::Idle;
    case wire::WATER_TANK_STATE_FILLING:
        return model::WaterTankState::Filling;
    case wire::WATER_TANK_STATE_DRAINING:
        return model::WaterTankState::Draining;
    case wire::WATER_TANK_STATE_MANUAL_OVERRIDE:
        return model::WaterTankState::ManualOverride;
    case wire::WATER_TANK_STATE_FAULT:
        return model::WaterTankState::Fault;
    case wire::WATER_TANK_STATE_FILL_DONE:
        return model::WaterTankState::FillDone;
    case wire::WATER_TANK_STATE_DRAIN_DONE:
        return model::WaterTankState::DrainDone;
    case wire::WATER_TANK_STATE_UNKNOWN:
    default:
        return model::WaterTankState::Unknown;
    }
}

model::VerticalControlMode convertVerticalControlMode(wire::VerticalControlMode mode)
{
    switch (mode) {
    case wire::VERTICAL_CONTROL_MODE_DEPTH_HOLD:
        return model::VerticalControlMode::DepthHold;
    case wire::VERTICAL_CONTROL_MODE_HEIGHT_HOLD:
        return model::VerticalControlMode::HeightHold;
    case wire::VERTICAL_CONTROL_MODE_NONE:
    default:
        return model::VerticalControlMode::None;
    }
}

int convertBuoyancyCommand(wire::BuoyancyCommand command)
{
    switch (command) {
    case wire::BUOYANCY_COMMAND_STOP:
        return 0;
    case wire::BUOYANCY_COMMAND_FILL:
        return 1;
    case wire::BUOYANCY_COMMAND_DRAIN:
        return 2;
    default:
        return -1;
    }
}

model::VehicleLocation convertVehicle(const wire::VehicleState& source)
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

model::LocalizationStatus convertLocalization(const wire::VehicleState& source)
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
    if (source.has_underwater()) {
        target.odomZ = source.underwater().odom_z_m();
        target.depth = source.underwater().depth_m();
        target.height = source.underwater().height_above_bottom_m();
    }
    return target;
}

model::CrawlMotorRuntimeStatus convertMotor(const wire::CrawlMotorState& source)
{
    model::CrawlMotorRuntimeStatus target;
    target.valid = source.valid();
    target.speedRpm = source.speed_rpm();
    target.torqueOrQAxisCurrent = source.torque_or_q_axis_current();
    target.temperature = source.temperature_c();
    target.busVoltage = source.bus_voltage_v();
    target.controllerReady = source.controller_ready();
    target.outputEnabled = source.output_enabled();
    target.controllerUTemperature = source.controller_u_temperature_c();
    target.controllerVTemperature = source.controller_v_temperature_c();
    target.fault = source.fault();
    target.faultCode = source.motor_fault_code();
    target.commandEnable = source.command_enable();
    target.commandSpeedMode = source.command_speed_mode();
    target.commandReverse = source.command_reverse();
    target.commandSpeedRpm = source.command_speed_rpm();
    target.commandTorqueOrQAxisCurrent = source.command_torque_or_q_axis_current();
    return target;
}

model::VehicleChassisInfo convertChassisInfo(const wire::ChassisState& source)
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

model::ChassisRuntimeStatus convertChassisStatus(const wire::ChassisState& source)
{
    model::ChassisRuntimeStatus target;
    target.valid = true;
    target.timestampMs = source.has_header() ? timestampMs(source.header())
                                             : QDateTime::currentMSecsSinceEpoch();
    target.currentSpeed = source.speed_mps();
    target.currentAngularVelocity = source.yaw_rate_radps();
    target.gearStatus = source.gear();
    if (source.has_underwater()) {
        const auto& underwater = source.underwater();
        target.waterTankLevelStatus = underwater.water_tank_level();
        target.waterTankLevelIsRaw = underwater.water_tank_level_is_raw();
        target.waterTankState = convertWaterTankState(underwater.water_tank_state());
        target.waterHeartbeat = underwater.water_heartbeat();
        target.emergencyAscentActive = underwater.emergency_ascent_active();
        for (const auto& thruster : underwater.thruster()) {
            if (thruster.id() == "left_tail_thruster") {
                target.leftTailActuatorStatus = thruster.fault_code();
            } else if (thruster.id() == "right_tail_thruster") {
                target.rightTailActuatorStatus = thruster.fault_code();
            } else if (thruster.id() == "left_vertical_thruster") {
                target.leftVerticalActuatorStatus = thruster.fault_code();
            } else if (thruster.id() == "right_vertical_thruster") {
                target.rightVerticalActuatorStatus = thruster.fault_code();
            } else if (thruster.id() == "back_vertical_thruster") {
                target.backVerticalActuatorStatus = thruster.fault_code();
            }
        }
    }
    if (source.has_platform()) {
        const auto& platform = source.platform();
        target.crawlHeartbeat = platform.crawl_heartbeat();
        target.dccdcStatus = platform.dcdc_enabled();
        target.smartPowerInputVoltageStatus = platform.smart_power_input_voltage_v();
        if (platform.has_left_crawl_motor()) {
            target.leftCrawlMotor = convertMotor(platform.left_crawl_motor());
            target.leftCrawlActuatorFaultCode =
                platform.left_crawl_motor().actuator_fault_code();
        }
        if (platform.has_right_crawl_motor()) {
            target.rightCrawlMotor = convertMotor(platform.right_crawl_motor());
            target.rightCrawlActuatorFaultCode =
                platform.right_crawl_motor().actuator_fault_code();
        }
        if (platform.has_battery()) {
            const auto& battery = platform.battery();
            target.highVoltageBmsStatus = battery.self_check_status() != 0;
            target.highVoltageBmsSocStatus = battery.state_of_charge_percent();
            auto& bms = target.bms;
            bms.valid = battery.valid();
            bms.selfCheckStatus = battery.self_check_status();
            bms.heartbeat = battery.heartbeat();
            bms.alarmLevel = battery.alarm_level();
            bms.currentStatus = battery.current_status();
            bms.packVoltage = battery.pack_voltage_v();
            bms.packCurrent = battery.pack_current_a();
            bms.maxCellVoltageIndex = battery.max_cell_voltage_index();
            bms.maxCellVoltage = battery.max_cell_voltage_v();
            bms.minCellVoltageIndex = battery.min_cell_voltage_index();
            bms.minCellVoltage = battery.min_cell_voltage_v();
            bms.maxTemperatureIndex = battery.max_temperature_index();
            bms.maxTemperature = battery.max_temperature_c();
            bms.minTemperatureIndex = battery.min_temperature_index();
            bms.minTemperature = battery.min_temperature_c();
            bms.soc = battery.state_of_charge_percent();
            for (const int warning : battery.warning_code()) {
                bms.warningCodes.push_back(warning);
            }
        }
        for (const auto& channel : platform.power_channel()) {
            target.powerSupplyStatuses.push_back(channel.status());
        }
    }
    return target;
}

model::TrajectoryPoint convertTrajectoryPoint(const wire::TrajectoryPoint& source)
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

model::Trajectory convertTrajectory(const wire::Trajectory& source)
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

model::ReferenceLine convertReferenceLine(const wire::ReferenceLine& source)
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

model::ObstacleList convertObstacles(const wire::ObstacleSet& source)
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
        obstacle.position.theta = item.has_heading_rad() ? item.heading_rad() : 0.0;
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

model::ControlMode convertControlMode(wire::ControlCommand::Mode mode)
{
    if (mode == wire::ControlCommand::MODE_CRAWL) {
        return model::ControlMode::Crawl;
    }
    if (mode == wire::ControlCommand::MODE_SAILING) {
        return model::ControlMode::Sailing;
    }
    return model::ControlMode::Unknown;
}

model::ControlCmd convertControl(const wire::ControlCommand& source)
{
    model::ControlCmd target;
    // `enabled` 表示控制器是否正在执行，不表示这条命令不存在。保留未使能
    // 命令，才能在 rosbag 回放和使能切换阶段连续观察命令与反馈曲线。
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

model::ControlCommandStatus convertControlStatus(const wire::ControlCommand& source)
{
    model::ControlCommandStatus target;
    target.valid = true;
    target.timestampMs = source.has_header() ? timestampMs(source.header())
                                             : QDateTime::currentMSecsSinceEpoch();
    const bool yawInPlace = source.maneuver()
                            == wire::ControlCommand::MANEUVER_YAW_IN_PLACE;
    const auto verticalMode = source.has_underwater()
                                  ? convertVerticalControlMode(source.underwater().vertical_control_mode())
                                  : model::VerticalControlMode::None;
    target.mode = source.mode() == wire::ControlCommand::MODE_CRAWL
                      ? (yawInPlace ? 11 : 6)
                      : (source.mode() == wire::ControlCommand::MODE_SAILING
                             ? (yawInPlace ? 10
                                           : (verticalMode == model::VerticalControlMode::HeightHold ? 5
                                              : (verticalMode == model::VerticalControlMode::DepthHold ? 4 : 0)))
                             : 0);
    target.isEnable = source.enabled();
    target.speed = source.target_speed_mps();
    target.angularVelocity = source.target_yaw_rate_radps();
    target.expectedGear = source.target_gear();
    target.heading = source.target_heading_rad();
    if (source.has_underwater()) {
        const auto& underwater = source.underwater();
        target.isUseWaterActuator = underwater.water_actuator_enabled();
        target.naviMode = underwater.navigation_mode();
        target.verticalControlMode = convertVerticalControlMode(underwater.vertical_control_mode());
        target.depth = underwater.target_depth_m();
        target.height = underwater.target_height_above_bottom_m();
        target.leftWaterActuatorSpeed = underwater.left_thruster_command();
        target.rightWaterActuatorSpeed = underwater.right_thruster_command();
        target.buoyancyAdjust = convertBuoyancyCommand(underwater.buoyancy_command());
        target.isOpenSonarPower = underwater.sonar_power_enabled();
        target.emergencyAscent = underwater.emergency_ascent();
    }
    return target;
}

model::ActionRuntimeStatus convertAction(const wire::ActionState& source)
{
    model::ActionRuntimeStatus target;
    target.valid = true;
    target.timestampMs = source.has_header() ? timestampMs(source.header())
                                             : QDateTime::currentMSecsSinceEpoch();
    target.owner = source.owner();
    target.state = source.state();
    target.goalUuid = QString::fromStdString(source.goal_id());
    target.message = QString::fromStdString(source.message());
    target.actionName = QString::fromStdString(source.action_name());
    target.chassisMode = source.chassis_mode();
    target.isEnable = source.enabled();
    target.naviMode = source.navigation_mode();
    target.targetSpeed = source.target_speed_mps();
    target.targetHeading = source.target_heading_rad();
    target.targetAngularVelocity = source.target_yaw_rate_radps();
    target.hasNativeStatus = source.has_native_status();
    target.nativeStatus = source.native_status();
    target.nativeStatusTimestampMs = static_cast<qint64>(source.native_status_time_ns() / 1000000ULL);
    target.hasFeedbackProgress = source.has_feedback_progress();
    target.feedbackProgress = source.feedback_progress();
    target.feedbackTimestampMs = static_cast<qint64>(source.feedback_time_ns() / 1000000ULL);
    if (source.has_underwater()) {
        const auto& underwater = source.underwater();
        target.naviMode = underwater.navigation_mode();
        target.verticalControlMode = convertVerticalControlMode(underwater.vertical_control_mode());
        target.targetDepth = underwater.target_depth_m();
        target.targetHeight = underwater.target_height_above_bottom_m();
        target.buoyancyAdjust = convertBuoyancyCommand(underwater.buoyancy_command());
        target.emergencyAscent = underwater.emergency_ascent();
    }
    return target;
}

model::TaskRuntimeStatus convertTask(const wire::TaskState& source)
{
    model::TaskRuntimeStatus target;
    target.valid = true;
    target.timestampMs = source.has_header() ? timestampMs(source.header())
                                             : QDateTime::currentMSecsSinceEpoch();
    target.taskType = source.task_type();
    target.taskId = source.task_id();
    target.taskEnable = source.enabled();
    target.emergencyStop = source.emergency_stop();
    target.releaseEmergencyAscent = source.has_underwater()
                                        && source.underwater().release_emergency_ascent();
    target.remoteMode = source.remote_mode();
    target.powerEnable = source.power_enable();
    return target;
}

model::TopicStatusList convertTopics(const wire::RuntimeState& source)
{
    model::TopicStatusList target;
    target.reserve(source.topic_size());
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (const auto& item : source.topic()) {
        model::TopicStatus status;
        status.channel = convertDataKind(item.data_kind());
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

model::PathRuntimeStatus makePathStatus(const wire::Trajectory& source)
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
    const wire::VisualizationSnapshot& source)
{
    datacenter::VisualizationSnapshot target;
    target.runtimeStatus.inputSource = datacenter::VisualizationInputSource::Remote;
    target.runtimeStatus.hasCommonPlanningControlCapability = false;
    target.runtimeStatus.sourceTimeMs = source.has_server_time_ns()
                                            ? static_cast<qint64>(source.server_time_ns() / 1000000ULL)
                                            : 0;
    if (source.has_source()) {
        for (const auto capabilityValue : source.source().capability()) {
            const auto capability = static_cast<wire::Capability>(capabilityValue);
            target.runtimeStatus.hasCommonPlanningControlCapability |=
                capability == wire::CAPABILITY_COMMON_PLANNING_CONTROL;
            target.runtimeStatus.hasVerticalMotionCapability |=
                capability == wire::CAPABILITY_VERTICAL_MOTION;
            target.runtimeStatus.hasUnderwaterSystemCapability |=
                capability == wire::CAPABILITY_UNDERWATER_SYSTEM;
            target.runtimeStatus.hasPlatformDiagnosticsCapability |=
                capability == wire::CAPABILITY_PLATFORM_DIAGNOSTICS;
        }
    }
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
        // 数据存在与执行使能是两个独立概念：前者驱动可视化，后者由
        // ControlCommandStatus::isEnable 供状态面板明确展示。
        target.runtimeStatus.hasControlCmdData = true;
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
        if (source.action_state().has_recent_terminal()) {
            target.recentTerminalActionStatus = convertAction(source.action_state().recent_terminal());
        }
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

}  // namespace autoviz::network
