#include "autoviz_server/VisualizationServer.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <iomanip>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/asio.hpp>

#include "autoviz/ProtocolVersion.h"
#include "autoviz_server/TcpSession.h"

namespace autoviz_server {
namespace asio = boost::asio;
using tcp = asio::ip::tcp;
namespace wire = ::autoviz;

namespace {
std::uint64_t systemNowNs()
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

std::string makeSessionId()
{
    std::ostringstream stream;
    stream << std::hex << systemNowNs();
    return stream.str();
}
}  // namespace

class VisualizationServer::Impl {
public:
    bool start(const VisualizationServerConfig& config,
               const VisualizationServerIdentity& identity,
               std::string* errorMessage)
    {
        if (m_running.exchange(true)) {
            if (errorMessage != nullptr) {
                *errorMessage = "VisualizationServer is already running";
            }
            return false;
        }

        m_context.restart();
        m_config = config;
        m_config.maxClients = std::max<std::size_t>(1U, m_config.maxClients);
        m_identity = identity;
        m_sessionId = makeSessionId();
        m_sequence = 0;
        m_hasSnapshot = false;

        boost::system::error_code error;
        const auto address = asio::ip::make_address(m_config.bindAddress, error);
        if (error) {
            return failStart("invalid bind address: " + error.message(), errorMessage);
        }
        m_acceptor = std::make_unique<tcp::acceptor>(m_context);
        m_acceptor->open(tcp::v4(), error);
        if (!error) {
            m_acceptor->set_option(tcp::acceptor::reuse_address(true), error);
        }
        if (!error) {
            m_acceptor->bind({address, m_config.port}, error);
        }
        if (!error) {
            m_acceptor->listen(asio::socket_base::max_listen_connections, error);
        }
        if (error) {
            return failStart("listen failed: " + error.message(), errorMessage);
        }

        m_boundPort.store(m_acceptor->local_endpoint().port());
        m_heartbeatTimer = std::make_unique<asio::steady_timer>(m_context);
        acceptNext();
        scheduleHeartbeat();
        m_thread = std::thread([this]() { m_context.run(); });
        if (errorMessage != nullptr) {
            errorMessage->clear();
        }
        return true;
    }

    void publishSnapshot(wire::VisualizationSnapshot snapshot)
    {
        if (!m_running.load()) {
            return;
        }
        asio::post(m_context, [this, snapshot = std::move(snapshot)]() mutable {
            snapshot.set_sequence(++m_sequence);
            snapshot.set_server_time_ns(systemNowNs());
            snapshot.set_session_id(m_sessionId);
            snapshot.mutable_source()->CopyFrom(m_identity.source);
            m_latestSnapshot.Swap(&snapshot);
            m_hasSnapshot = true;
            sendLatestSnapshotToReadyClients();
        });
    }

    void stop()
    {
        if (!m_running.exchange(false)) {
            return;
        }

        auto completed = std::make_shared<std::promise<void>>();
        auto future = completed->get_future();
        asio::post(m_context, [this, completed]() {
            boost::system::error_code ignored;
            if (m_heartbeatTimer) {
                m_heartbeatTimer->cancel(ignored);
            }
            if (m_acceptor) {
                m_acceptor->close(ignored);
            }
            std::vector<std::shared_ptr<TcpSession>> sessions;
            sessions.reserve(m_clients.size());
            for (const auto& item : m_clients) {
                sessions.push_back(item.second.session);
            }
            m_clients.clear();
            m_clientCount.store(0);
            for (const auto& session : sessions) {
                session->close();
            }
            completed->set_value();
        });
        future.wait();
        m_context.stop();
        if (m_thread.joinable()) {
            m_thread.join();
        }
        m_acceptor.reset();
        m_heartbeatTimer.reset();
        m_boundPort.store(0);
    }

    std::size_t clientCount() const { return m_clientCount.load(); }
    std::uint16_t boundPort() const { return m_boundPort.load(); }
    std::string sessionId() const { return m_sessionId; }

private:
    struct ClientState {
        std::shared_ptr<TcpSession> session;
        std::chrono::steady_clock::time_point lastReceive;
        bool ready{false};
    };

    bool failStart(const std::string& message, std::string* errorMessage)
    {
        m_acceptor.reset();
        m_running.store(false);
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        return false;
    }

    // 与 Tcptest demo 一样：一次 async_accept 只接收一个连接，回调末尾继续登记下一次。
    void acceptNext()
    {
        m_acceptor->async_accept(
            [this](const boost::system::error_code& error, tcp::socket socket) {
                if (!error && m_clients.size() < m_config.maxClients) {
                    const auto id = ++m_nextClientId;
                    auto session = std::make_shared<TcpSession>(
                        std::move(socket), id,
                        [this](std::uint64_t clientId, const wire::Envelope& envelope) {
                            handleEnvelope(clientId, envelope);
                        },
                        [this](std::uint64_t clientId) { removeClient(clientId); });
                    m_clients.emplace(
                        id, ClientState{session, std::chrono::steady_clock::now(), false});
                    m_clientCount.store(m_clients.size());
                    session->start();
                } else if (!error) {
                    boost::system::error_code ignored;
                    socket.close(ignored);
                }
                if (m_running.load() && m_acceptor && m_acceptor->is_open()) {
                    acceptNext();
                }
            });
    }

