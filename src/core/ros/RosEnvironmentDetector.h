#pragma once

#include "core/ros/RosTypes.h"

namespace autoviz::ros {

class RosEnvironmentDetector
{
public:
    static RosEnvironmentInfo detect();
};

}  // namespace autoviz::ros
