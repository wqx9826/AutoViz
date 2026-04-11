#pragma once

#include <QWidget>

class ChartPanel : public QWidget
{
public:
    explicit ChartPanel(QWidget* parent = nullptr);

private:
    void setupUi();
};
