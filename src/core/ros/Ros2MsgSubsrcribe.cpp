#include "core/ros/Ros2MsgSubsrcribe.h"

#include <chrono>
#include <cmath>
#include <functional>
#include <utility>

#include <QDateTime>

#include "utils/Logger.h"



namespace autoviz::ros {

namespace {
// 回放和低频发布都可能出现短暂空档；5 秒后才视为订阅链路异常。
constexpr qint64 kDefaultTopicTimeoutMs = 5000;
constexpr qint64 kPathTopicTimeoutMs = 5000;

qint64 currentTimestampMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

struct TopicSpec {
    const char* name;
    const char* type;
    qint64 timeoutMs;
};

const TopicSpec kTopicSpecs[] = {
    {"/location", "custom_msgs/msg/Location", kDefaultTopicTimeoutMs},
    {"/targets/final_objects", "custom_msgs/msg/FinalTargetArray", kDefaultTopicTimeoutMs},
    {"/chassis_command", "custom_msgs/msg/ChassisCommand", kDefaultTopicTimeoutMs},
    {"/chassis_states", "custom_msgs/msg/ChassisStates", kDefaultTopicTimeoutMs},
    {"/system_run_states", "custom_msgs/msg/SystemRunStates", kDefaultTopicTimeoutMs},
    {"/task_params", "custom_msgs/msg/TaskParams", kDefaultTopicTimeoutMs},
    {"/local_path", "custom_msgs/msg/TrajectoryMsg", kPathTopicTimeoutMs},
    {"/global_path", "nav_msgs/msg/Path", kPathTopicTimeoutMs},
};

template <typename PointAccessor>
double polylineLength(int count, PointAccessor pointAt)
{
    double length = 0.0;
    for (int index = 1; index < count; ++index) {
        const auto previous = pointAt(index - 1);
        const auto current = pointAt(index);
        const double dx = current.first - previous.first;
        const double dy = current.second - previous.second;
        length += std::hypot(dx, dy);
    }
    return length;
}
}

Ros2MsgSubsrcribe::Ros2MsgSubsrcribe(datacenter::DataManager* dataManager)
    : RosMsgSubscribeBase(dataManager)
{
}

Ros2MsgSubsrcribe::~Ros2MsgSubsrcribe()
{
    stop();
}

SubscribeBackend Ros2MsgSubsrcribe::backend() const
{
    return SubscribeBackend::Ros2;
}

bool Ros2MsgSubsrcribe::initialize(QString* errorMessage)
{
    resetVisualizationData();
    initializeTopicMonitors();

#if !AUTOVIZ_ENABLE_ROS2
    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("当前构建未启用 AUTOVIZ_ENABLE_ROS2。");
    }
    return false;
#else
    if (dataManager() == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Ros2MsgSubsrcribe 未绑定 DataManager。");
        }
        return false;
    }

    if (!rclcpp::ok()) {
        rclcpp::init(0, nullptr);
    }

    try {
        // 【标准 ROS2 节点初始化】
        m_node = std::make_shared<rclcpp::Node>("autoviz_ros2_node");
        Logger::instance().info(QStringLiteral("[ROS2] 节点初始化成功"));
        Logger::instance().info(
            QStringLiteral("ROS2 订阅准备完成：/location /targets/final_objects /chassis_command /chassis_states /system_run_states /task_params /local_path /global_path"));
        return true;
    } catch (...) {
        Logger::instance().error(QStringLiteral("[ROS2] 节点初始化失败"));
        return false;
    }
#endif
}

bool Ros2MsgSubsrcribe::start(QString* errorMessage)
{
    Q_UNUSED(errorMessage);

#if !AUTOVIZ_ENABLE_ROS2
    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("当前构建未启用 AUTOVIZ_ENABLE_ROS2。");
    }
    return false;
