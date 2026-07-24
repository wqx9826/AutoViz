#include "core/model/PathTypes.h"

#include <cmath>

namespace autoviz::model {

namespace {
Trajectory createArcTrajectory(double lateralOffset, double headingBias, int count)
{
    Trajectory path;
    path.header.timestamp = 1712800000000;
    path.header.frameId = QStringLiteral("map");
    for (int index = 0; index < count; ++index) {
        const double x = index * 2.5;
        const double y = std::sin(index * 0.16) * 6.0 + lateralOffset;
        TrajectoryPoint point;
        point.position = {x, y};
        point.theta = 0.03 * index + headingBias;
        point.kappa = 0.01 * std::sin(index * 0.08);
        point.velocity = 0.38;
        point.acceleration = 0.03;
        point.s = x;
        point.relativeTime = index * 0.12;
        point.absoluteTime = path.header.timestamp / 1000.0 + point.relativeTime;
        path.points.push_back(point);
    }
    return path;
}
}  // namespace

Trajectory createMockGlobalPath()
{
    return createArcTrajectory(0.0, 0.0, 40);
}

Trajectory createMockLocalPath()
{
    return createArcTrajectory(-1.4, 0.04, 18);
}

ReferenceLine createMockReferenceLine()
{
    ReferenceLine line;
    line.header.timestamp = 1712800000000;
    line.header.frameId = QStringLiteral("map");
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
