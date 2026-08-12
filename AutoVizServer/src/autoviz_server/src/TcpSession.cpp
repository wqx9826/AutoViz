#include "autoviz_server/TcpSession.h"

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

#include <boost/asio.hpp>

namespace autoviz_server {
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

TcpSession::TcpSession(tcp::socket socket,
                       const std::uint64_t id,
                       EnvelopeHandler onEnvelope,
                       CloseHandler onClose)
    : m_socket(std::move(socket))
    , m_id(id)
    , m_onEnvelope(std::move(onEnvelope))
    , m_onClose(std::move(onClose))
{
}

void TcpSession::start()
{
    readNext();
}

void TcpSession::send(const ::autoviz::Envelope& envelope, bool replaceableSnapshot)
{
    ::autoviz::FrameBytes frame;
    std::string encodeError;
    if (!::autoviz::encodeFrame(envelope, frame, encodeError) || m_closed) {
        return;
    }
    const bool writing = !m_writeQueue.empty();
    auto pendingSnapshot = m_writeQueue.end();
    if (replaceableSnapshot && m_writeQueue.size() > 1U) {
        // 队首可能正被 async_write 使用，绝不能修改；从其后的待发送区查找旧快照。
        pendingSnapshot = std::find_if(std::next(m_writeQueue.begin()), m_writeQueue.end(),
            [](const QueuedFrame& queued) { return queued.replaceableSnapshot; });
    }
    if (pendingSnapshot != m_writeQueue.end()) {
        pendingSnapshot->bytes = std::move(frame);
    } else {
        m_writeQueue.push_back(QueuedFrame{std::move(frame), replaceableSnapshot});
    }
    if (!writing) {
        writeNext();
    }
}

void TcpSession::closeAfterOutput()
{
    m_closeAfterOutput = true;
    if (m_writeQueue.empty()) {
        close();
    }
}

void TcpSession::close()
{
    if (m_closed) {
        return;
    }
    m_closed = true;
    boost::system::error_code ignored;
    m_socket.shutdown(tcp::socket::shutdown_both, ignored);
    m_socket.close(ignored);
    notifyClosed();
}

void TcpSession::readNext()
{
    auto self = shared_from_this();
    m_socket.async_read_some(asio::buffer(m_readBuffer),
        [self](const boost::system::error_code& error, const std::size_t size) {
            if (error) {
                self->close();
                return;
            }
            std::vector<::autoviz::Envelope> envelopes;
            std::string parseError;
            if (!self->m_decoder.decode(
                    std::string_view(self->m_readBuffer.data(), size), envelopes, parseError)) {
                self->close();
                return;
            }
            for (const auto& envelope : envelopes) {
                self->m_onEnvelope(self->m_id, envelope);
            }
            self->readNext();
        });
}

void TcpSession::writeNext()
{
    auto self = shared_from_this();
    asio::async_write(m_socket, asio::buffer(m_writeQueue.front().bytes),
        [self](const boost::system::error_code& error, std::size_t) {
            if (error) {
                self->close();
                return;
            }
            self->m_writeQueue.pop_front();
            if (!self->m_writeQueue.empty()) {
                self->writeNext();
            } else if (self->m_closeAfterOutput) {
                self->close();
            }
        });
}

void TcpSession::notifyClosed()
{
    if (m_onClose) {
        m_onClose(m_id);
    }
}

}  // namespace autoviz_server
