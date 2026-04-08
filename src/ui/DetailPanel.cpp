#include "ui/DetailPanel.h"

#include <QLabel>
#include <QTextEdit>
#include <QVBoxLayout>

DetailPanel::DetailPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void DetailPanel::setDetailText(const QString& text)
{
    if (m_textEdit != nullptr) {
        m_textEdit->setPlainText(text);
    }
}

void DetailPanel::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* title = new QLabel(tr("详细信息"), this);
    title->setStyleSheet("font-weight: 600;");
    layout->addWidget(title);

    m_textEdit = new QTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setPlainText(
        tr("在二维视图中选择对象后，可在此查看对应属性。\n\n"
           "该面板预留用于展示对象元数据、解析结果和调试上下文。"));
    layout->addWidget(m_textEdit, 1);
}
