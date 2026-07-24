#pragma once

#include <QString>

#include "core/model/VehicleState.h"

namespace autoviz::config {

class VehicleConfigLoader {
public:
    static model::VehicleConfig loadFromJson(const QString& filePath, QString* errorMessage = nullptr);
};

}  // namespace autoviz::config
