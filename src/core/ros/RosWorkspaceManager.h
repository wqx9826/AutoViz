#pragma once

#include <QString>

#include "core/ros/RosTypes.h"

namespace autoviz::ros {

class RosWorkspaceManager
{
public:
    RosWorkspaceManager();

    QString projectRoot() const;
    QString runtimeRoot() const;
    QString workspaceRootFor(const RosEnvironmentInfo& environmentInfo) const;
    QString workspaceSrcRootFor(const RosEnvironmentInfo& environmentInfo) const;

    bool initializeWorkspace(const RosEnvironmentInfo& environmentInfo, QString* workspaceRoot, QString* errorMessage) const;
    bool packageExists(const RosEnvironmentInfo& environmentInfo, const QString& packageName) const;
    bool copyPackageToWorkspace(
        const RosEnvironmentInfo& environmentInfo,
        const RosPackageValidationResult& validationResult,
        bool overwriteExisting,
        QString* workspacePackagePath,
        QString* errorMessage) const;
    bool removePackageFromWorkspace(
        const RosEnvironmentInfo& environmentInfo,
        const QString& packageName,
        QString* errorMessage) const;
    bool removePackageDirectory(const QString& packagePath, QString* errorMessage) const;

private:
    static QString detectProjectRoot();
    static bool ensureDirectory(const QString& path, QString* errorMessage);
    static bool copyDirectoryRecursively(const QString& sourcePath, const QString& targetPath, QString* errorMessage);

    QString m_projectRoot;
};

}  // namespace autoviz::ros
