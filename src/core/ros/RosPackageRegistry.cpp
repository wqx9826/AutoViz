#include "core/ros/RosPackageRegistry.h"

namespace autoviz::ros {

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
