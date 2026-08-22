#include <QApplication>
#include <QEventLoop>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextStream>
#include <QTimer>

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
                                      const autoviz::model::ControlStateEventList& events = {},
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
    snapshot.controlStateEvents = events;
    return snapshot;
}

autoviz::model::ControlStateEvent modeEvent(int previousMode,
                                            int currentMode,
                                            quint64 sequence)
{
    autoviz::model::ControlStateEvent event;
    event.source = autoviz::model::ControlEventSource::ControlCommand;
    event.header.receiveTimestamp = 1786936821000LL + static_cast<qint64>(sequence);
    event.header.sequence = sequence;
    event.hasPreviousMode = true;
    event.previousMode = previousMode;
    event.hasCurrentMode = true;
    event.currentMode = currentMode;
    event.hasPreviousGear = true;
    event.previousGear = previousMode == 11 ? 4 : (previousMode == 6 ? 1 : 0);
    event.hasCurrentGear = true;
    event.currentGear = currentMode == 11 ? 4 : (currentMode == 6 ? 1 : 0);
    event.hasPreviousEnabled = true;
    event.previousEnabled = previousMode != 0;
    event.hasCurrentEnabled = true;
    event.currentEnabled = currentMode != 0;
    return event;
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

    autoviz::model::ControlStateEventList events;
    auto initialSnapshot = controlSnapshot(11, 4, 4, 101, events);
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
    auto* crawlSpeedLabel = panel.findChild<QLabel*>(QStringLiteral("overview.control.crawl_speed"));
    auto* sailingSpeedLabel = panel.findChild<QLabel*>(QStringLiteral("overview.control.sailing_speed"));
    auto* crawlAngularLabel = panel.findChild<QLabel*>(QStringLiteral("overview.control.crawl_angular"));
    auto* omegaZLabel = panel.findChild<QLabel*>(QStringLiteral("overview.control.omega_z"));
    auto* commandSpeedLabel = panel.findChild<QLabel*>(QStringLiteral("overview.command.speed"));
    auto* commandAngularLabel = panel.findChild<QLabel*>(QStringLiteral("overview.command.angular"));
    auto* bmsLabel = panel.findChild<QLabel*>(QStringLiteral("overview.hardware.bms"));
    if (modeLabel->text() != QStringLiteral("爬行中心转向")
        || !stateLabel->text().startsWith(QStringLiteral("爬行中心转向"))
        || crawlSpeedLabel == nullptr || sailingSpeedLabel == nullptr
        || crawlAngularLabel == nullptr || omegaZLabel == nullptr
        || commandSpeedLabel == nullptr || commandAngularLabel == nullptr
        || bmsLabel == nullptr
        || crawlSpeedLabel->text() != QStringLiteral("2.00")
        || sailingSpeedLabel->text() != QStringLiteral("0.50")
        || commandSpeedLabel->text() != QStringLiteral("1.00 / 0.50")
        || bmsLabel->text() != QStringLiteral("自检 正常 / 告警等级 3 / 告警项 1 / SOC 58%")) {
        error << "initial mode=11 overview text is incorrect\n";
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

    tabs->setCurrentIndex(1);
    detailTabs->setCurrentIndex(detailTabs->count() - 1);
    events.push_back(modeEvent(11, 0, 102));
    events.push_back(modeEvent(0, 6, 103));
    const auto coalescedSnapshot = controlSnapshot(6, 1, 1, 103, events);
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

    panel.updateSnapshot(controlSnapshot(6, 1, 1, 1, events, true,
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

    auto* modeLabel = panel.findChild<QLabel*>(QStringLiteral("overview.control.mode"));
    auto* stateLabel = panel.findChild<QLabel*>(QStringLiteral("overview.command.state"));
    if (modeLabel == nullptr || stateLabel == nullptr) {
        error << "real-bag control labels are missing\n";
        return false;
    }

    QEventLoop loop;
    QTimer timeout;
    QTimer refresh;
    timeout.setSingleShot(true);
    timeout.setInterval(120000);
    refresh.setInterval(20);
    bool failed = false;
    bool seekRequested = false;
    bool playing = false;
    bool sawFinalCrawl = false;
    qint64 latestPositionNs = 0;

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
                         seekRequested = true;
                         source.seek(79000000000LL);
                     });
    QObject::connect(&source,
                     &autoviz::playback::LocalRosbagPlaybackSource::positionChanged,
                     &loop,
                     [&](qint64 positionNs, qint64) {
                         latestPositionNs = positionNs;
                         if (seekRequested && !playing
                             && qAbs(positionNs - 79000000000LL) < 1000000LL) {
                             panel.updateSnapshot(manager.getSnapshot());
                             source.setPlaybackRate(8.0);
                             source.play();
                             playing = true;
                         }
                     });
    QObject::connect(&refresh, &QTimer::timeout, &loop, [&] {
        panel.updateSnapshot(manager.getSnapshot());
        app.processEvents();
        if (latestPositionNs >= 82000000000LL
            && modeLabel->text() == QStringLiteral("自主爬行")
            && stateLabel->text() == QStringLiteral("自主爬行 / 已使能")) {
            sawFinalCrawl = true;
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

    if (failed || !sawFinalCrawl) {
        error << "real bag did not visibly render the current mode=6 command; crawl=" << sawFinalCrawl
              << ", mode='" << modeLabel->text() << "', state='"
              << stateLabel->text() << "'\n";
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
