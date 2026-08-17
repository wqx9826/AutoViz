#pragma once

#include "core/model/RuntimeStatusTypes.h"

namespace autoviz::ui::status {

struct ControlStatusSummary {
    bool replayingTransition = false;
    bool hasCommand = false;
    int commandMode = 0;
    bool commandEnabled = false;
    double commandSpeed = 0.0;
    double commandHeading = 0.0;
    double commandAngularVelocity = 0.0;
    int commandGear = 0;

    bool hasChassisFeedback = false;
    double feedbackSpeed = 0.0;
    double feedbackAngularVelocity = 0.0;
    int feedbackGear = 0;

    bool hasHeadingFeedback = false;
    double feedbackHeading = 0.0;
};

inline ControlStatusSummary makeControlStatusSummary(
    const model::ControlCommandStatus& command,
    const model::ChassisRuntimeStatus& chassis,
    const model::LocalizationStatus& localization)
{
    ControlStatusSummary result;
    result.hasCommand = command.valid;
    result.commandMode = command.mode;
    result.commandEnabled = command.isEnable;
    result.commandSpeed = command.speed;
    result.commandHeading = command.heading;
    result.commandAngularVelocity = command.angularVelocity;
    result.commandGear = command.expectedGear;

    result.hasChassisFeedback = chassis.valid;
    result.feedbackSpeed = chassis.currentSpeed;
    result.feedbackAngularVelocity = chassis.currentAngularVelocity;
    result.feedbackGear = chassis.gearStatus;

    result.hasHeadingFeedback = localization.valid;
    result.feedbackHeading = localization.heading;
    return result;
}

}  // namespace autoviz::ui::status
