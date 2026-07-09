#include "ui/charts/ControlPanelWidget.h"

#include <algorithm>
#include <cmath>

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>

#include "core/datacenter/DataManager.h"
#include "ui/charts/ControlPanelStyle.h"
#include "ui/charts/PlotCardWidget.h"
#include "ui/charts/StatusSummaryWidget.h"
#include "ui/theme/UiScaleManager.h"

namespace autoviz::ui::charts {

namespace {
constexpr qint64 kTimeoutMs = 1000;

double normalizeAngle(double angle)
{
    while (angle > M_PI) {
        angle -= 2.0 * M_PI;
    }
    while (angle < -M_PI) {
        angle += 2.0 * M_PI;
    }
    return angle;
}

bool calculatePathError(const autoviz::model::VehicleLocation& location,
                        const autoviz::model::Trajectory& path,
                        double* lateralError,
                        double* headingError)
{
    if (path.points.size() < 2 || lateralError == nullptr || headingError == nullptr) {
        return false;
    }

    double bestDistanceSquared = std::numeric_limits<double>::max();
    double bestSignedDistance = 0.0;
    double bestHeading = 0.0;

    for (int index = 0; index + 1 < path.points.size(); ++index) {
        const auto& start = path.points.at(index).position;
        const auto& end = path.points.at(index + 1).position;
        const double dx = end.x - start.x;
        const double dy = end.y - start.y;
        const double lengthSquared = dx * dx + dy * dy;
        if (lengthSquared <= 1.0e-9) {
            continue;
        }

        const double vx = location.position.x - start.x;
        const double vy = location.position.y - start.y;
        const double ratio = std::clamp((vx * dx + vy * dy) / lengthSquared, 0.0, 1.0);
        const double projectionX = start.x + ratio * dx;
        const double projectionY = start.y + ratio * dy;
        const double errorX = location.position.x - projectionX;
        const double errorY = location.position.y - projectionY;
        const double distanceSquared = errorX * errorX + errorY * errorY;

        if (distanceSquared < bestDistanceSquared) {
            bestDistanceSquared = distanceSquared;
            bestSignedDistance = (dx * vy - dy * vx) / std::sqrt(lengthSquared);
            bestHeading = std::atan2(dy, dx);
        }
    }

    if (bestDistanceSquared == std::numeric_limits<double>::max()) {
        return false;
    }

    *lateralError = bestSignedDistance;
    *headingError = normalizeAngle(location.heading - bestHeading);
    return true;
}

qint64 maxTimestamp(qint64 lhs, qint64 rhs)
{
    return std::max(lhs, rhs);
}
}  // namespace

ControlPanelWidget::ControlPanelWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    configurePlots();
    m_buffer.setWindowMs(30000);

    m_repaintTimer = new QTimer(this);
    m_repaintTimer->setInterval(100);
    connect(m_repaintTimer, &QTimer::timeout, this, [this]() { refreshPlots(); });
    m_repaintTimer->start();
}

void ControlPanelWidget::updateSnapshot(const autoviz::datacenter::VisualizationSnapshot& snapshot)
{
    m_latestData = buildDebugData(snapshot);
    if (!m_paused) {
        m_buffer.pushData(m_latestData);
    }
}

