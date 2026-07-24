#include "ui/LogPanel.h"

#include <QDateTime>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>

LogPanel::LogPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void LogPanel::appendLog(const QString& message)
{
    if (m_output == nullptr) {
        return;
    }

    const QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    m_output->appendPlainText(QString("[%1] %2").arg(timestamp, message));
}

void LogPanel::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* title = new QLabel(tr("日志输出"), this);
    title->setStyleSheet("font-weight: 600;");
    layout->addWidget(title);

    m_output = new QPlainTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setMaximumBlockCount(2000);
    m_output->setPlaceholderText(tr("运行日志将在此显示。"));
    layout->addWidget(m_output, 1);
}
