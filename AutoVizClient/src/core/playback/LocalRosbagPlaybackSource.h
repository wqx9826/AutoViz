#pragma once

#include <QObject>

#include "core/playback/RosbagPlaybackTypes.h"

namespace autoviz::datacenter { class DataManager; }

namespace autoviz::playback {

class LocalRosbagPlaybackSource final : public QObject {
    Q_OBJECT
public:
    explicit LocalRosbagPlaybackSource(datacenter::DataManager* dataManager, QObject* parent=nullptr);
    ~LocalRosbagPlaybackSource() override;

    PlaybackState state() const { return m_state; }
    RosbagInfo bagInfo() const { return m_info; }
    double playbackRate() const { return m_rate; }

public slots:
    void loadAndValidate(const QString& directory);
    void play();
    void pause();
    void stop();
    void seek(qint64 relativeTimeNs);
    void setPlaybackRate(double rate);

signals:
    void validationProgress(int percent, const QString& text);
    void bagLoaded(const autoviz::playback::RosbagInfo& info);
    void playbackStateChanged(autoviz::playback::PlaybackState state, const QString& text);
    void positionChanged(qint64 relativeTimeNs, qint64 durationNs);
    void playbackRateChanged(double rate);
    void errorOccurred(const QString& error);
    void requestLoad(const QString& directory);
    void requestPlay();
    void requestPause();
    void requestStop();
    void requestSeek(qint64 relativeTimeNs);
    void requestRate(double rate);

private:
    class Worker;
    Worker* m_worker = nullptr;
    class QThread* m_thread = nullptr;
    PlaybackState m_state = PlaybackState::Empty;
    RosbagInfo m_info;
    double m_rate = 1.0;
};

}  // namespace autoviz::playback
