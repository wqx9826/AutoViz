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
    autoviz::playback::LocalRosbagPlaybackSource source(&manager);
    autoviz::playback::RosbagInfo bagInfo;
    QTimer timeout;
    QElapsedTimer pauseLatency;
    bool pauseRequested = false;

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
                         source.setPlaybackRate(8.0);
                         source.play();
                         QTimer::singleShot(1500, &app, [&] {
                             pauseRequested = true;
                             pauseLatency.start();
                             source.pause();
                         });
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
                             QTextStream(stdout) << "8x pause latency: " << latency << " ms\n";
                             pauseRequested = false;
                             source.seek(qMax<qint64>(0, bagInfo.durationNs() - 1000000000LL));
                             source.play();
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
