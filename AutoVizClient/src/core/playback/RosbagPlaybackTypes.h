#pragma once

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

namespace autoviz::playback {

enum class PlaybackState {
    Empty,
    Validating,
    Ready,
    Playing,
    Paused,
    Stopped,
    Completed,
    Error
};

struct RosbagChannelInfo {
    QString topic;
    QString type;
    qint64 messageCount = 0;
    bool supported = false;
    bool present = false;
};

struct RosbagInfo {
    QString directory;
    QString name;
    QString storageId;
    int metadataVersion = 0;
    int splitCount = 0;
    qint64 totalBytes = 0;
    qint64 startTimeNs = 0;
    qint64 endTimeNs = 0;
    QVector<RosbagChannelInfo> channels;
    QStringList warnings;

    qint64 durationNs() const { return qMax<qint64>(0, endTimeNs - startTimeNs); }
    bool isValid() const { return !directory.isEmpty() && startTimeNs > 0 && endTimeNs >= startTimeNs; }
};

QString playbackStateText(PlaybackState state);

}  // namespace autoviz::playback

Q_DECLARE_METATYPE(autoviz::playback::PlaybackState)
Q_DECLARE_METATYPE(autoviz::playback::RosbagInfo)
