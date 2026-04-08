#include "core/datasource/MsgRegistry.h"

#include <QFileInfo>

namespace autoviz::datasource {

bool MsgRegistry::addMsgFile(const QString& path)
{
    const MsgDefinition definition = MsgDefinition::fromFile(path);
    if (!definition.isValid()) {
        m_lastError = definition.parseError.isEmpty() ? QStringLiteral("消息文件无效。") : definition.parseError;
        return false;
    }

    for (auto& existing : m_definitions) {
        if (QFileInfo(existing.filePath) == QFileInfo(path)) {
            existing = definition;
            m_lastError.clear();
            return true;
        }
    }

    m_definitions.push_back(definition);
    m_lastError.clear();
    return true;
}

bool MsgRegistry::removeMsgFile(const QString& path)
{
    for (auto it = m_definitions.begin(); it != m_definitions.end(); ++it) {
        if (QFileInfo(it->filePath) == QFileInfo(path)) {
            m_definitions.erase(it);
            m_lastError.clear();
            return true;
        }
    }

    m_lastError = QStringLiteral("未找到要移除的消息文件。");
    return false;
}

QVector<MsgDefinition> MsgRegistry::listMsgDefinitions() const
{
    return m_definitions;
}

std::optional<MsgDefinition> MsgRegistry::findMsgByName(const QString& name) const
{
    for (const auto& definition : m_definitions) {
        if (definition.messageName == name || definition.displayName() == name) {
            return definition;
        }
    }
    return std::nullopt;
}

QStringList MsgRegistry::availableMsgTypeNames() const
{
    QStringList names;
    for (const auto& definition : m_definitions) {
        names.push_back(definition.displayName());
    }
    names.removeDuplicates();
    return names;
}

QString MsgRegistry::lastError() const
{
    return m_lastError;
}

}  // namespace autoviz::datasource
