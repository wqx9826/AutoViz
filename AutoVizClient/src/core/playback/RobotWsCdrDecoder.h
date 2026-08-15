#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>

#include "autoviz/transport.pb.h"

namespace autoviz::playback {

class RobotWsCdrDecoder {
public:
    struct ActionDiagnostic {
        bool hasNativeStatus = false;
        qint32 nativeStatus = 0;
        quint64 nativeStatusTimeNs = 0;
        bool hasFeedbackProgress = false;
        double feedbackProgress = 0.0;
        quint64 feedbackTimeNs = 0;
    };
    using ActionDiagnosticCache = QHash<QString, ActionDiagnostic>;
    static bool isSupported(const QString& topic, const QString& type);
    static QString expectedType(const QString& topic);
    static ::autoviz::DataKind dataKind(const QString& topic);
    static QStringList supportedTopics();
    // robot_ws 的 SystemRunStates.goal_uuid 可能丢字节前导零（%x），与隐藏 action
    // topic 的 canonical UUID 直接比较会失配；本函数把 canonical 侧转成 lossy 形式做
    // 对称比对，兼容修复前（丢零）和修复后（canonical）两种 robot_ws 输出。
    static bool sameGoalUuid(const QString& a, const QString& b);

    static bool decode(const QString& topic,
                       const QString& type,
                       const QByteArray& payload,
                       quint64 receiveTimeNs,
                       ::autoviz::VisualizationSnapshot* snapshot,
                       QString* error,
                       ActionDiagnosticCache* actionDiagnostics = nullptr);
};

}  // namespace autoviz::playback
