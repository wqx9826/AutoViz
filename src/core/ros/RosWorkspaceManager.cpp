#include "core/ros/RosWorkspaceManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace autoviz::ros {
namespace {
QString catkinTopLevelFileContent(const QString& distro)
{
    return QStringLiteral(
               "cmake_minimum_required(VERSION 3.0.2)\n"
               "project(AutoVizCatkinWorkspace)\n"
               "set(CATKIN_TOPLEVEL TRUE)\n"
               "include(/opt/ros/%1/share/catkin/cmake/toplevel.cmake)\n")
        .arg(distro);
}
}  // namespace

RosWorkspaceManager::RosWorkspaceManager()
    : m_projectRoot(detectProjectRoot())
{
}

QString RosWorkspaceManager::projectRoot() const
{
    return m_projectRoot;
}

QString RosWorkspaceManager::runtimeRoot() const
{
    return QDir(m_projectRoot).filePath(QStringLiteral("runtime"));
}

QString RosWorkspaceManager::workspaceRootFor(const RosEnvironmentInfo& environmentInfo) const
{
    if (environmentInfo.rosVersion == RosVersion::Ros1) {
        return QDir(runtimeRoot()).filePath(QStringLiteral("catkin_ws"));
    }

    if (environmentInfo.rosVersion == RosVersion::Ros2) {
        return QDir(runtimeRoot()).filePath(QStringLiteral("ros2_ws"));
    }

    return QString();
}

QString RosWorkspaceManager::workspaceSrcRootFor(const RosEnvironmentInfo& environmentInfo) const
{
    const QString workspaceRoot = workspaceRootFor(environmentInfo);
    return workspaceRoot.isEmpty() ? QString() : QDir(workspaceRoot).filePath(QStringLiteral("src"));
}

bool RosWorkspaceManager::initializeWorkspace(
    const RosEnvironmentInfo& environmentInfo,
    QString* workspaceRoot,
    QString* errorMessage) const
{
    const QString resolvedWorkspaceRoot = workspaceRootFor(environmentInfo);
    if (resolvedWorkspaceRoot.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("未检测到 ROS 环境，无法初始化内部工作区。");
        }
        return false;
    }

    const QString srcRoot = workspaceSrcRootFor(environmentInfo);
    if (!ensureDirectory(runtimeRoot(), errorMessage) || !ensureDirectory(resolvedWorkspaceRoot, errorMessage)
        || !ensureDirectory(srcRoot, errorMessage)) {
        return false;
    }

    if (environmentInfo.rosVersion == RosVersion::Ros1) {
        const QString srcCmakeLists = QDir(srcRoot).filePath(QStringLiteral("CMakeLists.txt"));
        if (!QFileInfo::exists(srcCmakeLists) && !environmentInfo.rosDistro.isEmpty()) {
            QFile file(srcCmakeLists);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                if (errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("无法创建 ROS1 工作区的 src/CMakeLists.txt。");
                }
                return false;
            }
            file.write(catkinTopLevelFileContent(environmentInfo.rosDistro).toUtf8());
            file.close();
        }
    }

    if (workspaceRoot != nullptr) {
        *workspaceRoot = resolvedWorkspaceRoot;
    }
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    return true;
}

bool RosWorkspaceManager::packageExists(const RosEnvironmentInfo& environmentInfo, const QString& packageName) const
{
    const QString packagePath = QDir(workspaceSrcRootFor(environmentInfo)).filePath(packageName);
    return QFileInfo::exists(packagePath);
}

bool RosWorkspaceManager::copyPackageToWorkspace(
    const RosEnvironmentInfo& environmentInfo,
    const RosPackageValidationResult& validationResult,
    bool overwriteExisting,
    QString* workspacePackagePath,
    QString* errorMessage) const
{
    const QString srcRoot = workspaceSrcRootFor(environmentInfo);
    if (srcRoot.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("未检测到 ROS 环境，无法复制消息包。");
        }
        return false;
    }

    if (!ensureDirectory(srcRoot, errorMessage)) {
        return false;
    }

    const QString targetPath = QDir(srcRoot).filePath(validationResult.packageName);
    const QFileInfo targetInfo(targetPath);
    if (targetInfo.exists()) {
        if (!overwriteExisting) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("工作区中已存在同名消息包。");
            }
            return false;
        }

        QDir targetDir(targetPath);
        if (!targetDir.removeRecursively()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("无法删除旧的工作区包目录：%1").arg(targetPath);
            }
            return false;
        }
    }

    if (!copyDirectoryRecursively(validationResult.packagePath, targetPath, errorMessage)) {
        return false;
    }

    if (workspacePackagePath != nullptr) {
        *workspacePackagePath = targetPath;
    }
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    return true;
}

bool RosWorkspaceManager::removePackageFromWorkspace(
    const RosEnvironmentInfo& environmentInfo,
    const QString& packageName,
    QString* errorMessage) const
{
    const QString targetPath = QDir(workspaceSrcRootFor(environmentInfo)).filePath(packageName);
    const QFileInfo targetInfo(targetPath);
    if (!targetInfo.exists()) {
        if (errorMessage != nullptr) {
            errorMessage->clear();
        }
        return true;
    }

    QDir packageDir(targetPath);
    if (!packageDir.removeRecursively()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("删除工作区中的消息包失败：%1").arg(targetPath);
        }
        return false;
    }

    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    return true;
}

QString RosWorkspaceManager::detectProjectRoot()
{
    QStringList candidates;
    candidates << QDir::currentPath() << QCoreApplication::applicationDirPath();

    for (const QString& candidate : candidates) {
        QDir dir(candidate);
        for (int depth = 0; depth < 6; ++depth) {
            if (QFileInfo::exists(dir.filePath(QStringLiteral("CMakeLists.txt")))
                && QFileInfo::exists(dir.filePath(QStringLiteral("src")))) {
                return dir.absolutePath();
            }
            if (!dir.cdUp()) {
                break;
            }
        }
    }

    return QDir::currentPath();
}

bool RosWorkspaceManager::ensureDirectory(const QString& path, QString* errorMessage)
{
    QDir dir(path);
    if (dir.exists()) {
        return true;
    }

    if (QDir().mkpath(path)) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("无法创建目录：%1").arg(path);
    }
    return false;
}

bool RosWorkspaceManager::copyDirectoryRecursively(const QString& sourcePath, const QString& targetPath, QString* errorMessage)
{
    QDir sourceDir(sourcePath);
    if (!sourceDir.exists()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("源消息包目录不存在：%1").arg(sourcePath);
        }
        return false;
    }

    if (!QDir().mkpath(targetPath)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("无法创建目标目录：%1").arg(targetPath);
        }
        return false;
    }

    const QFileInfoList entries = sourceDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for (const QFileInfo& entry : entries) {
        const QString sourceEntryPath = entry.absoluteFilePath();
        const QString targetEntryPath = QDir(targetPath).filePath(entry.fileName());

        if (entry.isDir()) {
            if (!copyDirectoryRecursively(sourceEntryPath, targetEntryPath, errorMessage)) {
                return false;
            }
            continue;
        }

        QFile::remove(targetEntryPath);
        if (!QFile::copy(sourceEntryPath, targetEntryPath)) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("复制文件失败：%1").arg(sourceEntryPath);
            }
            return false;
        }
    }

    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    return true;
}

}  // namespace autoviz::ros
