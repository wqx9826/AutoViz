#pragma once

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QSpinBox;

namespace autoviz::ui {

class ConnectionDialog final : public QDialog {
    Q_OBJECT

public:
    explicit ConnectionDialog(QWidget* parent = nullptr);

    void setConnection(const QString& host, quint16 port, bool autoConnect);
    QString host() const;
    quint16 port() const;
    bool autoConnect() const;

private:
    QLineEdit* m_hostEdit = nullptr;
    QSpinBox* m_portSpin = nullptr;
    QCheckBox* m_autoConnectCheck = nullptr;
};

}  // namespace autoviz::ui
