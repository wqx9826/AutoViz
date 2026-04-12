#include "core/ros/Ros2MsgSubsrcribe.h"

#include <chrono>
#include <functional>

#include "utils/Logger.h"



namespace autoviz::ros {


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
        Logger::instance().info(QStringLiteral("ROS2 订阅准备完成：/location /scene /local_path /global_path"));
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

    m_sub_trajectory = m_node->create_subscription<custom_msgs::msg::TrajectoryMsg>(
        "/local_path", 10, std::bind(&Ros2MsgSubsrcribe::callbackTrajectoryMsg, this, std::placeholders::_1));

    m_sub_path = m_node->create_subscription<nav_msgs::msg::Path>(
        "/global_path", 10, std::bind(&Ros2MsgSubsrcribe::callbackPathMsg, this, std::placeholders::_1));

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
    m_sub_trajectory.reset();
    m_sub_path.reset();
    m_node.reset();
#else
    m_running.store(false);
#endif
}

QString Ros2MsgSubsrcribe::statusSummary() const
{
    return m_running.load() ? QStringLiteral("ROS2 订阅中：/location /scene /local_path /global_path")
                            : QStringLiteral("ROS2 未启动");
}

#if AUTOVIZ_ENABLE_ROS2
void Ros2MsgSubsrcribe::callbackLocationMsg(const custom_msgs::msg::Location::ConstSharedPtr msg)
{
    // 处理 location 消息
    Q_UNUSED(msg);
}
void Ros2MsgSubsrcribe::callbackSceneMsg(const custom_msgs::msg::Scene::ConstSharedPtr msg)
{
    Q_UNUSED(msg);
}
void Ros2MsgSubsrcribe::callbackTrajectoryMsg(const custom_msgs::msg::TrajectoryMsg::ConstSharedPtr msg)
{
    Q_UNUSED(msg);
}
void Ros2MsgSubsrcribe::callbackPathMsg(const nav_msgs::msg::Path::ConstSharedPtr msg)
{
    Q_UNUSED(msg);
}
#endif
} // namespace autoviz::ros
