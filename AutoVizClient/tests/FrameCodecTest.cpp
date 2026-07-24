#include <gtest/gtest.h>

#include <array>
#include <string>
#include <vector>

#include "autoviz/protocol/FrameCodec.h"

namespace {

namespace v1 = autoviz::protocol::v1;

v1::Envelope heartbeatEnvelope(std::uint64_t sequence)
{
    v1::Envelope envelope;
    envelope.mutable_heartbeat()->set_sequence(sequence);
    return envelope;
}

TEST(FrameCodecTest, RoundTripsSingleFrame)
{
    const std::string frame = autoviz::protocol::encodeFrame(heartbeatEnvelope(42));
    ASSERT_FALSE(frame.empty());

    autoviz::protocol::FrameDecoder decoder;
    std::vector<v1::Envelope> decoded;
    std::string error;
    ASSERT_TRUE(decoder.append(frame.data(), frame.size(), &decoded, &error));
    ASSERT_EQ(decoded.size(), 1U);
    EXPECT_EQ(decoded.front().heartbeat().sequence(), 42U);
    EXPECT_TRUE(error.empty());
}

TEST(FrameCodecTest, HandlesFragmentedFrame)
{
    const std::string frame = autoviz::protocol::encodeFrame(heartbeatEnvelope(7));
    ASSERT_GT(frame.size(), autoviz::protocol::kFrameHeaderSize);

    autoviz::protocol::FrameDecoder decoder;
    std::vector<v1::Envelope> decoded;
    std::string error;
    for (const char byte : frame) {
        ASSERT_TRUE(decoder.append(&byte, 1U, &decoded, &error));
    }

    ASSERT_EQ(decoded.size(), 1U);
    EXPECT_EQ(decoded.front().heartbeat().sequence(), 7U);
}

TEST(FrameCodecTest, HandlesCoalescedFramesAndClearUpdate)
{
    v1::Envelope clear;
    auto* update = clear.mutable_channel_update();
    update->set_channel(v1::CHANNEL_LOCAL_TRAJECTORY);
    update->set_operation(v1::ChannelUpdate::OPERATION_CLEAR);

    const std::string stream =
        autoviz::protocol::encodeFrame(heartbeatEnvelope(1))
        + autoviz::protocol::encodeFrame(clear)
        + autoviz::protocol::encodeFrame(heartbeatEnvelope(2));

    autoviz::protocol::FrameDecoder decoder;
    std::vector<v1::Envelope> decoded;
    std::string error;
    ASSERT_TRUE(decoder.append(stream.data(), stream.size(), &decoded, &error));
    ASSERT_EQ(decoded.size(), 3U);
    EXPECT_EQ(decoded[0].heartbeat().sequence(), 1U);
    EXPECT_EQ(decoded[1].channel_update().channel(), v1::CHANNEL_LOCAL_TRAJECTORY);
    EXPECT_EQ(decoded[1].channel_update().operation(),
              v1::ChannelUpdate::OPERATION_CLEAR);
    EXPECT_EQ(decoded[2].heartbeat().sequence(), 2U);
}

TEST(FrameCodecTest, RejectsZeroAndOversizedFrames)
{
    const std::array<char, 4> zeroLength = {0, 0, 0, 0};
    const std::array<char, 4> oversized = {0x01, 0, 0, 0x01};

    autoviz::protocol::FrameDecoder decoder;
    std::vector<v1::Envelope> decoded;
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

    autoviz::protocol::FrameDecoder decoder;
    std::vector<v1::Envelope> decoded;
    std::string error;
    EXPECT_FALSE(decoder.append(malformed.data(), malformed.size(), &decoded, &error));
    EXPECT_FALSE(error.empty());

    const std::string valid = autoviz::protocol::encodeFrame(heartbeatEnvelope(99));
    error.clear();
    ASSERT_TRUE(decoder.append(valid.data(), valid.size(), &decoded, &error));
    ASSERT_EQ(decoded.size(), 1U);
    EXPECT_EQ(decoded.front().heartbeat().sequence(), 99U);
}

TEST(ProtocolEnvelopeTest, PreservesHandshakeSnapshotAndIncrementalMessages)
{
    std::vector<v1::Envelope> expected(4);
    expected[0].mutable_client_hello()->set_protocol_major(1);
    expected[1].mutable_server_hello()->set_session_id("session-a");
    auto* snapshot = expected[2].mutable_snapshot();
    snapshot->set_session_id("session-a");
    snapshot->mutable_vehicle_state()->set_speed_mps(1.5);
    auto* update = expected[3].mutable_channel_update();
    update->set_session_id("session-a");
    update->set_channel(v1::CHANNEL_VEHICLE_STATE);
    update->set_operation(v1::ChannelUpdate::OPERATION_UPSERT);
    update->mutable_vehicle_state()->set_speed_mps(2.5);

    std::string stream;
    for (const auto& envelope : expected) {
        stream += autoviz::protocol::encodeFrame(envelope);
    }

    autoviz::protocol::FrameDecoder decoder;
    std::vector<v1::Envelope> decoded;
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
