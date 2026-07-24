#include "autoviz_server/AutoVizServerNode.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <sstream>
#include <utility>
#include <vector>

#include <geometry_msgs/msg/quaternion.hpp>

namespace autoviz_server {

namespace wire = ::autoviz;

namespace {
constexpr double kDegreesToRadians = 0.017453292519943295769;

std::uint64_t rosStampNs(const builtin_interfaces::msg::Time& stamp)
{
    return static_cast<std::uint64_t>(stamp.sec) * 1000000000ULL
           + static_cast<std::uint64_t>(stamp.nanosec);
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
    header->set_source_time_ns(sourceTimeNs);
    header->set_server_receive_time_ns(receiveTimeNs);
    header->set_module_name(module);
    if (!frame.empty()) {
        header->set_frame_id(frame);
    }
}

wire::DiagnosticMetric* addIntMetric(google::protobuf::RepeatedPtrField<wire::DiagnosticMetric>* metrics,
                                   const std::string& key,
                                   std::int64_t value,
                                   const std::string& unit = {})
{
    auto* metric = metrics->Add();
    metric->set_key(key);
    metric->set_int_value(value);
    if (!unit.empty()) {
        metric->set_unit(unit);
    }
    return metric;
}

wire::DiagnosticMetric* addDoubleMetric(google::protobuf::RepeatedPtrField<wire::DiagnosticMetric>* metrics,
                                      const std::string& key,
                                      double value,
                                      const std::string& unit = {})
{
    auto* metric = metrics->Add();
    metric->set_key(key);
    metric->set_double_value(value);
    if (!unit.empty()) {
        metric->set_unit(unit);
    }
    return metric;
}

wire::DiagnosticMetric* addBoolMetric(google::protobuf::RepeatedPtrField<wire::DiagnosticMetric>* metrics,
                                    const std::string& key,
                                    bool value)
{
    auto* metric = metrics->Add();
    metric->set_key(key);
    metric->set_bool_value(value);
    return metric;
}

void addMotorMetrics(wire::ActuatorState* actuator,
                     double speedRpm,
                     double torque,
                     int temperature,
                     double busVoltage,
                     bool controllerReady,
                     bool outputEnabled,
                     int uTemperature,
                     int vTemperature,
                     bool commandEnable,
                     bool commandSpeedMode,
                     bool commandReverse,
                     double commandSpeedRpm,
                     double commandTorque)
{
    addDoubleMetric(actuator->mutable_metric(), "speed_rpm", speedRpm, "rpm");
    addDoubleMetric(actuator->mutable_metric(), "torque_or_q_axis_current", torque);
    addIntMetric(actuator->mutable_metric(), "temperature", temperature, "degC");
    addDoubleMetric(actuator->mutable_metric(), "bus_voltage", busVoltage, "V");
    addBoolMetric(actuator->mutable_metric(), "controller_ready", controllerReady);
    addBoolMetric(actuator->mutable_metric(), "output_enabled", outputEnabled);
    addIntMetric(actuator->mutable_metric(), "controller_u_temperature", uTemperature, "degC");
    addIntMetric(actuator->mutable_metric(), "controller_v_temperature", vTemperature, "degC");
    addBoolMetric(actuator->mutable_metric(), "command_enable", commandEnable);
    addBoolMetric(actuator->mutable_metric(), "command_speed_mode", commandSpeedMode);
    addBoolMetric(actuator->mutable_metric(), "command_reverse", commandReverse);
    addDoubleMetric(actuator->mutable_metric(), "command_speed_rpm", commandSpeedRpm, "rpm");
    addDoubleMetric(actuator->mutable_metric(), "command_torque_or_q_axis_current", commandTorque);
}

double polylineLength(const wire::Trajectory& trajectory)
{
    double length = 0.0;
    for (int index = 1; index < trajectory.point_size(); ++index) {
        const auto& previous = trajectory.point(index - 1).path_point().position();
        const auto& current = trajectory.point(index).path_point().position();
        length += std::hypot(current.x_m() - previous.x_m(), current.y_m() - previous.y_m());
    }
    return length;
}
}  // namespace

AutoVizServerNode::AutoVizServerNode()
    : Node("autoviz_server")
{
    const auto bindAddress = declare_parameter<std::string>("bind_address", "0.0.0.0");
    const auto port = declare_parameter<std::int64_t>("port", 39090);
    const auto maxClients = declare_parameter<std::int64_t>("max_clients", 8);
    m_topicTimeout = std::chrono::milliseconds(declare_parameter<std::int64_t>("topic_timeout_ms", 5000));

    const auto now = nowNs();
    std::ostringstream session;
    session << std::hex << now;
    m_sessionId = session.str();
    m_snapshot.set_session_id(m_sessionId);
    m_snapshot.set_server_time_ns(now);
    auto* source = m_snapshot.mutable_source();
    source->set_source_id("robot_ws");
    source->set_adapter_type("ros2");
    source->set_adapter_version("1.0");
    source->set_description("robot_ws ROS2 adapter");

    auto* parameters = m_snapshot.mutable_vehicle_parameters();
    parameters->set_length_m(declare_parameter<double>("vehicle_length_m", 4.9));
    parameters->set_width_m(declare_parameter<double>("vehicle_width_m", 1.95));
    parameters->set_wheel_base_m(declare_parameter<double>("wheel_base_m", 2.85));

    auto addTopic = [this](const std::string& parameter,
                           const std::string& defaultName,
                           const std::string& type,
                           wire::ChannelId channel) {
        const std::string name = declare_parameter<std::string>(parameter, defaultName);
        m_topics.emplace(name, TopicMonitor{name, type, channel});
        return name;
    };

    addTopic("topics.location", "/location", "custom_msgs/msg/Location", wire::CHANNEL_VEHICLE_STATE);
    addTopic("topics.obstacles", "/targets/final_objects", "custom_msgs/msg/FinalTargetArray", wire::CHANNEL_OBSTACLES);
    addTopic("topics.control_command", "/chassis_command", "custom_msgs/msg/ChassisCommand", wire::CHANNEL_CONTROL_COMMAND);
    addTopic("topics.chassis_state", "/chassis_states", "custom_msgs/msg/ChassisStates", wire::CHANNEL_CHASSIS_STATE);
    addTopic("topics.action_state", "/system_run_states", "custom_msgs/msg/SystemRunStates", wire::CHANNEL_ACTION_STATE);
    addTopic("topics.task_state", "/task_params", "custom_msgs/msg/TaskParams", wire::CHANNEL_TASK_STATE);
    addTopic("topics.local_path", "/local_path", "custom_msgs/msg/TrajectoryMsg", wire::CHANNEL_LOCAL_TRAJECTORY);
    addTopic("topics.global_path", "/global_path", "nav_msgs/msg/Path", wire::CHANNEL_GLOBAL_TRAJECTORY);

    updateRuntimeState();
    createSubscriptions();
    m_tcpServer.start(bindAddress,
                      static_cast<std::uint16_t>(port),
                      static_cast<std::size_t>(std::max<std::int64_t>(1, maxClients)),
                      [this]() { return snapshot(); });
    m_timer = create_wall_timer(std::chrono::seconds(1), std::bind(&AutoVizServerNode::onTimer, this));
    RCLCPP_INFO(get_logger(),
                "AutoViz Server listening on %s:%d (session=%s)",
                bindAddress.c_str(),
                static_cast<int>(port),
                m_sessionId.c_str());
}

AutoVizServerNode::~AutoVizServerNode()
{
    m_tcpServer.stop();
}

std::uint64_t AutoVizServerNode::nowNs() const
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

wire::VisualizationSnapshot AutoVizServerNode::snapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto result = m_snapshot;
    result.set_server_time_ns(nowNs());
    return result;
}