    void handleEnvelope(std::uint64_t clientId, const wire::Envelope& envelope)
    {
        auto iter = m_clients.find(clientId);
        if (iter == m_clients.end()) {
            return;
        }
        auto& client = iter->second;
        client.lastReceive = std::chrono::steady_clock::now();

        if (envelope.has_client_hello()) {
            const auto& hello = envelope.client_hello();
            if (!wire::isProtocolMajorCompatible(hello.protocol_major())) {
                sendError(client, "protocol major version is incompatible", true);
                return;
            }
            client.ready = true;
            wire::Envelope responseEnvelope;
            auto* response = responseEnvelope.mutable_server_hello();
            response->set_server_name(m_identity.serverName);
            response->set_server_version(m_identity.serverVersion);
            response->set_protocol_major(wire::kProtocolMajor);
            response->set_protocol_minor(wire::kProtocolMinor);
            response->set_session_id(m_sessionId);
            response->mutable_source()->CopyFrom(m_identity.source);
            for (const auto capability : m_identity.source.capability()) {
                response->add_capability(static_cast<wire::Capability>(capability));
            }
            client.session->send(responseEnvelope);
            if (m_hasSnapshot) {
                sendSnapshot(client);
            }
            return;
        }

        if (envelope.has_heartbeat() && client.ready) {
            if (envelope.heartbeat().has_session_id()
                && !envelope.heartbeat().session_id().empty()
                && envelope.heartbeat().session_id() != m_sessionId) {
                sendError(client, "heartbeat session_id does not match server session", true);
            }
            return;
        }
        sendError(client, "ClientHello is required before other messages", false);
    }

    void sendError(ClientState& client, const std::string& message, bool fatal)
    {
        wire::Envelope envelope;
        auto* error = envelope.mutable_error();
        error->set_code(1);
        error->set_message(message);
        error->set_fatal(fatal);
        client.session->send(envelope);
        if (fatal) {
            client.session->closeAfterOutput();
        }
    }

    void sendSnapshot(ClientState& client)
    {
        wire::Envelope envelope;
        envelope.mutable_snapshot()->CopyFrom(m_latestSnapshot);
        client.session->send(envelope, true);
    }

    void sendLatestSnapshotToReadyClients()
    {
        for (auto& item : m_clients) {
            if (item.second.ready) {
                sendSnapshot(item.second);
            }
        }
    }

    void scheduleHeartbeat()
    {
        m_heartbeatTimer->expires_after(m_config.heartbeatInterval);
        m_heartbeatTimer->async_wait([this](const boost::system::error_code& error) {
            if (error || !m_running.load()) {
                return;
            }
            const auto now = std::chrono::steady_clock::now();
            std::vector<std::shared_ptr<TcpSession>> timedOut;
            for (auto& item : m_clients) {
                auto& client = item.second;
                if (now - client.lastReceive > m_config.clientTimeout) {
                    timedOut.push_back(client.session);
                    continue;
                }
                if (client.ready) {
                    wire::Envelope envelope;
                    auto* heartbeat = envelope.mutable_heartbeat();
                    heartbeat->set_sequence(++m_sequence);
                    heartbeat->set_send_time_ns(systemNowNs());
                    heartbeat->set_session_id(m_sessionId);
                    client.session->send(envelope);
                }
            }
            for (const auto& session : timedOut) {
                session->close();
            }
            scheduleHeartbeat();
        });
    }

    void removeClient(std::uint64_t clientId)
    {
        if (m_clients.erase(clientId) != 0U) {
            m_clientCount.store(m_clients.size());
        }
    }

    asio::io_context m_context;
    std::unique_ptr<tcp::acceptor> m_acceptor;
    std::unique_ptr<asio::steady_timer> m_heartbeatTimer;
    std::thread m_thread;
    std::unordered_map<std::uint64_t, ClientState> m_clients;
    VisualizationServerConfig m_config;
    VisualizationServerIdentity m_identity;
    wire::VisualizationSnapshot m_latestSnapshot;
    std::atomic<bool> m_running{false};
    std::atomic<std::size_t> m_clientCount{0};
    std::atomic<std::uint16_t> m_boundPort{0};
    std::string m_sessionId;
    std::uint64_t m_nextClientId{0};
    std::uint64_t m_sequence{0};
    bool m_hasSnapshot{false};
};

VisualizationServer::VisualizationServer() : m_impl(std::make_unique<Impl>()) {}
VisualizationServer::~VisualizationServer() { stop(); }

bool VisualizationServer::start(const VisualizationServerConfig& config,
                                const VisualizationServerIdentity& identity,
                                std::string* errorMessage)
{
    return m_impl->start(config, identity, errorMessage);
}

void VisualizationServer::publishSnapshot(const wire::VisualizationSnapshot& snapshot)
{
    m_impl->publishSnapshot(snapshot);
}

void VisualizationServer::stop() { m_impl->stop(); }
std::size_t VisualizationServer::clientCount() const { return m_impl->clientCount(); }
std::uint16_t VisualizationServer::boundPort() const { return m_impl->boundPort(); }
std::string VisualizationServer::sessionId() const { return m_impl->sessionId(); }

}  // namespace autoviz_server
