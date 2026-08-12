#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "autoviz/transport.pb.h"

namespace autoviz_server {

struct VisualizationServerConfig {
    std::string bindAddress{"0.0.0.0"};
    std::uint16_t port{39090};
    std::size_t maxClients{8};
    std::chrono::milliseconds heartbeatInterval{1000};
    std::chrono::milliseconds clientTimeout{5000};
};

struct VisualizationServerIdentity {
    std::string serverName{"AutoViz Server"};
    std::string serverVersion{"0.4.0"};
    ::autoviz::SourceInfo source;
};

// 面向 AutoVizServerNode 的网络外观层。Node 只需要 start/publishSnapshot/stop，
// ClientHello、协议版本、session、心跳和多客户端生命周期全部封装在内部。
class VisualizationServer final {
public:
    VisualizationServer();
    ~VisualizationServer();
    VisualizationServer(const VisualizationServer&) = delete;
    VisualizationServer& operator=(const VisualizationServer&) = delete;

    bool start(const VisualizationServerConfig& config,
               const VisualizationServerIdentity& identity,
               std::string* errorMessage = nullptr);
    void publishSnapshot(const ::autoviz::VisualizationSnapshot& snapshot);
    void stop();

    std::size_t clientCount() const;
    std::uint16_t boundPort() const;
    std::string sessionId() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace autoviz_server
