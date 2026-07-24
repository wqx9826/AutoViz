#include "autoviz_server/TcpServer.h"

#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <utility>

#include <boost/asio.hpp>

#include "autoviz/FrameCodec.h"

namespace autoviz_server {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
namespace wire = ::autoviz;

class TcpServer::Impl {
public:
    class Session : public std::enable_shared_from_this<Session> {
    public:
        Session(tcp::socket socket, Impl* server, std::uint64_t id)
            : m_socket(std::move(socket))
            , m_server(server)
            , m_id(id)
            , m_lastReceive(std::chrono::steady_clock::now())
        {
        }

        void start()
        {
            readHeader();
        }

        void stop()
        {
            boost::system::error_code ignored;
            m_socket.shutdown(tcp::socket::shutdown_both, ignored);
            m_socket.close(ignored);
        }

        void send(const wire::Envelope& envelope)
        {
            auto frame = wire::encodeFrame(envelope);
            if (frame.empty()) {
                return;
            }
            const bool writeInProgress = !m_writeQueue.empty();
            m_writeQueue.push_back(std::move(frame));
            if (!writeInProgress) {
                writeNext();
            }
        }

        bool accepts(wire::ChannelId channel) const
        {
            return m_subscribeAll || m_channels.count(channel) != 0U;
        }

        bool timedOut(std::chrono::steady_clock::time_point now) const
        {
            return now - m_lastReceive > std::chrono::seconds(5);
        }

    private:
        void readHeader()
        {
            auto self = shared_from_this();
            asio::async_read(
                m_socket,
                asio::buffer(m_header),
                [self](const boost::system::error_code& error, std::size_t) {
                    if (error) {
                        self->m_server->removeSession(self->m_id);
                        return;
                    }
                    const auto* bytes = reinterpret_cast<const unsigned char*>(self->m_header.data());
                    const std::uint32_t size = (static_cast<std::uint32_t>(bytes[0]) << 24U)
                                               | (static_cast<std::uint32_t>(bytes[1]) << 16U)
                                               | (static_cast<std::uint32_t>(bytes[2]) << 8U)
                                               | static_cast<std::uint32_t>(bytes[3]);
                    if (size == 0U || size > wire::kMaxFrameSize) {
                        self->stop();
                        self->m_server->removeSession(self->m_id);
                        return;
                    }
                    self->m_payload.resize(size);
                    self->readPayload();
                });
        }

        void readPayload()
        {
            auto self = shared_from_this();
            asio::async_read(
                m_socket,
                asio::buffer(m_payload),
                [self](const boost::system::error_code& error, std::size_t) {
                    if (error) {
                        self->m_server->removeSession(self->m_id);
                        return;
                    }
                    wire::Envelope envelope;
                    if (!envelope.ParseFromArray(self->m_payload.data(),
                                                 static_cast<int>(self->m_payload.size()))) {
                        self->stop();
                        self->m_server->removeSession(self->m_id);
                        return;
                    }
                    self->m_lastReceive = std::chrono::steady_clock::now();
                    self->handle(envelope);
                    self->readHeader();
                });
        }

        void handle(const wire::Envelope& envelope)
        {
            if (envelope.has_client_hello()) {
                wire::Envelope response;
                auto* hello = response.mutable_server_hello();
                hello->set_server_name("AutoViz Server");
                hello->set_server_version("0.3.0");
                hello->set_protocol_major(1);
                hello->set_protocol_minor(0);
                const auto snapshot = m_server->snapshot();
                hello->set_session_id(snapshot.session_id());
                if (snapshot.has_source()) {
                    hello->mutable_source()->CopyFrom(snapshot.source());
                }
                for (int channel = wire::CHANNEL_VEHICLE_STATE;
                     channel <= wire::CHANNEL_VEHICLE_PARAMETERS;
                     ++channel) {
                    hello->add_available_channel(static_cast<wire::ChannelId>(channel));
                }
                send(response);
                return;
            }

            if (envelope.has_subscribe_request()) {
                m_channels.clear();
                m_subscribeAll = envelope.subscribe_request().channel_size() == 0;
                for (const auto channel : envelope.subscribe_request().channel()) {
                    m_channels.insert(static_cast<wire::ChannelId>(channel));
                }
                if (envelope.subscribe_request().request_full_snapshot()) {
                    wire::Envelope response;
                    response.mutable_snapshot()->CopyFrom(m_server->snapshot());
                    send(response);
                }
                return;
            }
        }

        void writeNext()
        {
            auto self = shared_from_this();
            asio::async_write(
                m_socket,
                asio::buffer(m_writeQueue.front()),
                [self](const boost::system::error_code& error, std::size_t) {
                    if (error) {
                        self->m_server->removeSession(self->m_id);
                        return;
                    }
                    self->m_writeQueue.pop_front();
                    if (!self->m_writeQueue.empty()) {
                        self->writeNext();
                    }
                });
        }

