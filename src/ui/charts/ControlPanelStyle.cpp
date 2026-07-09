#include "ui/charts/ControlPanelStyle.h"

#include <QFontDatabase>
#include <QStringList>
#include <QWidget>

#include "ui/theme/UiScaleManager.h"
#include "ui/theme/UiThemeManager.h"

namespace autoviz::ui::charts::style {

namespace {
QString fontFamily()
{
    static const QString selected = []() {
        const QStringList candidates = {
            QStringLiteral("Noto Sans CJK SC"),
            QStringLiteral("Microsoft YaHei"),
            QStringLiteral("Arial"),
            QStringLiteral("Sans Serif"),
        };
        const QStringList families = QFontDatabase().families();
        for (const auto& candidate : candidates) {
            if (families.contains(candidate, Qt::CaseInsensitive)) {
                return candidate;
            }
        }
        return QFont().defaultFamily();
    }();
    return selected;
}
}

QFont font(int pointSize, int weight)
{
    QFont result = autoviz::ui::theme::UiScaleManager::instance().font(pointSize, weight);
    result.setFamily(fontFamily());
    result.setWeight(weight);
    return result;
}

QFont panelTitleFont()
{
    return font(autoviz::ui::theme::UiScaleManager::instance().fontSizeNormal(), QFont::Bold);
}

QFont cardTitleFont()
{
    return font(autoviz::ui::theme::UiScaleManager::instance().fontSizeNormal(), QFont::Bold);
}

QFont captionFont()
{
    return font(autoviz::ui::theme::UiScaleManager::instance().fontSizeSmall(), QFont::Normal);
}

QFont statusValueFont()
{
    return font(autoviz::ui::theme::UiScaleManager::instance().fontSizeTitle(), QFont::Bold);
}

QFont currentValueFont()
{
    return font(autoviz::ui::theme::UiScaleManager::instance().fontSizeSmall(), QFont::Normal);
}

QFont legendFont()
{
    return font(autoviz::ui::theme::UiScaleManager::instance().fontSizeSmall(), QFont::Normal);
}

QFont axisFont()
{
    return font(autoviz::ui::theme::UiScaleManager::instance().fontSizeSmall(), QFont::Normal);
}

QFont controlFont()
{
    return font(autoviz::ui::theme::UiScaleManager::instance().fontSizeNormal(), QFont::Normal);
}

QString panelStyleSheet()
{
    const auto p = autoviz::ui::theme::UiThemeManager::instance().effectivePalette();
    return QStringLiteral("background: %1; font-family: \"%2\"; color: %3;")
        .arg(p.window.name(), fontFamily(), p.text.name());
}

QString toolbarStyleSheet()
{
    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    const auto p = autoviz::ui::theme::UiThemeManager::instance().effectivePalette();
    return QStringLiteral(
        "#controlPanelToolbar { background: %1; border: 1px solid %2; border-radius: 6px; }"
        "QPushButton { color: %3; background: %4; border: 1px solid %2; border-radius: 5px; padding: %5px %6px; min-height: %7px; font-weight: 400; }"
        "QPushButton:hover { border-color: %8; }"
        "QPushButton:checked { background: %9; border-color: %8; color: %10; }"
        "QComboBox { color: %3; background: %4; border: 1px solid %2; border-radius: 5px; padding: %11px %14px %11px %12px; min-height: %7px; font-weight: 400; }"
        "QComboBox::drop-down { width: %15px; border: 0; }"
        "QCheckBox { color: %3; font-weight: 400; }"
        "QLabel { color: %13; font-weight: 400; }")
        .arg(p.panel.name())
        .arg(p.border.name())
        .arg(p.text.name())
        .arg(p.plotBackground.name())
        .arg(scale.scaled(3))
        .arg(scale.scaled(8))
        .arg(scale.scaled(24))
        .arg(p.accent.name())
        .arg(p.selection.name())
        .arg(p.dark ? QStringLiteral("#FFFFFF") : QStringLiteral("#0F172A"))
        .arg(scale.scaled(2))
        .arg(scale.scaled(7))
        .arg(p.mutedText.name())
        .arg(scale.scaled(38))
        .arg(scale.scaled(30));
}

QString statusCardStyleSheet()
{
    const auto p = autoviz::ui::theme::UiThemeManager::instance().effectivePalette();
    return QStringLiteral("#statusSummaryCard { background: %1; border: 1px solid %2; border-radius: 6px; }")
        .arg(p.panel.name(), p.border.name());
}

QString captionStyleSheet()
{
    const auto p = autoviz::ui::theme::UiThemeManager::instance().effectivePalette();
    return QStringLiteral("color: %1; font-weight: 400;").arg(p.mutedText.name());
}

QString statusValueStyleSheet(const QColor& color)
{
    return QStringLiteral("color: %1; font-weight: 700;").arg(color.name());
}

void polishControls(QWidget* widget)
{
    if (widget == nullptr) {
        return;
    }
    widget->setFont(controlFont());
}

}  // namespace autoviz::ui::charts::style
