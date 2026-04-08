#include "core/ros/RosEnvironmentDetector.h"

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStandardPaths>

namespace autoviz::ros {
namespace {
bool hasExecutable(const QString& name)
{
    return !QStandardPaths::findExecutable(name).isEmpty();
}
}  // namespace

RosEnvironmentInfo RosEnvironmentDetector::detect()
{
    RosEnvironmentInfo info;
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    const QString rosVersion = env.value(QStringLiteral("ROS_VERSION")).trimmed();
    const QString rosDistro = env.value(QStringLiteral("ROS_DISTRO")).trimmed();

    info.rosDistro = rosDistro;
    info.hasRoscore = hasExecutable(QStringLiteral("roscore"));
    info.hasCatkinMake = hasExecutable(QStringLiteral("catkin_make"));
    info.hasColcon = hasExecutable(QStringLiteral("colcon"));

    if (rosVersion == QStringLiteral("1")) {
        info.rosAvailable = true;
        info.rosVersion = RosVersion::Ros1;
        return info;
    }

    if (rosVersion == QStringLiteral("2")) {
        info.rosAvailable = true;
        info.rosVersion = RosVersion::Ros2;
        return info;
    }

    if (info.hasCatkinMake || info.hasRoscore) {
        info.rosAvailable = true;
        info.rosVersion = RosVersion::Ros1;
        if (info.rosDistro.isEmpty()) {
            info.errorMessage = QStringLiteral("未检测到 ROS_VERSION，已根据 roscore/catkin_make 推断为 ROS1。");
        }
        return info;
    }

    if (info.hasColcon) {
        info.rosAvailable = true;
        info.rosVersion = RosVersion::Ros2;
        if (info.rosDistro.isEmpty()) {
            info.errorMessage = QStringLiteral("未检测到 ROS_VERSION，已根据 colcon 推断为 ROS2。");
        }
        return info;
    }

    info.errorMessage = QStringLiteral("未检测到 ROS_VERSION、roscore、catkin_make 或 colcon。");
    return info;
}

}  // namespace autoviz::ros
