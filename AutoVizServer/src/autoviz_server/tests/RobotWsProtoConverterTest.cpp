#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <vector>

#include "autoviz_server/RobotWsProtoConverter.h"
#include "autoviz_server/SnapshotStore.h"

namespace {

constexpr std::uint64_t kReceiveTimeNs = 123456789ULL;
constexpr double kPi = 3.14159265358979323846;

TEST(RobotWsProtoConverterTest, ConvertsLocationAndKeepsVerticalQuantitiesDistinct)
{
    custom_msgs::msg::Location source;
    source.odom_x = 1.25;
    source.odom_y = -2.5;
    source.odom_z = 3.75;
    source.heading = 0.6;
    source.pitch = 0.2;
    source.roll = -0.1;
    source.velocity_x = 1.0;
    source.velocity_y = 2.0;
    source.velocity_z = -0.3;
    source.velocity = 2.25;
    source.omega_z = 0.4;
    source.acc_x = 0.1;
    source.acc_y = 0.2;
    source.acc_z = 0.3;
    source.acc = 0.5;
    source.depth = 8.0;
    source.height = 1.4;
    source.status = 2;
    source.error = 9;
    source.gps_time = 77;
    source.longitude = 120.123456;
    source.latitude = 30.654321;
    source.usbl_x = 9.5;
    source.usbl_y = -1.25;
    source.usbl_z = 4.75;

    const auto actual = autoviz_server::RobotWsProtoConverter::vehicleState(
        source, kReceiveTimeNs);
    EXPECT_DOUBLE_EQ(actual.position().x_m(), 1.25);
    EXPECT_DOUBLE_EQ(actual.position().z_m(), 3.75);
    EXPECT_DOUBLE_EQ(actual.underwater().odom_z_m(), 3.75);
    EXPECT_DOUBLE_EQ(actual.underwater().depth_m(), 8.0);
    EXPECT_DOUBLE_EQ(actual.underwater().height_above_bottom_m(), 1.4);
    EXPECT_DOUBLE_EQ(actual.underwater().vertical_velocity_mps(), -0.3);
    EXPECT_EQ(actual.localization_status(), 2);
    EXPECT_EQ(actual.localization_error(), 9);
    EXPECT_EQ(actual.gps_time(), 77U);
    EXPECT_DOUBLE_EQ(actual.longitude_deg(), 120.123456);
    EXPECT_DOUBLE_EQ(actual.latitude_deg(), 30.654321);
    EXPECT_DOUBLE_EQ(actual.underwater().usbl_x_m(), 9.5);
    EXPECT_DOUBLE_EQ(actual.underwater().usbl_y_m(), -1.25);
    EXPECT_DOUBLE_EQ(actual.underwater().usbl_z_m(), 4.75);
}

TEST(RobotWsProtoConverterTest, ConvertsOnlyDrawableFinalTargets)
{
    custom_msgs::msg::FinalTargetArray source;
    source.header.frame_id = "odom";
    source.header.stamp.sec = 5;
    source.targets.resize(2);
    auto& valid = source.targets[0];
    valid.target_id = 42;
    valid.final_class = valid.CLASS_MINE;
    valid.real_center_point.x = 10.0;
    valid.real_center_point.y = 20.0;
    valid.real_center_point.z = -2.0;
    valid.length = 3.0;
    valid.width = 1.5;
    valid.height = 0.8;
    valid.heading = 1.2;
    valid.dimensions_valid = true;
    valid.heading_valid = true;
    source.targets[1].target_id = 99;
    source.targets[1].dimensions_valid = false;

    const auto actual = autoviz_server::RobotWsProtoConverter::obstacles(
        source, kReceiveTimeNs);
    ASSERT_EQ(actual.obstacle_size(), 1);
    EXPECT_EQ(actual.obstacle(0).id(), "42");
    EXPECT_EQ(actual.obstacle(0).source_class(), valid.CLASS_MINE);
    EXPECT_DOUBLE_EQ(actual.obstacle(0).center().x_m(), 10.0);
    EXPECT_DOUBLE_EQ(actual.obstacle(0).length_m(), 3.0);
    EXPECT_TRUE(actual.obstacle(0).has_heading_rad());
}

TEST(RobotWsProtoConverterTest, ConvertsCommonAndUnderwaterControlCommand)
{
    custom_msgs::msg::ChassisCommand source;
    source.mode = 6;
    source.is_enable = true;
    source.speed = 0.8;
    source.angular_velocity = 0.3;
    source.expected_gear = 1;
    source.heading = 2.2;
    source.is_use_water_actuator = true;
    source.navi_mode = 2;
    source.depth = 6.0;
    source.height = 1.0;
    source.left_water_actuator_speed = -10;
    source.right_water_actuator_speed = 11;
    source.buoyancy_adjust = source.BUOYANCY_DRAIN;
    source.is_open_sonar_power = true;
    source.emergency_ascent = true;

    const auto actual = autoviz_server::RobotWsProtoConverter::controlCommand(
        source, kReceiveTimeNs);
    EXPECT_EQ(actual.mode(), autoviz::ControlCommand::MODE_CRAWL);
    EXPECT_DOUBLE_EQ(actual.target_yaw_rate_radps(), 0.3);
    ASSERT_TRUE(actual.has_underwater());
    EXPECT_EQ(actual.underwater().navigation_mode(), 2);
    EXPECT_EQ(actual.underwater().vertical_control_mode(),
              autoviz::VERTICAL_CONTROL_MODE_HEIGHT_HOLD);
    EXPECT_EQ(actual.underwater().left_thruster_command(), -10);
    EXPECT_EQ(actual.underwater().buoyancy_command(),
              autoviz::BUOYANCY_COMMAND_DRAIN);
    EXPECT_TRUE(actual.underwater().emergency_ascent());
}

TEST(RobotWsProtoConverterTest, PreservesCenterTurningAsGenericManeuver)
{
    custom_msgs::msg::ChassisCommand sailingSource;
    sailingSource.mode = 10;
    const auto sailing = autoviz_server::RobotWsProtoConverter::controlCommand(
        sailingSource, kReceiveTimeNs);
    EXPECT_EQ(sailing.mode(), autoviz::ControlCommand::MODE_SAILING);
    EXPECT_EQ(sailing.maneuver(), autoviz::ControlCommand::MANEUVER_YAW_IN_PLACE);

    custom_msgs::msg::ChassisCommand crawlSource;
    crawlSource.mode = 11;
    const auto crawl = autoviz_server::RobotWsProtoConverter::controlCommand(
        crawlSource, kReceiveTimeNs);
    EXPECT_EQ(crawl.mode(), autoviz::ControlCommand::MODE_CRAWL);
    EXPECT_EQ(crawl.maneuver(), autoviz::ControlCommand::MANEUVER_YAW_IN_PLACE);

    custom_msgs::msg::ChassisCommand gearSource;
    gearSource.mode = 6;
    gearSource.expected_gear = 4;
    const auto gear = autoviz_server::RobotWsProtoConverter::controlCommand(
        gearSource, kReceiveTimeNs);
    EXPECT_EQ(gear.mode(), autoviz::ControlCommand::MODE_CRAWL);
    EXPECT_EQ(gear.maneuver(), autoviz::ControlCommand::MANEUVER_NONE);
}

TEST(RobotWsProtoConverterTest, NormalizesChassisYawAndUsesTypedDiagnostics)
{
    custom_msgs::msg::ChassisStates source;
    source.current_speed = 1.2;
    source.current_angular_velocity = -0.75;
    source.gear_status = 2;
    source.water_tank_level_status = 73;
    source.water_tank_status = source.WATER_TANK_FILLING;
    source.left_tail_actuator_status = 3;
    source.crawl_heartbeat = 12;
    source.left_crawl_motor_speed_rpm = 321.0;
    source.left_crawl_motor_fault = true;
    source.left_crawl_motor_fault_code = 5;
    source.left_crawl_actuator_fault_code = 6;
    source.high_voltage_bms_pack_voltage = 325.5;
    source.high_voltage_bms_soc_status = 88;
    source.dccdc_status = true;
    source.power_supply_1_status = 2;
    source.smart_power_input_voltage_status = 24.2;
    source.emergency_ascent_active = true;

    const auto actual = autoviz_server::RobotWsProtoConverter::chassisState(
        source, kReceiveTimeNs);
    EXPECT_DOUBLE_EQ(actual.yaw_rate_radps(), 0.75);
    EXPECT_EQ(actual.underwater().water_tank_level(), 73);
    EXPECT_EQ(actual.underwater().water_tank_state(),
              autoviz::WATER_TANK_STATE_FILLING);
    EXPECT_TRUE(actual.underwater().emergency_ascent_active());
    EXPECT_EQ(actual.underwater().thruster(0).fault_code(), 3);
    ASSERT_TRUE(actual.has_platform());
    EXPECT_EQ(actual.platform().crawl_heartbeat(), 12);
    EXPECT_DOUBLE_EQ(actual.platform().left_crawl_motor().speed_rpm(), 321.0);
    EXPECT_EQ(actual.platform().left_crawl_motor().actuator_fault_code(), 6);
    EXPECT_DOUBLE_EQ(actual.platform().battery().pack_voltage_v(), 325.5);
    EXPECT_EQ(actual.platform().battery().state_of_charge_percent(), 88);
    ASSERT_EQ(actual.platform().power_channel_size(), 16);
    EXPECT_EQ(actual.platform().power_channel(0).status(), 2);
}

TEST(RobotWsProtoConverterTest, ConvertsActionDegreesPerSecondToRadiansPerSecond)
{
    custom_msgs::msg::SystemRunStates source;
    source.owner = 2;
    source.state = 1;
    source.goal_uuid = "goal-1";
    source.message = "executing";
    source.chassis_mode = 1;
    source.is_enable = true;
    source.navi_mode = 1;
    source.target_depth = 5.0;
    source.target_height = 1.5;
    source.buoyancy_adjust = 1;
    source.target_speed = 0.4;
    source.target_heading = 0.9;
    source.target_angular_velocity = 180.0;
    source.emergency_ascent = true;

    const auto actual = autoviz_server::RobotWsProtoConverter::actionState(
        source, kReceiveTimeNs);
    EXPECT_NEAR(actual.target_yaw_rate_radps(), kPi, 1e-12);
    EXPECT_EQ(actual.underwater().vertical_control_mode(),
              autoviz::VERTICAL_CONTROL_MODE_DEPTH_HOLD);
    EXPECT_EQ(actual.goal_id(), "goal-1");
    EXPECT_EQ(actual.message(), "executing");
    EXPECT_EQ(actual.action_name(), "custom_msgs/action/DepthCommand");
    EXPECT_DOUBLE_EQ(actual.underwater().target_depth_m(), 5.0);
    EXPECT_EQ(actual.underwater().buoyancy_command(),
              autoviz::BUOYANCY_COMMAND_FILL);
    EXPECT_TRUE(actual.underwater().emergency_ascent());
}

TEST(RobotWsProtoConverterTest, InfersHeightHoldFromDepthActionWhenNaviModeIsUnset)
{
    custom_msgs::msg::SystemRunStates source;
    source.owner = 2;
    source.state = 1;
    source.chassis_mode = 2;
    source.navi_mode = 0;
    source.target_height = 0.5;
    source.buoyancy_adjust = 1;

    const auto actual = autoviz_server::RobotWsProtoConverter::actionState(
        source, kReceiveTimeNs);
    ASSERT_TRUE(actual.has_underwater());
    EXPECT_EQ(actual.underwater().vertical_control_mode(),
              autoviz::VERTICAL_CONTROL_MODE_HEIGHT_HOLD);
    EXPECT_DOUBLE_EQ(actual.underwater().target_height_above_bottom_m(), 0.5);
    EXPECT_EQ(actual.underwater().buoyancy_command(), autoviz::BUOYANCY_COMMAND_FILL);
}

TEST(RobotWsProtoConverterTest, DoesNotTreatMoveNavigationDependencyAsVerticalAction)
{
    custom_msgs::msg::SystemRunStates source;
    source.owner = 1;          // custom_msgs/action/Move
    source.chassis_mode = 4;   // ordinary autonomous navigation
    source.navi_mode = 1;      // depth-hold dependency for the navigation action

    const auto actual = autoviz_server::RobotWsProtoConverter::actionState(
        source, kReceiveTimeNs);

    ASSERT_TRUE(actual.has_underwater());
    EXPECT_EQ(actual.underwater().vertical_control_mode(),
              autoviz::VERTICAL_CONTROL_MODE_NONE);
}

TEST(RobotWsProtoConverterTest, MatchesGoalUuidAcrossDroppedLeadingZero)
{
    // robot_ws 用 %x 逐字节格式化 SystemRunStates.goal_uuid，会丢掉字节前导零
    //（例如 0x06 变成 "6"）。canonical 是隐藏 action topic 16 字节 UUID 的标准
    // 32 位 hex，lossy 是同一 UUID 丢零后的形式。
    const std::string canonical = "c5cc4cd52699c9c14e81484088068456";
    const std::string lossy = "c5cc4cd52699c9c14e8148408868456";

    EXPECT_TRUE(autoviz_server::RobotWsProtoConverter::sameGoalUuid(canonical, lossy));
    EXPECT_TRUE(autoviz_server::RobotWsProtoConverter::sameGoalUuid(lossy, canonical));
    EXPECT_TRUE(autoviz_server::RobotWsProtoConverter::sameGoalUuid(canonical, canonical));
    EXPECT_FALSE(autoviz_server::RobotWsProtoConverter::sameGoalUuid(
        canonical, "c5cc4cd52699c9c14e81484088068457"));
    EXPECT_FALSE(autoviz_server::RobotWsProtoConverter::sameGoalUuid(
        lossy, "c5cc4cd52699c9c14e8148408868457"));
}

TEST(RobotWsProtoConverterTest, ConvertsTaskIncludingEmergencyRelease)
{
    custom_msgs::msg::TaskParams source;
    source.task_type = 2;
    source.task_id = 8;
    source.task_enable = true;
    source.emergency_stop = true;
    source.release_emergency_ascent = true;
    source.remote_mode = 3;
    source.power_enable = 1;
    source.crawl_gear = 2;
    source.crawl_speed = 0.75;
    source.crawl_angular_velocity = -0.2;
    source.forward_percent = 60;
    source.turn_percent = -20;
    source.dive_percent = 15;
    source.left_tail_actuator_speed = -90;
    source.right_tail_actuator_speed = 91;
    source.left_vertical_actuator_speed = -30;
    source.right_vertical_actuator_speed = 31;
    source.back_vertical_actuator_speed = 32;
    source.power_supply1 = true;
    source.power_supply16 = true;

    const auto actual = autoviz_server::RobotWsProtoConverter::taskState(
        source, kReceiveTimeNs);
    EXPECT_EQ(actual.task_id(), 8);
    EXPECT_TRUE(actual.emergency_stop());
    EXPECT_TRUE(actual.underwater().release_emergency_ascent());
    EXPECT_EQ(actual.remote_mode(), 3);
    ASSERT_TRUE(actual.has_remote_control());
    const auto& remote = actual.remote_control();
    EXPECT_EQ(remote.crawl_gear(), 2);
    EXPECT_DOUBLE_EQ(remote.crawl_speed_mps(), 0.75);
    EXPECT_DOUBLE_EQ(remote.crawl_angular_velocity_radps(), -0.2);
    EXPECT_EQ(remote.forward_percent(), 60);
    EXPECT_EQ(remote.turn_percent(), -20);
    EXPECT_EQ(remote.dive_percent(), 15);
    EXPECT_EQ(remote.left_tail_actuator_speed(), -90);
    EXPECT_EQ(remote.right_tail_actuator_speed(), 91);
    EXPECT_EQ(remote.left_vertical_actuator_speed(), -30);
    EXPECT_EQ(remote.right_vertical_actuator_speed(), 31);
    EXPECT_EQ(remote.back_vertical_actuator_speed(), 32);
    ASSERT_EQ(remote.power_supply_enabled_size(), 16);
    EXPECT_TRUE(remote.power_supply_enabled(0));
    EXPECT_FALSE(remote.power_supply_enabled(1));
    EXPECT_TRUE(remote.power_supply_enabled(15));
}

TEST(RobotWsProtoConverterTest, ConvertsLocalAndGlobalPaths)
{
    custom_msgs::msg::TrajectoryMsg local;
    local.header.frame_id = "odom";
    local.header.stamp.sec = 10;
    local.goal_uuid = "goal-local";
    local.trajectory.resize(2);
    local.trajectory[0].pose.position.x = 1.0;
    local.trajectory[0].pose.orientation.w = 1.0;
    local.trajectory[0].velocity.linear.x = 0.5;
    local.trajectory[0].acceleration.linear.x = 0.1;
    local.trajectory[1].pose.position.x = 4.0;
    local.trajectory[1].pose.position.y = 4.0;
    local.trajectory[1].pose.orientation.z = std::sin(kPi / 4.0);
    local.trajectory[1].pose.orientation.w = std::cos(kPi / 4.0);
    local.trajectory[1].time_from_start.sec = 2;
    local.trajectory[1].time_from_start.nanosec = 500000000;

    const auto localActual = autoviz_server::RobotWsProtoConverter::localTrajectory(
        local, kReceiveTimeNs);
    EXPECT_EQ(localActual.kind(), autoviz::Trajectory::KIND_LOCAL);
    EXPECT_EQ(localActual.goal_id(), "goal-local");
    EXPECT_NEAR(localActual.point(1).path_point().heading_rad(), kPi / 2.0, 1e-12);
    EXPECT_DOUBLE_EQ(localActual.point(1).relative_time_s(), 2.5);
    EXPECT_DOUBLE_EQ(localActual.total_length_m(), 5.0);

    nav_msgs::msg::Path global;
    global.header.frame_id = "map";
    global.poses.resize(2);
    global.poses[0].pose.orientation.w = 1.0;
    global.poses[1].pose.position.x = 3.0;
    global.poses[1].pose.position.y = 4.0;
    global.poses[1].pose.orientation.w = 1.0;
    const auto globalActual = autoviz_server::RobotWsProtoConverter::globalTrajectory(
        global, kReceiveTimeNs);
    EXPECT_EQ(globalActual.kind(), autoviz::Trajectory::KIND_GLOBAL);
    EXPECT_DOUBLE_EQ(globalActual.total_length_m(), 5.0);
}

TEST(SnapshotStoreTest, TracksTopicsAndClearsTimedOutSnapshotFields)
{
    autoviz_server::SnapshotStore store({
        {"/location", "custom_msgs/msg/Location", autoviz::DATA_KIND_VEHICLE_STATE},
        {"/local_path", "custom_msgs/msg/TrajectoryMsg", autoviz::DATA_KIND_LOCAL_TRAJECTORY}});
    autoviz::VehicleState vehicle;
    vehicle.mutable_position()->set_x_m(1.0);
    store.updateVehicleState(vehicle, kReceiveTimeNs);

    const auto before = store.buildSnapshot(kReceiveTimeNs, 2);
    ASSERT_TRUE(before.has_vehicle_state());
    ASSERT_EQ(before.runtime_state().topic_size(), 2);
    EXPECT_EQ(before.runtime_state().topic(0).message_count(), 1U);
    EXPECT_FALSE(before.runtime_state().topic(0).timed_out());
    EXPECT_TRUE(before.runtime_state().topic(1).timed_out());

    store.markPublished();
    store.expire(std::chrono::steady_clock::now() + std::chrono::seconds(6),
                 std::chrono::milliseconds(5000));
    EXPECT_TRUE(store.dirty());
    const auto after = store.buildSnapshot(kReceiveTimeNs, 0);
    EXPECT_FALSE(after.has_vehicle_state());
    EXPECT_TRUE(after.runtime_state().topic(0).timed_out());
    EXPECT_EQ(after.runtime_state().topic(0).timeout_ns(), 5000000000ULL);
}

TEST(SnapshotStoreTest, ExpiresActionCurrentStateButRetainsAuditEvent)
{
    autoviz_server::SnapshotStore store({
        {"/system_run_states", "custom_msgs/msg/SystemRunStates", autoviz::DATA_KIND_ACTION_STATE}});
    autoviz::ActionState action;
    action.set_owner(2);
    action.set_state(1);
    action.set_goal_id("vertical-goal");
    store.updateActionState(action, kReceiveTimeNs);

    store.markPublished();
    store.expire(std::chrono::steady_clock::now() + std::chrono::seconds(6),
                 std::chrono::milliseconds(5000));
    const auto snapshot = store.buildSnapshot(kReceiveTimeNs, 0);

    EXPECT_FALSE(snapshot.has_action_state());
    ASSERT_EQ(snapshot.control_state_event_size(), 1);
    EXPECT_EQ(snapshot.control_state_event(0).goal_id(), "vertical-goal");
    EXPECT_EQ(snapshot.control_state_event(0).header().sequence(), 1U);
    ASSERT_EQ(snapshot.runtime_state().topic_size(), 1);
    EXPECT_TRUE(snapshot.runtime_state().topic(0).timed_out());
}

TEST(SnapshotStoreTest, RecordsCommandAndChassisTransitionsWithGoal)
{
    autoviz_server::SnapshotStore store({
        {"/system_run_states", "custom_msgs/msg/SystemRunStates", autoviz::DATA_KIND_ACTION_STATE},
        {"/chassis_command", "custom_msgs/msg/ChassisCommand", autoviz::DATA_KIND_CONTROL_COMMAND},
        {"/chassis_states", "custom_msgs/msg/ChassisStates", autoviz::DATA_KIND_CHASSIS_STATE}});
    autoviz::ActionState action; action.set_goal_id("goal-6"); action.set_chassis_mode(6);
    store.updateActionState(action, kReceiveTimeNs);
    autoviz::ControlCommand command; command.set_mode(autoviz::ControlCommand::MODE_CRAWL); command.set_enabled(false); command.set_target_gear(0);
    store.updateControlCommand(command, kReceiveTimeNs + 1);
    command.set_enabled(true); command.set_target_gear(1);
    store.updateControlCommand(command, kReceiveTimeNs + 2);
    const auto snapshot = store.buildSnapshot(kReceiveTimeNs + 2, 0);
    ASSERT_EQ(snapshot.control_command().header().sequence(), 2U);
    ASSERT_EQ(snapshot.control_state_event_size(), 3);
    EXPECT_EQ(snapshot.control_state_event(2).goal_id(), "goal-6");
    EXPECT_TRUE(snapshot.control_state_event(2).current_enabled());
    EXPECT_EQ(snapshot.control_state_event(2).current_gear(), 1);

    autoviz::ChassisState chassis;
    chassis.set_gear(0);
    chassis.mutable_platform()->mutable_left_crawl_motor()->set_output_enabled(false);
    chassis.mutable_platform()->mutable_right_crawl_motor()->set_output_enabled(false);
    store.updateChassisState(chassis, kReceiveTimeNs + 3);
    chassis.mutable_platform()->mutable_left_crawl_motor()->set_output_enabled(true);
    chassis.mutable_platform()->mutable_right_crawl_motor()->set_output_enabled(true);
    store.updateChassisState(chassis, kReceiveTimeNs + 4);
    const auto outputSnapshot = store.buildSnapshot(kReceiveTimeNs + 4, 0);
    ASSERT_EQ(outputSnapshot.control_state_event_size(), 5);
    const auto& outputEvent = outputSnapshot.control_state_event(4);
    EXPECT_EQ(outputEvent.header().sequence(), 2U);
    EXPECT_FALSE(outputEvent.previous_crawl_output_enabled());
    EXPECT_TRUE(outputEvent.current_crawl_output_enabled());
}

TEST(SnapshotStoreTest, RetainsIntermediateCommandEventsBetweenPublishes)
{
    autoviz_server::SnapshotStore store({
        {"/chassis_command", "custom_msgs/msg/ChassisCommand",
         autoviz::DATA_KIND_CONTROL_COMMAND}});

    autoviz::ControlCommand command;
    command.set_mode(autoviz::ControlCommand::MODE_CRAWL);
    command.set_maneuver(autoviz::ControlCommand::MANEUVER_YAW_IN_PLACE);
    command.set_enabled(true);
    command.set_target_gear(4);
    store.updateControlCommand(command, kReceiveTimeNs);

    command.set_mode(autoviz::ControlCommand::MODE_UNKNOWN);
    command.set_maneuver(autoviz::ControlCommand::MANEUVER_NONE);
    command.set_enabled(false);
    command.set_target_gear(0);
    store.updateControlCommand(command, kReceiveTimeNs + 1);

    command.set_mode(autoviz::ControlCommand::MODE_CRAWL);
    command.set_enabled(true);
    command.set_target_gear(1);
    store.updateControlCommand(command, kReceiveTimeNs + 2);

    const auto snapshot = store.buildSnapshot(kReceiveTimeNs + 2, 0);
    ASSERT_TRUE(snapshot.has_control_command());
    EXPECT_EQ(snapshot.control_command().header().sequence(), 3U);
    ASSERT_EQ(snapshot.control_state_event_size(), 3);
    EXPECT_EQ(snapshot.control_state_event(1).previous_mode(), 11);
    EXPECT_EQ(snapshot.control_state_event(1).current_mode(), 0);
    EXPECT_FALSE(snapshot.control_state_event(1).current_enabled());
    EXPECT_EQ(snapshot.control_state_event(2).previous_mode(), 0);
    EXPECT_EQ(snapshot.control_state_event(2).current_mode(), 6);
    EXPECT_TRUE(snapshot.control_state_event(2).current_enabled());
}

TEST(SnapshotStoreTest, ActionDiagnosticsDoNotAdvanceSystemRunStatesMetadata)
{
    autoviz_server::SnapshotStore store({
        {"/system_run_states", "custom_msgs/msg/SystemRunStates",
         autoviz::DATA_KIND_ACTION_STATE}});
    autoviz::ActionState action;
    action.mutable_header()->set_server_receive_time_ns(kReceiveTimeNs);
    action.set_goal_id("goal-diagnostics");
    action.set_chassis_mode(6);
    store.updateActionState(action, kReceiveTimeNs);

    action.set_native_status(2);
    action.set_native_status_time_ns(kReceiveTimeNs + 10);
    action.set_feedback_progress(0.5);
    action.set_feedback_time_ns(kReceiveTimeNs + 11);
    action.mutable_header()->set_server_receive_time_ns(kReceiveTimeNs + 11);
    store.updateActionDiagnostics(action);

    const auto snapshot = store.buildSnapshot(kReceiveTimeNs + 11, 0);
    ASSERT_TRUE(snapshot.has_action_state());
    EXPECT_EQ(snapshot.action_state().header().sequence(), 1U);
    EXPECT_EQ(snapshot.action_state().header().server_receive_time_ns(),
              kReceiveTimeNs);
    EXPECT_EQ(snapshot.action_state().native_status(), 2);
    EXPECT_DOUBLE_EQ(snapshot.action_state().feedback_progress(), 0.5);
    ASSERT_EQ(snapshot.control_state_event_size(), 1);
    ASSERT_EQ(snapshot.runtime_state().topic_size(), 1);
    EXPECT_EQ(snapshot.runtime_state().topic(0).message_count(), 1U);
    EXPECT_EQ(snapshot.runtime_state().topic(0).last_update_time_ns(),
              kReceiveTimeNs);
}

TEST(SnapshotStoreTest, ClearsAndSuppressesPathsDuringCenterTurn)
{
    autoviz_server::SnapshotStore store({
        {"/chassis_command", "custom_msgs/msg/ChassisCommand", autoviz::DATA_KIND_CONTROL_COMMAND},
        {"/global_path", "nav_msgs/msg/Path", autoviz::DATA_KIND_GLOBAL_TRAJECTORY},
        {"/local_path", "custom_msgs/msg/TrajectoryMsg", autoviz::DATA_KIND_LOCAL_TRAJECTORY}});

    autoviz::Trajectory global;
    global.add_point()->mutable_path_point()->mutable_position()->set_x_m(1.0);
    autoviz::Trajectory local;
    local.add_point()->mutable_path_point()->mutable_position()->set_x_m(2.0);
    store.updateGlobalTrajectory(global, kReceiveTimeNs);
    store.updateLocalTrajectory(local, kReceiveTimeNs);

    autoviz::ControlCommand command;
    command.set_mode(autoviz::ControlCommand::MODE_CRAWL);
    command.set_maneuver(autoviz::ControlCommand::MANEUVER_YAW_IN_PLACE);
    command.set_target_gear(4);
    store.updateControlCommand(command, kReceiveTimeNs + 1);
    auto snapshot = store.buildSnapshot(kReceiveTimeNs + 1, 0);
    EXPECT_FALSE(snapshot.has_global_trajectory());
    EXPECT_FALSE(snapshot.has_local_trajectory());

    store.updateGlobalTrajectory(global, kReceiveTimeNs + 2);
    store.updateLocalTrajectory(local, kReceiveTimeNs + 2);
    snapshot = store.buildSnapshot(kReceiveTimeNs + 2, 0);
    EXPECT_FALSE(snapshot.has_global_trajectory());
    EXPECT_FALSE(snapshot.has_local_trajectory());
    EXPECT_EQ(snapshot.runtime_state().topic(1).message_count(), 2U);
    EXPECT_EQ(snapshot.runtime_state().topic(2).message_count(), 2U);

    command.set_maneuver(autoviz::ControlCommand::MANEUVER_NONE);
    command.set_target_gear(0);
    store.updateControlCommand(command, kReceiveTimeNs + 3);
    snapshot = store.buildSnapshot(kReceiveTimeNs + 3, 0);
    EXPECT_FALSE(snapshot.has_global_trajectory());
    EXPECT_FALSE(snapshot.has_local_trajectory());

    store.updateGlobalTrajectory(global, kReceiveTimeNs + 4);
    store.updateLocalTrajectory(local, kReceiveTimeNs + 4);
    snapshot = store.buildSnapshot(kReceiveTimeNs + 4, 0);
    EXPECT_TRUE(snapshot.has_global_trajectory());
    EXPECT_TRUE(snapshot.has_local_trajectory());

    autoviz_server::SnapshotStore timeoutStore({
        {"/chassis_command", "custom_msgs/msg/ChassisCommand", autoviz::DATA_KIND_CONTROL_COMMAND},
        {"/global_path", "nav_msgs/msg/Path", autoviz::DATA_KIND_GLOBAL_TRAJECTORY}});
    command.set_maneuver(autoviz::ControlCommand::MANEUVER_YAW_IN_PLACE);
    timeoutStore.updateControlCommand(command, kReceiveTimeNs);
    timeoutStore.expire(std::chrono::steady_clock::now() + std::chrono::seconds(6),
                        std::chrono::milliseconds(5000));
    timeoutStore.updateGlobalTrajectory(global, kReceiveTimeNs + 1);
    EXPECT_TRUE(timeoutStore.buildSnapshot(kReceiveTimeNs + 1, 0).has_global_trajectory());
}

}  // namespace
