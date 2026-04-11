#pragma once

#include <memory>

#include "core/ros/RosMsgSubscribeBase.h"

namespace autoviz::ros {

std::shared_ptr<RosMsgSubscribeBase> createRosMsgSubsrcribe(SubscribeBackend backend, datacenter::DataManager* dataManager);

}  // namespace autoviz::ros
