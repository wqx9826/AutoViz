#pragma once

#include "core/ros/RosTypes.h"

#include <QHash>
#include <QWidget>

class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

class DetailPanel : public QWidget
{
public:
    explicit DetailPanel(QWidget* parent = nullptr);

    void setSummary(
        const autoviz::ros::RosEnvironmentInfo& environmentInfo,
        const autoviz::ros::ManagedRosPackageList& packages);

private:
    void setupUi();
    static QString summarizePackageNames(const autoviz::ros::ManagedRosPackageList& packages);
    static QString simplifyEnvironmentValue(const autoviz::ros::RosEnvironmentInfo& environmentInfo);
    QTreeWidgetItem* addSection(const QString& title);
    void updateSectionItem(QTreeWidgetItem* section, const QString& key, const QString& value);

    QTreeWidget* m_treeWidget = nullptr;
    QTreeWidgetItem* m_rosPackageSection = nullptr;
    QHash<QString, QTreeWidgetItem*> m_rosPackageItems;
    QHash<QTreeWidgetItem*, QToolButton*> m_sectionButtons;
};
