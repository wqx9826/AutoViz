#pragma once

#include <QByteArray>
#include <QString>

#include "autoviz/transport.pb.h"

namespace autoviz::playback {

class RobotWsCdrDecoder {
public:
    static bool isSupported(const QString& topic, const QString& type);
    static QString expectedType(const QString& topic);
    static ::autoviz::DataKind dataKind(const QString& topic);
    static QStringList supportedTopics();

    static bool decode(const QString& topic,
                       const QString& type,
                       const QByteArray& payload,
                       quint64 receiveTimeNs,
                       ::autoviz::VisualizationSnapshot* snapshot,
                       QString* error);
};

}  // namespace autoviz::playback
