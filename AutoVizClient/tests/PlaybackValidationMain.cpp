#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QTextStream>

#include <cmath>
#include <cstring>
#include <utility>

#include "core/network/ProtocolModelConverter.h"
#include "core/playback/RobotWsCdrDecoder.h"
#include "ui/status/ControlStatusSummary.h"

namespace {

void appendCdrByte(QByteArray* payload, quint8 value)
{
    payload->append(static_cast<char>(value));
}

void appendCdrFloat64(QByteArray* payload, double value)
{
    while ((payload->size() - 4) % 8 != 0) {
        payload->append('\0');
    }
    quint64 bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    for (int index = 0; index < 8; ++index) {
        appendCdrByte(payload, static_cast<quint8>((bits >> (index * 8)) & 0xffU));
    }
}

void appendCdrInt16(QByteArray* payload, qint16 value)
{
    while ((payload->size() - 4) % 2 != 0) {
        payload->append('\0');
    }
    appendCdrByte(payload, static_cast<quint8>(value & 0xff));
    appendCdrByte(payload, static_cast<quint8>((static_cast<quint16>(value) >> 8) & 0xff));
}

void appendZeroMotorState(QByteArray* payload)
{
    appendCdrFloat64(payload, 0.0);
    appendCdrFloat64(payload, 0.0);
    appendCdrInt16(payload, 0);
    appendCdrFloat64(payload, 0.0);
    appendCdrByte(payload, 1);
    appendCdrByte(payload, 0);
    appendCdrInt16(payload, 0);
    appendCdrInt16(payload, 0);
    appendCdrByte(payload, 0);
    appendCdrByte(payload, 0);
}

void appendZeroMotorCommand(QByteArray* payload)
{
    appendCdrByte(payload, 0);
    appendCdrByte(payload, 0);
    appendCdrByte(payload, 0);
    appendCdrFloat64(payload, 0.0);
    appendCdrFloat64(payload, 0.0);
}

bool runCurrentChassisCommandLayoutChecks(QTextStream& error)
{
    QByteArray payload(4, '\0');
    payload[1] = char(1);  // CDR_LE
    appendCdrByte(&payload, 4);       // mode: autonomous sailing depth hold
    appendCdrByte(&payload, 1);       // enabled
    appendCdrByte(&payload, 0);       // emergency ascent
    appendCdrFloat64(&payload, 0.8);  // speed
    appendCdrFloat64(&payload, -0.2); // angular velocity
    appendCdrByte(&payload, 1);       // expected gear
    appendCdrByte(&payload, 1);       // water actuator enabled
    appendCdrByte(&payload, 1);       // navigation mode: depth dependency
    appendCdrFloat64(&payload, 7.5);  // depth
    appendCdrFloat64(&payload, 2.0);  // height
    appendCdrFloat64(&payload, 0.3);  // heading
    appendCdrByte(&payload, static_cast<quint8>(-12));
    appendCdrByte(&payload, 34);
    appendCdrByte(&payload, 1);       // buoyancy fill
    appendCdrByte(&payload, 1);       // sonar power

    ::autoviz::VisualizationSnapshot snapshot;
    QString detail;
    if (payload.size() != 64
        || !autoviz::playback::RobotWsCdrDecoder::decode(
            "/chassis_command", "custom_msgs/msg/ChassisCommand", payload, 1, &snapshot, &detail)) {
        error << "current ChassisCommand CDR self-test failed: " << detail << '\n';
        return false;
    }
    const auto& command = snapshot.control_command();
    if (command.mode() != ::autoviz::ControlCommand::MODE_SAILING
        || command.target_gear() != 1
        || std::abs(command.underwater().target_depth_m() - 7.5) > 1.0e-12
        || command.underwater().left_thruster_command() != -12
        || command.underwater().right_thruster_command() != 34
        || !command.underwater().sonar_power_enabled()) {
        error << "current ChassisCommand CDR values self-test failed\n";
        return false;
    }
    return true;
}

bool runModeGearIndependenceChecks(QTextStream& error)
{
    QByteArray payload(4, '\0');
    payload[1] = char(1);  // CDR_LE
    appendCdrByte(&payload, 6);       // autonomous crawl
    appendCdrByte(&payload, 1);       // enabled
    appendCdrByte(&payload, 0);       // emergency ascent
    appendCdrFloat64(&payload, 0.0);
    appendCdrFloat64(&payload, 0.2);
    appendCdrByte(&payload, 4);       // center-turn gear, independent of mode
    appendCdrByte(&payload, 0);
    appendCdrByte(&payload, 0);
    appendCdrFloat64(&payload, 0.0);
    appendCdrFloat64(&payload, 0.0);
    appendCdrFloat64(&payload, 0.0);
    appendCdrByte(&payload, 0);
    appendCdrByte(&payload, 0);
    appendCdrByte(&payload, 0);
    appendCdrByte(&payload, 0);

    ::autoviz::VisualizationSnapshot wireSnapshot;
    QString detail;
    if (!autoviz::playback::RobotWsCdrDecoder::decode(
            "/chassis_command", "custom_msgs/msg/ChassisCommand", payload, 1,
            &wireSnapshot, &detail)) {
        error << "mode/gear independence CDR self-test failed: " << detail << '\n';
        return false;
    }
    const auto modelSnapshot = autoviz::network::ProtocolModelConverter::toModelSnapshot(wireSnapshot);
    const bool ok = wireSnapshot.control_command().maneuver()
                        == ::autoviz::ControlCommand::MANEUVER_NONE
                    && modelSnapshot.controlCommandStatus.mode == 6
                    && modelSnapshot.controlCommandStatus.expectedGear == 4;
    if (!ok) {
        error << "mode=6/gear=4 was incorrectly inferred as center turn\n";
    }
    return ok;
}

bool runLegacyChassisPassThroughChecks(QTextStream& error)
{
    QByteArray payload(4, '\0');
    payload[1] = char(1);  // CDR_LE
    for (int index = 0; index < 5; ++index) appendCdrByte(&payload, 0);
    appendCdrByte(&payload, 1);  // water heartbeat
    appendCdrByte(&payload, 0);  // tank level
    appendCdrByte(&payload, 0);  // tank level is raw
    appendCdrByte(&payload, 0);  // tank state
    appendCdrByte(&payload, 2);  // gear
    appendCdrFloat64(&payload, -0.48);
    appendCdrFloat64(&payload, -0.22);  // robot_ws 原始反馈值，回放保持不变
    appendCdrByte(&payload, 0);         // left actuator fault
    appendCdrByte(&payload, 0);         // right actuator fault
    appendCdrByte(&payload, 1);         // crawl heartbeat
    appendZeroMotorState(&payload);
    appendZeroMotorState(&payload);
    appendZeroMotorCommand(&payload);
    appendZeroMotorCommand(&payload);

    for (int index = 0; index < 4; ++index) appendCdrByte(&payload, 0);
    appendCdrFloat64(&payload, 24.0);
    appendCdrFloat64(&payload, 0.0);
    appendCdrByte(&payload, 0);
    appendCdrFloat64(&payload, 0.0);
    appendCdrByte(&payload, 0);
    appendCdrFloat64(&payload, 0.0);
    appendCdrByte(&payload, 0);
    appendCdrByte(&payload, 0);
    appendCdrByte(&payload, 0);
    appendCdrByte(&payload, 0);
    for (int index = 0; index < 12; ++index) appendCdrByte(&payload, 0);
    appendCdrByte(&payload, 1);  // DCDC enabled
    for (int index = 0; index < 16; ++index) appendCdrByte(&payload, 0);
    appendCdrByte(&payload, 80);  // SOC
    appendCdrFloat64(&payload, 24.0);
    appendCdrByte(&payload, 0);   // emergency ascent

    ::autoviz::VisualizationSnapshot snapshot;
    QString detail;
    if (!autoviz::playback::RobotWsCdrDecoder::decode(
            "/chassis_states", "custom_msgs/msg/ChassisStates", payload, 1,
            &snapshot, &detail)) {
        error << "legacy ChassisStates CDR self-test failed: " << detail << '\n';
        return false;
    }
    if (autoviz::playback::RobotWsCdrDecoder::decode(
            "/chassis_states", "custom_msgs/msg/ChassisStates", payload.left(payload.size() - 1), 1,
            &snapshot, &detail)) {
        error << "invalid ChassisStates CDR length was accepted\n";
        return false;
    }
    const auto modelSnapshot = autoviz::network::ProtocolModelConverter::toModelSnapshot(snapshot);
    const bool ok = std::abs(snapshot.chassis_state().yaw_rate_radps() + 0.22) < 1.0e-12
                    && std::abs(modelSnapshot.chassisRuntimeStatus.currentAngularVelocity + 0.22) < 1.0e-12
                    && snapshot.chassis_state().tail_thruster_motor_size() == 0
                    && std::abs(modelSnapshot.chassisRuntimeStatus.currentSpeed + 0.48) < 1.0e-12;
    if (!ok) {
        error << "legacy ChassisStates was not passed through unchanged\n";
    }
    return ok;
}

bool runControlStatusSummaryChecks(QTextStream& error)
{
    ::autoviz::VisualizationSnapshot wireSnapshot;
    auto* command = wireSnapshot.mutable_control_command();
    command->mutable_header()->set_server_receive_time_ns(1234000000ULL);
    command->mutable_header()->set_sequence(42);
    command->set_mode(::autoviz::ControlCommand::MODE_CRAWL);
    command->set_enabled(true);
    command->set_target_speed_mps(0.5144444704);
    command->set_target_heading_rad(0.75);
    command->set_target_yaw_rate_radps(0.369556);
    command->set_target_gear(2);

    auto* action = wireSnapshot.mutable_action_state();
    action->set_state(1);
    action->set_chassis_mode(6);
    action->set_enabled(true);
    action->set_target_speed_mps(0.0);
    action->set_target_heading_rad(0.0);
    action->set_target_yaw_rate_radps(0.0);

    auto* chassis = wireSnapshot.mutable_chassis_state();
    chassis->mutable_header()->set_server_receive_time_ns(1235000000ULL);
    chassis->mutable_header()->set_sequence(43);
    chassis->set_speed_mps(-0.48);
    chassis->set_yaw_rate_radps(0.22);
    chassis->set_gear(2);
    auto* location = wireSnapshot.mutable_vehicle_state();
    location->set_heading_rad(1.25);
    location->set_speed_mps(0.73);
    location->set_yaw_rate_radps(-0.31);

    const auto snapshot = autoviz::network::ProtocolModelConverter::toModelSnapshot(wireSnapshot);
    const auto summary = autoviz::ui::status::makeControlStatusSummary(
        snapshot.controlCommandStatus,
        snapshot.chassisRuntimeStatus,
        snapshot.localizationStatus);
    const bool ok = summary.hasCommand && summary.commandMode == 6
                    && summary.commandEnabled && summary.commandGear == 2
                    && std::abs(summary.commandSpeed - 0.5144444704) < 1.0e-12
                    && std::abs(summary.commandHeading - 0.75) < 1.0e-12
                    && std::abs(summary.commandAngularVelocity - 0.369556) < 1.0e-12
                    && snapshot.controlCommandStatus.header.receiveTimestamp == 1234
                    && snapshot.controlCommandStatus.header.sourceTimestamp == 0
                    && snapshot.controlCommandStatus.header.sequence == 42
                    && summary.hasChassisFeedback && summary.feedbackGear == 2
                    && std::abs(summary.feedbackSpeed - 0.73) < 1.0e-12
                    && std::abs(summary.feedbackAngularVelocity + 0.31) < 1.0e-12
                    && snapshot.chassisRuntimeStatus.header.receiveTimestamp == 1235
                    && snapshot.chassisRuntimeStatus.header.sourceTimestamp == 0
                    && snapshot.chassisRuntimeStatus.header.sequence == 43
                    && summary.hasLocalizationFeedback
                    && std::abs(summary.feedbackHeading - 1.25) < 1.0e-12;
    if (!ok) {
        error << "control status summary selected Action expectations instead of command/feedback topics\n";
    }
    return ok;
}

bool runCdrSafetyChecks(QTextStream& error)
{
    for (const bool littleEndian : {false, true}) {
        QByteArray payload(64, 0);
        payload[1] = littleEndian ? char(1) : char(0);
        ::autoviz::VisualizationSnapshot snapshot;
        QString detail;
        if (!autoviz::playback::RobotWsCdrDecoder::decode("/task_params", "custom_msgs/msg/TaskParams", payload, 1, &snapshot, &detail)) {
            error << "CDR endian self-test failed: " << detail << '\n';
            return false;
        }
        if (autoviz::playback::RobotWsCdrDecoder::decode("/task_params", "custom_msgs/msg/TaskParams", payload.left(12), 1, &snapshot, &detail)) {
            error << "CDR truncation self-test failed\n";
            return false;
        }
        payload[6] = char(2);
        if (autoviz::playback::RobotWsCdrDecoder::decode("/task_params", "custom_msgs/msg/TaskParams", payload, 1, &snapshot, &detail)) {
            error << "CDR bool validation self-test failed\n";
            return false;
        }
    }
    return true;
}

bool runCenterTurnConversionChecks(QTextStream& error)
{
    for (const auto [platform, expectedMode] : {
             std::pair{::autoviz::ControlCommand::MODE_SAILING, 10},
             std::pair{::autoviz::ControlCommand::MODE_CRAWL, 11}}) {
        ::autoviz::VisualizationSnapshot wireSnapshot;
        auto* command = wireSnapshot.mutable_control_command();
        command->set_mode(platform);
        command->set_maneuver(::autoviz::ControlCommand::MANEUVER_YAW_IN_PLACE);
        const auto modelSnapshot = autoviz::network::ProtocolModelConverter::toModelSnapshot(wireSnapshot);
        if (modelSnapshot.controlCommandStatus.mode != expectedMode) {
            error << "center-turn conversion expected mode " << expectedMode << ", got "
                  << modelSnapshot.controlCommandStatus.mode << '\n';
            return false;
        }
    }
    return true;
}

bool runVerticalControlConversionChecks(QTextStream& error)
{
    for (const auto [verticalMode, expectedChassisMode] : {
             std::pair{::autoviz::VERTICAL_CONTROL_MODE_DEPTH_HOLD, 4},
             std::pair{::autoviz::VERTICAL_CONTROL_MODE_HEIGHT_HOLD, 5}}) {
        ::autoviz::VisualizationSnapshot wireSnapshot;
        auto* command = wireSnapshot.mutable_control_command();
        command->set_mode(::autoviz::ControlCommand::MODE_SAILING);
        command->mutable_underwater()->set_vertical_control_mode(verticalMode);
        const auto modelSnapshot = autoviz::network::ProtocolModelConverter::toModelSnapshot(wireSnapshot);
        if (modelSnapshot.controlCommandStatus.mode != expectedChassisMode) {
            error << "vertical conversion expected mode " << expectedChassisMode << ", got "
                  << modelSnapshot.controlCommandStatus.mode << '\n';
            return false;
        }
    }
    return true;
}

bool runActionClassificationChecks(QTextStream& error)
{
    struct ActionCase {
        int owner;
        int chassisMode;
        int navigationMode;
        ::autoviz::VerticalControlMode verticalMode;
        autoviz::model::RunVisualizationMode expected;
    };
    const ActionCase cases[] = {
        {1, 4, 1, ::autoviz::VERTICAL_CONTROL_MODE_NONE, autoviz::model::RunVisualizationMode::HorizontalMotion},
        {1, 5, 2, ::autoviz::VERTICAL_CONTROL_MODE_NONE, autoviz::model::RunVisualizationMode::HorizontalMotion},
        {1, 10, 1, ::autoviz::VERTICAL_CONTROL_MODE_NONE, autoviz::model::RunVisualizationMode::HorizontalMotion},
        {2, 1, 0, ::autoviz::VERTICAL_CONTROL_MODE_DEPTH_HOLD, autoviz::model::RunVisualizationMode::VerticalMotion},
        {2, 2, 0, ::autoviz::VERTICAL_CONTROL_MODE_HEIGHT_HOLD, autoviz::model::RunVisualizationMode::VerticalMotion},
    };
    for (const auto& item : cases) {
        ::autoviz::VisualizationSnapshot wireSnapshot;
        auto* action = wireSnapshot.mutable_action_state();
        action->set_owner(item.owner);
        action->set_state(1);
        action->set_chassis_mode(item.chassisMode);
        action->set_navigation_mode(item.navigationMode);
        action->mutable_underwater()->set_vertical_control_mode(item.verticalMode);
        const auto modelSnapshot = autoviz::network::ProtocolModelConverter::toModelSnapshot(wireSnapshot);
        if (modelSnapshot.runVisualizationMode != item.expected) {
            error << "action classification self-test failed for owner=" << item.owner
                  << ", chassis_mode=" << item.chassisMode << '\n';
            return false;
        }
    }
    return true;
}

bool runActionDiagnosticConversionChecks(QTextStream& error)
{
    ::autoviz::VisualizationSnapshot wireSnapshot;
    auto* action = wireSnapshot.mutable_action_state();
    action->set_goal_id("current-goal");
    action->set_action_name("custom_msgs/action/DepthCommand");
    action->set_state(1);
    action->set_message("executing");
    action->set_native_status(2);
    action->set_native_status_time_ns(1000000000ULL);
    action->set_feedback_progress(0.25);
    action->set_feedback_time_ns(2000000000ULL);
    auto* terminal = action->mutable_recent_terminal();
    terminal->set_goal_id("previous-goal");
    terminal->set_action_name("custom_msgs/action/Move");
    terminal->set_state(3);
    terminal->set_message("completed");
    const auto modelSnapshot = autoviz::network::ProtocolModelConverter::toModelSnapshot(wireSnapshot);
    const auto& current = modelSnapshot.actionRuntimeStatus;
    const auto& recent = modelSnapshot.recentTerminalActionStatus;
    if (!current.hasNativeStatus || current.nativeStatus != 2 || !current.hasFeedbackProgress
        || std::abs(current.feedbackProgress - 0.25) > 1.0e-12
        || current.message != QStringLiteral("executing")
        || recent.goalUuid != QStringLiteral("previous-goal") || recent.state != 3) {
        error << "action diagnostic conversion self-test failed\n";
        return false;
    }
    return true;
}

bool runSharedFieldConversionChecks(QTextStream& error)
{
    // Server TCP snapshots and the local CDR decoder both enter the Client through
    // VisualizationSnapshot, so this protects their shared model/UI contract.
    ::autoviz::VisualizationSnapshot wireSnapshot;
    auto* vehicle = wireSnapshot.mutable_vehicle_state();
    vehicle->set_longitude_deg(120.123456);
    vehicle->set_latitude_deg(30.654321);
    auto* underwater = vehicle->mutable_underwater();
    underwater->set_usbl_x_m(9.5);
    underwater->set_usbl_y_m(-1.25);
    underwater->set_usbl_z_m(4.75);

    auto* remote = wireSnapshot.mutable_task_state()->mutable_remote_control();
    remote->set_crawl_gear(2);
    remote->set_crawl_speed_mps(0.75);
    remote->set_crawl_angular_velocity_radps(-0.2);
    remote->set_forward_percent(60);
    remote->set_turn_percent(-20);
    remote->set_dive_percent(15);
    remote->set_left_tail_actuator_speed(-90);
    remote->set_right_tail_actuator_speed(91);
    remote->set_left_vertical_actuator_speed(-30);
    remote->set_right_vertical_actuator_speed(31);
    remote->set_back_vertical_actuator_speed(32);
    for (int index = 0; index < 16; ++index) {
        remote->add_power_supply_enabled(index == 0 || index == 15);
    }

    const auto modelSnapshot = autoviz::network::ProtocolModelConverter::toModelSnapshot(wireSnapshot);
    const auto& location = modelSnapshot.localizationStatus;
    const auto& task = modelSnapshot.taskRuntimeStatus;
    const bool valuesMatch = location.hasLongitude && location.hasLatitude
                             && location.hasUsblX && location.hasUsblY && location.hasUsblZ
                             && std::abs(location.longitude - 120.123456) < 1.0e-12
                             && std::abs(location.latitude - 30.654321) < 1.0e-12
                             && std::abs(location.usblX - 9.5) < 1.0e-12
                             && std::abs(location.usblY + 1.25) < 1.0e-12
                             && std::abs(location.usblZ - 4.75) < 1.0e-12
                             && task.hasRemoteControl && task.crawlGear == 2
                             && std::abs(task.crawlSpeed - 0.75) < 1.0e-12
                             && std::abs(task.crawlAngularVelocity + 0.2) < 1.0e-12
                             && task.forwardPercent == 60 && task.turnPercent == -20
                             && task.divePercent == 15 && task.leftTailActuatorSpeed == -90
                             && task.rightTailActuatorSpeed == 91
                             && task.leftVerticalActuatorSpeed == -30
                             && task.rightVerticalActuatorSpeed == 31
                             && task.backVerticalActuatorSpeed == 32
                             && task.powerSupplyCommands.size() == 16
                             && task.powerSupplyCommands.front() == 1
                             && task.powerSupplyCommands.back() == 1
                             && task.powerSupplyCommands.at(1) == 0;

    const auto absentModel = autoviz::network::ProtocolModelConverter::toModelSnapshot(
        ::autoviz::VisualizationSnapshot{});
    const bool absencePreserved = !absentModel.localizationStatus.hasLongitude
                                 && !absentModel.localizationStatus.hasLatitude
                                 && !absentModel.localizationStatus.hasUsblX
                                 && !absentModel.localizationStatus.hasUsblY
                                 && !absentModel.localizationStatus.hasUsblZ
                                 && !absentModel.taskRuntimeStatus.hasRemoteControl;
    if (!valuesMatch || !absencePreserved) {
        error << "shared Server/bag field conversion self-test failed\n";
    }
    return valuesMatch;
}

bool runGoalUuidNormalizationChecks(QTextStream& error)
{
    // robot_ws 用 %x 逐字节格式化 SystemRunStates.goal_uuid，会丢掉字节前导零。
    const QString canonical = QStringLiteral("c5cc4cd52699c9c14e81484088068456");
    const QString lossy = QStringLiteral("c5cc4cd52699c9c14e8148408868456");
    const bool ok =
        autoviz::playback::RobotWsCdrDecoder::sameGoalUuid(canonical, lossy)
        && autoviz::playback::RobotWsCdrDecoder::sameGoalUuid(lossy, canonical)
        && autoviz::playback::RobotWsCdrDecoder::sameGoalUuid(canonical, canonical)
        && !autoviz::playback::RobotWsCdrDecoder::sameGoalUuid(
            canonical, QStringLiteral("c5cc4cd52699c9c14e81484088068457"))
        && !autoviz::playback::RobotWsCdrDecoder::sameGoalUuid(
            lossy, QStringLiteral("c5cc4cd52699c9c14e8148408868457"));
    if (!ok) {
        error << "goal UUID normalization self-test failed\n";
    }
    return ok;
}

}  // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream error(stderr);
    if (!runCdrSafetyChecks(error)) {
        return 1;
    }
    if (!runCurrentChassisCommandLayoutChecks(error)) {
        return 1;
    }
    if (!runModeGearIndependenceChecks(error)) {
        return 1;
    }
    if (!runLegacyChassisPassThroughChecks(error)) {
        return 1;
    }
    if (!runControlStatusSummaryChecks(error)) {
        return 1;
    }
    if (!runCenterTurnConversionChecks(error)) {
        return 1;
    }
    if (!runVerticalControlConversionChecks(error)) {
        return 1;
    }
    if (!runActionClassificationChecks(error)) {
        return 1;
    }
    if (!runActionDiagnosticConversionChecks(error)) {
        return 1;
    }
    if (!runSharedFieldConversionChecks(error)) {
        return 1;
    }
    if (!runGoalUuidNormalizationChecks(error)) {
        return 1;
    }
    out << "CDR, command-summary source selection, legacy chassis pass-through, ChassisCommand layout, action classification, center-turn, vertical-control, action-diagnostic, shared Server/bag fields, and goal-UUID normalization self-tests: OK\n";

    const QStringList arguments = app.arguments().mid(1);
    if (arguments.isEmpty()) {
        error << "usage: AutoVizClientPlaybackTests BAG_DIR...\n";
        return 2;
    }

    int failures = 0;
    for (const auto& argument : arguments) {
        const QDir directory(argument);
        const auto files = directory.entryList({"*.db3"}, QDir::Files, QDir::Name);
        if (files.isEmpty()) {
            error << directory.absolutePath() << ": no db3\n";
            ++failures;
            continue;
        }

        qint64 total = 0;
        qint64 commandCenterTurnMessages = 0;
        qint64 invalidDisabledCommandMessages = 0;
        qint64 nonZeroCommandYawMessages = 0;
        qint64 actionCenterTurnMessages = 0;
        qint64 hiddenStatusMessages = 0;
        qint64 hiddenFeedbackMessages = 0;
        qint64 actionWithNativeStatus = 0;
        qint64 actionWithFeedback = 0;
        qint64 chassisWithTailTelemetry = 0;
        autoviz::playback::RobotWsCdrDecoder::ActionDiagnosticCache diagnostics;
        QStringList quotedTopics;
        for (const auto& topic : autoviz::playback::RobotWsCdrDecoder::supportedTopics()) {
            quotedTopics << QStringLiteral("'%1'").arg(topic);
        }
        const QString topicInList = QStringLiteral("(%1)").arg(quotedTopics.join(','));
        for (const auto& file : files) {
            const QString connectionName = QStringLiteral("validation_%1_%2").arg(argument).arg(file);
            {
                QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", connectionName);
                database.setDatabaseName(directory.filePath(file));
                if (!database.open()) {
                    error << file << ": " << database.lastError().text() << '\n';
                    ++failures;
                    continue;
                }
                QSqlQuery quickCheck(database);
                if (!quickCheck.exec("PRAGMA quick_check") || !quickCheck.next() || quickCheck.value(0).toString() != "ok") {
                    error << file << ": quick_check failed\n";
                    ++failures;
                    continue;
                }

                QSqlQuery query(database);
                query.setForwardOnly(true);
                const QString decodeSql = QStringLiteral(
                    "SELECT m.timestamp,t.name,t.type,m.data FROM messages m "
                    "JOIN topics t ON t.id=m.topic_id WHERE t.name IN %1 ORDER BY m.timestamp")
                    .arg(topicInList);
                if (!query.exec(decodeSql)) {
                    error << query.lastError().text() << '\n';
                    ++failures;
                    continue;
                }

                while (query.next()) {
                    const QString topic = query.value(1).toString();
                    ::autoviz::VisualizationSnapshot wireSnapshot;
                    QString detail;
                    if (!autoviz::playback::RobotWsCdrDecoder::decode(topic, query.value(2).toString(), query.value(3).toByteArray(), query.value(0).toULongLong(), &wireSnapshot, &detail, &diagnostics)) {
                        error << directory.dirName() << ' ' << topic << " row " << (total + 1) << ": " << detail << '\n';
                        ++failures;
                        break;
                    }
                    if (wireSnapshot.has_control_command()
                        && wireSnapshot.control_command().maneuver() == ::autoviz::ControlCommand::MANEUVER_YAW_IN_PLACE) {
                        const auto modelSnapshot = autoviz::network::ProtocolModelConverter::toModelSnapshot(wireSnapshot);
                        const int expectedMode = wireSnapshot.control_command().mode() == ::autoviz::ControlCommand::MODE_SAILING ? 10 : 11;
                        if (modelSnapshot.controlCommandStatus.mode != expectedMode) {
                            error << directory.dirName() << ": center-turn conversion expected mode " << expectedMode
                                  << ", got " << modelSnapshot.controlCommandStatus.mode << '\n';
                            ++failures;
                            break;
                        }
                        ++commandCenterTurnMessages;
                    }
                    if (wireSnapshot.has_control_command()) {
                        const auto modelSnapshot = autoviz::network::ProtocolModelConverter::toModelSnapshot(wireSnapshot);
                        if (wireSnapshot.control_command().mode() == ::autoviz::ControlCommand::MODE_UNKNOWN
                            && !wireSnapshot.control_command().enabled()) {
                            if (!modelSnapshot.controlCommandStatus.valid
                                || modelSnapshot.controlCommandStatus.mode != 0
                                || modelSnapshot.controlCommandStatus.isEnable) {
                                error << directory.dirName() << ": invalid disabled command changed during model conversion\n";
                                ++failures;
                                break;
                            }
                            ++invalidDisabledCommandMessages;
                        }
                        const double wireYawRate = wireSnapshot.control_command().target_yaw_rate_radps();
                        if (std::abs(modelSnapshot.controlCmd.desiredAngularVelocity - wireYawRate) > 1.0e-12
                            || std::abs(modelSnapshot.controlCommandStatus.angularVelocity - wireYawRate) > 1.0e-12) {
                            error << directory.dirName() << ": command angular velocity changed during model conversion\n";
                            ++failures;
                            break;
                        }
                        if (std::abs(wireYawRate) > 1.0e-9) {
                            ++nonZeroCommandYawMessages;
                        }
                    }
                    if (wireSnapshot.has_action_state()
                        && (wireSnapshot.action_state().chassis_mode() == 10
                            || wireSnapshot.action_state().chassis_mode() == 11)) {
                        const auto modelSnapshot = autoviz::network::ProtocolModelConverter::toModelSnapshot(wireSnapshot);
                        if (modelSnapshot.actionRuntimeStatus.chassisMode
                            != wireSnapshot.action_state().chassis_mode()) {
                            error << directory.dirName() << ": action center-turn mode was not preserved\n";
                            ++failures;
                            break;
                        }
                        ++actionCenterTurnMessages;
                    }
                    if (topic == QLatin1String("/system_run_states")) {
                        if (wireSnapshot.has_action_state()) {
                            if (wireSnapshot.action_state().has_native_status()) ++actionWithNativeStatus;
                            if (wireSnapshot.action_state().has_feedback_progress()) ++actionWithFeedback;
                        }
                    } else if (topic.endsWith(QLatin1String("/_action/status"))) {
                        ++hiddenStatusMessages;
                    } else if (topic.endsWith(QLatin1String("/_action/feedback"))) {
                        ++hiddenFeedbackMessages;
                    }
                    if (topic == QLatin1String("/chassis_states")
                        && wireSnapshot.has_chassis_state()
                        && wireSnapshot.chassis_state().tail_thruster_motor_size() > 0) {
                        ++chassisWithTailTelemetry;
                    }
                    ++total;
                }
                database.close();
            }
            QSqlDatabase::removeDatabase(connectionName);
            if (failures) {
                break;
            }
        }
        if (!failures) {
            out << directory.dirName() << ": OK, " << total << " supported messages, "
                << commandCenterTurnMessages << " command center-turn messages, "
                << invalidDisabledCommandMessages << " invalid disabled command messages, "
                << nonZeroCommandYawMessages << " non-zero command yaw-rate messages, "
                << actionCenterTurnMessages << " action center-turn messages, "
                << "hidden[status:" << hiddenStatusMessages
                << ",feedback:" << hiddenFeedbackMessages << "], "
                << "action[native_status:" << actionWithNativeStatus
                << ",feedback:" << actionWithFeedback << "], chassis_tail:" << chassisWithTailTelemetry << "\n";
        }
    }
    return failures ? 1 : 0;
}
