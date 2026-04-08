#pragma once

#include <QString>
#include <QVector>

namespace autoviz::datasource {

// VisualizationRole describes what kind of visualization or business meaning
// a topic carries after binding to a msg definition.
enum class VisualizationRole {
    GlobalPath,
    LocalPath,
    Obstacles,
    Ego,
    Control,
    Unknown
};

// CompileStatus is a placeholder lifecycle status for msg build support.
// In later stages it will reflect whether the selected msg has been compiled
// into a type support artifact that can be consumed by the runtime.
enum class CompileStatus {
    NotCompiled,
    Pending,
    Compiled,
    Error
};

inline QString toDisplayString(VisualizationRole role)
{
    switch (role) {
    case VisualizationRole::GlobalPath:
        return QStringLiteral("全局路径");
    case VisualizationRole::LocalPath:
        return QStringLiteral("局部路径");
    case VisualizationRole::Obstacles:
        return QStringLiteral("障碍物");
    case VisualizationRole::Ego:
        return QStringLiteral("自车状态");
    case VisualizationRole::Control:
        return QStringLiteral("控制指令");
    case VisualizationRole::Unknown:
    default:
        return QStringLiteral("未指定");
    }
}

inline VisualizationRole visualizationRoleFromDisplayString(const QString& text)
{
    if (text == QStringLiteral("全局路径")) {
        return VisualizationRole::GlobalPath;
    }
    if (text == QStringLiteral("局部路径")) {
        return VisualizationRole::LocalPath;
    }
    if (text == QStringLiteral("障碍物")) {
        return VisualizationRole::Obstacles;
    }
    if (text == QStringLiteral("自车状态")) {
        return VisualizationRole::Ego;
    }
    if (text == QStringLiteral("控制指令")) {
        return VisualizationRole::Control;
    }
    return VisualizationRole::Unknown;
}

inline QString toDisplayString(CompileStatus status)
{
    switch (status) {
    case CompileStatus::NotCompiled:
        return QStringLiteral("未编译");
    case CompileStatus::Pending:
        return QStringLiteral("待编译");
    case CompileStatus::Compiled:
        return QStringLiteral("已编译");
    case CompileStatus::Error:
        return QStringLiteral("异常");
    }

    return QStringLiteral("未编译");
}

inline CompileStatus compileStatusFromDisplayString(const QString& text)
{
    if (text == QStringLiteral("待编译")) {
        return CompileStatus::Pending;
    }
    if (text == QStringLiteral("已编译")) {
        return CompileStatus::Compiled;
    }
    if (text == QStringLiteral("异常")) {
        return CompileStatus::Error;
    }
    return CompileStatus::NotCompiled;
}

// TopicConfig now focuses on user-selected msg files and topic binding.
// The old field_mapping-driven workflow is intentionally weakened in favor of
// future msg compilation and typed parsing.
struct TopicConfig {
    QString topicName;
    QString msgTypeName;
    QString msgFilePath;
    VisualizationRole visualizationRole = VisualizationRole::Unknown;
    bool enabled = true;
    CompileStatus compileStatus = CompileStatus::NotCompiled;
    QString notes;

    bool isValid() const
    {
        return !topicName.isEmpty();
    }
};

using TopicConfigList = QVector<TopicConfig>;

}  // namespace autoviz::datasource
