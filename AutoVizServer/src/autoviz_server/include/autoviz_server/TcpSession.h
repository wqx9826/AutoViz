#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>

#include <boost/asio/ip/tcp.hpp>

#include "autoviz/FrameCodec.h"

namespace autoviz_server {

// 一个客户端连接的 transport 状态。它不理解 ROS、握手或订阅语义。
class TcpSession final : public std::enable_shared_from_this<TcpSession> {
public:
    using EnvelopeHandler = std::function<void(std::uint64_t, const ::autoviz::Envelope&)>;
    using CloseHandler = std::function<void(std::uint64_t)>;

    TcpSession(boost::asio::ip::tcp::socket socket,
               std::uint64_t id,
               EnvelopeHandler onEnvelope,
               CloseHandler onClose);

    void start();
    // 协议控制帧必须排队；完整快照允许用更新的快照替换尚未发送的旧快照。
    void send(const ::autoviz::Envelope& envelope, bool replaceableSnapshot = false);
    void closeAfterOutput();
    void close();

private:
    void readNext();
    void writeNext();
    void notifyClosed();

    boost::asio::ip::tcp::socket m_socket;
    std::uint64_t m_id;
    EnvelopeHandler m_onEnvelope;
    CloseHandler m_onClose;
    std::array<char, 4096> m_readBuffer{};
    ::autoviz::FrameDecoder m_decoder;
    struct QueuedFrame {
        ::autoviz::FrameBytes bytes;
        bool replaceableSnapshot{false};
    };
    std::deque<QueuedFrame> m_writeQueue;
    bool m_closeAfterOutput{false};
    bool m_closed{false};
};

}  // namespace autoviz_server
