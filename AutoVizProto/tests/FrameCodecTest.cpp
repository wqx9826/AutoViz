#include <gtest/gtest.h>

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "autoviz/FrameCodec.h"
#include "autoviz/ProtocolVersion.h"

namespace {

namespace wire = autoviz;

wire::Envelope heartbeatEnvelope(std::uint64_t sequence)
{
    wire::Envelope envelope;
    envelope.mutable_heartbeat()->set_sequence(sequence);
    return envelope;
}

wire::FrameBytes encode(const wire::Envelope& envelope)
{
    wire::FrameBytes frame;
    std::string error;
    EXPECT_TRUE(wire::encodeFrame(envelope, frame, error)) << error;
    return frame;
}

TEST(FrameCodecTest, RoundTripsSingleBinaryFrame)
{
    wire::Envelope expected;
    expected.mutable_client_hello()->set_client_name(std::string("A\0B", 3));
    expected.mutable_client_hello()->set_protocol_major(wire::kProtocolMajor);
    const auto frame = encode(expected);
    ASSERT_GT(frame.size(), wire::kFrameHeaderSize);
    EXPECT_NE(frame.find('\0'), std::string::npos);

    wire::FrameDecoder decoder;
    std::vector<wire::Envelope> decoded;
    std::string error;
    ASSERT_TRUE(decoder.decode(frame, decoded, error)) << error;
    ASSERT_EQ(decoded.size(), 1U);
    EXPECT_EQ(decoded.front().client_hello().client_name(), std::string("A\0B", 3));
}

TEST(FrameCodecTest, HandlesFrameSplitAtEveryByte)
{
    const auto frame = encode(heartbeatEnvelope(7));
    wire::FrameDecoder decoder;
    std::vector<wire::Envelope> decoded;
    std::string error;
    for (const char byte : frame) {
        ASSERT_TRUE(decoder.decode(std::string_view(&byte, 1), decoded, error)) << error;
    }
    ASSERT_EQ(decoded.size(), 1U);
    EXPECT_EQ(decoded.front().heartbeat().sequence(), 7U);
}

TEST(FrameCodecTest, HandlesCoalescedFrames)
{
    const auto stream = encode(heartbeatEnvelope(1)) + encode(heartbeatEnvelope(2))
                        + encode(heartbeatEnvelope(3));
    wire::FrameDecoder decoder;
    std::vector<wire::Envelope> decoded;
    std::string error;
    ASSERT_TRUE(decoder.decode(stream, decoded, error)) << error;
    ASSERT_EQ(decoded.size(), 3U);
    EXPECT_EQ(decoded[0].heartbeat().sequence(), 1U);
    EXPECT_EQ(decoded[1].heartbeat().sequence(), 2U);
    EXPECT_EQ(decoded[2].heartbeat().sequence(), 3U);
}

TEST(FrameCodecTest, RejectsZeroAndOversizedFrames)
{
    const std::array<char, 4> zeroLength = {0, 0, 0, 0};
    const std::array<char, 4> oversized = {0x01, 0, 0, 0x01};
    wire::FrameDecoder decoder;
    std::vector<wire::Envelope> decoded;
    std::string error;
    EXPECT_FALSE(decoder.decode(
        std::string_view(zeroLength.data(), zeroLength.size()), decoded, error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(decoder.decode(
        std::string_view(oversized.data(), oversized.size()), decoded, error));
    EXPECT_FALSE(error.empty());
}

TEST(FrameCodecTest, RejectsMalformedPayloadAndRecovers)
{
    const std::array<char, 5> malformed = {0, 0, 0, 1, 0};
    wire::FrameDecoder decoder;
    std::vector<wire::Envelope> decoded;
    std::string error;
    EXPECT_FALSE(decoder.decode(
        std::string_view(malformed.data(), malformed.size()), decoded, error));
    EXPECT_FALSE(error.empty());

    const auto valid = encode(heartbeatEnvelope(99));
    ASSERT_TRUE(decoder.decode(valid, decoded, error)) << error;
    ASSERT_EQ(decoded.size(), 1U);
    EXPECT_EQ(decoded.front().heartbeat().sequence(), 99U);
}

TEST(FrameCodecTest, ResetDiscardsIncompleteFrame)
{
    const auto incomplete = encode(heartbeatEnvelope(1));
    const auto complete = encode(heartbeatEnvelope(2));
    wire::FrameDecoder decoder;
    std::vector<wire::Envelope> decoded;
    std::string error;
    ASSERT_TRUE(decoder.decode(
        std::string_view(incomplete.data(), incomplete.size() - 1), decoded, error));
    EXPECT_TRUE(decoded.empty());

    decoder.reset();
    ASSERT_TRUE(decoder.decode(complete, decoded, error)) << error;
    ASSERT_EQ(decoded.size(), 1U);
    EXPECT_EQ(decoded.front().heartbeat().sequence(), 2U);
}

TEST(ProtocolEnvelopeTest, PreservesV2HandshakeCapabilitiesAndFullSnapshot)
{
    std::vector<wire::Envelope> expected(3);
    expected[0].mutable_client_hello()->set_protocol_major(wire::kProtocolMajor);
    auto* hello = expected[1].mutable_server_hello();
    hello->set_session_id("session-v2");
    hello->add_capability(wire::CAPABILITY_COMMON_PLANNING_CONTROL);
    hello->add_capability(wire::CAPABILITY_UNDERWATER_SYSTEM);

    auto* snapshot = expected[2].mutable_snapshot();
    snapshot->set_session_id("session-v2");
    snapshot->mutable_vehicle_state()->set_speed_mps(1.5);
    snapshot->mutable_vehicle_state()->mutable_underwater()->set_depth_m(3.2);
    auto* underwater = snapshot->mutable_chassis_state()->mutable_underwater();
    underwater->set_water_tank_state(wire::WATER_TANK_STATE_FILLING);
    underwater->set_emergency_ascent_active(true);
    auto* platform = snapshot->mutable_chassis_state()->mutable_platform();
    platform->mutable_battery()->set_pack_voltage_v(312.5);
    platform->add_power_channel()->set_status(1);
    snapshot->mutable_control_command()->mutable_underwater()->set_emergency_ascent(true);
    snapshot->mutable_task_state()
        ->mutable_underwater()
        ->set_release_emergency_ascent(true);
    auto* topic = snapshot->mutable_runtime_state()->add_topic();
    topic->set_name("/location");
    topic->set_data_kind(wire::DATA_KIND_VEHICLE_STATE);
    auto* event = snapshot->add_control_state_event();
    event->set_source(wire::ControlStateEvent::SOURCE_CONTROL_COMMAND);
    event->set_previous_mode(11);
    event->set_current_mode(6);
    event->set_goal_id("goal-v2.1");

    wire::FrameBytes stream;
    for (const auto& envelope : expected) {
        stream += encode(envelope);
    }

    wire::FrameDecoder decoder;
    std::vector<wire::Envelope> decoded;
    std::string error;
    ASSERT_TRUE(decoder.decode(stream, decoded, error)) << error;
    ASSERT_EQ(decoded.size(), expected.size());
    EXPECT_EQ(decoded[0].client_hello().protocol_major(), 2U);
    EXPECT_EQ(decoded[1].server_hello().capability(1),
              wire::CAPABILITY_UNDERWATER_SYSTEM);
    const auto& actual = decoded[2].snapshot();
    EXPECT_DOUBLE_EQ(actual.vehicle_state().underwater().depth_m(), 3.2);
    EXPECT_EQ(actual.chassis_state().underwater().water_tank_state(),
              wire::WATER_TANK_STATE_FILLING);
    EXPECT_DOUBLE_EQ(actual.chassis_state().platform().battery().pack_voltage_v(), 312.5);
    EXPECT_TRUE(actual.control_command().underwater().emergency_ascent());
    EXPECT_TRUE(actual.task_state().underwater().release_emergency_ascent());
    EXPECT_EQ(actual.runtime_state().topic(0).data_kind(), wire::DATA_KIND_VEHICLE_STATE);
    ASSERT_EQ(actual.control_state_event_size(), 1);
    EXPECT_EQ(actual.control_state_event(0).previous_mode(), 11);
    EXPECT_EQ(actual.control_state_event(0).current_mode(), 6);
    EXPECT_EQ(actual.control_state_event(0).goal_id(), "goal-v2.1");
}

TEST(ProtocolVersionTest, AcceptsOnlyV2MajorVersion)
{
    EXPECT_EQ(wire::kProtocolMajor, 2U);
    EXPECT_EQ(wire::kProtocolMinor, 1U);
    EXPECT_TRUE(wire::isProtocolMajorCompatible(2U));
    EXPECT_FALSE(wire::isProtocolMajorCompatible(1U));
}

}  // namespace
