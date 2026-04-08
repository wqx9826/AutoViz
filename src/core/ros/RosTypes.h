#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace autoviz::ros {

enum class RosVersion {
    Unknown,
    Ros1,
    Ros2
};

enum class PackageCopyStatus {
    NotCopied,
    Copied,
    CopyFailed
};

enum class PackageBuildStatus {
    NotBuilt,
    Building,
    BuildSucceeded,
    BuildFailed
};

struct RosEnvironmentInfo {
    bool rosAvailable = false;
    RosVersion rosVersion = RosVersion::Unknown;
    QString rosDistro;
    bool hasRoscore = false;
    bool hasCatkinMake = false;
    bool hasColcon = false;
    QString errorMessage;
};

struct RosPackageValidationResult {
    bool isValid = false;
    QString packageName;
    QString packagePath;
    QString packageXmlPath;
    QString cmakeListsPath;
    QString msgDirectoryPath;
    QStringList msgFiles;
    int msgCount = 0;
    QString errorMessage;
};

struct ManagedRosPackage {
    bool enabled = true;
    QString packageName;
    QString sourcePath;
    QString workspacePath;
    int msgCount = 0;
    QString rosCompatibility;
    PackageCopyStatus copyStatus = PackageCopyStatus::NotCopied;
    PackageBuildStatus buildStatus = PackageBuildStatus::NotBuilt;
    QString lastError;
};

using ManagedRosPackageList = QVector<ManagedRosPackage>;

inline QString toDisplayString(RosVersion version)
{
    switch (version) {
    case RosVersion::Ros1:
        return QStringLiteral("ROS1");
    case RosVersion::Ros2:
        return QStringLiteral("ROS2");
    case RosVersion::Unknown:
    default:
        return QStringLiteral("未检测到");
    }
}

inline QString toDisplayString(PackageCopyStatus status)
{
    switch (status) {
    case PackageCopyStatus::Copied:
        return QStringLiteral("复制成功");
    case PackageCopyStatus::CopyFailed:
        return QStringLiteral("复制失败");
    case PackageCopyStatus::NotCopied:
    default:
        return QStringLiteral("未复制");
    }
}

inline QString toDisplayString(PackageBuildStatus status)
{
    switch (status) {
    case PackageBuildStatus::Building:
        return QStringLiteral("编译中");
    case PackageBuildStatus::BuildSucceeded:
        return QStringLiteral("编译成功");
    case PackageBuildStatus::BuildFailed:
        return QStringLiteral("编译失败");
    case PackageBuildStatus::NotBuilt:
    default:
        return QStringLiteral("未编译");
    }
}

inline QString toEnvironmentSummary(const RosEnvironmentInfo& info)
{
    if (!info.rosAvailable) {
        return QStringLiteral("未检测到 ROS 环境");
    }

    if (info.rosDistro.isEmpty()) {
        return QStringLiteral("当前 ROS 环境：%1").arg(toDisplayString(info.rosVersion));
    }

    return QStringLiteral("当前 ROS 环境：%1 / %2").arg(toDisplayString(info.rosVersion), info.rosDistro);
}

}  // namespace autoviz::ros