wire::ChannelUpdate AutoVizServerNode::makeUpdate(wire::ChannelId channel)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    wire::ChannelUpdate update;
    update.set_sequence(++m_sequence);
    update.set_server_time_ns(nowNs());
    update.set_session_id(m_sessionId);
    update.set_channel(channel);
    m_snapshot.set_sequence(m_sequence);
    return update;
}

void AutoVizServerNode::publish(wire::ChannelUpdate update)
{
    wire::Envelope envelope;
    envelope.mutable_channel_update()->Swap(&update);
    m_tcpServer.broadcast(envelope);
}

void AutoVizServerNode::recordTopic(const std::string& topic)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto iter = m_topics.find(topic);
    if (iter == m_topics.end()) {
        return;
    }
    const auto steadyNow = std::chrono::steady_clock::now();
    auto& monitor = iter->second;
    if (monitor.messageCount > 0) {
        const auto interval = std::chrono::duration<double>(steadyNow - monitor.lastReceive).count();
        if (interval > 0.0) {
            monitor.frequencyHz = 1.0 / interval;
        }
    }
    monitor.lastReceive = steadyNow;
    monitor.lastReceiveNs = nowNs();
    ++monitor.messageCount;
    monitor.timedOut = false;
}

std::string AutoVizServerNode::topicName(wire::ChannelId channel) const
{
    for (const auto& item : m_topics) {
        if (item.second.channel == channel) {
            return item.first;
        }
    }
    return {};
}

