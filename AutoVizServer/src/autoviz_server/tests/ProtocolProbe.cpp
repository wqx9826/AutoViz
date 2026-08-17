#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>

#include "autoviz/FrameCodec.h"
#include "autoviz/ProtocolVersion.h"

namespace {
using namespace std::chrono_literals;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

bool sendEnvelope(tcp::socket& socket, const autoviz::Envelope& envelope)
{
    autoviz::FrameBytes frame;
    std::string error;
    if (!autoviz::encodeFrame(envelope, frame, error)) {
        std::cerr << "encode failed: " << error << '\n';
        return false;
    }
    boost::system::error_code ioError;
    asio::write(socket, asio::buffer(frame), ioError);
    if (ioError) {
        std::cerr << "write failed: " << ioError.message() << '\n';
    }
    return !ioError;
}
}  // namespace

int main(int argc, char** argv)
{
    if (argc < 4) {
        std::cerr << "usage: autoviz_protocol_probe HOST PORT SECONDS "
                     "[--require-obstacles] [--require-command-transition]\n";
        return 2;
    }
    const std::string host = argv[1];
    const auto port = static_cast<std::uint16_t>(std::stoul(argv[2]));
    const auto duration = std::chrono::seconds(std::stoi(argv[3]));
    bool requireObstacles = false;
    bool requireCommandTransition = false;
    for (int index = 4; index < argc; ++index) {
        const std::string option = argv[index];
        requireObstacles |= option == "--require-obstacles";
        requireCommandTransition |= option == "--require-command-transition";
    }

    asio::io_context context;
    tcp::socket socket(context);
    boost::system::error_code error;
    socket.connect({asio::ip::make_address(host), port}, error);
    if (error) {
        std::cerr << "connect failed: " << error.message() << '\n';
        return 3;
    }
    socket.non_blocking(true, error);

    autoviz::Envelope hello;
    auto* request = hello.mutable_client_hello();
    request->set_client_name("AutoViz protocol probe");
    request->set_client_version("2.0-test");
    request->set_protocol_major(autoviz::kProtocolMajor);
    request->set_protocol_minor(autoviz::kProtocolMinor);
    if (!sendEnvelope(socket, hello)) return 4;

    autoviz::FrameDecoder decoder;
    std::string session;
    std::uint64_t heartbeatSequence = 0;
    std::uint64_t snapshotCount = 0;
    bool gotHello = false;
    bool vehicle = false;
    bool chassis = false;
    bool control = false;
    bool globalPath = false;
    bool localPath = false;
    bool action = false;
    bool task = false;
    bool obstacles = false;
    bool commonCapability = false;
    bool verticalCapability = false;
    bool underwaterCapability = false;
    bool platformCapability = false;
    bool sawCommandExit = false;
    bool sawCommandCrawl = false;
    std::vector<autoviz::TopicStatus> latestTopics;

    const auto deadline = std::chrono::steady_clock::now() + duration;
    auto nextHeartbeat = std::chrono::steady_clock::now() + 500ms;
    while (std::chrono::steady_clock::now() < deadline) {
        if (std::chrono::steady_clock::now() >= nextHeartbeat && !session.empty()) {
            autoviz::Envelope heartbeat;
            heartbeat.mutable_heartbeat()->set_sequence(++heartbeatSequence);
            heartbeat.mutable_heartbeat()->set_session_id(session);
            if (!sendEnvelope(socket, heartbeat)) return 5;
            nextHeartbeat += 1s;
        }

        std::array<char, 65536> bytes{};
        const auto size = socket.read_some(asio::buffer(bytes), error);
        if (error == asio::error::would_block || error == asio::error::try_again) {
            error.clear();
            std::this_thread::sleep_for(2ms);
            continue;
        }
        if (error) {
            std::cerr << "read failed: " << error.message() << '\n';
            return 6;
        }
        std::vector<autoviz::Envelope> messages;
        std::string decodeError;
        if (!decoder.decode(std::string_view(bytes.data(), size), messages, decodeError)) {
            std::cerr << "decode failed: " << decodeError << '\n';
            return 7;
        }
        for (const auto& envelope : messages) {
            if (envelope.has_server_hello()) {
                gotHello = true;
                session = envelope.server_hello().session_id();
                for (const auto value : envelope.server_hello().capability()) {
                    const auto capability = static_cast<autoviz::Capability>(value);
                    commonCapability |= capability == autoviz::CAPABILITY_COMMON_PLANNING_CONTROL;
                    verticalCapability |= capability == autoviz::CAPABILITY_VERTICAL_MOTION;
                    underwaterCapability |= capability == autoviz::CAPABILITY_UNDERWATER_SYSTEM;
                    platformCapability |= capability == autoviz::CAPABILITY_PLATFORM_DIAGNOSTICS;
                }
            } else if (envelope.has_snapshot()) {
                const auto& snapshot = envelope.snapshot();
                ++snapshotCount;
                vehicle |= snapshot.has_vehicle_state();
                chassis |= snapshot.has_chassis_state();
                control |= snapshot.has_control_command();
                globalPath |= snapshot.has_global_trajectory();
                localPath |= snapshot.has_local_trajectory();
                action |= snapshot.has_action_state();
                task |= snapshot.has_task_state();
                obstacles |= snapshot.has_obstacles();
                for (const auto& event : snapshot.control_state_event()) {
                    if (event.source()
                            != autoviz::ControlStateEvent::SOURCE_CONTROL_COMMAND
                        || !event.has_previous_mode() || !event.has_current_mode()) {
                        continue;
                    }
                    sawCommandExit |= event.previous_mode() == 11
                                      && event.current_mode() == 0;
                    sawCommandCrawl |= event.previous_mode() == 0
                                       && event.current_mode() == 6;
                }
                if (snapshot.has_runtime_state()) {
                    latestTopics.assign(snapshot.runtime_state().topic().begin(),
                                        snapshot.runtime_state().topic().end());
                }
            } else if (envelope.has_error()) {
                std::cerr << "protocol error: " << envelope.error().message() << '\n';
                if (envelope.error().fatal()) return 8;
            }
        }
    }

    std::cout << "hello=" << gotHello
              << " session=" << session
              << " snapshots=" << snapshotCount
              << " capabilities=[common:" << commonCapability
              << ",vertical:" << verticalCapability
              << ",underwater:" << underwaterCapability
              << ",platform:" << platformCapability << "]\n";
    std::cout << "fields=[vehicle:" << vehicle
              << ",chassis:" << chassis
              << ",control:" << control
              << ",global_path:" << globalPath
              << ",local_path:" << localPath
              << ",action:" << action
              << ",task:" << task
              << ",obstacles:" << obstacles << "]\n";
    std::cout << "command_transitions=[11_to_0:" << sawCommandExit
              << ",0_to_6:" << sawCommandCrawl << "]\n";
    for (const auto& topic : latestTopics) {
        std::cout << "topic=" << topic.name()
                  << " count=" << topic.message_count()
                  << " hz=" << topic.frequency_hz()
                  << " timed_out=" << topic.timed_out() << '\n';
    }

    const bool sevenBagFields = vehicle && chassis && control && globalPath
                                && localPath && action && task;
    const bool expectedFields = requireObstacles ? obstacles : sevenBagFields;
    const bool expectedTransitions = !requireCommandTransition
                                     || (sawCommandExit && sawCommandCrawl);
    return gotHello && commonCapability && verticalCapability
                   && underwaterCapability && platformCapability && expectedFields
                   && expectedTransitions
               ? 0
               : 9;
}
