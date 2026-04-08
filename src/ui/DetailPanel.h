#pragma once

#include <QWidget>

class QTextEdit;

// Placeholder detail panel. It will later display selected object attributes,
// parser diagnostics and other context-sensitive metadata.
class DetailPanel : public QWidget
{
public:
    explicit DetailPanel(QWidget* parent = nullptr);

    void setDetailText(const QString& text);

private:
    void setupUi();

    QTextEdit* m_textEdit = nullptr;
};
