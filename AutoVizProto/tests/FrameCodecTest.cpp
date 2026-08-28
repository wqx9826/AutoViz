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
    expected[0].mutable_client_hello()->set_protocol_minor(wire::kProtocolMinor);
    auto* hello = expected[1].mutable_server_hello();
    hello->set_protocol_major(wire::kProtocolMajor);
    hello->set_protocol_minor(wire::kProtocolMinor);
    hello->set_session_id("session-v2");
    hello->add_capability(wire::CAPABILITY_COMMON_PLANNING_CONTROL);
    hello->add_capability(wire::CAPABILITY_UNDERWATER_SYSTEM);

    auto* snapshot = expected[2].mutable_snapshot();
    snapshot->set_session_id("session-v2");
    snapshot->mutable_vehicle_state()->set_speed_mps(1.5);
    snapshot->mutable_vehicle_state()->set_odom_heading_rad(0.7);
    snapshot->mutable_vehicle_state()->set_start_time_s(12);
    snapshot->mutable_vehicle_state()->mutable_underwater()->set_depth_m(3.2);
    auto* underwater = snapshot->mutable_chassis_state()->mutable_underwater();
    underwater->set_water_tank_state(wire::WATER_TANK_STATE_FILLING);
    underwater->set_emergency_ascent_active(true);
    snapshot->mutable_chassis_state()->add_tail_thruster_motor()->set_actual_speed_rpm(118.0);
    auto* thrusterMotor = snapshot->mutable_chassis_state()->add_thruster_motor();
    thrusterMotor->set_id("left_vertical_thruster");
    thrusterMotor->set_bus_current_a(4.6);
    thrusterMotor->set_controller_temperature_c(32);
    thrusterMotor->set_target_speed_rpm(120.0);
    thrusterMotor->set_actual_speed_rpm(118.0);
    auto* platform = snapshot->mutable_chassis_state()->mutable_platform();
    platform->mutable_battery()->set_pack_voltage_v(312.5);
    platform->add_power_channel()->set_status(1);
    snapshot->mutable_control_command()->mutable_underwater()->set_emergency_ascent(true);
    snapshot->mutable_control_command()->set_source_mode(11);
    snapshot->mutable_task_state()
        ->mutable_underwater()
        ->set_release_emergency_ascent(true);
    auto* topic = snapshot->mutable_runtime_state()->add_topic();
    topic->set_name("/location");
    topic->set_data_kind(wire::DATA_KIND_VEHICLE_STATE);
    auto* range = snapshot->mutable_perception_state()->mutable_range_motion_directive();
    range->mutable_header()->set_frame_id("odom");
    range->set_task_id(8);
    range->set_command_sequence(17);
    range->set_motion(wire::RangeMotionDirective::MOTION_SLOW);
    range->set_speed_limit_mps(0.35);
    range->set_reason("range limit");
    auto* finalTargets = snapshot->mutable_final_targets();
    finalTargets->mutable_header()->set_frame_id("odom");
    finalTargets->set_source_task_id(10);
    finalTargets->set_mine_number(2);
    auto* finalTarget = finalTargets->add_target();
    finalTarget->set_target_id(42);
    finalTarget->set_final_class(2);
    finalTarget->mutable_reference_point()->set_x_m(1.0);
    finalTarget->mutable_reference_point()->set_y_m(-2.0);
    finalTarget->set_radius_m(3.0);
    auto* boundaryPoint = finalTarget->mutable_boundary()->add_point();
    boundaryPoint->set_x_m(0.0);
    boundaryPoint->set_y_m(0.0);
    boundaryPoint = finalTarget->mutable_boundary()->add_point();
    boundaryPoint->set_x_m(1.0);
    boundaryPoint->set_y_m(0.0);
    boundaryPoint = finalTarget->mutable_boundary()->add_point();
    boundaryPoint->set_x_m(0.0);
    boundaryPoint->set_y_m(1.0);
    auto* inspection = snapshot->mutable_perception_state()->mutable_inspection_goal();
    inspection->mutable_header()->set_frame_id("odom");
    inspection->set_task_id(9);
    inspection->set_goal_id(18);
    inspection->set_target_id(42);
    inspection->mutable_target_position()->set_z_m(-3.0);
    inspection->mutable_observation_position()->set_z_m(-1.0);
    inspection->set_target_heading_rad(1.2);
    inspection->set_hold_on_arrival(true);
    inspection->set_mode(wire::InspectionGoal::MODE_OBSERVE);
    inspection->set_speed_limit_mps(0.4);

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
    EXPECT_EQ(decoded[0].client_hello().protocol_minor(), 8U);
    EXPECT_EQ(decoded[1].server_hello().protocol_minor(), 8U);
    EXPECT_EQ(decoded[1].server_hello().capability(1),
              wire::CAPABILITY_UNDERWATER_SYSTEM);
    const auto& actual = decoded[2].snapshot();
    EXPECT_DOUBLE_EQ(actual.vehicle_state().underwater().depth_m(), 3.2);
    EXPECT_DOUBLE_EQ(actual.vehicle_state().odom_heading_rad(), 0.7);
    EXPECT_EQ(actual.vehicle_state().start_time_s(), 12);
    EXPECT_EQ(actual.chassis_state().underwater().water_tank_state(),
              wire::WATER_TANK_STATE_FILLING);
    EXPECT_DOUBLE_EQ(actual.chassis_state().platform().battery().pack_voltage_v(), 312.5);
    ASSERT_EQ(actual.chassis_state().tail_thruster_motor_size(), 1);
    EXPECT_DOUBLE_EQ(actual.chassis_state().tail_thruster_motor(0).actual_speed_rpm(), 118.0);
    ASSERT_EQ(actual.chassis_state().thruster_motor_size(), 1);
    EXPECT_EQ(actual.chassis_state().thruster_motor(0).id(), "left_vertical_thruster");
    EXPECT_DOUBLE_EQ(actual.chassis_state().thruster_motor(0).bus_current_a(), 4.6);
    EXPECT_TRUE(actual.control_command().underwater().emergency_ascent());
    EXPECT_EQ(actual.control_command().source_mode(), 11);
    EXPECT_TRUE(actual.task_state().underwater().release_emergency_ascent());
    EXPECT_EQ(actual.runtime_state().topic(0).data_kind(), wire::DATA_KIND_VEHICLE_STATE);
    ASSERT_TRUE(actual.has_perception_state());
    EXPECT_EQ(actual.perception_state().range_motion_directive().task_id(), 8U);
    EXPECT_EQ(actual.perception_state().range_motion_directive().motion(),
              wire::RangeMotionDirective::MOTION_SLOW);
    EXPECT_EQ(actual.perception_state().inspection_goal().target_id(), 42U);
    EXPECT_DOUBLE_EQ(actual.perception_state().inspection_goal().target_position().z_m(), -3.0);
    EXPECT_DOUBLE_EQ(actual.perception_state().inspection_goal().speed_limit_mps(), 0.4);
    ASSERT_TRUE(actual.has_final_targets());
    EXPECT_EQ(actual.final_targets().mine_number(), 2U);
    ASSERT_EQ(actual.final_targets().target_size(), 1);
    EXPECT_DOUBLE_EQ(actual.final_targets().target(0).radius_m(), 3.0);
    ASSERT_EQ(actual.final_targets().target(0).boundary().point_size(), 3);
    EXPECT_DOUBLE_EQ(actual.final_targets().target(0).boundary().point(1).x_m(), 1.0);
    EXPECT_DOUBLE_EQ(actual.final_targets().target(0).boundary().point(2).y_m(), 1.0);
}