#else

    if (!m_node) {
        if (errorMessage) *errorMessage = "ROS2 节点未初始化";
        return false;
    }
    if (m_running.load()) {
        Logger::instance().warning(QStringLiteral("[ROS2] start() 被重复调用，当前订阅已在运行。"));
        return true;
    }
    if (m_spinThread.joinable()) {
        m_spinThread.join();
    }
    // 【你最熟悉的 ROS2 订阅写法】
    m_sub_location = m_node->create_subscription<custom_msgs::msg::Location>(
        "/location", 10, std::bind(&Ros2MsgSubsrcribe::callbackLocationMsg, this, std::placeholders::_1));

    m_sub_final_targets = m_node->create_subscription<custom_msgs::msg::FinalTargetArray>(
        "/targets/final_objects", 10, std::bind(&Ros2MsgSubsrcribe::callbackFinalTargetArrayMsg, this, std::placeholders::_1));

    m_sub_chassis_command = m_node->create_subscription<custom_msgs::msg::ChassisCommand>(
        "/chassis_command", 10, std::bind(&Ros2MsgSubsrcribe::callbackChassisCommandMsg, this, std::placeholders::_1));

    m_sub_chassis_states = m_node->create_subscription<custom_msgs::msg::ChassisStates>(
        "/chassis_states", 10, std::bind(&Ros2MsgSubsrcribe::callbackChassisStatesMsg, this, std::placeholders::_1));
    m_sub_system_run_states = m_node->create_subscription<custom_msgs::msg::SystemRunStates>(
        "/system_run_states", 10, std::bind(&Ros2MsgSubsrcribe::callbackSystemRunStatesMsg, this, std::placeholders::_1));
    m_sub_task_params = m_node->create_subscription<custom_msgs::msg::TaskParams>(
        "/task_params", 10, std::bind(&Ros2MsgSubsrcribe::callbackTaskParamsMsg, this, std::placeholders::_1));
    m_sub_trajectory = m_node->create_subscription<custom_msgs::msg::TrajectoryMsg>(
        "/local_path", 10, std::bind(&Ros2MsgSubsrcribe::callbackLocalPathMsg, this, std::placeholders::_1));

    m_sub_path = m_node->create_subscription<nav_msgs::msg::Path>(
        "/global_path", 10, std::bind(&Ros2MsgSubsrcribe::callbackGlobalPathMsg, this, std::placeholders::_1));

    // 使用长期存活的执行器。rclcpp::spin_some(node) 每次调用都会临时创建并
    // 销毁执行器；在 Fast DDS 下高频重复挂接 WaitSet 会造成堆损坏。
    m_executor = std::make_unique<rclcpp::executors::SingleThreadedExecutor>();
    m_executor->add_node(m_node);

    m_running.store(true);
    m_spinThread = std::thread([this]() {
        try {
            m_executor->spin();
        } catch (const std::exception& exception) {
            Logger::instance().error(
                QStringLiteral("[ROS2] 执行器异常退出：%1").arg(QString::fromLocal8Bit(exception.what())));
        }
    });

    Logger::instance().info(QStringLiteral("[ROS2] 订阅已启动"));
    return true;

#endif
}

void Ros2MsgSubsrcribe::stop()
{
#if AUTOVIZ_ENABLE_ROS2
    if (!m_running.load() && !m_spinThread.joinable() && !m_node) {
        return;
    }

    m_running.store(false);
    if (m_executor) {
        m_executor->cancel();
    }
    if (m_spinThread.joinable()) {
        m_spinThread.join();
    }
    if (m_executor && m_node) {
        m_executor->remove_node(m_node);
    }
    m_executor.reset();
    m_sub_location.reset();
    m_sub_scene.reset();
    m_sub_final_targets.reset();
    m_sub_chassis_command.reset();
    m_sub_chassis_states.reset();
    m_sub_system_run_states.reset();
    m_sub_task_params.reset();
    m_sub_trajectory.reset();
    m_sub_path.reset();
    m_node.reset();
#else
    m_running.store(false);
#endif
}

QString Ros2MsgSubsrcribe::statusSummary() const
{
    return m_running.load() ? QStringLiteral("ROS2 订阅中：/location /targets/final_objects /chassis_command /chassis_states /system_run_states /task_params /local_path /global_path")
                            : QStringLiteral("ROS2 未启动");
}