void AutoVizServerNode::onTimer()
{
    std::vector<wire::ChannelId> timedOutChannels;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto now = std::chrono::steady_clock::now();
        for (auto& item : m_topics) {
            auto& monitor = item.second;
            const bool timedOut = monitor.messageCount == 0
                                  || now - monitor.lastReceive > m_topicTimeout;
            if (timedOut && !monitor.timedOut && monitor.messageCount > 0) {
                timedOutChannels.push_back(monitor.channel);
            }
            monitor.timedOut = timedOut;
        }
    }
    for (const auto channel : timedOutChannels) {
        clearTimedOutChannel(channel);
    }
    updateRuntimeState();
    std::uint64_t heartbeatSequence = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        heartbeatSequence = ++m_sequence;
        m_snapshot.set_sequence(m_sequence);
    }
    m_tcpServer.broadcastHeartbeat(heartbeatSequence, nowNs(), m_sessionId);
}

void AutoVizServerNode::clearTimedOutChannel(wire::ChannelId channel)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        switch (channel) {
        case wire::CHANNEL_VEHICLE_STATE:
            m_snapshot.clear_vehicle_state();
            break;
        case wire::CHANNEL_CHASSIS_STATE:
            m_snapshot.clear_chassis_state();
            break;
        case wire::CHANNEL_CONTROL_COMMAND:
            m_snapshot.clear_control_command();
            break;
        case wire::CHANNEL_GLOBAL_TRAJECTORY:
            m_snapshot.clear_global_trajectory();
            break;
        case wire::CHANNEL_LOCAL_TRAJECTORY:
            m_snapshot.clear_local_trajectory();
            break;
        case wire::CHANNEL_REFERENCE_LINE:
            m_snapshot.clear_reference_line();
            break;
        case wire::CHANNEL_OBSTACLES:
            m_snapshot.clear_obstacles();
            break;
        case wire::CHANNEL_ACTION_STATE:
            m_snapshot.clear_action_state();
            break;
        case wire::CHANNEL_TASK_STATE:
            m_snapshot.clear_task_state();
            break;
        default:
            break;
        }
    }
    auto update = makeUpdate(channel);
    update.set_operation(wire::ChannelUpdate::OPERATION_CLEAR);
    publish(std::move(update));
}

void AutoVizServerNode::updateRuntimeState()
{
    wire::RuntimeState runtime;
    const auto currentNs = nowNs();
    fillHeader(runtime.mutable_header(), currentNs, currentNs, "autoviz_server");
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& item : m_topics) {
            const auto& monitor = item.second;
            auto* topic = runtime.add_topic();
            topic->set_name(monitor.name);
            topic->set_type(monitor.type);
            topic->set_last_update_time_ns(monitor.lastReceiveNs);
            topic->set_timeout_ns(static_cast<std::uint64_t>(m_topicTimeout.count()) * 1000000ULL);
            topic->set_frequency_hz(monitor.frequencyHz);
            topic->set_message_count(monitor.messageCount);
            topic->set_timed_out(monitor.timedOut);
        }
        auto* diagnostics = runtime.mutable_diagnostics();
        diagnostics->set_id("autoviz_server");
        diagnostics->set_display_name("AutoViz Server");
        diagnostics->set_level(wire::DiagnosticNode::LEVEL_OK);
        addIntMetric(diagnostics->mutable_metric(),
                     "connected_clients",
                     static_cast<std::int64_t>(m_tcpServer.clientCount()));
        m_snapshot.mutable_runtime_state()->CopyFrom(runtime);
    }
    auto update = makeUpdate(wire::CHANNEL_RUNTIME_STATE);
    update.mutable_runtime_state()->Swap(&runtime);
    publish(std::move(update));
}

