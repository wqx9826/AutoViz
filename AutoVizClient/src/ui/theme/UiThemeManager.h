#pragma once

#include <QColor>
#include <QString>

namespace autoviz::ui::theme {

enum class ThemeMode {
    Auto,
    Light,
    Dark
};

struct ThemePalette {
    bool dark = true;
    QColor window;
    QColor panel;
    QColor panelAlt;
    QColor plotBackground;
    QColor border;
    QColor text;
    QColor mutedText;
    QColor accent;
    QColor selection;
    QColor normalText;
    QColor normalBackground;
    QColor warnText;
    QColor warnBackground;
    QColor offlineText;
    QColor offlineBackground;
    QColor grid;
    QColor axis;
    QColor overlayBackground;
    QColor overlayBorder;
    QColor overlayText;
    QColor switchKnob;
};

class UiThemeManager {
public:
    static UiThemeManager& instance();

    void setMode(ThemeMode mode);
    ThemeMode mode() const;
    ThemePalette palette() const;
    ThemePalette effectivePalette() const;
    QString styleSheet() const;

private:
    UiThemeManager() = default;

    bool systemPrefersDark() const;
    ThemePalette darkPalette() const;
    ThemePalette lightPalette() const;

    ThemeMode m_mode = ThemeMode::Auto;
};

}  // namespace autoviz::ui::theme
