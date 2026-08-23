#include "core/network/RemoteVisualizationSource.h"

#include <QAbstractSocket>
#include <QDateTime>
#include <QTcpSocket>
#include <QTimer>

#include "core/datacenter/DataManager.h"
#include "core/network/ProtocolModelConverter.h"
#include "autoviz/ProtocolVersion.h"
#include "utils/Logger.h"

namespace autoviz::network {

namespace wire = ::autoviz;

RemoteVisualizationSource::RemoteVisualizationSource(datacenter::DataManager* dataManager,
                                                     QObject* parent)
    : QObject(parent)
    , m_dataManager(dataManager)
    , m_socket(new QTcpSocket(this))
    , m_heartbeatTimer(new QTimer(this))
    , m_watchdogTimer(new QTimer(this))
    , m_reconnectTimer(new QTimer(this))
{
    m_heartbeatTimer->setInterval(1000);
    m_watchdogTimer->setInterval(500);
    m_reconnectTimer->setSingleShot(true);
    m_reconnectTimer->setInterval(2000);

    connect(m_socket, &QTcpSocket::connected, this, [this]() {
        m_decoder.reset();
        m_lastReceive.restart();
        setState(tr("已连接 %1:%2，协议握手中").arg(m_host).arg(m_port), false);
        sendHello();
        m_heartbeatTimer->start();
        m_watchdogTimer->start();
    });
    connect(m_socket, &QTcpSocket::readyRead, this, [this]() {
        const QByteArray bytes = m_socket->readAll();
        std::vector<wire::Envelope> envelopes;
        std::string error;
        if (!m_decoder.decode(
                std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())),
                envelopes,
                error)) {
            Logger::instance().warning(
                QStringLiteral("远程协议帧解析失败：%1").arg(QString::fromStdString(error)));
            m_socket->abort();
            return;
        }
        m_lastReceive.restart();
        for (const auto& envelope : envelopes) {
            handleEnvelope(envelope);
        }
    });
    connect(m_socket, &QTcpSocket::disconnected, this, [this]() {
        m_heartbeatTimer->stop();
        m_watchdogTimer->stop();
        m_sessionId.clear();
        if (m_dataManager != nullptr) {
            m_dataManager->resetVisualizationData(datacenter::VisualizationInputSource::Remote);
        }
        setState(m_manualDisconnect ? tr("已断开服务器")
                                    : tr("与 %1:%2 的连接已断开").arg(m_host).arg(m_port),
                 false);
        scheduleReconnect();
    });
    connect(m_socket,
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
            qOverload<QAbstractSocket::SocketError>(&QTcpSocket::errorOccurred),
#else
            qOverload<QAbstractSocket::SocketError>(&QAbstractSocket::error),
#endif
            this,
            [this](QAbstractSocket::SocketError) {
                setState(tr("连接错误：%1").arg(m_socket->errorString()), false);
                // connectToHost 被拒绝时 Qt 不保证再发 disconnected；下一事件循环确认
                // socket 已回到 Unconnected 后主动续上自动重连链。
                QTimer::singleShot(0, this, [this]() {
                    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
                        scheduleReconnect();
                    }
                });
            });
    connect(m_heartbeatTimer, &QTimer::timeout, this, &RemoteVisualizationSource::sendHeartbeat);
    connect(m_watchdogTimer, &QTimer::timeout, this, [this]() {
        if (m_lastReceive.isValid() && m_lastReceive.elapsed() > 5000) {
            Logger::instance().warning(QStringLiteral("AutoViz Server 心跳超时，正在重连。"));
            m_socket->abort();
        }
    });
    connect(m_reconnectTimer, &QTimer::timeout, this, [this]() {
        if (!m_manualDisconnect) {
            setState(tr("正在重连 %1:%2").arg(m_host).arg(m_port), false);
            m_socket->connectToHost(m_host, m_port);
        }
    });
}

RemoteVisualizationSource::~RemoteVisualizationSource()
{
    m_manualDisconnect = true;
    m_socket->abort();
}

bool RemoteVisualizationSource::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState && !m_sessionId.isEmpty();
}

QString RemoteVisualizationSource::host() const
{
    return m_host;
}

quint16 RemoteVisualizationSource::port() const
{
    return m_port;
}

void RemoteVisualizationSource::connectToServer(const QString& host,
                                                quint16 port,
                                                bool autoReconnect)
{
    m_host = host.trimmed().isEmpty() ? QStringLiteral("127.0.0.1") : host.trimmed();
    m_port = port == 0 ? 39090 : port;
    m_autoReconnect = autoReconnect;
    m_manualDisconnect = false;
    m_reconnectTimer->stop();
    m_sessionId.clear();
    m_decoder.reset();
    if (m_dataManager != nullptr) {
        m_dataManager->resetVisualizationData(datacenter::VisualizationInputSource::Remote);
    }
    m_socket->abort();
    setState(tr("正在连接 %1:%2").arg(m_host).arg(m_port), false);
    m_socket->connectToHost(m_host, m_port);
}