void Ros2MsgSubsrcribe::initializeTopicMonitors()
{
    m_topicMonitors.clear();
    autoviz::model::TopicStatusList statuses;
    for (const auto& spec : kTopicSpecs) {
        TopicMonitorState state;
        state.status.name = QString::fromLatin1(spec.name);
        state.status.type = QString::fromLatin1(spec.type);
        state.status.timeoutMs = spec.timeoutMs;
        state.status.timedOut = true;
        m_topicMonitors.insert(state.status.name, state);
        statuses.push_back(state.status);
    }
    if (dataManager() != nullptr) {
        dataManager()->setTopicStatuses(statuses);
    }
}

void Ros2MsgSubsrcribe::recordTopicMessage(const QString& topicName)
{
    if (dataManager() == nullptr) {
        return;
    }

    auto iter = m_topicMonitors.find(topicName);
    if (iter == m_topicMonitors.end()) {
        return;
    }

    auto& state = iter.value();
    const qint64 nowMs = currentTimestampMs();
    if (state.previousUpdateMs > 0) {
        const qint64 intervalMs = nowMs - state.previousUpdateMs;
        if (intervalMs > 0) {
            state.status.frequencyHz = 1000.0 / static_cast<double>(intervalMs);
        }
    }
    state.previousUpdateMs = nowMs;
    state.status.lastUpdateMs = nowMs;
    state.status.ageMs = 0;
    state.status.timedOut = false;
    ++state.status.messageCount;
    dataManager()->setTopicStatus(state.status);
}

#if AUTOVIZ_ENABLE_ROS2
void Ros2MsgSubsrcribe::callbackLocationMsg(const custom_msgs::msg::Location::ConstSharedPtr msg)
{
    if (dataManager() == nullptr || msg == nullptr) {
        return;
    }
    recordTopicMessage(QStringLiteral("/location"));
    vehicleLocation_.header.timestamp = currentTimestampMs();
    // 【将 location 消息转换为 VehicleLocation 结构体】
    vehicleLocation_.position.x = msg->odom_x;
    vehicleLocation_.position.y = msg->odom_y;
    vehicleLocation_.heading = msg->heading;
    vehicleLocation_.speed = msg->velocity;
    vehicleLocation_.roll = msg->roll;
    vehicleLocation_.pitch = msg->pitch;
    vehicleLocation_.Velocity.x = msg->velocity_x;
    vehicleLocation_.Velocity.y = msg->velocity_y;
    vehicleLocation_.Velocity.z = msg->velocity_z;
    vehicleLocation_.yawRate = msg->omega_z;
    vehicleLocation_.acceleration = msg->acc;
    //std::cout << "vehicleLocation_: " << msg->odom_x << ", " << msg->odom_y << ", " << msg->heading << ", " << msg->velocity << std::endl;
    dataManager()->setVehicleLocation(vehicleLocation_);

    autoviz::model::LocalizationStatus status;
    status.valid = true;
    status.timestampMs = vehicleLocation_.header.timestamp;
    status.gpsTime = msg->gps_time;
    status.status = msg->status;
    status.error = msg->error;
    status.odomX = msg->odom_x;
    status.odomY = msg->odom_y;
    status.odomZ = msg->odom_z;
    status.heading = msg->heading;
    status.pitch = msg->pitch;
    status.roll = msg->roll;
    status.velocityX = msg->velocity_x;
    status.velocityY = msg->velocity_y;
    status.velocityZ = msg->velocity_z;
    status.velocity = msg->velocity;
    status.omegaZ = msg->omega_z;
    status.acc = msg->acc;
    status.depth = msg->depth;
    status.height = msg->height;
    dataManager()->setLocalizationStatus(status);
}
void Ros2MsgSubsrcribe::callbackSceneMsg(const custom_msgs::msg::Scene::ConstSharedPtr msg)
{
    if (dataManager() == nullptr || msg == nullptr) {
        return;
    }
    recordTopicMessage(QStringLiteral("/scene"));

    obstacles_.clear();
    for (const auto& object : msg->objects) {
        if (object.type == 0 || object.length <= 0.0 || object.width <= 0.0) {
            continue;
        }

        autoviz::model::Obstacle obstacle;
        obstacle.id = static_cast<int>(object.id);
        obstacle.type = autoviz::model::ObstacleType::Unknown;
        obstacle.shape = autoviz::model::ObstacleShapeType::Box;
        obstacle.header.timestamp = currentTimestampMs();
        obstacle.isStatic = true;
        obstacle.isVirtual = false;
        obstacle.position.position.x = object.real_center_point.x;
        obstacle.position.position.y = object.real_center_point.y;
        obstacle.position.theta = object.heading;
        obstacle.length = object.length;
        obstacle.width = object.width;
        obstacle.boundingBox.center = obstacle.position.position;
        obstacle.boundingBox.heading = object.heading;
        obstacle.boundingBox.length = object.length;
        obstacle.boundingBox.width = object.width;
        obstacle.anchorPoint = obstacle.position.position;
        obstacles_.push_back(obstacle);
    }

    dataManager()->setObstacles(obstacles_);
}