void AutoVizServerNode::createSubscriptions()
{
    const auto qos = rclcpp::QoS(rclcpp::KeepLast(10)).best_effort();
    m_locationSubscription = create_subscription<custom_msgs::msg::Location>(
        topicName(wire::CHANNEL_VEHICLE_STATE), qos, std::bind(&AutoVizServerNode::onLocation, this, std::placeholders::_1));
    m_obstacleSubscription = create_subscription<custom_msgs::msg::FinalTargetArray>(
        topicName(wire::CHANNEL_OBSTACLES), qos, std::bind(&AutoVizServerNode::onObstacles, this, std::placeholders::_1));
    m_controlSubscription = create_subscription<custom_msgs::msg::ChassisCommand>(
        topicName(wire::CHANNEL_CONTROL_COMMAND), qos, std::bind(&AutoVizServerNode::onControl, this, std::placeholders::_1));
    m_chassisSubscription = create_subscription<custom_msgs::msg::ChassisStates>(
        topicName(wire::CHANNEL_CHASSIS_STATE), qos, std::bind(&AutoVizServerNode::onChassis, this, std::placeholders::_1));
    m_actionSubscription = create_subscription<custom_msgs::msg::SystemRunStates>(
        topicName(wire::CHANNEL_ACTION_STATE), qos, std::bind(&AutoVizServerNode::onAction, this, std::placeholders::_1));
    m_taskSubscription = create_subscription<custom_msgs::msg::TaskParams>(
        topicName(wire::CHANNEL_TASK_STATE), qos, std::bind(&AutoVizServerNode::onTask, this, std::placeholders::_1));
    m_localPathSubscription = create_subscription<custom_msgs::msg::TrajectoryMsg>(
        topicName(wire::CHANNEL_LOCAL_TRAJECTORY), qos, std::bind(&AutoVizServerNode::onLocalPath, this, std::placeholders::_1));
    m_globalPathSubscription = create_subscription<nav_msgs::msg::Path>(
        topicName(wire::CHANNEL_GLOBAL_TRAJECTORY), qos, std::bind(&AutoVizServerNode::onGlobalPath, this, std::placeholders::_1));
}

void AutoVizServerNode::onLocation(custom_msgs::msg::Location::ConstSharedPtr message)
{
    const auto receiveNs = nowNs();
    recordTopic(topicName(wire::CHANNEL_VEHICLE_STATE));

    wire::VehicleState state;
    fillHeader(state.mutable_header(), receiveNs, receiveNs, "robot_ws.location", "odom");
    state.mutable_position()->set_x_m(message->odom_x);
    state.mutable_position()->set_y_m(message->odom_y);
    state.mutable_position()->set_z_m(message->odom_z);
    state.set_heading_rad(message->heading);
    state.set_pitch_rad(message->pitch);
    state.set_roll_rad(message->roll);
    state.mutable_linear_velocity_mps()->set_x(message->velocity_x);
    state.mutable_linear_velocity_mps()->set_y(message->velocity_y);
    state.mutable_linear_velocity_mps()->set_z(message->velocity_z);
    state.mutable_linear_acceleration_mps2()->set_x(message->acc_x);
    state.mutable_linear_acceleration_mps2()->set_y(message->acc_y);
    state.mutable_linear_acceleration_mps2()->set_z(message->acc_z);
    state.set_speed_mps(message->velocity);
    state.set_yaw_rate_radps(message->omega_z);
    state.set_longitudinal_acceleration_mps2(message->acc);
    auto* vertical = state.mutable_vertical();
    vertical->set_odom_z_m(message->odom_z);
    vertical->set_depth_m(message->depth);
    vertical->set_height_above_bottom_m(message->height);
    vertical->set_vertical_velocity_mps(message->velocity_z);
    state.set_localization_status(message->status);
    state.set_localization_error(message->error);
    state.set_gps_time(message->gps_time);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_snapshot.mutable_vehicle_state()->CopyFrom(state);
    }
    auto update = makeUpdate(wire::CHANNEL_VEHICLE_STATE);
    update.mutable_vehicle_state()->Swap(&state);
    publish(std::move(update));
}

