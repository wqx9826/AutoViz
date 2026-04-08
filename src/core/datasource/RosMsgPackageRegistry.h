#pragma once

#include <QString>

#include "core/datasource/RosMsgPackage.h"

namespace autoviz::datasource {

// RosMsgPackageRegistry owns the currently loaded ROS message package.
// Future versions can extend it to manage multiple packages, package
// dependencies and compilation artifacts.
class RosMsgPackageRegistry
{
public:
    bool loadPackage(const QString& directoryPath);
    void unloadCurrentPackage();

    bool hasLoadedPackage() const;
    const RosMsgPackage* currentPackage() const;
    QString lastError() const;

private:
    RosMsgPackage buildPackageFromDirectory(const QString& directoryPath) const;
    QString readPackageNameFromPackageXml(const QString& packageXmlPath) const;

    RosMsgPackage m_currentPackage;
    QString m_lastError;
};

}  // namespace autoviz::datasource
