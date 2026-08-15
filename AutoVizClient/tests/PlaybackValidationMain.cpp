#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>

#include <cmath>
#include <utility>

#include "core/network/ProtocolModelConverter.h"
#include "core/playback/RobotWsCdrDecoder.h"

namespace {

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

}  // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream error(stderr);
    if (!runCdrSafetyChecks(error)) {
        return 1;
    }
    if (!runCenterTurnConversionChecks(error)) {
        return 1;
    }
    if (!runVerticalControlConversionChecks(error)) {
        return 1;
    }
    if (!runActionDiagnosticConversionChecks(error)) {
        return 1;
    }
    out << "CDR, center-turn, vertical-control, and action-diagnostic conversion self-tests: OK\n";

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
        qint64 actionCenterTurnMessages = 0;
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
                if (!query.exec("SELECT m.timestamp,t.name,t.type,m.data FROM messages m "
                                "JOIN topics t ON t.id=m.topic_id WHERE t.name IN "
                                "('/location','/targets/final_objects','/chassis_command',"
                                "'/chassis_states','/system_run_states','/task_params',"
                                "'/local_path','/global_path') ORDER BY m.timestamp")) {
                    error << query.lastError().text() << '\n';
                    ++failures;
                    continue;
                }

                while (query.next()) {
                    const QString topic = query.value(1).toString();
                    ::autoviz::VisualizationSnapshot wireSnapshot;
                    QString detail;
                    if (!autoviz::playback::RobotWsCdrDecoder::decode(topic, query.value(2).toString(), query.value(3).toByteArray(), query.value(0).toULongLong(), &wireSnapshot, &detail)) {
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
                << actionCenterTurnMessages << " action center-turn messages\n";
        }
    }
    return failures ? 1 : 0;
}
