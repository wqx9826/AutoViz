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

    datacenter::VisualizationInputSource inputSource = datacenter::VisualizationInputSource::Mock;
    switch (backend()) {
    case SubscribeBackend::Ros1:
        inputSource = datacenter::VisualizationInputSource::Ros1;
        break;
    case SubscribeBackend::Ros2:
        inputSource = datacenter::VisualizationInputSource::Ros2;
        break;
    case SubscribeBackend::None:
    default:
        inputSource = datacenter::VisualizationInputSource::Mock;
        break;
    }

    // 进入实时订阅模式后，先把可视化通道清成空值，避免残留旧的 mock 数据。
    m_dataManager->resetVisualizationData(inputSource);
}

datacenter::DataManager* RosMsgSubscribeBase::dataManager() const
{
    return m_dataManager;
}

}  // namespace autoviz::ros