void RemoteVisualizationSource::disconnectFromServer()
{
    m_manualDisconnect = true;
    m_reconnectTimer->stop();
    m_socket->disconnectFromHost();
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        if (m_dataManager != nullptr) {
            m_dataManager->resetVisualizationData(datacenter::VisualizationInputSource::Remote);
        }
        setState(tr("已断开服务器"), false);
    }
}

void RemoteVisualizationSource::sendEnvelope(const wire::Envelope& envelope)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    wire::FrameBytes frame;
    std::string error;
    if (wire::encodeFrame(envelope, frame, error)) {
        m_socket->write(frame.data(), static_cast<qint64>(frame.size()));
    } else {
        Logger::instance().warning(
            QStringLiteral("远程协议帧编码失败：%1").arg(QString::fromStdString(error)));
    }
}

void RemoteVisualizationSource::sendHello()
{
    wire::Envelope envelope;
    auto* hello = envelope.mutable_client_hello();
    hello->set_client_name("AutoViz Qt Client");
    hello->set_client_version("0.3.0");
    hello->set_protocol_major(wire::kProtocolMajor);
    hello->set_protocol_minor(wire::kProtocolMinor);
    sendEnvelope(envelope);
}

void RemoteVisualizationSource::sendHeartbeat()
{
    wire::Envelope envelope;
    auto* heartbeat = envelope.mutable_heartbeat();
    heartbeat->set_sequence(++m_heartbeatSequence);
    heartbeat->set_send_time_ns(
        static_cast<std::uint64_t>(QDateTime::currentMSecsSinceEpoch()) * 1000000ULL);
    heartbeat->set_session_id(m_sessionId.toStdString());
    sendEnvelope(envelope);
}

void RemoteVisualizationSource::handleEnvelope(const wire::Envelope& envelope)
{
    if (envelope.has_server_hello()) {
        const auto& hello = envelope.server_hello();
        if (!wire::isProtocolMajorCompatible(hello.protocol_major())) {
            Logger::instance().error(
                QStringLiteral("协议主版本不兼容：Server=%1，Client=%2")
                    .arg(hello.protocol_major())
                    .arg(wire::kProtocolMajor));
            m_socket->disconnectFromHost();
            return;
        }
        const QString newSession = QString::fromStdString(hello.session_id());
        if (!m_sessionId.isEmpty() && m_sessionId != newSession && m_dataManager != nullptr) {
            m_dataManager->resetVisualizationData(datacenter::VisualizationInputSource::Remote);
        }
        m_sessionId = newSession;
        const QString source = hello.has_source()
                                   ? QString::fromStdString(hello.source().description())
                                   : QStringLiteral("未知 Adapter");
        Logger::instance().info(
            QStringLiteral("协议握手成功：Client Protocol %1，Server Protocol %2.%3，Server %4")
                .arg(QString::fromLatin1(wire::kProtocolVersion))
                .arg(hello.protocol_major())
                .arg(hello.protocol_minor())
                .arg(QString::fromStdString(hello.server_version())));
        emit serverIdentityChanged(source);
        setState(tr("已连接 %1:%2（%3）").arg(m_host).arg(m_port).arg(source), true);
        return;
    }
    if (envelope.has_snapshot()) {
        const auto& snapshot = envelope.snapshot();
        const QString session = QString::fromStdString(snapshot.session_id());
        if (!m_sessionId.isEmpty() && !session.isEmpty() && session != m_sessionId) {
            m_sessionId = session;
            if (m_dataManager != nullptr) {
                m_dataManager->resetVisualizationData(datacenter::VisualizationInputSource::Remote);
            }
        }
        if (m_dataManager != nullptr) {
            m_dataManager->replaceVisualizationSnapshot(
                ProtocolModelConverter::toModelSnapshot(snapshot),
                datacenter::VisualizationInputSource::Remote);
        }
        return;
    }
    if (envelope.has_heartbeat()) {
        return;
    }
    if (envelope.has_error()) {
        Logger::instance().warning(
            QStringLiteral("Server 协议错误 %1：%2")
                .arg(envelope.error().code())
                .arg(QString::fromStdString(envelope.error().message())));
        if (envelope.error().fatal()) {
            m_socket->disconnectFromHost();
        }
    }
}

void RemoteVisualizationSource::scheduleReconnect()
{
    if (m_autoReconnect && !m_manualDisconnect && !m_reconnectTimer->isActive()) {
        m_reconnectTimer->start();
    }
}

void RemoteVisualizationSource::setState(const QString& text, bool connected)
{
    emit connectionStateChanged(text, connected);
}

}  // namespace autoviz::network
