#pragma once

#include <QStringList>
#include <QWidget>

#include "core/ros/RosTypes.h"

class QLabel;
class QPushButton;
class QTableWidget;

class MessagePackagePanel : public QWidget
{
    Q_OBJECT

public:
    explicit MessagePackagePanel(QWidget* parent = nullptr);

    void setEnvironmentInfo(const autoviz::ros::RosEnvironmentInfo& environmentInfo, const QString& workspaceRoot);
    void setPackages(const autoviz::ros::ManagedRosPackageList& packages);
    void setBuildInProgress(bool inProgress);
    QStringList selectedPackageNames() const;

signals:
    void addPackagesRequested();
    void removePackagesRequested(const QStringList& packageNames);
    void buildPackagesRequested();
    void packageEnabledChanged(const QString& packageName, bool enabled);

private:
    void setupUi();
    void refreshHeader();
    void refreshButtonState();

    autoviz::ros::RosEnvironmentInfo m_environmentInfo;
    autoviz::ros::ManagedRosPackageList m_packages;
    QString m_workspaceRoot;
    bool m_buildInProgress = false;

    QLabel* m_environmentLabel = nullptr;
    QLabel* m_workspaceLabel = nullptr;
    QLabel* m_hintLabel = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_removeButton = nullptr;
    QPushButton* m_buildButton = nullptr;
    QTableWidget* m_table = nullptr;
};
