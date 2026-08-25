#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTextStream>
#include <QTimer>

#include "core/datacenter/DataManager.h"
#include "core/playback/LocalRosbagPlaybackSource.h"

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().size() != 2) {
        QTextStream(stderr) << "usage: AutoVizPlaybackSourceSmoke BAG_DIR\n";
        return 2;
    }

    autoviz::datacenter::DataManager manager;
    manager.activateInputSource(autoviz::datacenter::VisualizationInputSource::Ros2Bag);
    autoviz::playback::LocalRosbagPlaybackSource source(&manager);
    autoviz::playback::RosbagInfo bagInfo;
    QTimer timeout;
    QElapsedTimer pauseLatency;
    bool pauseRequested = false;
    bool centerTurnScenario = false;
    enum class Phase { WaitingLoad, CenterSeek, ExitSeek, SecondCenterSeek, OneXSeek, OneXPlaying,
                       TimelineSeek, TimelinePlaying, FinalSeek, FinalPlaying };
    Phase phase = Phase::WaitingLoad;

    auto fail = [&](const QString& message) {
        QTextStream(stderr) << message << '\n';
        app.exit(1);
    };

    timeout.setSingleShot(true);
    timeout.setInterval(120000);
    QObject::connect(&timeout, &QTimer::timeout, &app, [&] {
        QTextStream(stderr) << "timeout\n";
        app.exit(1);
    });
    QObject::connect(&source,
                     &autoviz::playback::LocalRosbagPlaybackSource::errorOccurred,
                     &app,
                     [&](const QString& error) {
                         QTextStream(stderr) << error << '\n';
                         app.exit(1);
                     });
    QObject::connect(&source,
                     &autoviz::playback::LocalRosbagPlaybackSource::bagLoaded,
                     &app,
                     [&](const autoviz::playback::RosbagInfo& info) {
                         bagInfo = info;
                         QTextStream(stdout) << info.name << ": validated, "
                                             << info.channels.size() << " channels\n";
                         centerTurnScenario = info.name == QStringLiteral("rosbag2_2026_08_17-03_18_59");
                         if (centerTurnScenario) {
                             phase = Phase::CenterSeek;
                             source.seek(42000000000LL);
                         } else {
                             phase = Phase::TimelinePlaying;
                             source.setPlaybackRate(8.0);
                             source.play();
                             QTimer::singleShot(1500, &app, [&] {
                                 pauseRequested = true;
                                 pauseLatency.start();
                                 source.pause();
                             });
                         }
                     });
    QObject::connect(&source,
                     &autoviz::playback::LocalRosbagPlaybackSource::positionChanged,
                     &app,
                     [&](qint64 positionNs, qint64) {
                         if (phase == Phase::CenterSeek && qAbs(positionNs - 42000000000LL) < 1000000LL) {
                             const auto snapshot = manager.getSnapshot();
                             if (!snapshot.controlCommandStatus.valid
                                 || snapshot.controlCommandStatus.mode != 11
                                 || !snapshot.globalPath.points.isEmpty()
                                 || !snapshot.localPath.points.isEmpty()) {
                                 fail(QStringLiteral("seek inside center turn retained paths or wrong command mode"));
                                 return;
                             }
                             phase = Phase::ExitSeek;
                             source.seek(81000000000LL);
                         } else if (phase == Phase::ExitSeek && qAbs(positionNs - 81000000000LL) < 1000000LL) {
                             const auto snapshot = manager.getSnapshot();
                             if (!snapshot.controlCommandStatus.valid
                                 || snapshot.controlCommandStatus.mode != 6
                                 || snapshot.runtimeStatus.snapshotSequence == 0
                                 || snapshot.globalPath.points.isEmpty()
                                 || snapshot.localPath.points.isEmpty()) {
                                 fail(QStringLiteral("post-center seek did not restore mode=6 from new paths"));
                                 return;
                             }
                             phase = Phase::SecondCenterSeek;
                             source.seek(131000000000LL);
                         } else if (phase == Phase::SecondCenterSeek
                                    && qAbs(positionNs - 131000000000LL) < 1000000LL) {
                             const auto snapshot = manager.getSnapshot();
                             if (!snapshot.controlCommandStatus.valid
                                 || snapshot.controlCommandStatus.mode != 11
                                 || !snapshot.globalPath.points.isEmpty()
                                 || !snapshot.localPath.points.isEmpty()) {
                                 fail(QStringLiteral("second center-turn boundary did not restore mode=11 suppression"));
                                 return;
                             }
                             phase = Phase::OneXSeek;
                             source.seek(79000000000LL);
                         } else if (phase == Phase::OneXSeek
                                    && qAbs(positionNs - 79000000000LL) < 1000000LL) {
                             phase = Phase::OneXPlaying;
                             source.setPlaybackRate(1.0);
                             source.play();
                             QTimer::singleShot(2500, &app, [&] {
                                 pauseRequested = true;
                                 pauseLatency.start();
                                 source.pause();
                             });
                         } else if (phase == Phase::TimelineSeek && qAbs(positionNs - 38000000000LL) < 1000000LL) {
                             phase = Phase::TimelinePlaying;
                             source.setPlaybackRate(8.0);
                             source.play();
                             QTimer::singleShot(6000, &app, [&] {
                                 pauseRequested = true;
                                 pauseLatency.start();
                                 source.pause();
                             });
                         } else if (phase == Phase::FinalSeek
                                    && qAbs(positionNs - qMax<qint64>(0, bagInfo.durationNs() - 1000000000LL)) < 1000000LL) {
                             phase = Phase::FinalPlaying;
                             source.play();
                         }
                     });
    QObject::connect(&source,
                     &autoviz::playback::LocalRosbagPlaybackSource::playbackStateChanged,
                     &app,
                     [&](autoviz::playback::PlaybackState state, const QString&) {
                         if (state == autoviz::playback::PlaybackState::Paused && pauseRequested) {
                             const qint64 latency = pauseLatency.elapsed();
                             if (latency > 300) {
                                 QTextStream(stderr) << "pause latency too high: "
                                                     << latency << " ms\n";
                                 app.exit(1);
                                 return;
                             }
                             if (phase == Phase::OneXPlaying) {
                                 const auto snapshot = manager.getSnapshot();
                                 if (!snapshot.controlCommandStatus.valid
                                     || snapshot.controlCommandStatus.mode != 6
                                     || snapshot.runtimeStatus.snapshotSequence == 0) {
                                     fail(QStringLiteral("1x playback did not reach the current mode=6 command"));
                                     return;
                                 }
                                 QTextStream(stdout) << "1x transition pause latency: " << latency << " ms\n";
                                 pauseRequested = false;
                                 phase = Phase::TimelineSeek;
                                 source.seek(38000000000LL);
                                 return;
                             }
                             QTextStream(stdout) << "8x pause latency: " << latency << " ms\n";
                             if (centerTurnScenario) {
                                 const auto snapshot = manager.getSnapshot();
                                 if (!snapshot.controlCommandStatus.valid
                                     || snapshot.controlCommandStatus.mode != 6
                                     || !snapshot.chassisRuntimeStatus.valid
                                     || !snapshot.chassisRuntimeStatus.leftCrawlMotor.outputEnabled
                                     || !snapshot.chassisRuntimeStatus.rightCrawlMotor.outputEnabled) {
                                     fail(QStringLiteral("8x playback did not reach the current command and chassis state"));
                                     return;
                                 }
                             }
                             pauseRequested = false;
                             phase = Phase::FinalSeek;
                             source.seek(qMax<qint64>(0, bagInfo.durationNs() - 1000000000LL));
                         }
                         if (state == autoviz::playback::PlaybackState::Completed) {
                             const auto snapshot = manager.getSnapshot();
                             if (snapshot.runtimeStatus.inputSource
                                 != autoviz::datacenter::VisualizationInputSource::Ros2Bag) {
                                 QTextStream(stderr) << "wrong input source\n";
                                 app.exit(1);
                                 return;
                             }
                             QTextStream(stdout) << "seek/8x/EOF: OK\n";
                             app.quit();
                         }
                     });

    timeout.start();
    source.loadAndValidate(app.arguments().at(1));
    return app.exec();
}