        tcp::socket m_socket;
        Impl* m_server;
        std::uint64_t m_id;
        std::array<char, wire::kFrameHeaderSize> m_header{};
        std::vector<char> m_payload;
        std::deque<std::string> m_writeQueue;
        std::set<wire::ChannelId> m_channels;
        bool m_subscribeAll = true;
        std::chrono::steady_clock::time_point m_lastReceive;
    };

    void start(const std::string& bindAddress,
               std::uint16_t port,
               std::size_t maxClients,
               SnapshotProvider snapshotProvider)
    {
        if (m_running.exchange(true)) {
            return;
        }
        m_maxClients = maxClients;
        m_snapshotProvider = std::move(snapshotProvider);
        const auto endpoint = tcp::endpoint(asio::ip::make_address(bindAddress), port);
        m_acceptor = std::make_unique<tcp::acceptor>(m_context);
        m_acceptor->open(endpoint.protocol());
        m_acceptor->set_option(tcp::acceptor::reuse_address(true));
        m_acceptor->bind(endpoint);
        m_acceptor->listen();
        acceptNext();
        m_thread = std::thread([this]() { m_context.run(); });
    }

    void stop()
    {
        if (!m_running.exchange(false)) {
            return;
        }
        asio::post(m_context, [this]() {
            if (m_acceptor) {
                boost::system::error_code ignored;
                m_acceptor->close(ignored);
            }
            for (auto& item : m_sessions) {
                item.second->stop();
            }
            m_sessions.clear();
        });
        m_context.stop();
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    void broadcast(const wire::Envelope& envelope)
    {
        asio::post(m_context, [this, envelope]() {
            const auto channel = envelope.has_channel_update()
                                     ? envelope.channel_update().channel()
                                     : wire::CHANNEL_UNKNOWN;
            for (auto& item : m_sessions) {
                if (channel == wire::CHANNEL_UNKNOWN || item.second->accepts(channel)) {
                    item.second->send(envelope);
                }
            }
        });
    }

    void pruneAndHeartbeat(const wire::Envelope& envelope)
    {
        asio::post(m_context, [this, envelope]() {
            const auto now = std::chrono::steady_clock::now();
            for (auto iter = m_sessions.begin(); iter != m_sessions.end();) {
                if (iter->second->timedOut(now)) {
                    iter->second->stop();
                    iter = m_sessions.erase(iter);
                } else {
                    iter->second->send(envelope);
                    ++iter;
                }
            }
            m_clientCount.store(m_sessions.size());
        });
    }

    wire::VisualizationSnapshot snapshot() const
    {
        return m_snapshotProvider ? m_snapshotProvider() : wire::VisualizationSnapshot{};
    }

    std::size_t clientCount() const
    {
        return m_clientCount.load();
    }

    void removeSession(std::uint64_t id)
    {
        asio::post(m_context, [this, id]() {
            m_sessions.erase(id);
            m_clientCount.store(m_sessions.size());
        });
    }

private:
    void acceptNext()
    {
        m_acceptor->async_accept([this](const boost::system::error_code& error, tcp::socket socket) {
            if (!error && m_sessions.size() < m_maxClients) {
                const auto id = ++m_nextSessionId;
                auto session = std::make_shared<Session>(std::move(socket), this, id);
                m_sessions.emplace(id, session);
                m_clientCount.store(m_sessions.size());
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

    asio::io_context m_context;
    std::unique_ptr<tcp::acceptor> m_acceptor;
    std::thread m_thread;
    std::unordered_map<std::uint64_t, std::shared_ptr<Session>> m_sessions;
    SnapshotProvider m_snapshotProvider;
    std::atomic<bool> m_running{false};
    std::atomic<std::size_t> m_clientCount{0};
    std::size_t m_maxClients = 8;
    std::uint64_t m_nextSessionId = 0;

    friend class Session;
};

TcpServer::TcpServer()
    : m_impl(std::make_unique<Impl>())
{
}

TcpServer::~TcpServer()
{
    stop();
}

void TcpServer::start(const std::string& bindAddress,
                      std::uint16_t port,
                      std::size_t maxClients,
                      SnapshotProvider snapshotProvider)
{
    m_impl->start(bindAddress, port, maxClients, std::move(snapshotProvider));
}

void TcpServer::stop()
{
    m_impl->stop();
}

void TcpServer::broadcast(const wire::Envelope& envelope)
{
    m_impl->broadcast(envelope);
}

void TcpServer::broadcastHeartbeat(std::uint64_t sequence,
                                   std::uint64_t sendTimeNs,
                                   const std::string& sessionId)
{
    wire::Envelope envelope;
    auto* heartbeat = envelope.mutable_heartbeat();
    heartbeat->set_sequence(sequence);
    heartbeat->set_send_time_ns(sendTimeNs);
    heartbeat->set_session_id(sessionId);
    m_impl->pruneAndHeartbeat(envelope);
}

std::size_t TcpServer::clientCount() const
{
    return m_impl->clientCount();
}

}  // namespace autoviz_server
