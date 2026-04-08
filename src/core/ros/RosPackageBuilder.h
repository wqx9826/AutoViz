#pragma once

#include <QObject>
#include <QStringList>

#include "core/ros/ProcessRunner.h"
#include "core/ros/RosTypes.h"

namespace autoviz::ros {

class RosPackageBuilder : public QObject
{
    Q_OBJECT

public:
    explicit RosPackageBuilder(QObject* parent = nullptr);

    bool isBuilding() const;
    bool startBuild(
        const RosEnvironmentInfo& environmentInfo,
        const QString& workspaceRoot,
        const QStringList& packageNames,
        QString* errorMessage);

signals:
    void buildStarted(const QString& commandSummary);
    void buildOutput(const QString& text);
    void buildFinished(bool success, const QString& summary, const QString& fullOutput);

private:
    QString createBuildCommand(
        const RosEnvironmentInfo& environmentInfo,
        const QStringList& packageNames,
        QString* errorMessage) const;
    QString buildEnvironmentPrefix(const RosEnvironmentInfo& environmentInfo) const;
    QString extractBasicError() const;

    ProcessRunner m_processRunner;
    QString m_fullOutput;
};

}  // namespace autoviz::ros
