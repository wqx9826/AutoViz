#include "core/ros/RosPackageBuilder.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace autoviz::ros {

RosPackageBuilder::RosPackageBuilder(QObject* parent)
    : QObject(parent)
    , m_processRunner(this)
{
    connect(&m_processRunner, &ProcessRunner::standardOutputReady, this, [this](const QString& text) {
        m_fullOutput.append(text);
        emit buildOutput(text);
    });
    connect(&m_processRunner, &ProcessRunner::standardErrorReady, this, [this](const QString& text) {
        m_fullOutput.append(text);
        emit buildOutput(text);
    });
    connect(
        &m_processRunner,
        &ProcessRunner::processFinished,
        this,
        [this](int exitCode, QProcess::ExitStatus exitStatus) {
            const bool success = exitStatus == QProcess::NormalExit && exitCode == 0;
            const QString summary =
                success ? QStringLiteral("编译成功。") : QStringLiteral("编译失败：%1").arg(extractBasicError());
            emit buildFinished(success, summary, m_fullOutput);
        });
}

bool RosPackageBuilder::isBuilding() const
{
    return m_processRunner.isRunning();
}

bool RosPackageBuilder::startBuild(
    const RosEnvironmentInfo& environmentInfo,
    const QString& workspaceRoot,
    const QStringList& packageNames,
    QString* errorMessage)
{
    if (isBuilding()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("当前已有编译任务正在执行。");
        }
        return false;
    }

    if (workspaceRoot.isEmpty() || !QFileInfo::exists(workspaceRoot)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("工作区不存在，无法开始编译。");
        }
        return false;
    }

    if (packageNames.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("没有可编译的已启用消息包。");
        }
        return false;
    }

    QString commandError;
    const QString command = createBuildCommand(environmentInfo, packageNames, &commandError);
    if (command.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = commandError;
        }
        return false;
    }

    m_fullOutput.clear();
    emit buildStarted(command);
    m_processRunner.start(QStringLiteral("/bin/bash"), {QStringLiteral("-lc"), command}, workspaceRoot);

    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    return true;
}

QString RosPackageBuilder::createBuildCommand(
    const RosEnvironmentInfo& environmentInfo,
    const QStringList& packageNames,
    QString* errorMessage) const
{
    const QString prefix = buildEnvironmentPrefix(environmentInfo);

    if (environmentInfo.rosVersion == RosVersion::Ros1) {
        if (!environmentInfo.hasCatkinMake) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("当前环境缺少 catkin_make，无法编译 ROS1 消息包。");
            }
            return QString();
        }

        return prefix + QStringLiteral(
                            "if [ ! -f src/CMakeLists.txt ]; then "
                            "if command -v catkin_init_workspace >/dev/null 2>&1; then "
                            "(cd src && catkin_init_workspace); "
                            "fi; "
                            "fi && catkin_make");
    }

    if (environmentInfo.rosVersion == RosVersion::Ros2) {
        if (!environmentInfo.hasColcon) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("当前环境缺少 colcon，无法编译 ROS2 消息包。");
            }
            return QString();
        }

        return prefix + QStringLiteral("colcon build --packages-select %1").arg(packageNames.join(QLatin1Char(' ')));
    }

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("未检测到 ROS 环境，无法执行编译。");
    }
    return QString();
}

QString RosPackageBuilder::buildEnvironmentPrefix(const RosEnvironmentInfo& environmentInfo) const
{
    if (environmentInfo.rosDistro.isEmpty()) {
        return QString();
    }

    const QString setupPath = QStringLiteral("/opt/ros/%1/setup.bash").arg(environmentInfo.rosDistro);
    if (!QFileInfo::exists(setupPath)) {
        return QString();
    }

    return QStringLiteral("source %1 && ").arg(setupPath);
}

QString RosPackageBuilder::extractBasicError() const
{
    const QStringList lines = m_fullOutput.split(QRegularExpression(QStringLiteral("[\r\n]")), Qt::SkipEmptyParts);
    for (int index = lines.size() - 1; index >= 0; --index) {
        const QString line = lines.at(index).trimmed();
        if (!line.isEmpty()) {
            return line;
        }
    }

    return QStringLiteral("请查看日志面板中的完整输出。");
}

}  // namespace autoviz::ros
