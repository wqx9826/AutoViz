#include <gtest/gtest.h>

#include <chrono>
#include <deque>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>

#include "autoviz/FrameCodec.h"
#include "autoviz/ProtocolVersion.h"
#include "autoviz_server/VisualizationServer.h"

namespace {

using namespace std::chrono_literals;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

autoviz_server::VisualizationServerIdentity testIdentity()
{
    autoviz_server::VisualizationServerIdentity identity;
    identity.serverName = "test-server";
    identity.serverVersion = "2.0-test";
    identity.source.set_source_id("test-adapter");
    identity.source.set_description("loopback test adapter");
    identity.source.add_capability(autoviz::CAPABILITY_COMMON_PLANNING_CONTROL);
    identity.source.add_capability(autoviz::CAPABILITY_VERTICAL_MOTION);
    return identity;
}

autoviz_server::VisualizationServerConfig testConfig()
{
    autoviz_server::VisualizationServerConfig config;
    config.bindAddress = "127.0.0.1";
    config.port = 0;
    config.heartbeatInterval = 30ms;
    config.clientTimeout = 500ms;
    return config;
}

class TestClient final {
public:
    bool connect(std::uint16_t port)
    {
        boost::system::error_code error;
        m_socket.connect({asio::ip::make_address("127.0.0.1"), port}, error);
        if (error) {
            return false;
        }
        m_socket.non_blocking(true, error);
        return !error;
    }

    bool send(const autoviz::Envelope& envelope, std::size_t firstChunk = 0)
    {
        autoviz::FrameBytes frame;
        std::string error;
        if (!autoviz::encodeFrame(envelope, frame, error)) {
            return false;
        }
        boost::system::error_code ioError;
        if (firstChunk > 0 && firstChunk < frame.size()) {
            asio::write(m_socket, asio::buffer(frame.data(), firstChunk), ioError);
            if (ioError) return false;
            asio::write(m_socket,
                        asio::buffer(frame.data() + firstChunk, frame.size() - firstChunk),
                        ioError);
        } else {
            asio::write(m_socket, asio::buffer(frame), ioError);
        }
        return !ioError;
    }

    bool read(autoviz::Envelope& envelope,
              std::chrono::milliseconds timeout = 1000ms)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (!m_messages.empty()) {
                envelope = std::move(m_messages.front());
                m_messages.pop_front();
                return true;
            }
            std::array<char, 8192> bytes{};
            boost::system::error_code error;
            const auto size = m_socket.read_some(asio::buffer(bytes), error);
            if (!error) {
                std::vector<autoviz::Envelope> decoded;
                std::string decodeError;
                if (!m_decoder.decode(std::string_view(bytes.data(), size), decoded, decodeError)) {
                    return false;
                }
                for (auto& item : decoded) {
                    m_messages.push_back(std::move(item));
                }
                continue;
            }
            if (error != asio::error::would_block && error != asio::error::try_again) {
                return false;
            }
            std::this_thread::sleep_for(2ms);
        }
        return false;
    }

    template <typename Predicate>
    bool readUntil(autoviz::Envelope& envelope,
                   Predicate predicate,
                   std::chrono::milliseconds timeout = 1000ms)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            autoviz::Envelope item;
            if (!read(item, 50ms)) {
                continue;
            }
            if (predicate(item)) {
                envelope = std::move(item);
                return true;
            }
        }
        return false;
    }

private:
    asio::io_context m_context;
    tcp::socket m_socket{m_context};
    autoviz::FrameDecoder m_decoder;
    std::deque<autoviz::Envelope> m_messages;
};

autoviz::Envelope hello(std::uint32_t major = autoviz::kProtocolMajor)
{
    autoviz::Envelope envelope;
    auto* request = envelope.mutable_client_hello();
    request->set_client_name("loopback-client");
    request->set_client_version("test");
    request->set_protocol_major(major);
    request->set_protocol_minor(autoviz::kProtocolMinor);
    return envelope;
}

bool waitForClientCount(const autoviz_server::VisualizationServer& server,
                        std::size_t expected,
                        std::chrono::milliseconds timeout = 1000ms)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (server.clientCount() == expected) return true;
        std::this_thread::sleep_for(2ms);
    }
    return false;
}

TEST(VisualizationServerTest, RejectsInvalidListenAddress)
{
    autoviz_server::VisualizationServer server;
    auto config = testConfig();
    config.bindAddress = "not-an-ip-address";
    std::string error;
    EXPECT_FALSE(server.start(config, testIdentity(), &error));
    EXPECT_FALSE(error.empty());
}

