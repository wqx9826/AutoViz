#include "autoviz_server/RobotWsProtoConverter.h"

#include <cmath>
#include <iterator>
#include <string>
#include <string_view>

#include <geometry_msgs/msg/quaternion.hpp>
#include <geometry_msgs/msg/pose.hpp>
#include <geometry_msgs/msg/twist.hpp>

namespace autoviz_server {
namespace wire = ::autoviz;

namespace {
constexpr double kDegreesToRadians = 0.017453292519943295769;

std::uint64_t rosStampNs(const builtin_interfaces::msg::Time& stamp,
                         std::uint64_t fallback)
{
    const auto value = static_cast<std::uint64_t>(stamp.sec) * 1000000000ULL
                       + static_cast<std::uint64_t>(stamp.nanosec);
    return value == 0 ? fallback : value;
}

double yawFromQuaternion(const geometry_msgs::msg::Quaternion& value)
{
    const double siny = 2.0 * (value.w * value.z + value.x * value.y);
    const double cosy = 1.0 - 2.0 * (value.y * value.y + value.z * value.z);
    return std::atan2(siny, cosy);
}

void fillHeader(wire::Header* header,
                std::uint64_t sourceTimeNs,
                std::uint64_t receiveTimeNs,
                const std::string& module,
                const std::string& frame = {})
{
    if (sourceTimeNs > 0) header->set_source_time_ns(sourceTimeNs);
    if (receiveTimeNs > 0) header->set_server_receive_time_ns(receiveTimeNs);
    header->set_module_name(module);
    if (!frame.empty()) {
        header->set_frame_id(frame);
    }
}

wire::BuoyancyCommand buoyancyCommand(std::uint8_t value)
{
    switch (value) {
    case 0:
        return wire::BUOYANCY_COMMAND_STOP;
    case 1:
        return wire::BUOYANCY_COMMAND_FILL;
    case 2:
        return wire::BUOYANCY_COMMAND_DRAIN;
    default:
        return wire::BUOYANCY_COMMAND_UNKNOWN;
    }
}

wire::VerticalControlMode verticalControlMode(std::uint8_t navigationMode)
{
    if (navigationMode == 1) {
        return wire::VERTICAL_CONTROL_MODE_DEPTH_HOLD;
    }
    // navi_mode=2 was used by older custom_msgs revisions. Keep decoding it
    // as the legacy height-hold value for old bags and old publishers.
    if (navigationMode == 2) {
        return wire::VERTICAL_CONTROL_MODE_HEIGHT_HOLD;
    }
    return wire::VERTICAL_CONTROL_MODE_NONE;
}

// DepthCommand action 在 SystemRunStates 中以 owner=2 和 chassis_mode=1/2 表示。
// 实机发布的该 action 可以将 navi_mode 留为 0，不能因此丢失定深/定高语义。
wire::VerticalControlMode actionVerticalControlMode(std::uint8_t owner,
                                                    std::uint8_t chassisMode,
                                                    std::uint8_t navigationMode)
{
    (void)navigationMode;
    if (owner == 2) {
        if (chassisMode == 1) {
            return wire::VERTICAL_CONTROL_MODE_DEPTH_HOLD;
        }
        if (chassisMode == 2) {
            return wire::VERTICAL_CONTROL_MODE_LANDING;
        }
    }
    // Move action 的 navi_mode 只描述水平航行的定深/定高依赖，不能把它切换成 T-Z 主视图。
    return wire::VERTICAL_CONTROL_MODE_NONE;
}

std::string actionName(std::uint8_t owner)
{
    switch (owner) {
    case 1:
        return "custom_msgs/action/Move";
    case 2:
        return "custom_msgs/action/DepthCommand";
    default:
        return {};
    }
}

wire::WaterTankState waterTankState(std::uint8_t value)
{
    switch (value) {
    case custom_msgs::msg::ChassisStates::WATER_TANK_IDLE:
        return wire::WATER_TANK_STATE_IDLE;
    case custom_msgs::msg::ChassisStates::WATER_TANK_FILLING:
        return wire::WATER_TANK_STATE_FILLING;
    case custom_msgs::msg::ChassisStates::WATER_TANK_DRAINING:
        return wire::WATER_TANK_STATE_DRAINING;
    case custom_msgs::msg::ChassisStates::WATER_TANK_MANUAL_OVERRIDE:
        return wire::WATER_TANK_STATE_MANUAL_OVERRIDE;
    case custom_msgs::msg::ChassisStates::WATER_TANK_FAULT:
        return wire::WATER_TANK_STATE_FAULT;
    case custom_msgs::msg::ChassisStates::WATER_TANK_FILL_DONE:
        return wire::WATER_TANK_STATE_FILL_DONE;
    case custom_msgs::msg::ChassisStates::WATER_TANK_DRAIN_DONE:
        return wire::WATER_TANK_STATE_DRAIN_DONE;
    default:
        return wire::WATER_TANK_STATE_UNKNOWN;
    }
}

double polylineLength(const wire::Trajectory& trajectory)
{
    double length = 0.0;
    for (int index = 1; index < trajectory.point_size(); ++index) {
        const auto& previous = trajectory.point(index - 1).path_point().position();
        const auto& current = trajectory.point(index).path_point().position();
        length += std::hypot(current.x_m() - previous.x_m(),
                             current.y_m() - previous.y_m());
    }
    return length;
}

void fillMotor(wire::CrawlMotorState* target,
               bool valid,
               double speedRpm,
               double torque,
               int temperature,
               double busVoltage,
               bool controllerReady,
               bool outputEnabled,
               int uTemperature,
               int vTemperature,
               bool fault,
               int motorFaultCode,
               int actuatorFaultCode,
               bool commandEnable,
               bool commandSpeedMode,
               bool commandReverse,
               double commandSpeedRpm,
               double commandTorque)
{
    target->set_valid(valid);
    target->set_speed_rpm(speedRpm);
    target->set_torque_or_q_axis_current(torque);
    target->set_temperature_c(temperature);
    target->set_bus_voltage_v(busVoltage);
    target->set_controller_ready(controllerReady);
    target->set_output_enabled(outputEnabled);
    target->set_controller_u_temperature_c(uTemperature);
    target->set_controller_v_temperature_c(vTemperature);
    target->set_fault(fault);
    target->set_motor_fault_code(motorFaultCode);
    target->set_actuator_fault_code(actuatorFaultCode);
    target->set_command_enable(commandEnable);
    target->set_command_speed_mode(commandSpeedMode);
    target->set_command_reverse(commandReverse);
    target->set_command_speed_rpm(commandSpeedRpm);
    target->set_command_torque_or_q_axis_current(commandTorque);
}

void addThruster(wire::UnderwaterChassisState* target,
                 const char* id,
                 int faultCode)
{
    auto* thruster = target->add_thruster();
    thruster->set_id(id);
    thruster->set_fault_code(faultCode);
}

void addTailMotor(wire::ChassisState* target,
                  const char* id,
                  double busCurrent,
                  int controllerTemperature,
                  double targetSpeed,
                  double actualSpeed)
{
    auto* motor = target->add_tail_thruster_motor();
    motor->set_id(id);
    motor->set_bus_current_a(busCurrent);
    motor->set_controller_temperature_c(controllerTemperature);
    motor->set_target_speed_rpm(targetSpeed);
    motor->set_actual_speed_rpm(actualSpeed);
}

void fillPose(wire::Pose3D* target, const geometry_msgs::msg::Pose& source)
{
    target->mutable_position()->set_x_m(source.position.x);
    target->mutable_position()->set_y_m(source.position.y);
    target->mutable_position()->set_z_m(source.position.z);
    target->set_quaternion_x(source.orientation.x);
    target->set_quaternion_y(source.orientation.y);
    target->set_quaternion_z(source.orientation.z);
    target->set_quaternion_w(source.orientation.w);
}

void fillTwist(wire::Twist3D* target, const geometry_msgs::msg::Twist& source)
{
    target->mutable_linear()->set_x(source.linear.x);
    target->mutable_linear()->set_y(source.linear.y);
    target->mutable_linear()->set_z(source.linear.z);
    target->mutable_angular()->set_x(source.angular.x);
    target->mutable_angular()->set_y(source.angular.y);
    target->mutable_angular()->set_z(source.angular.z);
}

const char* finalTargetClassLabel(std::uint8_t value)
{
    switch (value) {
    case custom_msgs::msg::FinalTarget::CLASS_MINE:
        return "mine";
    case custom_msgs::msg::FinalTarget::CLASS_NET:
        return "net";
    case custom_msgs::msg::FinalTarget::CLASS_OBSTACLE:
        return "obstacle";
    case custom_msgs::msg::FinalTarget::CLASS_OTHER:
        return "other";
    default:
        return "unknown";
    }
}

bool hasFiniteCenter(const custom_msgs::msg::FinalTarget& target)
{
    return std::isfinite(target.real_center_point.x)
           && std::isfinite(target.real_center_point.y);
}

bool hasUsableDimensions(const custom_msgs::msg::FinalTarget& target)
{
    return target.dimensions_valid && std::isfinite(target.length)
           && std::isfinite(target.width) && target.length > 0.0 && target.width > 0.0;
}

bool hasUsableHeading(const custom_msgs::msg::FinalTarget& target, bool dimensionsValid)
{
    return dimensionsValid && target.heading_valid && std::isfinite(target.heading);
}

bool hasUsableGeodeticPosition(const custom_msgs::msg::FinalTarget& target)
{
    const auto& point = target.real_center_point;
    return target.geodetic_valid && std::isfinite(point.longitude) && std::isfinite(point.latitude)
           && std::isfinite(point.depth) && std::isfinite(point.height);
}

// 把 canonical 32 位 hex 转成 robot_ws 用 %x 逐字节格式化会得到的 lossy 形式：
// 每个字节若有前导零则丢弃。只应传入 canonical（32 位）字符串。
std::string lossyUuidHex(std::string_view canonical)
{
    std::string result;
    result.reserve(32);
    for (std::size_t index = 0; index + 1 < canonical.size(); index += 2) {
        if (canonical[index] != '0') {
            result.push_back(canonical[index]);
        }
        result.push_back(canonical[index + 1]);
    }
    return result;
}
}  // namespace

wire::VehicleState RobotWsProtoConverter::vehicleState(
    const custom_msgs::msg::Location& message, std::uint64_t receiveTimeNs)
{
    wire::VehicleState state;
    fillHeader(state.mutable_header(), 0, receiveTimeNs,
               "robot_ws.location", "odom");
    state.mutable_position()->set_x_m(message.odom_x);
    state.mutable_position()->set_y_m(message.odom_y);
    state.mutable_position()->set_z_m(message.odom_z);
    state.set_heading_rad(message.heading);
    state.set_pitch_rad(message.pitch);
    state.set_roll_rad(message.roll);
    state.mutable_linear_velocity_mps()->set_x(message.velocity_x);
    state.mutable_linear_velocity_mps()->set_y(message.velocity_y);
    state.mutable_linear_velocity_mps()->set_z(message.velocity_z);
    state.mutable_linear_acceleration_mps2()->set_x(message.acc_x);
    state.mutable_linear_acceleration_mps2()->set_y(message.acc_y);
    state.mutable_linear_acceleration_mps2()->set_z(message.acc_z);
    state.set_speed_mps(message.velocity);
    state.set_yaw_rate_radps(message.omega_z);
    state.set_longitudinal_acceleration_mps2(message.acc);
    auto* underwater = state.mutable_underwater();
    underwater->set_odom_z_m(message.odom_z);
    underwater->set_depth_m(message.depth);
    underwater->set_height_above_bottom_m(message.height);
    underwater->set_vertical_velocity_mps(message.velocity_z);
    underwater->set_usbl_x_m(message.usbl_x);
    underwater->set_usbl_y_m(message.usbl_y);
    underwater->set_usbl_z_m(message.usbl_z);
    state.set_localization_status(message.status);
    state.set_localization_error(message.error);
    if (message.gps_time > 0) {
        state.set_gps_time(static_cast<std::uint64_t>(message.gps_time));
    }
    state.set_longitude_deg(message.longitude);
    state.set_latitude_deg(message.latitude);
    state.set_start_time_s(message.start_time);
    state.set_gauss_x_m(message.gauss_x);
    state.set_gauss_y_m(message.gauss_y);
    state.set_gauss_z_m(message.gauss_z);
    state.set_origin_longitude_deg(message.origin_longitude);
    state.set_origin_latitude_deg(message.origin_latitude);
    state.set_origin_x_m(message.origin_x);
    state.set_origin_y_m(message.origin_y);
    state.set_origin_z_m(message.origin_z);
    state.set_odom_heading_rad(message.odom_heading);
    state.set_angular_velocity_x_radps(message.omega_x);
    state.set_angular_velocity_y_radps(message.omega_y);
    state.set_usbl_message_word_1(message.usbl_msg1);
    state.set_usbl_message_word_2(message.usbl_msg2);
    state.set_usbl_message_word_3(message.usbl_msg3);
    state.set_usbl_message_word_4(message.usbl_msg4);
    return state;
}

wire::ObstacleSet RobotWsProtoConverter::obstacles(
    const custom_msgs::msg::FinalTargetArray& message, std::uint64_t receiveTimeNs)
{
    wire::ObstacleSet result;
    const auto sourceTimeNs = rosStampNs(message.header.stamp, receiveTimeNs);
    fillHeader(result.mutable_header(), sourceTimeNs, receiveTimeNs,
               "robot_ws.final_targets", message.header.frame_id);
    result.set_source_task_id(message.task_id);
    for (const auto& source : message.targets) {
        if (!hasFiniteCenter(source)) {
            // Match robot_ws obstacle processing: one invalid XY center invalidates the frame.
            result.set_rejection_reason(
                "obstacle set rejected: non-finite planar center (target_id="
                + std::to_string(source.target_id) + ")");
            return result;
        }
    }
    for (const auto& source : message.targets) {
        auto* target = result.add_obstacle();
        fillHeader(target->mutable_header(),
                   rosStampNs(source.header.stamp, sourceTimeNs),
                   receiveTimeNs,
                   "robot_ws.final_target",
                   source.header.frame_id);
        target->set_id(std::to_string(source.target_id));
        target->set_type(wire::Obstacle::TYPE_OTHER);
        target->set_source_class(source.final_class);
        target->set_class_label(finalTargetClassLabel(source.final_class));
        target->set_source("fusion");
        target->mutable_center()->set_x_m(source.real_center_point.x);
        target->mutable_center()->set_y_m(source.real_center_point.y);
        target->mutable_center()->set_z_m(source.real_center_point.z);
        const bool dimensionsValid = hasUsableDimensions(source);
        const bool headingValid = hasUsableHeading(source, dimensionsValid);
        const bool geodeticValid = hasUsableGeodeticPosition(source);
        target->set_geodetic_valid(geodeticValid);
        target->set_dimensions_valid(dimensionsValid);
        target->set_heading_valid(headingValid);
        if (geodeticValid) {
            auto* geodetic = target->mutable_geodetic_position();
            geodetic->set_longitude_deg(source.real_center_point.longitude);
            geodetic->set_latitude_deg(source.real_center_point.latitude);
            geodetic->set_depth_m(source.real_center_point.depth);
            geodetic->set_height_above_bottom_m(source.real_center_point.height);
        }
        if (headingValid) {
            target->set_heading_rad(source.heading);
        }
        if (dimensionsValid) {
            target->set_length_m(source.length);
            target->set_width_m(source.width);
            target->set_height_m(source.height);
        }
        target->set_is_static(true);
        target->set_is_virtual(false);
    }
    return result;
}

wire::RangeMotionDirective RobotWsProtoConverter::rangeMotionDirective(
    const custom_msgs::msg::RangeMotionRequest& message, std::uint64_t receiveTimeNs)
{
    wire::RangeMotionDirective result;
    fillHeader(result.mutable_header(), rosStampNs(message.header.stamp, receiveTimeNs), receiveTimeNs,
               "robot_ws.range_motion_request", message.header.frame_id);
    result.set_task_id(message.task_id);
    result.set_command_sequence(message.command_seq);
    result.set_motion(static_cast<wire::RangeMotionDirective::Motion>(message.motion));
    result.set_speed_limit_mps(message.speed_limit_mps);
    result.set_reason(message.reason);
    return result;
}

wire::InspectionGoal RobotWsProtoConverter::inspectionGoal(
    const custom_msgs::msg::InspectionRequestGoal& message, std::uint64_t receiveTimeNs)
{
    wire::InspectionGoal result;
    fillHeader(result.mutable_header(), rosStampNs(message.header.stamp, receiveTimeNs), receiveTimeNs,
               "robot_ws.inspection_request_goal", message.header.frame_id);
    result.set_task_id(message.task_id);
    result.set_goal_id(message.goal_id);
    result.set_target_id(message.target_id);
    auto* target = result.mutable_target_position();
    target->set_x_m(message.target_point.x);
    target->set_y_m(message.target_point.y);
    target->set_z_m(message.target_point.z);
    auto* observation = result.mutable_observation_position();
    observation->set_x_m(message.goal_point.x);
    observation->set_y_m(message.goal_point.y);
    observation->set_z_m(message.goal_point.z);
    result.set_target_heading_rad(message.goal_heading);
    result.set_hold_on_arrival(message.hold_on_arrival);
    result.set_mode(static_cast<wire::InspectionGoal::Mode>(message.mode));
    result.set_speed_limit_mps(message.speed_limit);
    return result;
}

wire::ControlCommand RobotWsProtoConverter::controlCommand(
    const custom_msgs::msg::ChassisCommand& message, std::uint64_t receiveTimeNs)
{
    wire::ControlCommand command;
    fillHeader(command.mutable_header(), 0, receiveTimeNs,
               "robot_ws.chassis_command");
    const bool crawl = message.mode == 6 || message.mode == 8 || message.mode == 11;
    const bool sailing = (message.mode >= 1 && message.mode <= 5)
                          || message.mode == 7 || message.mode == 9 || message.mode == 10;
    command.set_mode(crawl ? wire::ControlCommand::MODE_CRAWL
                           : (sailing ? wire::ControlCommand::MODE_SAILING
                                      : wire::ControlCommand::MODE_UNKNOWN));
    command.set_maneuver((message.mode == 10 || message.mode == 11)
                             ? wire::ControlCommand::MANEUVER_YAW_IN_PLACE
                             : wire::ControlCommand::MANEUVER_NONE);
    command.set_source_mode(message.mode);
    command.set_enabled(message.is_enable);
    command.set_target_speed_mps(message.speed);
    command.set_target_yaw_rate_radps(message.angular_velocity);
    command.set_target_heading_rad(message.heading);
    command.set_target_gear(message.expected_gear);
    auto* underwater = command.mutable_underwater();
    underwater->set_water_actuator_enabled(message.is_use_water_actuator);
    underwater->set_navigation_mode(message.navi_mode);
    underwater->set_target_depth_m(message.depth);
    underwater->set_target_height_above_bottom_m(message.height);
    underwater->set_left_thruster_command(message.left_water_actuator_speed);
    underwater->set_right_thruster_command(message.right_water_actuator_speed);
    underwater->set_buoyancy_command(buoyancyCommand(message.buoyancy_adjust));
    underwater->set_vertical_control_mode(
        message.mode == 2 ? wire::VERTICAL_CONTROL_MODE_LANDING
                          : verticalControlMode(message.navi_mode));
    underwater->set_sonar_power_enabled(message.is_open_sonar_power);
    underwater->set_emergency_ascent(message.emergency_ascent);
    return command;
}

wire::ChassisState RobotWsProtoConverter::chassisState(
    const custom_msgs::msg::ChassisStates& message, std::uint64_t receiveTimeNs)
{
    wire::ChassisState chassis;
    fillHeader(chassis.mutable_header(), 0, receiveTimeNs,
               "robot_ws.chassis_states");
    chassis.set_speed_mps(message.current_speed);
    chassis.set_yaw_rate_radps(message.current_angular_velocity);
    chassis.set_gear(message.gear_status);
    chassis.set_heading_kp(message.heading_kp);
    chassis.set_heading_target_value(message.heading_target_value);
    chassis.set_heading_actual_value(message.heading_actual_value);
    chassis.set_heading_error(message.heading_error);
    chassis.set_heading_output(message.heading_output);

    auto* underwater = chassis.mutable_underwater();
    underwater->set_water_tank_level(message.water_tank_level_status);
    underwater->set_water_tank_level_is_raw(message.water_tank_level_is_raw);
    underwater->set_water_tank_state(waterTankState(message.water_tank_status));
    underwater->set_water_heartbeat(message.water_heartbeat);
    underwater->set_emergency_ascent_active(message.emergency_ascent_active);
    addThruster(underwater, "left_tail_thruster", message.left_tail_actuator_status);
    addThruster(underwater, "right_tail_thruster", message.right_tail_actuator_status);
    addThruster(underwater, "left_vertical_thruster", message.left_vertical_actuator_status);
    addThruster(underwater, "right_vertical_thruster", message.right_vertical_actuator_status);
    addThruster(underwater, "back_vertical_thruster", message.back_vertical_actuator_status);
    addTailMotor(&chassis, "left_tail_thruster", message.left_tail_motor_bus_current,
                 message.left_tail_motor_controller_temperature,
                 message.left_tail_motor_target_speed_rpm,
                 message.left_tail_motor_actual_speed_rpm);
    addTailMotor(&chassis, "right_tail_thruster", message.right_tail_motor_bus_current,
                 message.right_tail_motor_controller_temperature,
                 message.right_tail_motor_target_speed_rpm,
                 message.right_tail_motor_actual_speed_rpm);

    auto* platform = chassis.mutable_platform();
    platform->set_crawl_heartbeat(message.crawl_heartbeat);
    platform->set_dcdc_enabled(message.dccdc_status);
    platform->set_smart_power_input_voltage_v(message.smart_power_input_voltage_status);
    fillMotor(platform->mutable_left_crawl_motor(), true,
              message.left_crawl_motor_speed_rpm,
              message.left_crawl_motor_torque_or_q_axis_current,
              message.left_crawl_motor_temperature,
              message.left_crawl_motor_bus_voltage,
              message.left_crawl_motor_controller_ready,
              message.left_crawl_motor_output_enabled,
              message.left_crawl_motor_controller_u_temperature,
              message.left_crawl_motor_controller_v_temperature,
              message.left_crawl_motor_fault,
              message.left_crawl_motor_fault_code,
              message.left_crawl_actuator_fault_code,
              message.left_crawl_motor_command_enable,
              message.left_crawl_motor_command_speed_mode,
              message.left_crawl_motor_command_reverse,
              message.left_crawl_motor_command_speed_rpm,
              message.left_crawl_motor_command_torque_or_q_axis_current);
    fillMotor(platform->mutable_right_crawl_motor(), true,
              message.right_crawl_motor_speed_rpm,
              message.right_crawl_motor_torque_or_q_axis_current,
              message.right_crawl_motor_temperature,
              message.right_crawl_motor_bus_voltage,
              message.right_crawl_motor_controller_ready,
              message.right_crawl_motor_output_enabled,
              message.right_crawl_motor_controller_u_temperature,
              message.right_crawl_motor_controller_v_temperature,
              message.right_crawl_motor_fault,
              message.right_crawl_motor_fault_code,
              message.right_crawl_actuator_fault_code,
              message.right_crawl_motor_command_enable,
              message.right_crawl_motor_command_speed_mode,
              message.right_crawl_motor_command_reverse,
              message.right_crawl_motor_command_speed_rpm,
              message.right_crawl_motor_command_torque_or_q_axis_current);

    auto* battery = platform->mutable_battery();
    battery->set_valid(true);
    battery->set_self_check_status(message.high_voltage_bms_status);
    battery->set_heartbeat(message.high_voltage_bms_heartbeat);
    battery->set_alarm_level(message.high_voltage_bms_alarm_level);
    battery->set_current_status(message.high_voltage_bms_current_status);
    battery->set_pack_voltage_v(message.high_voltage_bms_pack_voltage);
    battery->set_pack_current_a(message.high_voltage_bms_pack_current);
    battery->set_max_cell_voltage_index(message.high_voltage_bms_max_cell_voltage_index);
    battery->set_max_cell_voltage_v(message.high_voltage_bms_max_cell_voltage);
    battery->set_min_cell_voltage_index(message.high_voltage_bms_min_cell_voltage_index);
    battery->set_min_cell_voltage_v(message.high_voltage_bms_min_cell_voltage);
    battery->set_max_temperature_index(message.high_voltage_bms_max_temperature_index);
    battery->set_max_temperature_c(message.high_voltage_bms_max_temperature);
    battery->set_min_temperature_index(message.high_voltage_bms_min_temperature_index);
    battery->set_min_temperature_c(message.high_voltage_bms_min_temperature);
    battery->set_state_of_charge_percent(message.high_voltage_bms_soc_status);
    const int warnings[] = {
        message.high_voltage_bms_total_voltage_over_high_warning,
        message.high_voltage_bms_cell_voltage_over_high_warning,
        message.high_voltage_bms_charge_temperature_over_high_warning,
        message.high_voltage_bms_charge_temperature_over_low_warning,
        message.high_voltage_bms_total_voltage_over_low_warning,
        message.high_voltage_bms_discharge_current_over_warning,
        message.high_voltage_bms_cell_voltage_over_low_warning,
        message.high_voltage_bms_discharge_temperature_over_high_warning,
        message.high_voltage_bms_discharge_temperature_over_low_warning,
        message.high_voltage_bms_soc_over_low_warning,
        message.high_voltage_bms_pack_voltage_difference_over_warning,
        message.high_voltage_bms_pack_temperature_difference_over_warning};
    for (const int warning : warnings) {
        battery->add_warning_code(warning);
    }
    const int power[] = {
        message.power_supply_1_status, message.power_supply_2_status,
        message.power_supply_3_status, message.power_supply_4_status,
        message.power_supply_5_status, message.power_supply_6_status,
        message.power_supply_7_status, message.power_supply_8_status,
        message.power_supply_9_status, message.power_supply_10_status,
        message.power_supply_11_status, message.power_supply_12_status,
        message.power_supply_13_status, message.power_supply_14_status,
        message.power_supply_15_status, message.power_supply_16_status};
    for (std::size_t index = 0; index < std::size(power); ++index) {
        auto* channel = platform->add_power_channel();
        channel->set_index(static_cast<std::uint32_t>(index + 1));
        channel->set_status(power[index]);
    }
    return chassis;
}

wire::ActionState RobotWsProtoConverter::actionState(
    const custom_msgs::msg::SystemRunStates& message, std::uint64_t receiveTimeNs)
{
    wire::ActionState state;
    fillHeader(state.mutable_header(), 0, receiveTimeNs,
               "robot_ws.system_run_states");
    state.set_owner(message.owner);
    state.set_state(message.state);
    state.set_goal_id(message.goal_uuid);
    state.set_message(message.message);
    state.set_action_name(actionName(message.owner));
    state.set_chassis_mode(message.chassis_mode);
    state.set_enabled(message.is_enable);
    state.set_navigation_mode(message.navi_mode);
    state.set_target_speed_mps(message.target_speed);
    state.set_target_heading_rad(message.target_heading);
    state.set_target_yaw_rate_radps(message.target_angular_velocity * kDegreesToRadians);
    auto* underwater = state.mutable_underwater();
    underwater->set_navigation_mode(message.navi_mode);
    underwater->set_target_depth_m(message.target_depth);
    underwater->set_target_height_above_bottom_m(message.target_height);
    underwater->set_buoyancy_command(buoyancyCommand(message.buoyancy_adjust));
    underwater->set_vertical_control_mode(
        actionVerticalControlMode(message.owner, message.chassis_mode, message.navi_mode));
    underwater->set_emergency_ascent(message.emergency_ascent);
    return state;
}

wire::TaskState RobotWsProtoConverter::taskState(
    const custom_msgs::msg::TaskParams& message, std::uint64_t receiveTimeNs)
{
    wire::TaskState state;
    fillHeader(state.mutable_header(), 0, receiveTimeNs,
               "robot_ws.task_params");
    state.set_task_type(message.task_type);
    state.set_task_id(message.task_id);
    state.set_enabled(message.task_enable);
    state.set_action_enabled(message.is_enable);
    state.set_emergency_stop(message.emergency_stop);
    state.set_remote_mode(message.remote_mode);
    state.set_power_enable(message.power_enable);
    state.mutable_underwater()->set_release_emergency_ascent(
        message.release_emergency_ascent);
    state.mutable_underwater()->set_buoyancy_command(
        buoyancyCommand(message.buoyancy_adjust));
    auto* remote = state.mutable_remote_control();
    remote->set_crawl_gear(message.crawl_gear);
    remote->set_crawl_speed_mps(message.crawl_speed);
    remote->set_crawl_angular_velocity_radps(message.crawl_angular_velocity);
    remote->set_forward_percent(message.forward_percent);
    remote->set_turn_percent(message.turn_percent);
    remote->set_dive_percent(message.dive_percent);
    remote->set_left_tail_actuator_speed(message.left_tail_actuator_speed);
    remote->set_right_tail_actuator_speed(message.right_tail_actuator_speed);
    remote->set_left_vertical_actuator_speed(message.left_vertical_actuator_speed);
    remote->set_right_vertical_actuator_speed(message.right_vertical_actuator_speed);
    remote->set_back_vertical_actuator_speed(message.back_vertical_actuator_speed);
    remote->add_power_supply_enabled(message.power_supply1);
    remote->add_power_supply_enabled(message.power_supply2);
    remote->add_power_supply_enabled(message.power_supply3);
    remote->add_power_supply_enabled(message.power_supply4);
    remote->add_power_supply_enabled(message.power_supply5);
    remote->add_power_supply_enabled(message.power_supply6);
    remote->add_power_supply_enabled(message.power_supply7);
    remote->add_power_supply_enabled(message.power_supply8);
    remote->add_power_supply_enabled(message.power_supply9);
    remote->add_power_supply_enabled(message.power_supply10);
    remote->add_power_supply_enabled(message.power_supply11);
    remote->add_power_supply_enabled(message.power_supply12);
    remote->add_power_supply_enabled(message.power_supply13);
    remote->add_power_supply_enabled(message.power_supply14);
    remote->add_power_supply_enabled(message.power_supply15);
    remote->add_power_supply_enabled(message.power_supply16);
    return state;
}

wire::Trajectory RobotWsProtoConverter::localTrajectory(
    const custom_msgs::msg::TrajectoryMsg& message, std::uint64_t receiveTimeNs)
{
    wire::Trajectory trajectory;
    const auto sourceTimeNs = rosStampNs(message.header.stamp, receiveTimeNs);
    fillHeader(trajectory.mutable_header(), sourceTimeNs, receiveTimeNs,
               "robot_ws.local_path", message.header.frame_id);
    trajectory.set_kind(wire::Trajectory::KIND_LOCAL);
    trajectory.set_goal_id(message.goal_uuid);
    for (const auto& source : message.trajectory) {
        auto* point = trajectory.add_point();
        auto* path = point->mutable_path_point();
        path->mutable_position()->set_x_m(source.pose.position.x);
        path->mutable_position()->set_y_m(source.pose.position.y);
        path->mutable_position()->set_z_m(source.pose.position.z);
        path->set_heading_rad(yawFromQuaternion(source.pose.orientation));
        fillPose(point->mutable_pose(), source.pose);
        fillTwist(point->mutable_velocity_3d(), source.velocity);
        fillTwist(point->mutable_acceleration_3d(), source.acceleration);
        point->set_speed_mps(source.velocity.linear.x);
        point->set_acceleration_mps2(source.acceleration.linear.x);
        const double relative = static_cast<double>(source.time_from_start.sec)
                                + static_cast<double>(source.time_from_start.nanosec) * 1e-9;
        point->set_relative_time_s(relative);
        point->set_absolute_time_s(static_cast<double>(sourceTimeNs) * 1e-9 + relative);
    }
    trajectory.set_total_length_m(polylineLength(trajectory));
    if (trajectory.point_size() > 0) {
        trajectory.set_total_time_s(trajectory.point(trajectory.point_size() - 1).relative_time_s());
    }
    return trajectory;
}

wire::Trajectory RobotWsProtoConverter::globalTrajectory(
    const nav_msgs::msg::Path& message, std::uint64_t receiveTimeNs)
{
    wire::Trajectory trajectory;
    const auto sourceTimeNs = rosStampNs(message.header.stamp, receiveTimeNs);
    fillHeader(trajectory.mutable_header(), sourceTimeNs, receiveTimeNs,
               "robot_ws.global_path", message.header.frame_id);
    trajectory.set_kind(wire::Trajectory::KIND_GLOBAL);
    for (const auto& source : message.poses) {
        auto* point = trajectory.add_point();
        auto* path = point->mutable_path_point();
        path->mutable_position()->set_x_m(source.pose.position.x);
        path->mutable_position()->set_y_m(source.pose.position.y);
        path->mutable_position()->set_z_m(source.pose.position.z);
        path->set_heading_rad(yawFromQuaternion(source.pose.orientation));
        fillPose(point->mutable_pose(), source.pose);
    }
    trajectory.set_total_length_m(polylineLength(trajectory));
    return trajectory;
}

bool RobotWsProtoConverter::sameGoalUuid(std::string_view a, std::string_view b)
{
    if (a == b) return true;
    if (a.size() == 32 && lossyUuidHex(a) == b) return true;
    if (b.size() == 32 && lossyUuidHex(b) == a) return true;
    return false;
}

}  // namespace autoviz_server