TEST(ProtocolEnvelopeTest, AcceptsOldSnapshotWithoutPerceptionState)
{
    wire::Envelope oldEnvelope;
    oldEnvelope.mutable_snapshot()->set_sequence(9);
    oldEnvelope.mutable_snapshot()->mutable_task_state()->set_task_id(7);
    oldEnvelope.mutable_snapshot()->mutable_chassis_state()
        ->add_tail_thruster_motor()->set_id("left_tail_thruster");

    wire::FrameDecoder decoder;
    std::vector<wire::Envelope> decoded;
    std::string error;
    ASSERT_TRUE(decoder.decode(encode(oldEnvelope), decoded, error)) << error;
    ASSERT_EQ(decoded.size(), 1U);
    EXPECT_FALSE(decoded.front().snapshot().has_perception_state());
    EXPECT_EQ(decoded.front().snapshot().task_state().task_id(), 7U);
    EXPECT_EQ(decoded.front().snapshot().chassis_state().tail_thruster_motor_size(), 1);
    EXPECT_EQ(decoded.front().snapshot().chassis_state().thruster_motor_size(), 0);
}

TEST(ProtocolVersionTest, AcceptsOnlyV2MajorVersion)
{
    EXPECT_EQ(wire::kProtocolMajor, 2U);
    EXPECT_EQ(wire::kProtocolMinor, 8U);
    EXPECT_TRUE(wire::isProtocolMajorCompatible(2U));
    EXPECT_FALSE(wire::isProtocolMajorCompatible(1U));
}

}  // namespace
