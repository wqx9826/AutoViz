#include "ui/charts/ControlPanelStyle.h"

#include <QFontDatabase>
#include <QStringList>
#include <QWidget>

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

QFont font(int pixelSize, int weight)
{
    QFont result(fontFamily());
    result.setPixelSize(pixelSize);
    result.setWeight(weight);
    return result;
}

QFont panelTitleFont()
{
    return font(14, QFont::Bold);
}

QFont cardTitleFont()
{
    return font(14, QFont::Bold);
}

QFont captionFont()
{
    return font(11, QFont::Normal);
}

QFont statusValueFont()
{
    return font(16, QFont::Bold);
}

QFont currentValueFont()
{
    return font(12, QFont::Normal);
}

QFont legendFont()
{
    return font(11, QFont::Normal);
}

QFont axisFont()
{
    return font(10, QFont::Normal);
}

QFont controlFont()
{
    return font(12, QFont::Normal);
}

QString panelStyleSheet()
{
    return QStringLiteral("background: #F3F4F6; font-family: \"%1\"; font-size: 12px; color: #374151;").arg(fontFamily());
}

QString toolbarStyleSheet()
{
    return QStringLiteral(
        "#controlPanelToolbar { background: #FFFFFF; border: 1px solid #E5E7EB; border-radius: 8px; }"
        "QPushButton { color: #111827; background: #F9FAFB; border: 1px solid #D1D5DB; border-radius: 5px; padding: 4px 10px; min-height: 28px; font-size: 12px; font-weight: 400; }"
        "QPushButton:checked { background: #DBEAFE; border-color: #2563EB; color: #1D4ED8; }"
        "QComboBox { color: #111827; background: #FFFFFF; border: 1px solid #D1D5DB; border-radius: 5px; padding: 3px 8px; min-height: 28px; font-size: 12px; font-weight: 400; }"
        "QCheckBox { color: #374151; font-size: 12px; font-weight: 400; }"
        "QLabel { color: #374151; font-size: 12px; font-weight: 400; }");
}

QString statusCardStyleSheet()
{
    return QStringLiteral("#statusSummaryCard { background: #FFFFFF; border: 1px solid #E5E7EB; border-radius: 10px; }");
}

QString captionStyleSheet()
{
    return QStringLiteral("color: #6B7280; font-size: 11px; font-weight: 400;");
}

QString statusValueStyleSheet(const QColor& color)
{
    return QStringLiteral("color: %1; font-size: 16px; font-weight: 700;").arg(color.name());
}

void polishControls(QWidget* widget)
{
    if (widget == nullptr) {
        return;
    }
    widget->setFont(controlFont());
}

}  // namespace autoviz::ui::charts::style
