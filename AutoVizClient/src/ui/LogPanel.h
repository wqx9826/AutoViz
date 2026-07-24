#pragma once

#include <QWidget>

class QPlainTextEdit;

// Centralized logging panel. It provides an append-only output area for
// runtime events and later parser/source diagnostics.
class LogPanel : public QWidget
{
public:
    explicit LogPanel(QWidget* parent = nullptr);

    void appendLog(const QString& message);

private:
    void setupUi();

    QPlainTextEdit* m_output = nullptr;
};
