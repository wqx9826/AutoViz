#pragma once

#include <QString>
#include <QVector>

#include "core/datasource/MsgDefinition.h"

namespace autoviz::datasource {

// RosMsgPackage represents one ROS package directory selected by the user.
// It is the package-level input for later msg compilation, type-support
// generation and runtime topic subscription/parsing.
struct RosMsgPackage {
    QString packageName;
    QString packagePath;
    QString packageXmlPath;
    QString msgDirectoryPath;
    QVector<MsgDefinition> msgFiles;
    bool isValid = false;
    QString errorMessage;

    int msgCount() const
    {
        return msgFiles.size();
    }
};

}  // namespace autoviz::datasource
