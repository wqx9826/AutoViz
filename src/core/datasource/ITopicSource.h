#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

#include "core/datasource/TopicConfig.h"

namespace autoviz::datasource {

struct RawTopicMessage {
    QString topicName;
    QString msgType;
    QByteArray payload;
    double timestampSec = 0.0;
};

// Abstract source of topic messages. Implementations can later support
// live ROS subscriptions, replay files or test fixtures.
class ITopicSource
{
public:
    virtual ~ITopicSource() = default;

    virtual QString sourceName() const = 0;
    virtual bool configure(const QVector<TopicConfig>& topicConfigs) = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
};

}  // namespace autoviz::datasource
