#pragma once

#include <QString>

#include "core/ros/RosTypes.h"

namespace autoviz::ros {

class RosPackageRegistry
{
public:
    void setEnvironmentInfo(const RosEnvironmentInfo& environmentInfo);
    const RosEnvironmentInfo& environmentInfo() const;

    void setWorkspaceRoot(const QString& workspaceRoot);
    QString workspaceRoot() const;

    const ManagedRosPackageList& packages() const;
    bool hasPackages() const;
    int packageCount() const;
    int compiledPackageCount() const;

    bool containsPackage(const QString& packageName) const;
    void upsertPackage(const ManagedRosPackage& package);
    bool removePackage(const QString& packageName);
    void setPackageEnabled(const QString& packageName, bool enabled);
    void setPackageBuildStatus(const QString& packageName, PackageBuildStatus status, const QString& errorMessage = QString());
    void setAllEnabledPackagesBuildStatus(PackageBuildStatus status, const QString& errorMessage = QString());
    void setPackages(const ManagedRosPackageList& packages);
    QString packageStateFilePath(const QString& projectRoot) const;
    bool saveToFile(const QString& filePath, QString* errorMessage = nullptr) const;
    bool loadFromFile(const QString& filePath, QString* errorMessage = nullptr);

    QString lastError() const;

private:
    ManagedRosPackage* findPackage(const QString& packageName);

    RosEnvironmentInfo m_environmentInfo;
    QString m_workspaceRoot;
    ManagedRosPackageList m_packages;
    QString m_lastError;
};

}  // namespace autoviz::ros
