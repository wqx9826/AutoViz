#include "core/ros/RosPackageRegistry.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace autoviz::ros {

namespace {

QString rosVersionToStorageString(RosVersion version)
{
    switch (version) {
    case RosVersion::Ros1:
        return QStringLiteral("ros1");
    case RosVersion::Ros2:
        return QStringLiteral("ros2");
    case RosVersion::Unknown:
    default:
        return QStringLiteral("unknown");
    }
}

RosVersion rosVersionFromStorageString(const QString& value)
{
    if (value == QStringLiteral("ros1")) {
        return RosVersion::Ros1;
    }
    if (value == QStringLiteral("ros2")) {
        return RosVersion::Ros2;
    }
    return RosVersion::Unknown;
}

QString buildStatusToStorageString(PackageBuildStatus status)
{
    switch (status) {
    case PackageBuildStatus::Building:
        return QStringLiteral("building");
    case PackageBuildStatus::BuildSucceeded:
        return QStringLiteral("success");
    case PackageBuildStatus::BuildFailed:
        return QStringLiteral("failed");
    case PackageBuildStatus::NotBuilt:
    default:
        return QStringLiteral("not_built");
    }
}

PackageBuildStatus buildStatusFromStorageString(const QString& value)
{
    if (value == QStringLiteral("building")) {
        return PackageBuildStatus::Building;
    }
    if (value == QStringLiteral("success")) {
        return PackageBuildStatus::BuildSucceeded;
    }
    if (value == QStringLiteral("failed")) {
        return PackageBuildStatus::BuildFailed;
    }
    return PackageBuildStatus::NotBuilt;
}

QJsonObject packageToJson(const ManagedRosPackage& package)
{
    QJsonObject object;
    object.insert(QStringLiteral("enabled"), package.enabled);
    object.insert(QStringLiteral("package_name"), package.packageName);
    object.insert(QStringLiteral("source_path"), package.sourcePath);
    object.insert(QStringLiteral("workspace_path"), package.workspacePath);
    object.insert(QStringLiteral("msg_count"), package.msgCount);
    object.insert(QStringLiteral("build_status"), buildStatusToStorageString(package.buildStatus));
    object.insert(QStringLiteral("ros_type"), rosVersionToStorageString(package.rosVersion));
    object.insert(QStringLiteral("ros_compatibility"), package.rosCompatibility);
    object.insert(QStringLiteral("last_error"), package.lastError);
    return object;
}

ManagedRosPackage packageFromJson(const QJsonObject& object)
{
    ManagedRosPackage package;
    package.enabled = object.value(QStringLiteral("enabled")).toBool(true);
    package.packageName = object.value(QStringLiteral("package_name")).toString();
    package.sourcePath = object.value(QStringLiteral("source_path")).toString();
    package.workspacePath = object.value(QStringLiteral("workspace_path")).toString();
    package.msgCount = object.value(QStringLiteral("msg_count")).toInt();
    package.rosVersion = rosVersionFromStorageString(object.value(QStringLiteral("ros_type")).toString());
    package.rosCompatibility = object.value(QStringLiteral("ros_compatibility")).toString();
    package.copyStatus = PackageCopyStatus::Copied;
    package.buildStatus = buildStatusFromStorageString(object.value(QStringLiteral("build_status")).toString());
    package.lastError = object.value(QStringLiteral("last_error")).toString();
    return package;
}

}  // namespace

void RosPackageRegistry::setEnvironmentInfo(const RosEnvironmentInfo& environmentInfo)
{
    m_environmentInfo = environmentInfo;
}

const RosEnvironmentInfo& RosPackageRegistry::environmentInfo() const
{
    return m_environmentInfo;
}

void RosPackageRegistry::setWorkspaceRoot(const QString& workspaceRoot)
{
    m_workspaceRoot = workspaceRoot;
}

QString RosPackageRegistry::workspaceRoot() const
{
    return m_workspaceRoot;
}

const ManagedRosPackageList& RosPackageRegistry::packages() const
{
    return m_packages;
}

bool RosPackageRegistry::hasPackages() const
{
    return !m_packages.isEmpty();
}

int RosPackageRegistry::packageCount() const
{
    return m_packages.size();
}

int RosPackageRegistry::compiledPackageCount() const
{
    int count = 0;
    for (const auto& package : m_packages) {
        if (package.buildStatus == PackageBuildStatus::BuildSucceeded) {
            ++count;
        }
    }
    return count;
}

