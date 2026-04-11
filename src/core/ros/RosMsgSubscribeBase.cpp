#include "core/ros/RosMsgSubscribeBase.h"

namespace autoviz::ros {

RosMsgSubscribeBase::RosMsgSubscribeBase(datacenter::DataManager* dataManager)
    : m_dataManager(dataManager)
{
}

RosMsgSubscribeBase::~RosMsgSubscribeBase() = default;


void RosMsgSubscribeBase::resetVisualizationData()
{
    if (m_dataManager == nullptr) {
        return;
    }

    // 进入 ROS 订阅模式后，先把所有可视化通道清成“空值”。
    // 这样即便某些 topic 当前没有接入，也不会继续显示旧的 mock 结果。
    m_dataManager->setVehicleState(model::VehicleState{});
    m_dataManager->setGlobalPath(model::Trajectory{});
    m_dataManager->setLocalPath(model::Trajectory{});
    m_dataManager->setReferenceLine(model::ReferenceLine{});
    m_dataManager->setObstacles(model::ObstacleList{});
    m_dataManager->setControlCmd(model::ControlCmd{});
}

datacenter::DataManager* RosMsgSubscribeBase::dataManager() const
{
    return m_dataManager;
}

}  // namespace autoviz::ros
