#include "core/ros/Ros2MsgSubsrcribe.h"

namespace autoviz::ros {

Ros2MsgSubsrcribe::Ros2MsgSubsrcribe(datacenter::DataManager* dataManager)
    : RosMsgSubscribeBase(dataManager)
{
}

SubscribeBackend Ros2MsgSubsrcribe::backend() const
{
    return SubscribeBackend::Ros2;
}

bool Ros2MsgSubsrcribe::initialize(QString* errorMessage)
{
    Q_UNUSED(errorMessage);
    // 建议在真正创建 subscription 之前先执行一次空帧发布。
    // 如果当前没有 reference line / control 等 topic，界面会得到空结构，而不是残留 mock 数据。
    resetVisualizationData();

    // TODO: 在这里直接写 ROS2 topic 名称并初始化 subscription。
    // TODO: 收到 ROS2 消息后，直接在 callback 里转换成内部模型。
    // TODO: 转换完成后，直接调用 dataManager()->setVehicleState(...) / setGlobalPath(...) / setObstacles(...)。
    // TODO: 如果某类数据当前没有 topic，请主动 set 空结构，例如 dataManager()->setReferenceLine(model::ReferenceLine{})。
    return true;
}

bool Ros2MsgSubsrcribe::start(QString* errorMessage)
{
    Q_UNUSED(errorMessage);
    m_running = true;
    // TODO: 启动 ROS2 executor / spin。
    return true;
}

void Ros2MsgSubsrcribe::stop()
{
    m_running = false;
    // TODO: 停止 ROS2 订阅。
}

QString Ros2MsgSubsrcribe::statusSummary() const
{
    return m_running ? QStringLiteral("ROS2 订阅中") : QStringLiteral("ROS2 未启动");
}

}  // namespace autoviz::ros
