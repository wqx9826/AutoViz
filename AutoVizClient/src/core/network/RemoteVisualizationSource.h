#pragma once

#include <cstdint>
#include <string>

#include <QElapsedTimer>
#include <QObject>
#include <QString>

#include "autoviz/protocol/FrameCodec.h"

class QTcpSocket;
class QTimer;

namespace autoviz::datacenter {
class DataManager;
}

namespace autoviz::network {

class RemoteVisualizationSource final : public QObject {
    Q_OBJECT

public:
    explicit RemoteVisualizationSource(datacenter::DataManager* dataManager,
                                       QObject* parent = nullptr);
    ~RemoteVisualizationSource() override;

    bool isConnected() const;
    QString host() const;
    quint16 port() const;

public slots:
    void connectToServer(const QString& host, quint16 port, bool autoReconnect);
    void disconnectFromServer();

signals:
    void connectionStateChanged(const QString& text, bool connected);
    void serverIdentityChanged(const QString& sourceDescription);

private:
    void sendEnvelope(const protocol::v1::Envelope& envelope);
    void sendHello();
    void sendSubscribe();
    void sendHeartbeat();
    void handleEnvelope(const protocol::v1::Envelope& envelope);
    void scheduleReconnect();
    void setState(const QString& text, bool connected);

    datacenter::DataManager* m_dataManager = nullptr;
    QTcpSocket* m_socket = nullptr;
    QTimer* m_heartbeatTimer = nullptr;
    QTimer* m_watchdogTimer = nullptr;
    QTimer* m_reconnectTimer = nullptr;
    protocol::FrameDecoder m_decoder;
    QElapsedTimer m_lastReceive;
    QString m_host = QStringLiteral("127.0.0.1");
    quint16 m_port = 39090;
    QString m_sessionId;
    bool m_autoReconnect = true;
    bool m_manualDisconnect = false;
    std::uint64_t m_heartbeatSequence = 0;
};

}  // namespace autoviz::network
