#pragma once

#include <QColor>
#include <QFont>
#include <QString>

class QWidget;

namespace autoviz::ui::charts::style {

QFont font(int pixelSize = 12, int weight = QFont::Normal);
QFont panelTitleFont();
QFont cardTitleFont();
QFont captionFont();
QFont statusValueFont();
QFont currentValueFont();
QFont legendFont();
QFont axisFont();
QFont controlFont();

QString panelStyleSheet();
QString toolbarStyleSheet();
QString statusCardStyleSheet();
QString captionStyleSheet();
QString statusValueStyleSheet(const QColor& color);

void polishControls(QWidget* widget);

}  // namespace autoviz::ui::charts::style
