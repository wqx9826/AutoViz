#include <gtest/gtest.h>

#include <array>
#include <string>
#include <vector>

#include "autoviz/FrameCodec.h"

namespace {

namespace wire = autoviz;

wire::Envelope heartbeatEnvelope(std::uint64_t sequence)
{
    wire::Envelope envelope;
    envelope.mutable_heartbeat()->set_sequence(sequence);
    return envelope;
}

TEST(FrameCodecTest, RoundTripsSingleFrame)
{
    const std::string frame = wire::encodeFrame(heartbeatEnvelope(42));
    ASSERT_FALSE(frame.empty());

    wire::FrameDecoder decoder;
    std::vector<wire::Envelope> decoded;
    std::string error;
    ASSERT_TRUE(decoder.append(frame.data(), frame.size(), &decoded, &error));
    ASSERT_EQ(decoded.size(), 1U);
    EXPECT_EQ(decoded.front().heartbeat().sequence(), 42U);
    EXPECT_TRUE(error.empty());
}

TEST(FrameCodecTest, HandlesFragmentedFrame)
{
    const std::string frame = wire::encodeFrame(heartbeatEnvelope(7));
    ASSERT_GT(frame.size(), wire::kFrameHeaderSize);

    wire::FrameDecoder decoder;
    std::vector<wire::Envelope> decoded;
    std::string error;
    for (const char byte : frame) {
        ASSERT_TRUE(decoder.append(&byte, 1U, &decoded, &error));
    }

    ASSERT_EQ(decoded.size(), 1U);
    EXPECT_EQ(decoded.front().heartbeat().sequence(), 7U);
}

TEST(FrameCodecTest, HandlesCoalescedFramesAndClearUpdate)
{
    wire::Envelope clear;
    auto* update = clear.mutable_channel_update();
    update->set_channel(wire::CHANNEL_LOCAL_TRAJECTORY);
    update->set_operation(wire::ChannelUpdate::OPERATION_CLEAR);

    const std::string stream =
        wire::encodeFrame(heartbeatEnvelope(1))
        + wire::encodeFrame(clear)
        + wire::encodeFrame(heartbeatEnvelope(2));

    wire::FrameDecoder decoder;
    std::vector<wire::Envelope> decoded;
    std::string error;
    ASSERT_TRUE(decoder.append(stream.data(), stream.size(), &decoded, &error));
    ASSERT_EQ(decoded.size(), 3U);
    EXPECT_EQ(decoded[0].heartbeat().sequence(), 1U);
    EXPECT_EQ(decoded[1].channel_update().channel(), wire::CHANNEL_LOCAL_TRAJECTORY);
    EXPECT_EQ(decoded[1].channel_update().operation(),
              wire::ChannelUpdate::OPERATION_CLEAR);
    EXPECT_EQ(decoded[2].heartbeat().sequence(), 2U);
}

TEST(FrameCodecTest, RejectsZeroAndOversizedFrames)
{
    const std::array<char, 4> zeroLength = {0, 0, 0, 0};
    const std::array<char, 4> oversized = {0x01, 0, 0, 0x01};

    wire::FrameDecoder decoder;
    std::vector<wire::Envelope> decoded;
    std::string error;
    EXPECT_FALSE(decoder.append(zeroLength.data(), zeroLength.size(), &decoded, &error));
    EXPECT_FALSE(error.empty());

    error.clear();
    EXPECT_FALSE(decoder.append(oversized.data(), oversized.size(), &decoded, &error));
    EXPECT_FALSE(error.empty());
}

TEST(FrameCodecTest, RejectsMalformedPayloadAndRecoversAfterReset)
{
    const std::array<char, 5> malformed = {0, 0, 0, 1, 0};

    wire::FrameDecoder decoder;
    std::vector<wire::Envelope> decoded;
    std::string error;
    EXPECT_FALSE(decoder.append(malformed.data(), malformed.size(), &decoded, &error));
    EXPECT_FALSE(error.empty());

    const std::string valid = wire::encodeFrame(heartbeatEnvelope(99));
    error.clear();
    ASSERT_TRUE(decoder.append(valid.data(), valid.size(), &decoded, &error));
    ASSERT_EQ(decoded.size(), 1U);
    EXPECT_EQ(decoded.front().heartbeat().sequence(), 99U);
}

TEST(ProtocolEnvelopeTest, PreservesHandshakeSnapshotAndIncrementalMessages)
{
    std::vector<wire::Envelope> expected(4);
    expected[0].mutable_client_hello()->set_protocol_major(1);
    expected[1].mutable_server_hello()->set_session_id("session-a");
    auto* snapshot = expected[2].mutable_snapshot();
    snapshot->set_session_id("session-a");
    snapshot->mutable_vehicle_state()->set_speed_mps(1.5);
    auto* update = expected[3].mutable_channel_update();
    update->set_session_id("session-a");
    update->set_channel(wire::CHANNEL_VEHICLE_STATE);
    update->set_operation(wire::ChannelUpdate::OPERATION_UPSERT);
    update->mutable_vehicle_state()->set_speed_mps(2.5);

    std::string stream;
    for (const auto& envelope : expected) {
        stream += wire::encodeFrame(envelope);
    }

    wire::FrameDecoder decoder;
    std::vector<wire::Envelope> decoded;
    std::string error;
    ASSERT_TRUE(decoder.append(stream.data(), stream.size(), &decoded, &error));
    ASSERT_EQ(decoded.size(), expected.size());
    EXPECT_TRUE(decoded[0].has_client_hello());
    EXPECT_EQ(decoded[0].client_hello().protocol_major(), 1U);
    EXPECT_EQ(decoded[1].server_hello().session_id(), "session-a");
    EXPECT_DOUBLE_EQ(decoded[2].snapshot().vehicle_state().speed_mps(), 1.5);
    EXPECT_DOUBLE_EQ(decoded[3].channel_update().vehicle_state().speed_mps(), 2.5);
}

}  // namespace