void ControlPanelWidget::setupUi()
{
    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    setMinimumWidth(scale.scaled(340));
    setFont(style::font());

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    root->addWidget(scrollArea);

    auto* content = new QWidget(scrollArea);
    content->setFont(style::font());
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(scale.spacingSmall(), scale.spacingSmall(), scale.spacingSmall(), scale.spacingSmall());
    layout->setSpacing(scale.spacingSmall());

    auto* title = new QLabel(tr("控制曲线面板"), content);
    title->setFont(style::panelTitleFont());
    title->setStyleSheet(QStringLiteral("font-weight: 700;"));
    layout->addWidget(title);

    m_statusSummary = new StatusSummaryWidget(content);
    layout->addWidget(m_statusSummary);

    auto* controls = new QFrame(content);
    controls->setObjectName(QStringLiteral("controlPanelToolbar"));
    controls->setFont(style::controlFont());
    auto* controlsLayout = new QHBoxLayout(controls);
    controlsLayout->setContentsMargins(scale.scaled(8), scale.spacingSmall(), scale.scaled(8), scale.spacingSmall());
    controlsLayout->setSpacing(scale.spacingSmall());
    controlsLayout->setAlignment(Qt::AlignVCenter);

    m_pauseButton = new QPushButton(tr("暂停"), controls);
    style::polishControls(m_pauseButton);
    m_pauseButton->setCheckable(true);
    m_clearButton = new QPushButton(tr("清空"), controls);
    style::polishControls(m_clearButton);
    m_windowCombo = new QComboBox(controls);
    style::polishControls(m_windowCombo);
    m_windowCombo->addItem(QStringLiteral("10s"), 10000);
    m_windowCombo->addItem(QStringLiteral("30s"), 30000);
    m_windowCombo->addItem(QStringLiteral("60s"), 60000);
    m_windowCombo->setCurrentIndex(1);
    m_windowCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_windowCombo->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    m_windowCombo->setMinimumWidth(m_windowCombo->minimumSizeHint().width() + scale.scaled(34));
    m_autoScaleCheck = new QCheckBox(tr("自动缩放"), controls);
    style::polishControls(m_autoScaleCheck);
    m_autoScaleCheck->setChecked(true);

    controlsLayout->addWidget(m_pauseButton, 0, Qt::AlignVCenter);
    controlsLayout->addWidget(m_clearButton, 0, Qt::AlignVCenter);
    controlsLayout->addStretch(1);
    auto* windowLabel = new QLabel(tr("窗口"), controls);
    windowLabel->setFont(style::controlFont());
    controlsLayout->addWidget(windowLabel, 0, Qt::AlignVCenter);
    controlsLayout->addWidget(m_windowCombo, 0, Qt::AlignVCenter);
    controlsLayout->addWidget(m_autoScaleCheck, 0, Qt::AlignVCenter);
    layout->addWidget(controls);

    m_speedPlot = new PlotCardWidget(content);
    m_yawPlot = new PlotCardWidget(content);
    m_pathErrorPlot = new PlotCardWidget(content);
    layout->addWidget(m_speedPlot);
    layout->addWidget(m_yawPlot);
    layout->addWidget(m_pathErrorPlot);
    layout->addStretch(1);

    scrollArea->setWidget(content);

    connect(m_pauseButton, &QPushButton::toggled, this, [this](bool checked) {
        m_paused = checked;
        m_pauseButton->setText(checked ? tr("继续") : tr("暂停"));
    });
    connect(m_clearButton, &QPushButton::clicked, this, [this]() { clearHistory(); });
    connect(m_windowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { setWindowFromCombo(); });
    connect(m_autoScaleCheck, &QCheckBox::toggled, this, [this]() {
        m_speedPlot->clearFrozenRange();
        m_yawPlot->clearFrozenRange();
        m_pathErrorPlot->clearFrozenRange();
        refreshPlots();
    });
}

void ControlPanelWidget::configurePlots()
{
    using Role = PlotCardWidget::ValueRole;
    using Axis = PlotCardWidget::AxisSide;

    m_speedPlot->configure(tr("速度跟踪"),
                           QStringLiteral("m/s"),
                           QString(),
                           {{QStringLiteral("cmd_speed"), QColor("#2563EB"), Role::CmdSpeed, Axis::Left, 2.3},
                            {QStringLiteral("feedback_speed"), QColor("#059669"), Role::FeedbackSpeed, Axis::Left, 2.1},
                            {QStringLiteral("speed_error"), QColor("#DC2626"), Role::SpeedError, Axis::Left, 1.6}});

    m_yawPlot->configure(tr("航向跟踪"),
                         QStringLiteral("yaw / rad"),
                         QString(),
                         {{QStringLiteral("cmd_yaw"), QColor("#4F46E5"), Role::CmdYaw, Axis::Left, 2.2},
                          {QStringLiteral("feedback_yaw"), QColor("#16A34A"), Role::FeedbackYaw, Axis::Left, 2.0},
                          {QStringLiteral("yaw_error"), QColor("#F97316"), Role::YawError, Axis::Left, 1.7}});

    m_pathErrorPlot->configure(tr("路径误差"),
                               QStringLiteral("lateral / m"),
                               QStringLiteral("body-path / rad"),
                               {{QStringLiteral("lateral_error"), QColor("#DC2626"), Role::LateralError, Axis::Left, 2.0},
                                {QStringLiteral("heading_to_path"), QColor("#0F766E"), Role::PathYawError, Axis::Right, 1.8}});
}

