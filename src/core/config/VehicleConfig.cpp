#include "core/config/VehicleConfig.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace autoviz::config {

VehicleConfig VehicleConfigLoader::loadFromJson(const QString& filePath, QString* errorMessage)
{
    VehicleConfig config;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("无法读取车辆参数配置：%1").arg(filePath);
        }
        return config;
    }

    const auto document = QJsonDocument::fromJson(file.readAll());
    const auto object = document.object();
    config.length = object.value(QStringLiteral("length")).toDouble(config.length);
    config.width = object.value(QStringLiteral("width")).toDouble(config.width);
    config.wheelBase = object.value(QStringLiteral("wheel_base")).toDouble(config.wheelBase);
    return config;
}

}  // namespace autoviz::config
