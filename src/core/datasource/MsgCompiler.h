#pragma once

#include <QString>

#include "core/datasource/RosMsgPackage.h"

namespace autoviz::datasource {

// MsgCompiler is a placeholder for the future ROS package-level msg build
// pipeline. Later it will organize one RosMsgPackage into a temporary
// workspace, invoke the ROS message generation toolchain and expose type
// support artifacts to the subscriber/parser layer.
class MsgCompiler
{
public:
    bool compilePackage(const RosMsgPackage& package);
    QString lastError() const;

private:
    QString m_lastError;
};

}  // namespace autoviz::datasource
