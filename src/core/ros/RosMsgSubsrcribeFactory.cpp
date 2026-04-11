#include "core/ros/RosMsgSubsrcribeFactory.h"

#include "core/ros/Ros1MsgSubsrcribe.h"
#include "core/ros/Ros2MsgSubsrcribe.h"

namespace autoviz::ros {

std::shared_ptr<RosMsgSubscribeBase> createRosMsgSubsrcribe(SubscribeBackend backend, datacenter::DataManager* dataManager)
{
    switch (backend) {
    case SubscribeBackend::Ros1:
        return std::make_shared<Ros1MsgSubsrcribe>(dataManager);
    case SubscribeBackend::Ros2:
        return std::make_shared<Ros2MsgSubsrcribe>(dataManager);
    case SubscribeBackend::None:
    default:
        return nullptr;
    }
}

}  // namespace autoviz::ros
