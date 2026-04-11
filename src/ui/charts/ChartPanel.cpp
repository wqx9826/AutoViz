#include "ui/charts/ChartPanel.h"

#include <QLabel>
#include <QVBoxLayout>

ChartPanel::ChartPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void ChartPanel::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* title = new QLabel(tr("控制曲线面板"), this);
    title->setStyleSheet("font-weight: 600;");
    layout->addWidget(title);

    auto* body = new QLabel(
        tr("当前为图表骨架。\nTODO：后续在此接入速度、加速度、转角、曲率、误差等历史曲线。"),
        this);
    body->setWordWrap(true);
    body->setStyleSheet("padding: 10px; background: #f4f7fb; border: 1px dashed #bfd0de; border-radius: 6px;");
    layout->addWidget(body, 1);
}
