#pragma once

#include <QString>

#include "core/datasource/ITopicSource.h"

namespace autoviz::datasource {

// Parser abstraction for adapting one message family into normalized internal
// models such as trajectory, obstacles or ego state.
class IMsgParser
{
public:
    virtual ~IMsgParser() = default;

    virtual QString parserName() const = 0;
    virtual QString supportedMsgType() const = 0;
    virtual bool parse(const RawTopicMessage& message, const TopicConfig& config) = 0;
    virtual QString lastError() const = 0;
};

}  // namespace autoviz::datasource
