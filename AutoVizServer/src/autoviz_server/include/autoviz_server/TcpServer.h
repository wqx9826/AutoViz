#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "autoviz/protocol/v1/transport.pb.h"

namespace autoviz_server {

class TcpServer {
public:
    using SnapshotProvider = std::function<autoviz::protocol::v1::VisualizationSnapshot()>;

    TcpServer();
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    void start(const std::string& bindAddress,
               std::uint16_t port,
               std::size_t maxClients,
               SnapshotProvider snapshotProvider);
    void stop();
    void broadcast(const autoviz::protocol::v1::Envelope& envelope);
    void broadcastHeartbeat(std::uint64_t sequence,
                            std::uint64_t sendTimeNs,
                            const std::string& sessionId);
    std::size_t clientCount() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace autoviz_server
