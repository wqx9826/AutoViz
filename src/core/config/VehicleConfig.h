#pragma once

#include <QString>

namespace autoviz::config {

struct VehicleConfig {
    double length = 4.9;
    double width = 1.95;
    double wheelBase = 2.85;
};

class VehicleConfigLoader {
public:
    static VehicleConfig loadFromJson(const QString& filePath, QString* errorMessage = nullptr);
};

}  // namespace autoviz::config
