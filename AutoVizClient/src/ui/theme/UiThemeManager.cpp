#include "ui/theme/UiThemeManager.h"

#include <QApplication>
#include <QPalette>
#include <QStyle>

#include "ui/theme/UiScaleManager.h"

namespace autoviz::ui::theme {

UiThemeManager& UiThemeManager::instance()
{
    static UiThemeManager manager;
    return manager;
}

void UiThemeManager::setMode(ThemeMode mode)
{
    m_mode = mode;
}

ThemeMode UiThemeManager::mode() const
{
    return m_mode;
}

ThemePalette UiThemeManager::palette() const
{
    return effectivePalette();
}

ThemePalette UiThemeManager::effectivePalette() const
{
    if (m_mode == ThemeMode::Light) {
        return lightPalette();
    }
    if (m_mode == ThemeMode::Dark) {
        return darkPalette();
    }
    return systemPrefersDark() ? darkPalette() : lightPalette();
}

QString UiThemeManager::styleSheet() const
{
    // 仅保留不随主题变化的几何规则；颜色全部由 QApplication::palette()
    // 提供。切换外观时不需要重新设置全局 QSS，也不会同步重抛光整棵 UI。
    return QStringLiteral(R"(
QMenuBar::item {
    background: transparent;
    padding: 4px 10px;
}
QMenu::item {
    padding: 4px 12px;
}
QSplitter::handle:horizontal {
    width: 3px;
}
QSplitter::handle:vertical {
    height: 3px;
}
QTabBar::tab {
    padding: 4px 10px;
    margin-right: 1px;
}
QGroupBox {
    border-radius: 6px;
    margin-top: 12px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 10px;
    padding: 0 4px;
    font-weight: 700;
}
QLabel[class="status-key"] {
    font-family: "Noto Sans CJK SC", "Microsoft YaHei", "Arial", sans-serif;
    font-size: 10pt;
    font-weight: 400;
}
QLabel[class="status-value"] {
    font-family: "JetBrains Mono", "Roboto Mono", "Consolas", "DejaVu Sans Mono", monospace;
    font-size: 10pt;
    font-weight: 600;
}
QLabel[class="status-badge"] {
    font-family: "JetBrains Mono", "Roboto Mono", "Consolas", "DejaVu Sans Mono", monospace;
    font-size: 10pt;
    font-weight: 700;
    border-radius: 4px;
    padding: 1px 4px;
    qproperty-alignment: AlignCenter;
}
QFrame#dialogRow, QFrame#statusSummaryCard, QFrame#controlPanelToolbar, QFrame#metricBlock {
    border-radius: 6px;
}
QTableWidget::item, QTableView::item {
    padding: 4px 10px;
}
QHeaderView::section {
    border: 0;
    padding: 4px 10px;
}
QTableWidget#topicDashboardTable {
    border-radius: 6px;
}
)")
        ;
}

bool UiThemeManager::systemPrefersDark() const
{
    // 使用 style 的系统标准 palette，而不是当前应用 palette；否则从显式深色
    // 切回“自动”时会把刚设置的深色错误地当作系统偏好。
    const QColor window = qApp->style()->standardPalette().color(QPalette::Window);
    return window.lightness() < 128;
}

ThemePalette UiThemeManager::darkPalette() const
{
    ThemePalette p;
    p.dark = true;
    p.window = QColor("#121214");
    p.panel = QColor("#1E1E22");
    p.panelAlt = QColor("#161619");
    p.plotBackground = QColor("#18181C");
    p.border = QColor("#2D2D34");
    p.text = QColor("#E3E3E6");
    p.mutedText = QColor("#8F8F94");
    p.accent = QColor("#4CC3FF");
    p.selection = QColor("#254B63");
    p.normalText = QColor("#6EF2A0");
    p.normalBackground = QColor("#153822");
    p.warnText = QColor("#FFB45C");
    p.warnBackground = QColor("#3A2614");
    p.offlineText = QColor("#B6B6BC");
    p.offlineBackground = QColor("#2A2A30");
    p.grid = QColor("#2D2D34");
    p.axis = QColor("#3A3A42");
    p.overlayBackground = QColor(18, 24, 31, 190);
    p.overlayBorder = QColor("#5D738B");
    p.overlayText = QColor("#DDE7EF");
    p.switchKnob = QColor("#F8FAFC");
    return p;
}

ThemePalette UiThemeManager::lightPalette() const
{
    ThemePalette p;
    p.dark = false;
    p.window = QColor("#F4F6F8");
    p.panel = QColor("#FFFFFF");
    p.panelAlt = QColor("#EEF2F6");
    p.plotBackground = QColor("#F8FAFC");
    p.border = QColor("#D5DAE1");
    p.text = QColor("#17202A");
    p.mutedText = QColor("#607080");
    p.accent = QColor("#0077B6");
    p.selection = QColor("#B9E3F7");
    p.normalText = QColor("#047857");
    p.normalBackground = QColor("#DDF8EA");
    p.warnText = QColor("#B45309");
    p.warnBackground = QColor("#FEF3C7");
    p.offlineText = QColor("#475569");
    p.offlineBackground = QColor("#E2E8F0");
    p.grid = QColor("#DDE3EA");
    p.axis = QColor("#AAB4C0");
    p.overlayBackground = QColor(255, 255, 255, 190);
    p.overlayBorder = QColor("#B6C3CF");
    p.overlayText = QColor("#1F2937");
    p.switchKnob = QColor("#FFFFFF");
    return p;
}

}  // namespace autoviz::ui::theme
