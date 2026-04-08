#pragma once

#include <optional>

#include <QString>
#include <QStringList>
#include <QVector>

#include "core/datasource/MsgDefinition.h"

namespace autoviz::datasource {

// MsgRegistry stores user-loaded .msg definitions for later binding and
// compilation. Future versions will extend this registry with generated
// type-support artifacts and parser factories.
class MsgRegistry
{
public:
    bool addMsgFile(const QString& path);
    bool removeMsgFile(const QString& path);

    QVector<MsgDefinition> listMsgDefinitions() const;
    std::optional<MsgDefinition> findMsgByName(const QString& name) const;
    QStringList availableMsgTypeNames() const;
    QString lastError() const;

private:
    QVector<MsgDefinition> m_definitions;
    QString m_lastError;
};

}  // namespace autoviz::datasource
