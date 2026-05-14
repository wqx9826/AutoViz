#include "core/ros/Ros2MsgSubsrcribe.h"

#include <chrono>
#include <functional>

#include <QDateTime>

#include "utils/Logger.h"



namespace autoviz::ros {

namespace {
qint64 currentTimestampMs()
{
    return QDateTime::currentMSecsSinceEpoch();
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
            QStringLiteral("ROS2 订阅准备完成：/location /scene /chassis_command /chassis_states /local_path /global_path"));
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

    m_sub_scene = m_node->create_subscription<custom_msgs::msg::Scene>(
        "/scene", 10, std::bind(&Ros2MsgSubsrcribe::callbackSceneMsg, this, std::placeholders::_1));

    m_sub_chassis_command = m_node->create_subscription<custom_msgs::msg::ChassisCommand>(
        "/chassis_command", 10, std::bind(&Ros2MsgSubsrcribe::callbackChassisCommandMsg, this, std::placeholders::_1));

    m_sub_chassis_states = m_node->create_subscription<custom_msgs::msg::ChassisStates>(
        "/chassis_states", 10, std::bind(&Ros2MsgSubsrcribe::callbackChassisStatesMsg, this, std::placeholders::_1));
    m_sub_trajectory = m_node->create_subscription<custom_msgs::msg::TrajectoryMsg>(
        "/local_path", 10, std::bind(&Ros2MsgSubsrcribe::callbackLocalPathMsg, this, std::placeholders::_1));

    m_sub_path = m_node->create_subscription<nav_msgs::msg::Path>(
        "/global_path", 10, std::bind(&Ros2MsgSubsrcribe::callbackGlobalPathMsg, this, std::placeholders::_1));

    // 开线程 spin
    m_running.store(true);
    m_spinThread = std::thread([this]() {
        while (m_running.load() && rclcpp::ok()) {
            rclcpp::spin_some(m_node);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
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
    if (m_spinThread.joinable()) {
        m_spinThread.join();
    }
    m_sub_location.reset();
    m_sub_scene.reset();
    m_sub_chassis_command.reset();
    m_sub_chassis_states.reset();
    m_sub_trajectory.reset();
    m_sub_path.reset();
    m_node.reset();
#else
    m_running.store(false);
#endif
}

QString Ros2MsgSubsrcribe::statusSummary() const
{
    return m_running.load() ? QStringLiteral("ROS2 订阅中：/location /scene /chassis_command /chassis_states /local_path /global_path")
                            : QStringLiteral("ROS2 未启动");
}

#if AUTOVIZ_ENABLE_ROS2
void Ros2MsgSubsrcribe::callbackLocationMsg(const custom_msgs::msg::Location::ConstSharedPtr msg)
{
    if (dataManager() == nullptr || msg == nullptr) {
        return;
    }
    vehicleLocation_.header.timestamp = currentTimestampMs();
    // 【将 location 消息转换为 VehicleLocation 结构体】
    vehicleLocation_.position.x = msg->odom_x;
    vehicleLocation_.position.y = msg->odom_y;
    vehicleLocation_.heading = msg->heading;
    vehicleLocation_.speed = msg->velocity;
    //std::cout << "vehicleLocation_: " << msg->odom_x << ", " << msg->odom_y << ", " << msg->heading << ", " << msg->velocity << std::endl;
    dataManager()->setVehicleLocation(vehicleLocation_);
}
void Ros2MsgSubsrcribe::callbackSceneMsg(const custom_msgs::msg::Scene::ConstSharedPtr msg)
{
    if (dataManager() == nullptr || msg == nullptr) {
        return;
    }

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
void Ros2MsgSubsrcribe::callbackChassisCommandMsg(const custom_msgs::msg::ChassisCommand::ConstSharedPtr msg)
{
    if (dataManager() == nullptr || msg == nullptr) {
        return;
    }

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
}
void Ros2MsgSubsrcribe::callbackChassisStatesMsg(const custom_msgs::msg::ChassisStates::ConstSharedPtr msg)
{
    if (dataManager() == nullptr || msg == nullptr) {
        return;
    }

    vehicleChassisInfo_ = autoviz::model::VehicleChassisInfo{};
    vehicleChassisInfo_.header.timestamp = currentTimestampMs();
    vehicleChassisInfo_.currentSpeed = msg->current_speed;
    vehicleChassisInfo_.currentAngularVelocity = msg->current_angular_velocity;
    vehicleChassisInfo_.currentGearPosition = msg->gear_status;

    dataManager()->setVehicleChassisInfo(vehicleChassisInfo_);
}
void Ros2MsgSubsrcribe::callbackLocalPathMsg(const custom_msgs::msg::TrajectoryMsg::ConstSharedPtr msg)
{
    if (dataManager() == nullptr || msg == nullptr) {
        return;
    }

    // 【将 local_path 消息转换为 Trajectory 结构体】
    local_path_.points.clear();

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
}
void Ros2MsgSubsrcribe::callbackGlobalPathMsg(const nav_msgs::msg::Path::ConstSharedPtr msg)
{
    if (dataManager() == nullptr || msg == nullptr) {
        return;
    }

    // 【将 global_path 消息转换为 Trajectory 结构体】
    global_path_.points.clear();
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
}
#endif
} // namespace autoviz::ros
