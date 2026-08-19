#pragma once

#include "core/model/RuntimeStatusTypes.h"

namespace autoviz::ui::status {

struct ControlStatusSummary {
    bool hasCommand = false;
    int commandMode = 0;
    bool commandEnabled = false;
    double commandSpeed = 0.0;
    double commandHeading = 0.0;
    double commandAngularVelocity = 0.0;
    int commandGear = 0;

    bool hasChassisFeedback = false;
    int feedbackGear = 0;

    // The cmd/rev rows use localization for all motion feedback. Chassis
    // feedback remains the source for the independent gear row.
    bool hasLocalizationFeedback = false;
    double feedbackSpeed = 0.0;
    double feedbackAngularVelocity = 0.0;
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
    result.feedbackGear = chassis.gearStatus;

    result.hasLocalizationFeedback = localization.valid;
    result.feedbackSpeed = localization.velocity;
    result.feedbackAngularVelocity = localization.omegaZ;
    result.feedbackHeading = localization.heading;
    return result;
}

}  // namespace autoviz::ui::status
