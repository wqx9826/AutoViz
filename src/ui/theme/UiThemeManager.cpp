#include "ui/theme/UiThemeManager.h"

#include <QApplication>
#include <QPalette>

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
    const auto p = effectivePalette();
    const auto& scale = UiScaleManager::instance();
    return QStringLiteral(R"(
QMainWindow, QDialog, QWidget {
    background: %1;
    color: %6;
}
QMenuBar {
    background: %3;
    color: %6;
    border-bottom: 1px solid %5;
}
QMenuBar::item {
    background: transparent;
    padding: %18px %19px;
}
QMenuBar::item:selected {
    background: %4;
}
QMenu {
    background: %2;
    color: %6;
    border: 1px solid %5;
}
QMenu::item {
    padding: %18px %20px;
}
QMenu::item:selected {
    background: %9;
    color: %21;
}
QStatusBar {
    background: %3;
    color: %7;
    border-top: 1px solid %5;
}
QSplitter::handle {
    background: %5;
}
QSplitter::handle:horizontal {
    width: %22px;
}
QSplitter::handle:vertical {
    height: %22px;
}
QTabWidget::pane {
    border: 1px solid %5;
    background: %1;
}
QTabBar::tab {
    background: %3;
    color: %7;
    border: 1px solid %5;
    padding: %18px %19px;
    margin-right: 1px;
}
QTabBar::tab:selected {
    background: %2;
    color: %6;
    border-bottom-color: %2;
}
QGroupBox {
    background: %2;
    border: 1px solid %5;
    border-radius: 6px;
    margin-top: %23px;
    color: %6;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: %19px;
    padding: 0 %18px;
    color: %8;
    font-weight: 700;
}
QLabel {
    background: transparent;
    color: %6;
}
QLabel[class="status-key"] {
    color: %7;
    font-family: "Noto Sans CJK SC", "Microsoft YaHei", "Arial", sans-serif;
    font-size: 10pt;
    font-weight: 400;
}
QLabel[class="status-value"] {
    color: %6;
    font-family: "JetBrains Mono", "Roboto Mono", "Consolas", "DejaVu Sans Mono", monospace;
    font-size: 10pt;
    font-weight: 600;
}
QLabel[class="status-badge"] {
    font-family: "JetBrains Mono", "Roboto Mono", "Consolas", "DejaVu Sans Mono", monospace;
    font-size: 10pt;
    font-weight: 700;
    border-radius: 4px;
    padding: 1px %18px;
    qproperty-alignment: AlignCenter;
}
QLabel#status-normal {
    color: %10;
    background: %11;
    border: 1px solid %10;
}
QLabel#status-warn {
    color: %12;
    background: %13;
    border: 1px solid %12;
}
QLabel#status-offline {
    color: %14;
    background: %15;
    border: 1px solid %5;
}
QFrame#dialogRow, QFrame#statusSummaryCard, QFrame#controlPanelToolbar {
    background: %2;
    border: 1px solid %5;
    border-radius: 6px;
}
QFrame#metricBlock {
    background: %4;
    border: 1px solid %5;
    border-radius: 6px;
}
QPlainTextEdit, QTableWidget, QTableView {
    background: %3;
    alternate-background-color: %3;
    color: %6;
    border: 1px solid %5;
    selection-background-color: %9;
    selection-color: %21;
    gridline-color: %5;
}
QTableWidget::item, QTableView::item {
    background: %3;
    color: %6;
    padding: %18px %19px;
}
QTableWidget::item:alternate, QTableView::item:alternate {
    background: %4;
}
QTableWidget::item:selected, QTableView::item:selected {
    background: %9;
    color: %21;
}
QHeaderView::section {
    background: %2;
    color: %7;
    border: 0;
    border-right: 1px solid %5;
    border-bottom: 1px solid %5;
    padding: %18px %19px;
}
QTableWidget#topicDashboardTable {
    background: %3;
    alternate-background-color: %3;
    border: 1px solid %5;
    border-radius: 6px;
    gridline-color: transparent;
    selection-background-color: %9;
    selection-color: %21;
}
QTableWidget#topicDashboardTable::item {
    background: %3;
    color: %6;
    border: 0;
    border-bottom: 1px solid %5;
    padding: %18px %19px;
}
QTableWidget#topicDashboardTable::item:selected {
    background: %9;
    color: %21;
}
QTableWidget#topicDashboardTable QHeaderView::section {
    background: %3;
    color: %7;
    border: 0;
    border-bottom: 1px solid %5;
    padding: %18px %19px;
    font-weight: 700;
}
QScrollArea {
    background: %1;
    border: 0;
}
QScrollBar:vertical, QScrollBar:horizontal {
    background: %1;
    border: 0;
}
QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
    background: %5;
    border-radius: 4px;
}
QPushButton {
    color: %6;
    background: %4;
    border: 1px solid %5;
    border-radius: 5px;
    padding: %18px %19px;
}
QPushButton:hover {
    border-color: %8;
}
QPushButton:checked {
    color: %21;
    background: %9;
    border-color: %8;
}
QComboBox {
    color: %6;
    background: %4;
    border: 1px solid %5;
    border-radius: 5px;
    padding: %18px %24px %18px %19px;
}
QComboBox::drop-down {
    width: %25px;
    border: 0;
}
QCheckBox {
    color: %6;
    background: transparent;
}
)")
        .arg(p.window.name())
        .arg(p.panel.name())
        .arg(p.panelAlt.name())
        .arg(p.plotBackground.name())
        .arg(p.border.name())
        .arg(p.text.name())
        .arg(p.mutedText.name())
        .arg(p.accent.name())
        .arg(p.selection.name())
        .arg(p.normalText.name())
        .arg(p.normalBackground.name())
        .arg(p.warnText.name())
        .arg(p.warnBackground.name())
        .arg(p.offlineText.name())
        .arg(p.offlineBackground.name())
        .arg(p.grid.name())
        .arg(p.axis.name())
        .arg(scale.scaled(4))
        .arg(scale.scaled(10))
        .arg(scale.scaled(24))
        .arg(p.dark ? QStringLiteral("#FFFFFF") : QStringLiteral("#0F172A"))
        .arg(scale.scaled(3))
        .arg(scale.scaled(12))
        .arg(scale.scaled(28))
        .arg(scale.scaled(22));
}

bool UiThemeManager::systemPrefersDark() const
{
    const QColor window = qApp->palette().color(QPalette::Window);
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
    p.overlayBackground = QColor(18, 24, 31, 220);
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
    p.overlayBackground = QColor(255, 255, 255, 225);
    p.overlayBorder = QColor("#B6C3CF");
    p.overlayText = QColor("#1F2937");
    p.switchKnob = QColor("#FFFFFF");
    return p;
}

}  // namespace autoviz::ui::theme