void AutoVizServerNode::onObstacles(custom_msgs::msg::FinalTargetArray::ConstSharedPtr message)
{
    const auto receiveNs = nowNs();
    recordTopic(topicName(wire::CHANNEL_OBSTACLES));
    wire::ObstacleSet obstacles;
    const auto sourceNs = rosStampNs(message->header.stamp);
    fillHeader(obstacles.mutable_header(), sourceNs, receiveNs, "robot_ws.final_targets", message->header.frame_id);
    for (const auto& target : message->targets) {
        if (target.length <= 0.0 || target.width <= 0.0) {
            continue;
        }
        auto* obstacle = obstacles.add_obstacle();
        fillHeader(obstacle->mutable_header(),
                   rosStampNs(target.header.stamp),
                   receiveNs,
                   "robot_ws.final_target",
                   target.header.frame_id);
        obstacle->set_id(std::to_string(target.target_id));
        obstacle->set_type(wire::Obstacle::TYPE_OTHER);
        obstacle->set_source_class(target.final_class);
        obstacle->set_class_label(target.final_class_label);
        obstacle->set_source(target.source_chain);
        obstacle->mutable_center()->set_x_m(target.real_center_point.x);
        obstacle->mutable_center()->set_y_m(target.real_center_point.y);
        obstacle->mutable_center()->set_z_m(target.real_center_point.z);
        obstacle->set_heading_rad(target.heading);
        obstacle->set_length_m(target.length);
        obstacle->set_width_m(target.width);
        obstacle->set_height_m(target.height);
        obstacle->set_is_static(true);
        obstacle->set_is_virtual(false);
        obstacle->set_confidence(target.final_confidence);
        addIntMetric(obstacle->mutable_attribute(), "target_status", target.target_status);
        addIntMetric(obstacle->mutable_attribute(), "magnetic_status", target.magnetic_status);
        addDoubleMetric(obstacle->mutable_attribute(), "sonar_confidence", target.sonar_confidence);
        addDoubleMetric(obstacle->mutable_attribute(), "laser_confidence", target.laser_confidence);
        addDoubleMetric(obstacle->mutable_attribute(), "magnetic_confidence", target.magnetic_confidence);
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_snapshot.mutable_obstacles()->CopyFrom(obstacles);
    }
    auto update = makeUpdate(wire::CHANNEL_OBSTACLES);
    update.mutable_obstacles()->Swap(&obstacles);
    publish(std::move(update));
}

void AutoVizServerNode::onControl(custom_msgs::msg::ChassisCommand::ConstSharedPtr message)
{
    const auto receiveNs = nowNs();
    recordTopic(topicName(wire::CHANNEL_CONTROL_COMMAND));
    wire::ControlCommand command;
    fillHeader(command.mutable_header(), receiveNs, receiveNs, "robot_ws.chassis_command");
    const bool crawl = message->mode == 6 || message->mode == 8 || message->mode == 11;
    const bool sailing = (message->mode >= 1 && message->mode <= 5) || message->mode == 7 || message->mode == 10;
    command.set_mode(crawl ? wire::ControlCommand::MODE_CRAWL
                           : (sailing ? wire::ControlCommand::MODE_SAILING : wire::ControlCommand::MODE_UNKNOWN));
    command.set_enabled(message->is_enable);
    command.set_target_speed_mps(message->speed);
    command.set_target_yaw_rate_radps(message->angular_velocity);
    command.set_target_heading_rad(message->heading);
    command.set_target_gear(message->expected_gear);
    command.set_navigation_mode(message->navi_mode);
    command.set_sonar_power_enabled(message->is_open_sonar_power);
    auto* vertical = command.mutable_vertical();
    vertical->set_target_depth_m(message->depth);
    vertical->set_target_height_above_bottom_m(message->height);
    vertical->set_dive_speed_mps(message->dive_speed);
    vertical->set_buoyancy_adjust(message->buoyancy_adjust);
    vertical->set_left_thruster_command(message->left_water_actuator_speed);
    vertical->set_right_thruster_command(message->right_water_actuator_speed);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_snapshot.mutable_control_command()->CopyFrom(command);
    }
    auto update = makeUpdate(wire::CHANNEL_CONTROL_COMMAND);
    update.mutable_control_command()->Swap(&command);
    publish(std::move(update));
}

