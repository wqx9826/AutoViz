#pragma once

#include <QString>

namespace autoviz::datasource {

// MsgDefinition represents one user-selected .msg source file.
// The current stage only reads file content and extracts lightweight metadata.
// In later stages this object will also participate in msg compilation,
// generated type support management and runtime parser binding.
struct MsgDefinition {
    QString filePath;
    QString packageName;
    QString messageName;
    QString rawText;
    bool parsed = false;
    QString parseError;

    bool isValid() const;
    QString displayName() const;

    static MsgDefinition fromFile(const QString& filePath);
};

}  // namespace autoviz::datasource
