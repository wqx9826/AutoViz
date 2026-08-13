#include "ui/theme/UiScaleManager.h"

#include <algorithm>

#include <QGuiApplication>
#include <QScreen>
#include <QtGlobal>

namespace autoviz::ui::theme {

namespace {
constexpr double kBaseDpi = 96.0;
constexpr double kMinFontScale = 0.80;
constexpr double kMaxFontScale = 2.00;
constexpr double kMinLayoutScale = 0.85;
constexpr double kMaxLayoutScale = 1.60;

int roundScaled(double value)
{
    return qMax(1, qRound(value));
}
}

UiScaleManager& UiScaleManager::instance()
{
    static UiScaleManager manager;
    return manager;
}

void UiScaleManager::initialize()
{
    auto* screen = QGuiApplication::primaryScreen();
    if (screen == nullptr) {
        m_uiScale = 1.0;
        m_fontScale = 1.0;
        m_layoutScale = 1.0;
        return;
    }

    const double dpiScale = std::max(1.0, screen->logicalDotsPerInch() / kBaseDpi);
    m_fontScale = std::clamp(dpiScale, kMinFontScale, kMaxFontScale);
    m_layoutScale = std::clamp(dpiScale, kMinLayoutScale, kMaxLayoutScale);
    m_uiScale = m_layoutScale;
}

double UiScaleManager::uiScale() const
{
    return m_uiScale;
}

double UiScaleManager::fontScale() const
{
    return m_fontScale;
}

double UiScaleManager::layoutScale() const
{
    return m_layoutScale;
}

int UiScaleManager::fontSizeSmall() const
{
    return roundScaled(9.0 * m_fontScale);
}

int UiScaleManager::fontSizeNormal() const
{
    return roundScaled(10.0 * m_fontScale);
}

int UiScaleManager::fontSizeTitle() const
{
    return roundScaled(11.0 * m_fontScale);
}

int UiScaleManager::spacingSmall() const
{
    return scaled(6);
}

int UiScaleManager::spacingNormal() const
{
    return scaled(8);
}

int UiScaleManager::marginNormal() const
{
    return scaled(8);
}

int UiScaleManager::scaled(int value) const
{
    return roundScaled(static_cast<double>(value) * m_layoutScale);
}

QFont UiScaleManager::font(int pointSize, int weight) const
{
    QFont result;
    result.setPointSize(pointSize);
    result.setWeight(static_cast<QFont::Weight>(weight));
    return result;
}

}  // namespace autoviz::ui::theme