void AutoVizServerNode::onChassis(custom_msgs::msg::ChassisStates::ConstSharedPtr message)
{
    const auto receiveNs = nowNs();
    recordTopic(topicName(wire::CHANNEL_CHASSIS_STATE));
    wire::ChassisState chassis;
    fillHeader(chassis.mutable_header(), receiveNs, receiveNs, "robot_ws.chassis_states");
    chassis.set_speed_mps(message->current_speed);
    // robot_ws 的反馈为左负右正；协议统一为左正右负。
    chassis.set_yaw_rate_radps(-message->current_angular_velocity);
    chassis.set_gear(message->gear_status);

    addIntMetric(chassis.mutable_metric(), "water_tank_level", message->water_tank_level_status);
    addBoolMetric(chassis.mutable_metric(), "water_tank_level_is_raw", message->water_tank_level_is_raw);
    addIntMetric(chassis.mutable_metric(), "water_tank_status", message->water_tank_status);
    addIntMetric(chassis.mutable_metric(), "water_heartbeat", message->water_heartbeat);
    addIntMetric(chassis.mutable_metric(), "crawl_heartbeat", message->crawl_heartbeat);
    addBoolMetric(chassis.mutable_metric(), "dccdc_status", message->dccdc_status);
    addDoubleMetric(chassis.mutable_metric(), "smart_power_input_voltage", message->smart_power_input_voltage_status, "V");

    const std::vector<std::pair<std::string, int>> simpleActuators{
        {"left_tail_thruster", message->left_tail_actuator_status},
        {"right_tail_thruster", message->right_tail_actuator_status},
        {"left_vertical_thruster", message->left_vertical_actuator_status},
        {"right_vertical_thruster", message->right_vertical_actuator_status},
        {"back_vertical_thruster", message->back_vertical_actuator_status}};
    for (const auto& item : simpleActuators) {
        auto* actuator = chassis.add_actuator();
        actuator->set_id(item.first);
        actuator->set_fault(item.second != 0);
        actuator->set_fault_code(item.second);
    }

    auto* left = chassis.add_actuator();
    left->set_id("left_crawl_motor");
    left->set_fault(message->left_crawl_motor_fault || message->left_crawl_actuator_fault_code != 0);
    left->set_fault_code(message->left_crawl_motor_fault_code);
    addMotorMetrics(left,
                    message->left_crawl_motor_speed_rpm,
                    message->left_crawl_motor_torque_or_q_axis_current,
                    message->left_crawl_motor_temperature,
                    message->left_crawl_motor_bus_voltage,
                    message->left_crawl_motor_controller_ready,
                    message->left_crawl_motor_output_enabled,
                    message->left_crawl_motor_controller_u_temperature,
                    message->left_crawl_motor_controller_v_temperature,
                    message->left_crawl_motor_command_enable,
                    message->left_crawl_motor_command_speed_mode,
                    message->left_crawl_motor_command_reverse,
                    message->left_crawl_motor_command_speed_rpm,
                    message->left_crawl_motor_command_torque_or_q_axis_current);

    auto* right = chassis.add_actuator();
    right->set_id("right_crawl_motor");
    right->set_fault(message->right_crawl_motor_fault || message->right_crawl_actuator_fault_code != 0);
    right->set_fault_code(message->right_crawl_motor_fault_code);
    addMotorMetrics(right,
                    message->right_crawl_motor_speed_rpm,
                    message->right_crawl_motor_torque_or_q_axis_current,
                    message->right_crawl_motor_temperature,
                    message->right_crawl_motor_bus_voltage,
                    message->right_crawl_motor_controller_ready,
                    message->right_crawl_motor_output_enabled,
                    message->right_crawl_motor_controller_u_temperature,
                    message->right_crawl_motor_controller_v_temperature,
                    message->right_crawl_motor_command_enable,
                    message->right_crawl_motor_command_speed_mode,
                    message->right_crawl_motor_command_reverse,
                    message->right_crawl_motor_command_speed_rpm,
                    message->right_crawl_motor_command_torque_or_q_axis_current);

    auto* battery = chassis.mutable_battery();
    battery->set_valid(true);
    battery->set_state_of_charge_percent(message->high_voltage_bms_soc_status);
    battery->set_pack_voltage_v(message->high_voltage_bms_pack_voltage);
    battery->set_pack_current_a(message->high_voltage_bms_pack_current);
    battery->set_alarm_level(message->high_voltage_bms_alarm_level);
    addIntMetric(battery->mutable_metric(), "self_check_status", message->high_voltage_bms_status);
    addIntMetric(battery->mutable_metric(), "heartbeat", message->high_voltage_bms_heartbeat);
    addIntMetric(battery->mutable_metric(), "current_status", message->high_voltage_bms_current_status);
    addIntMetric(battery->mutable_metric(), "max_cell_voltage_index", message->high_voltage_bms_max_cell_voltage_index);
    addDoubleMetric(battery->mutable_metric(), "max_cell_voltage", message->high_voltage_bms_max_cell_voltage, "V");
    addIntMetric(battery->mutable_metric(), "min_cell_voltage_index", message->high_voltage_bms_min_cell_voltage_index);
    addDoubleMetric(battery->mutable_metric(), "min_cell_voltage", message->high_voltage_bms_min_cell_voltage, "V");
    addIntMetric(battery->mutable_metric(), "max_temperature_index", message->high_voltage_bms_max_temperature_index);
    addIntMetric(battery->mutable_metric(), "max_temperature", message->high_voltage_bms_max_temperature, "degC");
    addIntMetric(battery->mutable_metric(), "min_temperature_index", message->high_voltage_bms_min_temperature_index);
    addIntMetric(battery->mutable_metric(), "min_temperature", message->high_voltage_bms_min_temperature, "degC");

    const int warnings[] = {
        message->high_voltage_bms_total_voltage_over_high_warning,
        message->high_voltage_bms_cell_voltage_over_high_warning,
        message->high_voltage_bms_charge_temperature_over_high_warning,
        message->high_voltage_bms_charge_temperature_over_low_warning,
        message->high_voltage_bms_total_voltage_over_low_warning,
        message->high_voltage_bms_discharge_current_over_warning,
        message->high_voltage_bms_cell_voltage_over_low_warning,
        message->high_voltage_bms_discharge_temperature_over_high_warning,
        message->high_voltage_bms_discharge_temperature_over_low_warning,
        message->high_voltage_bms_soc_over_low_warning,
        message->high_voltage_bms_pack_voltage_difference_over_warning,
        message->high_voltage_bms_pack_temperature_difference_over_warning};
    for (std::size_t index = 0; index < std::size(warnings); ++index) {
        addIntMetric(battery->mutable_metric(), "warning_" + std::to_string(index + 1), warnings[index]);
    }
    const int power[] = {
        message->power_supply_1_status, message->power_supply_2_status,
        message->power_supply_3_status, message->power_supply_4_status,
        message->power_supply_5_status, message->power_supply_6_status,
        message->power_supply_7_status, message->power_supply_8_status,
        message->power_supply_9_status, message->power_supply_10_status,
        message->power_supply_11_status, message->power_supply_12_status,
        message->power_supply_13_status, message->power_supply_14_status,
        message->power_supply_15_status, message->power_supply_16_status};
    for (std::size_t index = 0; index < std::size(power); ++index) {
        addIntMetric(chassis.mutable_metric(), "power_supply_" + std::to_string(index + 1), power[index]);
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_snapshot.mutable_chassis_state()->CopyFrom(chassis);
    }
    auto update = makeUpdate(wire::CHANNEL_CHASSIS_STATE);
    update.mutable_chassis_state()->Swap(&chassis);
    publish(std::move(update));
}