void Ros2MsgSubsrcribe::callbackFinalTargetArrayMsg(const custom_msgs::msg::FinalTargetArray::ConstSharedPtr msg)
{
    if (dataManager() == nullptr || msg == nullptr) {
        return;
    }
    recordTopicMessage(QStringLiteral("/targets/final_objects"));

    obstacles_.clear();
    for (const auto& target : msg->targets) {
        if (target.length <= 0.0 || target.width <= 0.0) {
            continue;
        }

        autoviz::model::Obstacle obstacle;
        obstacle.id = static_cast<int>(target.target_id);
        obstacle.type = autoviz::model::ObstacleType::Unknown;
        obstacle.sourceClass = static_cast<int>(target.final_class);
        obstacle.classLabel = QString::fromStdString(target.final_class_label);
        obstacle.sourceTopic = QStringLiteral("/targets/final_objects");
        obstacle.shape = autoviz::model::ObstacleShapeType::Box;
        obstacle.header.timestamp = currentTimestampMs();
        obstacle.header.frameId = QString::fromStdString(msg->header.frame_id);
        obstacle.isStatic = true;
        obstacle.isVirtual = false;
        obstacle.position.position.x = target.real_center_point.x;
        obstacle.position.position.y = target.real_center_point.y;
        obstacle.position.theta = target.heading;
        obstacle.length = target.length;
        obstacle.width = target.width;
        obstacle.boundingBox.center = obstacle.position.position;
        obstacle.boundingBox.heading = target.heading;
        obstacle.boundingBox.length = target.length;
        obstacle.boundingBox.width = target.width;
        obstacle.anchorPoint = obstacle.position.position;
        obstacles_.push_back(obstacle);
    }

    dataManager()->setObstacles(obstacles_);
}
void Ros2MsgSubsrcribe::callbackChassisCommandMsg(const custom_msgs::msg::ChassisCommand::ConstSharedPtr msg)
{
    if (dataManager() == nullptr || msg == nullptr) {
        return;
    }
    recordTopicMessage(QStringLiteral("/chassis_command"));

    controlCmd_ = autoviz::model::ControlCmd{};
    if (msg->is_enable) {
        controlCmd_.header.timestamp = currentTimestampMs();
        if (msg->mode == 6) { // 爬行
            controlCmd_.mode = autoviz::model::ControlMode::Crawl;
            controlCmd_.desiredVelocity = msg->velocity;
            controlCmd_.desiredAngularVelocity = msg->angular_velocity;
            controlCmd_.desiredGear = static_cast<int>(msg->expected_gear);
        } else if (msg->mode == 4 || msg->mode == 5) { // 航行
            controlCmd_.mode = autoviz::model::ControlMode::Sailing;
            controlCmd_.desiredVelocity = msg->speed;
            controlCmd_.desiredHeading = msg->heading;
        } else {
            controlCmd_ = autoviz::model::ControlCmd{};
        }
    }

    dataManager()->setControlCmd(controlCmd_);

    autoviz::model::ControlCommandStatus status;
    status.valid = true;
    status.timestampMs = currentTimestampMs();
    status.mode = msg->mode;
    status.isEnable = msg->is_enable;
    status.velocity = msg->velocity;
    status.angularVelocity = msg->angular_velocity;
    status.expectedGear = msg->expected_gear;
    status.isUseWaterActuator = msg->is_use_water_actuator;
    status.depth = msg->depth;
    status.height = msg->height;
    status.heading = msg->heading;
    status.speed = msg->speed;
    status.diveSpeed = msg->dive_speed;
    status.leftWaterActuatorSpeed = msg->left_water_actuator_speed;
    status.rightWaterActuatorSpeed = msg->right_water_actuator_speed;
    status.buoyancyAdjust = msg->buoyancy_adjust;
    status.isOpenSonarPower = msg->is_open_sonar_power;
    dataManager()->setControlCommandStatus(status);
}
void Ros2MsgSubsrcribe::callbackChassisStatesMsg(const custom_msgs::msg::ChassisStates::ConstSharedPtr msg)
{
    if (dataManager() == nullptr || msg == nullptr) {
        return;
    }
    recordTopicMessage(QStringLiteral("/chassis_states"));

    vehicleChassisInfo_ = autoviz::model::VehicleChassisInfo{};
    vehicleChassisInfo_.header.timestamp = currentTimestampMs();
    vehicleChassisInfo_.currentSpeed = msg->current_speed;
    vehicleChassisInfo_.currentAngularVelocity = msg->current_angular_velocity;
    vehicleChassisInfo_.currentGearPosition = msg->gear_status;

    dataManager()->setVehicleChassisInfo(vehicleChassisInfo_);

    autoviz::model::ChassisRuntimeStatus status;
    status.valid = true;
    status.timestampMs = vehicleChassisInfo_.header.timestamp;
    status.currentSpeed = msg->current_speed;
    status.currentAngularVelocity = msg->current_angular_velocity;
    status.gearStatus = msg->gear_status;
    status.waterTankLevelStatus = msg->water_tank_level_status;
    status.waterTankStatus = msg->water_tank_status;
    status.waterHeartbeat = msg->water_heartbeat;
    status.crawlHeartbeat = msg->crawl_heartbeat;
    status.leftTailActuatorStatus = msg->left_tail_actuator_status;
    status.rightTailActuatorStatus = msg->right_tail_actuator_status;
    status.leftVerticalActuatorStatus = msg->left_vertical_actuator_status;
    status.rightVerticalActuatorStatus = msg->right_vertical_actuator_status;
    status.backVerticalActuatorStatus = msg->back_vertical_actuator_status;
    status.leftCrawlActuatorFaultCode = msg->left_crawl_actuator_fault_code;
    status.rightCrawlActuatorFaultCode = msg->right_crawl_actuator_fault_code;
    status.highVoltageBmsStatus = msg->high_voltage_bms_status;
    status.dccdcStatus = msg->dccdc_status;
    status.highVoltageBmsSocStatus = msg->high_voltage_bms_soc_status;
    status.smartPowerInputVoltageStatus = msg->smart_power_input_voltage_status;
    dataManager()->setChassisRuntimeStatus(status);
}
void Ros2MsgSubsrcribe::callbackSystemRunStatesMsg(const custom_msgs::msg::SystemRunStates::ConstSharedPtr msg)
{
    if (dataManager() == nullptr || msg == nullptr) {
        return;
    }
    recordTopicMessage(QStringLiteral("/system_run_states"));

    autoviz::model::ActionRuntimeStatus status;
    status.valid = true;
    status.timestampMs = currentTimestampMs();
    status.owner = msg->owner;
    status.state = msg->state;
    status.chassisMode = msg->chassis_mode;
    status.isEnable = msg->is_enable;
    status.targetDepth = msg->target_depth;
    status.targetHeight = msg->target_height;
    status.buoyancyAdjust = msg->buoyancy_adjust;
    status.targetSpeed = msg->target_speed;
    dataManager()->setActionRuntimeStatus(status);
}