TEST(VisualizationServerTest, RequiresHelloThenSendsHelloAndLatestSnapshot)
{
    autoviz_server::VisualizationServer server;
    std::string startError;
    ASSERT_TRUE(server.start(testConfig(), testIdentity(), &startError)) << startError;
    ASSERT_NE(server.boundPort(), 0);

    autoviz::VisualizationSnapshot snapshot;
    snapshot.mutable_vehicle_state()->mutable_position()->set_x_m(12.5);
    auto* finalTargets = snapshot.mutable_final_targets();
    finalTargets->set_source_task_id(7);
    finalTargets->set_mine_number(1);
    auto* finalTarget = finalTargets->add_target();
    finalTarget->set_target_id(42);
    finalTarget->set_final_class(1);
    finalTarget->mutable_reference_point()->set_x_m(3.0);
    finalTarget->mutable_reference_point()->set_y_m(-2.0);
    finalTarget->set_radius_m(1.5);
    server.publishSnapshot(snapshot);

    TestClient client;
    ASSERT_TRUE(client.connect(server.boundPort()));
    ASSERT_TRUE(waitForClientCount(server, 1));
    autoviz::Envelope received;
    EXPECT_FALSE(client.read(received, 80ms));

    ASSERT_TRUE(client.send(hello(), 1));
    ASSERT_TRUE(client.readUntil(received, [](const auto& item) {
        return item.has_server_hello();
    }));
    EXPECT_EQ(received.server_hello().protocol_major(), autoviz::kProtocolMajor);
    EXPECT_EQ(received.server_hello().capability_size(), 2);
    const auto session = received.server_hello().session_id();
    EXPECT_FALSE(session.empty());

    ASSERT_TRUE(client.readUntil(received, [](const auto& item) {
        return item.has_snapshot();
    }));
    EXPECT_EQ(received.snapshot().session_id(), session);
    EXPECT_DOUBLE_EQ(received.snapshot().vehicle_state().position().x_m(), 12.5);
    ASSERT_TRUE(received.snapshot().has_final_targets());
    EXPECT_EQ(received.snapshot().final_targets().source_task_id(), 7U);
    ASSERT_EQ(received.snapshot().final_targets().target_size(), 1);
    EXPECT_DOUBLE_EQ(received.snapshot().final_targets().target(0).radius_m(), 1.5);
    server.stop();
}

TEST(VisualizationServerTest, RejectsIncompatibleMajorAndClosesClient)
{
    autoviz_server::VisualizationServer server;
    ASSERT_TRUE(server.start(testConfig(), testIdentity()));
    TestClient client;
    ASSERT_TRUE(client.connect(server.boundPort()));
    ASSERT_TRUE(client.send(hello(autoviz::kProtocolMajor + 1)));

    autoviz::Envelope received;
    ASSERT_TRUE(client.readUntil(received, [](const auto& item) {
        return item.has_error();
    }));
    EXPECT_TRUE(received.error().fatal());
    EXPECT_TRUE(waitForClientCount(server, 0));
    server.stop();
}

TEST(VisualizationServerTest, SupportsMultipleClientsAndEnforcesLimit)
{
    autoviz_server::VisualizationServer server;
    auto config = testConfig();
    config.maxClients = 2;
    ASSERT_TRUE(server.start(config, testIdentity()));

    TestClient first;
    TestClient second;
    TestClient rejected;
    ASSERT_TRUE(first.connect(server.boundPort()));
    ASSERT_TRUE(second.connect(server.boundPort()));
    ASSERT_TRUE(rejected.connect(server.boundPort()));
    ASSERT_TRUE(waitForClientCount(server, 2));
    ASSERT_TRUE(first.send(hello()));
    ASSERT_TRUE(second.send(hello()));

    autoviz::VisualizationSnapshot snapshot;
    snapshot.mutable_task_state()->set_task_id(7);
    server.publishSnapshot(snapshot);
    for (auto* client : {&first, &second}) {
        autoviz::Envelope received;
        ASSERT_TRUE(client->readUntil(received, [](const auto& item) {
            return item.has_snapshot();
        }));
        EXPECT_EQ(received.snapshot().task_state().task_id(), 7);
    }
    EXPECT_EQ(server.clientCount(), 2U);
    server.stop();
}

TEST(VisualizationServerTest, SendsHeartbeatAndTimesOutSilentClient)
{
    autoviz_server::VisualizationServer server;
    auto config = testConfig();
    config.clientTimeout = 120ms;
    ASSERT_TRUE(server.start(config, testIdentity()));
    TestClient client;
    ASSERT_TRUE(client.connect(server.boundPort()));
    ASSERT_TRUE(client.send(hello()));

    autoviz::Envelope received;
    ASSERT_TRUE(client.readUntil(received, [](const auto& item) {
        return item.has_heartbeat();
    }));
    EXPECT_EQ(received.heartbeat().session_id(), server.sessionId());
    EXPECT_TRUE(waitForClientCount(server, 0, 500ms));
    server.stop();
}

TEST(VisualizationServerTest, RestartCreatesNewSession)
{
    autoviz_server::VisualizationServer server;
    auto config = testConfig();
    ASSERT_TRUE(server.start(config, testIdentity()));
    const auto firstSession = server.sessionId();
    server.stop();
    ASSERT_TRUE(server.start(config, testIdentity()));
    EXPECT_FALSE(server.sessionId().empty());
    EXPECT_NE(server.sessionId(), firstSession);
    server.stop();
}

TEST(VisualizationServerTest, SlowClientEventuallyReceivesNewestSnapshot)
{
    autoviz_server::VisualizationServer server;
    auto config = testConfig();
    config.clientTimeout = 5s;
    ASSERT_TRUE(server.start(config, testIdentity()));
    TestClient client;
    ASSERT_TRUE(client.connect(server.boundPort()));
    ASSERT_TRUE(client.send(hello()));

    for (int index = 0; index < 20; ++index) {
        autoviz::VisualizationSnapshot snapshot;
        snapshot.mutable_vehicle_state()->mutable_position()->set_x_m(index);
        snapshot.mutable_source()->set_description(std::string(256 * 1024, 'x'));
        server.publishSnapshot(snapshot);
    }

    autoviz::Envelope received;
    bool foundNewest = false;
    const auto deadline = std::chrono::steady_clock::now() + 4s;
    while (std::chrono::steady_clock::now() < deadline && client.read(received, 200ms)) {
        if (received.has_snapshot()
            && received.snapshot().vehicle_state().position().x_m() == 19.0) {
            foundNewest = true;
            break;
        }
    }
    EXPECT_TRUE(foundNewest);
    server.stop();
}

}  // namespace
