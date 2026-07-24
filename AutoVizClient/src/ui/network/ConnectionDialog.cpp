#include "ui/network/ConnectionDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

namespace autoviz::ui {

ConnectionDialog::ConnectionDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("连接 AutoViz Server"));
    setModal(true);
    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    m_hostEdit = new QLineEdit(QStringLiteral("127.0.0.1"), this);
    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(39090);
    m_autoConnectCheck = new QCheckBox(tr("启动时自动连接，断线后自动重连"), this);
    m_autoConnectCheck->setChecked(true);
    form->addRow(tr("服务器地址"), m_hostEdit);
    form->addRow(tr("TCP 端口"), m_portSpin);
    form->addRow(QString(), m_autoConnectCheck);
    root->addLayout(form);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

void ConnectionDialog::setConnection(const QString& host, quint16 port, bool autoConnect)
{
    m_hostEdit->setText(host);
    m_portSpin->setValue(port);
    m_autoConnectCheck->setChecked(autoConnect);
}

QString ConnectionDialog::host() const
{
    return m_hostEdit->text().trimmed();
}

quint16 ConnectionDialog::port() const
{
    return static_cast<quint16>(m_portSpin->value());
}

bool ConnectionDialog::autoConnect() const
{
    return m_autoConnectCheck->isChecked();
}

}  // namespace autoviz::ui