void AutoVizServerNode::onAction(custom_msgs::msg::SystemRunStates::ConstSharedPtr message)
{
    const auto receiveNs = nowNs();
    recordTopic(topicName(wire::CHANNEL_ACTION_STATE));
    wire::ActionState state;
    fillHeader(state.mutable_header(), receiveNs, receiveNs, "robot_ws.system_run_states");
    state.set_owner(message->owner);
    state.set_state(message->state);
    state.set_goal_id(message->goal_uuid);
    state.set_chassis_mode(message->chassis_mode);
    state.set_enabled(message->is_enable);
    state.set_navigation_mode(message->navi_mode);
    state.set_target_speed_mps(message->target_speed);
    state.set_target_heading_rad(message->target_heading);
    state.set_target_yaw_rate_radps(message->target_angular_velocity * kDegreesToRadians);
    auto* vertical = state.mutable_vertical();
    vertical->set_target_depth_m(message->target_depth);
    vertical->set_target_height_above_bottom_m(message->target_height);
    vertical->set_buoyancy_adjust(message->buoyancy_adjust);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_snapshot.mutable_action_state()->CopyFrom(state);
    }
    auto update = makeUpdate(wire::CHANNEL_ACTION_STATE);
    update.mutable_action_state()->Swap(&state);
    publish(std::move(update));
}

