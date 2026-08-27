#include <QApplication>
#include <QEventLoop>
#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextStream>
#include <QTimer>

#include <algorithm>
#include <limits>

#include "core/datacenter/DataManager.h"
#include "core/playback/LocalRosbagPlaybackSource.h"
#include "ui/status/BottomStatusPanel.h"

namespace {

using autoviz::datacenter::VisualizationInputSource;
using autoviz::datacenter::VisualizationSnapshot;

autoviz::model::TopicStatus topicStatus(autoviz::model::VisualizationChannel channel,
                                        quint64 sequence,
                                        qint64 timestampMs)
{
    autoviz::model::TopicStatus status;
    status.channel = channel;
    status.lastUpdateMs = timestampMs;
    status.messageCount = sequence;
    status.timeoutMs = 5000;
    status.timedOut = false;
    return status;
}

VisualizationSnapshot controlSnapshot(int mode,
                                      int commandGear,
                                      int feedbackGear,
                                      quint64 sequence,
                                      bool commandEnabled = true,
                                      const QString& sessionId = {})
{
    const qint64 timestampMs = 1786936821000LL + static_cast<qint64>(sequence);
    VisualizationSnapshot snapshot;
    snapshot.runtimeStatus.inputSource = VisualizationInputSource::Ros2Bag;
    snapshot.runtimeStatus.snapshotSequence = sequence;
    snapshot.runtimeStatus.sessionId = sessionId;
    snapshot.runtimeStatus.sourceTimeMs = timestampMs;
    snapshot.runtimeStatus.hasCommonPlanningControlCapability = true;

    snapshot.controlCommandStatus.valid = true;
    snapshot.controlCommandStatus.header.receiveTimestamp = timestampMs;
    snapshot.controlCommandStatus.header.sequence = sequence;
    snapshot.controlCommandStatus.mode = mode;
    snapshot.controlCommandStatus.expectedGear = commandGear;
    snapshot.controlCommandStatus.isEnable = commandEnabled;
    snapshot.controlCommandStatus.speed = 1.0;
    snapshot.controlCommandStatus.angularVelocity = 0.1;
    snapshot.controlCommandStatus.heading = 0.3;

    snapshot.chassisRuntimeStatus.valid = true;
    snapshot.chassisRuntimeStatus.header.receiveTimestamp = timestampMs;
    snapshot.chassisRuntimeStatus.header.sequence = sequence;
    snapshot.chassisRuntimeStatus.currentSpeed = 2.0;
    snapshot.chassisRuntimeStatus.currentAngularVelocity = 0.2;
    snapshot.chassisRuntimeStatus.gearStatus = feedbackGear;
    snapshot.chassisRuntimeStatus.leftCrawlMotor.valid = true;
    snapshot.chassisRuntimeStatus.rightCrawlMotor.valid = true;
    snapshot.chassisRuntimeStatus.leftCrawlMotor.outputEnabled = true;
    snapshot.chassisRuntimeStatus.rightCrawlMotor.outputEnabled = true;

    snapshot.localizationStatus.valid = true;
    snapshot.localizationStatus.timestampMs = timestampMs;
    snapshot.localizationStatus.velocity = 0.5;
    snapshot.localizationStatus.omegaZ = 0.4;
    snapshot.localizationStatus.heading = 0.6;

    snapshot.topicStatuses = {
        topicStatus(autoviz::model::VisualizationChannel::ControlCommand, sequence, timestampMs),
        topicStatus(autoviz::model::VisualizationChannel::ChassisState, sequence, timestampMs),
    };
    return snapshot;
}

bool runInputOwnershipChecks(QTextStream& error)
{
    autoviz::datacenter::DataManager manager;
    manager.activateInputSource(VisualizationInputSource::Ros2Bag);
    if (!manager.resetVisualizationData(VisualizationInputSource::Ros2Bag)) {
        error << "failed to activate rosbag input\n";
        return false;
    }

    const auto bagSnapshot = controlSnapshot(6, 1, 1, 10);
    const auto delayedRemoteSnapshot = controlSnapshot(11, 4, 4, 99);
    if (!manager.replaceVisualizationSnapshot(bagSnapshot, VisualizationInputSource::Ros2Bag)
        || manager.replaceVisualizationSnapshot(delayedRemoteSnapshot, VisualizationInputSource::Remote)
        || manager.resetVisualizationData(VisualizationInputSource::Remote)) {
        error << "inactive remote source modified active rosbag data\n";
        return false;
    }
    auto current = manager.getSnapshot();
    if (current.runtimeStatus.inputSource != VisualizationInputSource::Ros2Bag
        || !current.controlCommandStatus.valid
        || current.controlCommandStatus.mode != 6) {
        error << "delayed remote callback overwrote rosbag mode=6\n";
        return false;
    }

    current.obstacleRejectionReason = QStringLiteral("旧会话拒绝原因");
    current.finalTargetSetRuntimeStatus.valid = true;
    current.rangeMotionRuntimeStatus.valid = true;
    current.inspectionGoalRuntimeStatus.valid = true;
    if (!manager.replaceVisualizationSnapshot(current, VisualizationInputSource::Ros2Bag)
        || !manager.resetVisualizationData(VisualizationInputSource::Ros2Bag)) {
        error << "failed to reset perception runtime state\n";
        return false;
    }
    current = manager.getSnapshot();
    if (!current.obstacleRejectionReason.isEmpty()
        || current.finalTargetSetRuntimeStatus.valid
        || current.rangeMotionRuntimeStatus.valid
        || current.inspectionGoalRuntimeStatus.valid) {
        error << "reset retained stale perception runtime state\n";
        return false;
    }

    manager.activateInputSource(VisualizationInputSource::Remote);
    if (!manager.resetVisualizationData(VisualizationInputSource::Remote)
        || !manager.replaceVisualizationSnapshot(delayedRemoteSnapshot, VisualizationInputSource::Remote)
        || manager.replaceVisualizationSnapshot(bagSnapshot, VisualizationInputSource::Ros2Bag)) {
        error << "remote activation did not reject delayed rosbag data\n";
        return false;
    }
    current = manager.getSnapshot();
    return current.runtimeStatus.inputSource == VisualizationInputSource::Remote
           && current.controlCommandStatus.valid
           && current.controlCommandStatus.mode == 11;
}

bool runControlWidgetChecks(QApplication& app, QTextStream& error)
{
    BottomStatusPanel panel;
    panel.resize(1280, 720);
    panel.show();

    auto initialSnapshot = controlSnapshot(11, 4, 4, 101);
    initialSnapshot.chassisRuntimeStatus.bms.valid = true;
    initialSnapshot.chassisRuntimeStatus.bms.alarmLevel = 3;
    initialSnapshot.chassisRuntimeStatus.bms.warningCodes = {0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0};
    initialSnapshot.chassisRuntimeStatus.bms.selfCheckStatus = 0;
    initialSnapshot.chassisRuntimeStatus.bms.soc = 58;
    initialSnapshot.chassisRuntimeStatus.highVoltageBmsSocStatus = 58;
    panel.updateSnapshot(initialSnapshot);

    auto* tabs = panel.findChild<QTabWidget*>(QStringLiteral("bottomStatusTabs"));
    auto* detailTabs = panel.findChild<QTabWidget*>(QStringLiteral("bottomStatusDetailTabs"));
    auto* modeLabel = panel.findChild<QLabel*>(QStringLiteral("overview.control.mode"));
    auto* stateLabel = panel.findChild<QLabel*>(QStringLiteral("overview.command.state"));
    auto* association = panel.findChild<QTableWidget*>(QStringLiteral("controlAssociationTable"));
    auto* timelineScrollArea = panel.findChild<QScrollArea*>(QStringLiteral("controlTimelineScrollArea"));
    if (tabs == nullptr || detailTabs == nullptr || modeLabel == nullptr
        || stateLabel == nullptr || association == nullptr || timelineScrollArea == nullptr) {
        error << "control status widgets are missing stable object names\n";
        return false;
    }
    const QStringList expectedDetailTabs = {QStringLiteral("ROS Topic"),
                                            QStringLiteral("TaskParams"),
                                            QStringLiteral("定位"),
                                            QStringLiteral("底盘"),
                                            QStringLiteral("控制"),
                                            QStringLiteral("路径"),
                                            QStringLiteral("感知信息"),
                                            QStringLiteral("Action 信息"),
                                            QStringLiteral("任务状态"),
                                            QStringLiteral("控制时序"),
                                            QStringLiteral("垂向")};
    if (detailTabs->count() != expectedDetailTabs.size()) {
        error << "detail tabs are incomplete\n";
        return false;
    }
    for (int index = 0; index < expectedDetailTabs.size(); ++index) {
        if (detailTabs->tabText(index) != expectedDetailTabs.at(index)) {
            error << "detail tab order is incorrect at " << index << '\n';
            return false;
        }
    }
    const auto hasDetailGroup = [&panel](const QString& title) {
        const auto groups = panel.findChildren<QGroupBox*>();
        return std::any_of(groups.cbegin(), groups.cend(), [&title](const auto* group) {
            return group->title() == title;
        });
    };
    for (const QString& group : {QStringLiteral("基础任务"), QStringLiteral("爬行遥控"),
                                 QStringLiteral("航行遥控"), QStringLiteral("推进器调试"),
                                 QStringLiteral("配电通路"), QStringLiteral("测距运动请求"),
                                 QStringLiteral("观察目标请求"), QStringLiteral("最终融合目标")}) {
        if (!hasDetailGroup(group)) {
            error << "missing detail group: " << group << '\n';
            return false;
        }
    }
    auto* taskParamsScrollArea = panel.findChild<QScrollArea*>(QStringLiteral("taskParamsScrollArea"));
    if (taskParamsScrollArea == nullptr) {
        error << "TaskParams scroll area is missing\n";
        return false;
    }
    const auto taskParamsGroup = [taskParamsScrollArea](const QString& title) {
        const auto groups = taskParamsScrollArea->findChildren<QGroupBox*>();
        const auto group = std::find_if(groups.cbegin(), groups.cend(), [&title](const auto* candidate) {
            return candidate->title() == title;
        });
        return group == groups.cend() ? nullptr : *group;
    };
    auto* basicTaskGroup = taskParamsGroup(QStringLiteral("基础任务"));
    auto* crawlRemoteGroup = taskParamsGroup(QStringLiteral("爬行遥控"));
    auto* sailingRemoteGroup = taskParamsGroup(QStringLiteral("航行遥控"));
    if (basicTaskGroup == nullptr || crawlRemoteGroup == nullptr || sailingRemoteGroup == nullptr
        || crawlRemoteGroup->height() >= basicTaskGroup->height()
        || sailingRemoteGroup->y() >= basicTaskGroup->geometry().bottom()) {
        error << "TaskParams compact remote-control layout is invalid\n";
        return false;
    }
    auto* crawlSpeedLabel = panel.findChild<QLabel*>(QStringLiteral("overview.control.crawl_speed"));
    auto* sailingSpeedLabel = panel.findChild<QLabel*>(QStringLiteral("overview.control.sailing_speed"));
    auto* crawlAngularLabel = panel.findChild<QLabel*>(QStringLiteral("overview.control.crawl_angular"));
    auto* omegaZLabel = panel.findChild<QLabel*>(QStringLiteral("overview.control.omega_z"));
    auto* commandSpeedLabel = panel.findChild<QLabel*>(QStringLiteral("overview.command.speed"));
    auto* commandAngularLabel = panel.findChild<QLabel*>(QStringLiteral("overview.command.angular"));
    auto* bmsLabel = panel.findChild<QLabel*>(QStringLiteral("overview.hardware.bms"));
    auto* obstacleStateLabel = panel.findChild<QLabel*>(QStringLiteral("overview.obstacle.state"));
    auto* obstacleRejectionLabel = panel.findChild<QLabel*>(QStringLiteral("overview.obstacle.rejection"));
    if (modeLabel->text() != QStringLiteral("爬行中心转向")
        || !stateLabel->text().startsWith(QStringLiteral("爬行中心转向"))
        || crawlSpeedLabel == nullptr || sailingSpeedLabel == nullptr
        || crawlAngularLabel == nullptr || omegaZLabel == nullptr
        || commandSpeedLabel == nullptr || commandAngularLabel == nullptr
        || bmsLabel == nullptr || obstacleStateLabel == nullptr || obstacleRejectionLabel == nullptr
        || crawlSpeedLabel->text() != QStringLiteral("2.00")
        || sailingSpeedLabel->text() != QStringLiteral("0.50")
        || commandSpeedLabel->text() != QStringLiteral("1.00 / 0.50")
        || bmsLabel->text() != QStringLiteral("自检 正常 / 告警等级 3 / 告警项 1 / SOC 58%")) {
        error << "initial mode=11 overview text is incorrect\n";
        return false;
    }
    if (!bmsLabel->wordWrap()) {
        error << "BMS overview does not wrap long status text\n";
        return false;
    }

    auto rejectedObstacleSnapshot = initialSnapshot;
    rejectedObstacleSnapshot.obstacleRejectionReason =
        QStringLiteral("目标集已拒绝：目标 ID 42 的平面中心 X/Y 不是有限数值。");
    panel.updateSnapshot(rejectedObstacleSnapshot);
    if (obstacleStateLabel->text() != QStringLiteral("已拒绝")
        || obstacleRejectionLabel->text()
               != QStringLiteral("目标集已拒绝：目标 ID 42 的平面中心 X/Y 不是有限数值。")
        || !obstacleRejectionLabel->wordWrap()) {
        error << "rejected obstacle frame reason is not visible in the UI\n";
        return false;
    }
    panel.updateSnapshot(initialSnapshot);
    if (obstacleRejectionLabel->text() != QStringLiteral("--")) {
        error << "obstacle rejection reason was not cleared by a later valid frame\n";
        return false;
    }

    auto missingBmsSnapshot = initialSnapshot;
    missingBmsSnapshot.chassisRuntimeStatus.bms.valid = false;
    panel.updateSnapshot(missingBmsSnapshot);
    if (bmsLabel->text() != QStringLiteral("自检 正常 / 告警等级 无此版本数据 / 告警项 无此版本数据 / SOC 58%")) {
        error << "missing BMS data was displayed as zero alarm values\n";
        return false;
    }
    panel.updateSnapshot(initialSnapshot);

    auto healthyBmsSnapshot = initialSnapshot;
    healthyBmsSnapshot.chassisRuntimeStatus.bms.alarmLevel = 0;
    healthyBmsSnapshot.chassisRuntimeStatus.bms.warningCodes.fill(0);
    panel.updateSnapshot(healthyBmsSnapshot);
    if (bmsLabel->text() != QStringLiteral("自检 正常 / 告警等级 0 / 告警项 0 / SOC 58%")
        || bmsLabel->property("statusLevel").toString() != QStringLiteral("status-normal")) {
        error << "healthy BMS overview is not styled as normal\n";
        return false;
    }
    panel.updateSnapshot(initialSnapshot);
    if (bmsLabel->property("statusLevel").toString() != QStringLiteral("status-warn")) {
        error << "BMS warning overview is not styled as warning\n";
        return false;
    }

    auto detailSnapshot = initialSnapshot;
    detailSnapshot.runtimeStatus.inputSource = VisualizationInputSource::Remote;
    detailSnapshot.taskRuntimeStatus.valid = true;
    detailSnapshot.taskRuntimeStatus.timestampMs = 1786936821100LL;
    detailSnapshot.taskRuntimeStatus.hasTaskType = true;
    detailSnapshot.taskRuntimeStatus.taskType = 2;
    detailSnapshot.taskRuntimeStatus.hasTaskId = true;
    detailSnapshot.taskRuntimeStatus.taskId = 9;
    // Simulate an old Server that has TaskState but lacks task_enable.
    detailSnapshot.taskRuntimeStatus.hasTaskEnable = false;
    detailSnapshot.rangeMotionRuntimeStatus.valid = true;
    detailSnapshot.rangeMotionRuntimeStatus.timestampMs = 1786936821101LL;
    detailSnapshot.rangeMotionRuntimeStatus.hasTaskId = true;
    detailSnapshot.rangeMotionRuntimeStatus.taskId = 8;
    detailSnapshot.rangeMotionRuntimeStatus.hasCommandSequence = true;
    detailSnapshot.rangeMotionRuntimeStatus.commandSequence = 17;
    detailSnapshot.rangeMotionRuntimeStatus.hasMotion = true;
    detailSnapshot.rangeMotionRuntimeStatus.motion = 2;
    detailSnapshot.rangeMotionRuntimeStatus.hasSpeedLimit = true;
    detailSnapshot.rangeMotionRuntimeStatus.speedLimitMps = 0.35;
    detailSnapshot.rangeMotionRuntimeStatus.hasReason = true;
    detailSnapshot.rangeMotionRuntimeStatus.reason = QStringLiteral("声呐限速");
    detailSnapshot.inspectionGoalRuntimeStatus.valid = true;
    detailSnapshot.inspectionGoalRuntimeStatus.timestampMs = 1786936821102LL;
    detailSnapshot.inspectionGoalRuntimeStatus.hasTaskId = true;
    detailSnapshot.inspectionGoalRuntimeStatus.taskId = 9;
    detailSnapshot.inspectionGoalRuntimeStatus.hasGoalId = true;
    detailSnapshot.inspectionGoalRuntimeStatus.goalId = 18;
    detailSnapshot.inspectionGoalRuntimeStatus.hasTargetId = true;
    detailSnapshot.inspectionGoalRuntimeStatus.targetId = 42;
    detailSnapshot.inspectionGoalRuntimeStatus.hasTargetPosition = true;
    detailSnapshot.inspectionGoalRuntimeStatus.targetPosition = {1.0, 2.0, -3.0};
    detailSnapshot.inspectionGoalRuntimeStatus.hasObservationPosition = true;
    detailSnapshot.inspectionGoalRuntimeStatus.observationPosition = {4.0, 5.0, -1.0};
    detailSnapshot.inspectionGoalRuntimeStatus.hasHeading = true;
    detailSnapshot.inspectionGoalRuntimeStatus.headingRad = 1.0;
    detailSnapshot.inspectionGoalRuntimeStatus.hasHoldOnArrival = true;
    detailSnapshot.inspectionGoalRuntimeStatus.holdOnArrival = true;
    detailSnapshot.inspectionGoalRuntimeStatus.hasMode = true;
    detailSnapshot.inspectionGoalRuntimeStatus.mode = 1;
    detailSnapshot.inspectionGoalRuntimeStatus.hasSpeedLimit = true;
    detailSnapshot.inspectionGoalRuntimeStatus.speedLimitMps = 0.4;
    detailSnapshot.finalTargetSetRuntimeStatus.valid = true;
    detailSnapshot.finalTargetSetRuntimeStatus.timestampMs = 1786936821103LL;
    detailSnapshot.finalTargetSetRuntimeStatus.hasTaskId = true;
    detailSnapshot.finalTargetSetRuntimeStatus.taskId = 9;
    detailSnapshot.finalTargetSetRuntimeStatus.hasMineNumber = true;
    detailSnapshot.finalTargetSetRuntimeStatus.mineNumber = 1;
    detailSnapshot.finalTargetSetRuntimeStatus.targetCount = 1;
    detailSnapshot.finalTargetSetRuntimeStatus.rejectionReason = QStringLiteral("目标集已拒绝：测试原因");
    detailSnapshot.obstacleRejectionReason = detailSnapshot.finalTargetSetRuntimeStatus.rejectionReason;
    autoviz::model::Obstacle target;
    target.id = 42;
    target.classLabel = QStringLiteral("障碍物");
    target.position.position = {1.0, 2.0};
    target.isFinalTarget = true;
    target.conservativeRadius = 3.0;
    target.finalTargetBoundaryState = autoviz::model::FinalTargetBoundaryState::InvalidFallbackCircle;
    target.finalTargetBoundaryNote = QStringLiteral("渔网边界无效，已回退保守圆");
    detailSnapshot.obstacles = {target};
    detailSnapshot.topicStatuses += {
        topicStatus(autoviz::model::VisualizationChannel::TaskState, 2, 1786936821100LL),
        topicStatus(autoviz::model::VisualizationChannel::RangeMotionDirective, 3, 1786936821101LL),
        topicStatus(autoviz::model::VisualizationChannel::InspectionGoal, 4, 1786936821102LL),
        topicStatus(autoviz::model::VisualizationChannel::Obstacles, 5, 1786936821103LL)};
    panel.updateSnapshot(detailSnapshot);
    const auto detailLabel = [&panel](const QString& key) {
        return panel.findChild<QLabel*>(QStringLiteral("detail.%1").arg(key));
    };
    auto* missingTaskEnable = detailLabel(QStringLiteral("taskparams.enable"));
    auto* rangeTaskId = detailLabel(QStringLiteral("perception.range.task_id"));
    auto* inspectionIds = detailLabel(QStringLiteral("perception.inspection.ids"));
    auto* finalRejection = detailLabel(QStringLiteral("perception.final.rejection"));
    auto* targetTable = panel.findChild<QTableWidget*>(QStringLiteral("finalTargetTable"));
    if (missingTaskEnable == nullptr || rangeTaskId == nullptr || inspectionIds == nullptr
        || finalRejection == nullptr || targetTable == nullptr
        || missingTaskEnable->text() != QStringLiteral("该 Server 无此信息")
        || rangeTaskId->text() != QStringLiteral("8")
        || inspectionIds->text() != QStringLiteral("9 / 18 / 42")
        || finalRejection->text() != QStringLiteral("目标集已拒绝：测试原因")
        || targetTable->rowCount() != 1
        || targetTable->item(0, 4) == nullptr || targetTable->item(0, 4)->text() != QStringLiteral("3.000")
        || targetTable->item(0, 6) == nullptr || targetTable->item(0, 6)->text() != QStringLiteral("边界无效，回退圆")) {
        error << "TaskParams/perception detail fields did not render correctly: task='"
              << (missingTaskEnable == nullptr ? QStringLiteral("<null>") : missingTaskEnable->text())
              << "', range='" << (rangeTaskId == nullptr ? QStringLiteral("<null>") : rangeTaskId->text())
              << "', inspection='" << (inspectionIds == nullptr ? QStringLiteral("<null>") : inspectionIds->text())
              << "', rejection='" << (finalRejection == nullptr ? QStringLiteral("<null>") : finalRejection->text())
              << "', rows=" << (targetTable == nullptr ? -1 : targetTable->rowCount())
              << ", radius='" << (targetTable == nullptr || targetTable->item(0, 4) == nullptr
                                         ? QStringLiteral("<null>") : targetTable->item(0, 4)->text())
              << "'\n";
        return false;
    }

    panel.updateSnapshot(initialSnapshot);

    tabs->setCurrentIndex(1);
    detailTabs->setCurrentIndex(detailTabs->count() - 1);
    const auto coalescedSnapshot = controlSnapshot(6, 1, 1, 103);
    panel.updateSnapshot(coalescedSnapshot);
    app.processEvents();

    if (modeLabel->text() != QStringLiteral("自主爬行")
        || stateLabel->text() != QStringLiteral("自主爬行 / 已使能")) {
        error << "coalesced 11->0->6 snapshot did not use the current command snapshot\n";
        return false;
    }

    if (association->item(1, 1) == nullptr
        || !association->item(1, 1)->text().startsWith(QStringLiteral("自主爬行"))
        || association->item(1, 3) == nullptr
        || association->item(1, 3)->text() != QStringLiteral("103")) {
        error << "control association row did not reach mode=6 sequence=103\n";
        return false;
    }

    panel.resize(1280, 360);
    app.processEvents();
    if (timelineScrollArea->verticalScrollBar()->maximum() <= 0) {
        error << "control timeline page cannot scroll at constrained height\n";
        return false;
    }
    timelineScrollArea->verticalScrollBar()->setValue(
        timelineScrollArea->verticalScrollBar()->maximum());
    app.processEvents();
    if (timelineScrollArea->verticalScrollBar()->value()
        != timelineScrollArea->verticalScrollBar()->maximum()) {
        error << "control timeline page cannot reach its lower content\n";
        return false;
    }

    tabs->setCurrentIndex(0);
    app.processEvents();
    if (modeLabel->text() != QStringLiteral("自主爬行")
        || stateLabel->text() != QStringLiteral("自主爬行 / 已使能")) {
        error << "overview retained mode=11 after hidden 11->0->6 updates\n";
        return false;
    }

    panel.updateSnapshot(controlSnapshot(6, 1, 1, 1, true,
                                         QStringLiteral("local:new-session")));
    if (modeLabel->text() != QStringLiteral("自主爬行")
        || stateLabel->text() != QStringLiteral("自主爬行 / 已使能")) {
        error << "new session replayed historical transitions in overview\n";
        return false;
    }
    return true;
}

bool runRealBagWidgetChecks(QApplication& app, const QString& bagPath, QTextStream& error)
{
    autoviz::datacenter::DataManager manager;
    manager.activateInputSource(VisualizationInputSource::Ros2Bag);
    autoviz::playback::LocalRosbagPlaybackSource source(&manager);
    BottomStatusPanel panel;
    panel.resize(1280, 720);
    panel.show();

    const auto detailLabel = [&panel](const QString& key) {
        return panel.findChild<QLabel*>(QStringLiteral("detail.%1").arg(key));
    };
    auto* taskIdLabel = detailLabel(QStringLiteral("taskparams.id"));
    auto* taskEnableLabel = detailLabel(QStringLiteral("taskparams.enable"));
    auto* actionEnableLabel = detailLabel(QStringLiteral("taskparams.action_enable"));
    auto* powerChannelsLabel = detailLabel(QStringLiteral("taskparams.power_channels"));
    if (taskIdLabel == nullptr || taskEnableLabel == nullptr || actionEnableLabel == nullptr
        || powerChannelsLabel == nullptr) {
        error << "real-bag TaskParams detail labels are missing\n";
        return false;
    }

    QEventLoop loop;
    QTimer timeout;
    QTimer refresh;
    timeout.setSingleShot(true);
    timeout.setInterval(120000);
    refresh.setInterval(20);
    bool failed = false;
    bool playing = false;
    bool sawTaskParams = false;

    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] {
        error << "real-bag UI test timed out\n";
        failed = true;
        loop.quit();
    });
    QObject::connect(&source,
                     &autoviz::playback::LocalRosbagPlaybackSource::errorOccurred,
                     &loop,
                     [&](const QString& message) {
                         error << "real-bag playback failed: " << message << '\n';
                         failed = true;
                         loop.quit();
                     });
    QObject::connect(&source,
                     &autoviz::playback::LocalRosbagPlaybackSource::bagLoaded,
                     &loop,
                     [&](const autoviz::playback::RosbagInfo&) {
                         source.setPlaybackRate(8.0);
                         source.play();
                         playing = true;
                     });
    QObject::connect(&source,
                     &autoviz::playback::LocalRosbagPlaybackSource::positionChanged,
                     &loop,
                     [&](qint64, qint64) {});
    QObject::connect(&refresh, &QTimer::timeout, &loop, [&] {
        panel.updateSnapshot(manager.getSnapshot());
        app.processEvents();
        const auto task = manager.getSnapshot().taskRuntimeStatus;
        if (playing && task.valid && task.hasTaskId && task.hasTaskEnable
            && task.hasActionEnabled && task.hasRemoteControl && !task.powerSupplyCommands.isEmpty()
            && taskIdLabel->text() != QStringLiteral("--")
            && taskEnableLabel->text() != QStringLiteral("--")
            && actionEnableLabel->text() != QStringLiteral("--")
            && powerChannelsLabel->text() != QStringLiteral("--")) {
            sawTaskParams = true;
            source.pause();
            loop.quit();
        }
    });

    timeout.start();
    refresh.start();
    source.loadAndValidate(bagPath);
    loop.exec();
    refresh.stop();
    timeout.stop();

    if (failed || !sawTaskParams) {
        error << "real bag did not visibly render complete TaskParams; task=" << sawTaskParams
              << ", id='" << taskIdLabel->text() << "', enable='"
              << taskEnableLabel->text() << "', action='" << actionEnableLabel->text()
              << "', power='" << powerChannelsLabel->text() << "'\n";
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv)
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    }
    QApplication app(argc, argv);
    QTextStream error(stderr);
    if (!runInputOwnershipChecks(error) || !runControlWidgetChecks(app, error)) {
        return 1;
    }
    if (app.arguments().size() > 1
        && !runRealBagWidgetChecks(app, app.arguments().at(1), error)) {
        return 1;
    }
    QTextStream(stdout) << "control source split, current-snapshot status, bag UI, and input ownership tests: OK\n";
    return 0;
}