void Ros2MsgSubsrcribe::callbackTaskParamsMsg(const custom_msgs::msg::TaskParams::ConstSharedPtr msg)
{
    if (dataManager() == nullptr || msg == nullptr) {
        return;
    }
    recordTopicMessage(QStringLiteral("/task_params"));

    autoviz::model::TaskRuntimeStatus status;
    status.valid = true;
    status.timestampMs = currentTimestampMs();
    status.taskType = msg->task_type;
    status.taskId = msg->task_id;
    status.taskEnable = msg->task_enable;
    status.emergencyStop = msg->emergency_stop;
    status.remoteMode = msg->remote_mode;
    status.powerEnable = msg->power_enable;
    dataManager()->setTaskRuntimeStatus(status);
}

void Ros2MsgSubsrcribe::callbackLocalPathMsg(const custom_msgs::msg::TrajectoryMsg::ConstSharedPtr msg)
{
    if (dataManager() == nullptr || msg == nullptr) {
        return;
    }
    recordTopicMessage(QStringLiteral("/local_path"));

    // 【将 local_path 消息转换为 Trajectory 结构体】
    local_path_.points.clear();
    local_path_.header.timestamp = currentTimestampMs();
    local_path_.header.frameId = QString::fromStdString(msg->header.frame_id);

    for (const auto& point : msg->trajectory)
    {
        // 正确获取 x y z
        double x = point.pose.position.x;
        double y = point.pose.position.y;
        double z = point.pose.position.z;

        // 正确构造 TrajectoryPoint 对象
        autoviz::model::TrajectoryPoint tp;
        tp.position.x = x;
        tp.position.y = y;

        local_path_.points.push_back(tp);
    }
    dataManager()->setLocalPath(local_path_);

    autoviz::model::PathRuntimeStatus status;
    status.valid = !local_path_.points.isEmpty();
    status.timestampMs = currentTimestampMs();
    status.frameId = QString::fromStdString(msg->header.frame_id);
    status.pointCount = local_path_.points.size();
    status.length = polylineLength(msg->trajectory.size(), [msg](int index) {
        const auto& position = msg->trajectory.at(index).pose.position;
        return std::make_pair(position.x, position.y);
    });
    dataManager()->setLocalPathStatus(status);
}
void Ros2MsgSubsrcribe::callbackGlobalPathMsg(const nav_msgs::msg::Path::ConstSharedPtr msg)
{
    if (dataManager() == nullptr || msg == nullptr) {
        return;
    }
    recordTopicMessage(QStringLiteral("/global_path"));

    // 【将 global_path 消息转换为 Trajectory 结构体】
    global_path_.points.clear();
    global_path_.header.timestamp = currentTimestampMs();
    global_path_.header.frameId = QString::fromStdString(msg->header.frame_id);
    for (const auto& point : msg->poses)
    {
        // nav_msgs/Path 正确层级：poses -> pose -> position
        double x = point.pose.position.x;
        double y = point.pose.position.y;
        double z = point.pose.position.z;

        // 正确构造
        autoviz::model::TrajectoryPoint tp;
        tp.position.x = x;
        tp.position.y = y;

        global_path_.points.push_back(tp);
    }
    dataManager()->setGlobalPath(global_path_);

    autoviz::model::PathRuntimeStatus status;
    status.valid = !global_path_.points.isEmpty();
    status.timestampMs = currentTimestampMs();
    status.frameId = QString::fromStdString(msg->header.frame_id);
    status.pointCount = global_path_.points.size();
    status.length = polylineLength(msg->poses.size(), [msg](int index) {
        const auto& position = msg->poses.at(index).pose.position;
        return std::make_pair(position.x, position.y);
    });
    dataManager()->setGlobalPathStatus(status);
}
#endif
} // namespace autoviz::ros