void AutoVizServerNode::onTask(custom_msgs::msg::TaskParams::ConstSharedPtr message)
{
    const auto receiveNs = nowNs();
    recordTopic(topicName(wire::CHANNEL_TASK_STATE));
    wire::TaskState state;
    fillHeader(state.mutable_header(), receiveNs, receiveNs, "robot_ws.task_params");
    state.set_task_type(message->task_type);
    state.set_task_id(message->task_id);
    state.set_enabled(message->task_enable);
    state.set_emergency_stop(message->emergency_stop);
    state.set_remote_mode(message->remote_mode);
    state.set_power_enable(message->power_enable);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_snapshot.mutable_task_state()->CopyFrom(state);
    }
    auto update = makeUpdate(wire::CHANNEL_TASK_STATE);
    update.mutable_task_state()->Swap(&state);
    publish(std::move(update));
}

void AutoVizServerNode::onLocalPath(custom_msgs::msg::TrajectoryMsg::ConstSharedPtr message)
{
    const auto receiveNs = nowNs();
    recordTopic(topicName(wire::CHANNEL_LOCAL_TRAJECTORY));
    wire::Trajectory trajectory;
    const auto sourceNs = rosStampNs(message->header.stamp);
    fillHeader(trajectory.mutable_header(), sourceNs, receiveNs, "robot_ws.local_path", message->header.frame_id);
    trajectory.set_kind(wire::Trajectory::KIND_LOCAL);
    trajectory.set_goal_id(message->goal_uuid);
    for (const auto& sourcePoint : message->trajectory) {
        auto* point = trajectory.add_point();
        auto* pathPoint = point->mutable_path_point();
        pathPoint->mutable_position()->set_x_m(sourcePoint.pose.position.x);
        pathPoint->mutable_position()->set_y_m(sourcePoint.pose.position.y);
        pathPoint->mutable_position()->set_z_m(sourcePoint.pose.position.z);
        pathPoint->set_heading_rad(yawFromQuaternion(sourcePoint.pose.orientation));
        point->set_speed_mps(sourcePoint.velocity.linear.x);
        point->set_acceleration_mps2(sourcePoint.acceleration.linear.x);
        const double relative = static_cast<double>(sourcePoint.time_from_start.sec)
                                + static_cast<double>(sourcePoint.time_from_start.nanosec) * 1e-9;
        point->set_relative_time_s(relative);
        point->set_absolute_time_s(static_cast<double>(sourceNs) * 1e-9 + relative);
    }
    trajectory.set_total_length_m(polylineLength(trajectory));
    if (trajectory.point_size() > 0) {
        trajectory.set_total_time_s(trajectory.point(trajectory.point_size() - 1).relative_time_s());
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_snapshot.mutable_local_trajectory()->CopyFrom(trajectory);
    }
    auto update = makeUpdate(wire::CHANNEL_LOCAL_TRAJECTORY);
    update.mutable_trajectory()->Swap(&trajectory);
    publish(std::move(update));
}

void AutoVizServerNode::onGlobalPath(nav_msgs::msg::Path::ConstSharedPtr message)
{
    const auto receiveNs = nowNs();
    recordTopic(topicName(wire::CHANNEL_GLOBAL_TRAJECTORY));
    wire::Trajectory trajectory;
    const auto sourceNs = rosStampNs(message->header.stamp);
    fillHeader(trajectory.mutable_header(), sourceNs, receiveNs, "robot_ws.global_path", message->header.frame_id);
    trajectory.set_kind(wire::Trajectory::KIND_GLOBAL);
    for (const auto& pose : message->poses) {
        auto* point = trajectory.add_point();
        auto* pathPoint = point->mutable_path_point();
        pathPoint->mutable_position()->set_x_m(pose.pose.position.x);
        pathPoint->mutable_position()->set_y_m(pose.pose.position.y);
        pathPoint->mutable_position()->set_z_m(pose.pose.position.z);
        pathPoint->set_heading_rad(yawFromQuaternion(pose.pose.orientation));
    }
    trajectory.set_total_length_m(polylineLength(trajectory));
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_snapshot.mutable_global_trajectory()->CopyFrom(trajectory);
    }
    auto update = makeUpdate(wire::CHANNEL_GLOBAL_TRAJECTORY);
    update.mutable_trajectory()->Swap(&trajectory);
    publish(std::move(update));
}

}  // namespace autoviz_server