void ControlPanelWidget::refreshPlots()
{
    m_statusSummary->setData(m_latestData);
    const auto samples = m_buffer.samples();
    const bool autoScale = m_autoScaleCheck->isChecked();
    const qint64 windowMs = m_buffer.windowMs();
    m_speedPlot->setSamples(samples, m_latestData, windowMs, autoScale);
    m_yawPlot->setSamples(samples, m_latestData, windowMs, autoScale);
    m_pathErrorPlot->setSamples(samples, m_latestData, windowMs, autoScale);
}

void ControlPanelWidget::clearHistory()
{
    m_buffer.clear();
    m_firstSampleTimestampMs = 0;
    m_speedPlot->clearFrozenRange();
    m_yawPlot->clearFrozenRange();
    m_pathErrorPlot->clearFrozenRange();
    refreshPlots();
}

void ControlPanelWidget::setWindowFromCombo()
{
    m_buffer.setWindowMs(m_windowCombo->currentData().toLongLong());
    m_speedPlot->clearFrozenRange();
    m_yawPlot->clearFrozenRange();
    m_pathErrorPlot->clearFrozenRange();
    refreshPlots();
}

ControlDebugData ControlPanelWidget::buildDebugData(const autoviz::datacenter::VisualizationSnapshot& snapshot)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    ControlDebugData data;
    data.timestampMs = nowMs;
    if (m_firstSampleTimestampMs == 0) {
        m_firstSampleTimestampMs = nowMs;
    }
    data.elapsedMs = nowMs - m_firstSampleTimestampMs;

    const auto& status = snapshot.runtimeStatus;
    const auto& command = snapshot.controlCmd;
    const bool hasControl = status.hasControlCmdData && command.mode != autoviz::model::ControlMode::Unknown;
    const bool hasLocation = status.hasVehicleLocationData;
    const bool hasChassis = status.hasVehicleChassisData;

    data.sourceTimestampMs = maxTimestamp(command.header.timestamp, snapshot.vehicleLocation.header.timestamp);
    data.sourceTimestampMs = maxTimestamp(data.sourceTimestampMs, snapshot.vehicleChassisInfo.header.timestamp);
    if (data.sourceTimestampMs == 0) {
        data.sourceTimestampMs = nowMs;
    }
    data.timedOut = nowMs - data.sourceTimestampMs > kTimeoutMs;
    data.mode = data.timedOut ? ControlDebugMode::Error : (hasControl ? ControlDebugMode::Running : ControlDebugMode::Standby);

    if (status.inputSource == autoviz::datacenter::VisualizationInputSource::Mock) {
        data.feedbackSource = QStringLiteral("仿真反馈");
    } else if (command.mode == autoviz::model::ControlMode::Sailing) {
        data.feedbackSource = hasLocation ? QStringLiteral("定位反馈") : QStringLiteral("定位反馈缺失");
    } else if (command.mode == autoviz::model::ControlMode::Crawl) {
        data.feedbackSource = hasChassis ? QStringLiteral("底盘反馈") : QStringLiteral("底盘反馈缺失");
    } else {
        data.feedbackSource = QStringLiteral("等待控制命令");
    }

    const bool fresh = !data.timedOut;
    data.hasCmdSpeed = fresh && hasControl;
    data.cmdSpeed = command.desiredVelocity;

    if (command.mode == autoviz::model::ControlMode::Sailing) {
        data.hasFeedbackSpeed = fresh && hasLocation;
        data.feedbackSpeed = snapshot.vehicleLocation.speed;
        data.hasCmdYaw = fresh && hasControl;
        data.cmdYaw = command.desiredHeading;
        data.hasFeedbackYaw = fresh && hasLocation;
        data.feedbackYaw = snapshot.vehicleLocation.heading;
    } else if (command.mode == autoviz::model::ControlMode::Crawl) {
        data.hasFeedbackSpeed = fresh && hasChassis;
        data.feedbackSpeed = snapshot.vehicleChassisInfo.currentSpeed;
    }

    data.hasSpeedError = data.hasCmdSpeed && data.hasFeedbackSpeed;
    data.speedError = data.cmdSpeed - data.feedbackSpeed;
    data.hasYawError = data.hasCmdYaw && data.hasFeedbackYaw;
    data.yawError = normalizeAngle(data.cmdYaw - data.feedbackYaw);

    if (fresh && hasLocation && status.hasLocalPathData) {
        data.hasLateralError = calculatePathError(snapshot.vehicleLocation,
                                                  snapshot.localPath,
                                                  &data.lateralError,
                                                  &data.pathYawError);
        data.hasPathYawError = data.hasLateralError;
    }

    return data;
}

}  // namespace autoviz::ui::charts
