#include "core/model/PathTypes.h"

#include <cmath>

namespace autoviz::model {

namespace {
Trajectory createArcTrajectory(double lateralOffset, double headingBias, int count)
{
    Trajectory path;
    path.meta.timestamp = 1712800000000;
    path.meta.frameId = QStringLiteral("map");
    for (int index = 0; index < count; ++index) {
        const double x = index * 2.5;
        const double y = std::sin(index * 0.16) * 6.0 + lateralOffset;
        TrajectoryPoint point;
        point.position = {x, y};
        point.theta = 0.03 * index + headingBias;
        point.kappa = 0.01 * std::sin(index * 0.08);
        point.velocity = 8.0;
        point.acceleration = 0.3;
        point.s = x;
        point.relativeTime = index * 0.12;
        point.absoluteTime = path.meta.timestamp / 1000.0 + point.relativeTime;
        path.points.push_back(point);
    }
    return path;
}
}  // namespace

Trajectory createMockGlobalPath()
{
    Trajectory path = createArcTrajectory(0.0, 0.0, 40);
    path.meta.sourceTopic = QStringLiteral("/planning/global_path");
    return path;
}

Trajectory createMockLocalPath()
{
    Trajectory path = createArcTrajectory(-1.4, 0.04, 18);
    path.meta.sourceTopic = QStringLiteral("/planning/local_path");
    return path;
}

ReferenceLine createMockReferenceLine()
{
    ReferenceLine line;
    line.meta.timestamp = 1712800000000;
    line.meta.frameId = QStringLiteral("map");
    line.meta.sourceTopic = QStringLiteral("/planning/reference_line");
    for (int index = 0; index < 34; ++index) {
        ReferencePoint point;
        point.position = {index * 3.0, std::sin(index * 0.15) * 4.0 + 1.5};
        point.theta = 0.028 * index;
        point.kappa = 0.006;
        point.s = index * 3.0;
        line.points.push_back(point);
    }
    return line;
}

}  // namespace autoviz::model
