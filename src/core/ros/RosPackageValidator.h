#pragma once

#include <QString>

#include "core/ros/RosTypes.h"

namespace autoviz::ros {

class RosPackageValidator
{
public:
    static RosPackageValidationResult validate(const QString& directoryPath);
};

}  // namespace autoviz::ros