bool RosPackageRegistry::containsPackage(const QString& packageName) const
{
    for (const auto& package : m_packages) {
        if (package.packageName == packageName) {
            return true;
        }
    }
    return false;
}

void RosPackageRegistry::upsertPackage(const ManagedRosPackage& package)
{
    for (auto& existingPackage : m_packages) {
        if (existingPackage.packageName == package.packageName) {
            existingPackage = package;
            m_lastError.clear();
            return;
        }
    }

    m_packages.push_back(package);
    m_lastError.clear();
}

bool RosPackageRegistry::removePackage(const QString& packageName)
{
    for (int index = 0; index < m_packages.size(); ++index) {
        if (m_packages.at(index).packageName == packageName) {
            m_packages.removeAt(index);
            m_lastError.clear();
            return true;
        }
    }

    m_lastError = QStringLiteral("未找到需要删除的消息包：%1").arg(packageName);
    return false;
}

void RosPackageRegistry::setPackageEnabled(const QString& packageName, bool enabled)
{
    if (auto* package = findPackage(packageName); package != nullptr) {
        package->enabled = enabled;
    }
}

void RosPackageRegistry::setPackageBuildStatus(const QString& packageName, PackageBuildStatus status, const QString& errorMessage)
{
    if (auto* package = findPackage(packageName); package != nullptr) {
        package->buildStatus = status;
        package->lastError = errorMessage;
    }
}

void RosPackageRegistry::setAllEnabledPackagesBuildStatus(PackageBuildStatus status, const QString& errorMessage)
{
    for (auto& package : m_packages) {
        if (package.enabled) {
            package.buildStatus = status;
            package.lastError = errorMessage;
        }
    }
}

void RosPackageRegistry::setPackages(const ManagedRosPackageList& packages)
{
    m_packages = packages;
    m_lastError.clear();
}

QString RosPackageRegistry::packageStateFilePath(const QString& projectRoot) const
{
    return QDir(projectRoot).filePath(QStringLiteral("configs/package_state.json"));
}

bool RosPackageRegistry::saveToFile(const QString& filePath, QString* errorMessage) const
{
    const QFileInfo fileInfo(filePath);
    if (!QDir().mkpath(fileInfo.dir().absolutePath())) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("无法创建消息包状态目录：%1").arg(fileInfo.dir().absolutePath());
        }
        return false;
    }

    QJsonArray packagesArray;
    for (const auto& package : m_packages) {
        packagesArray.push_back(packageToJson(package));
    }

    QJsonObject rootObject;
    rootObject.insert(QStringLiteral("packages"), packagesArray);
    rootObject.insert(QStringLiteral("workspace_root"), m_workspaceRoot);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("无法写入消息包状态文件：%1").arg(filePath);
        }
        return false;
    }

    file.write(QJsonDocument(rootObject).toJson(QJsonDocument::Indented));
    file.close();

    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    return true;
}

bool RosPackageRegistry::loadFromFile(const QString& filePath, QString* errorMessage)
{
    QFile file(filePath);
    if (!file.exists()) {
        m_packages.clear();
        if (errorMessage != nullptr) {
            errorMessage->clear();
        }
        return true;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("无法读取消息包状态文件：%1").arg(filePath);
        }
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!document.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("消息包状态文件格式无效：%1").arg(filePath);
        }
        return false;
    }

    const QJsonObject rootObject = document.object();
    ManagedRosPackageList packages;
    for (const QJsonValue& value : rootObject.value(QStringLiteral("packages")).toArray()) {
        if (!value.isObject()) {
            continue;
        }

        const ManagedRosPackage package = packageFromJson(value.toObject());
        if (!package.packageName.isEmpty()) {
            packages.push_back(package);
        }
    }

    m_packages = packages;
    const QString storedWorkspaceRoot = rootObject.value(QStringLiteral("workspace_root")).toString();
    if (!storedWorkspaceRoot.isEmpty() && m_workspaceRoot.isEmpty()) {
        m_workspaceRoot = storedWorkspaceRoot;
    }
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    return true;
}

QString RosPackageRegistry::lastError() const
{
    return m_lastError;
}

ManagedRosPackage* RosPackageRegistry::findPackage(const QString& packageName)
{
    for (auto& package : m_packages) {
        if (package.packageName == packageName) {
            return &package;
        }
    }

    return nullptr;
}

}  // namespace autoviz::ros
