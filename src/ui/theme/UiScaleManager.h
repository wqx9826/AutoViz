#pragma once

#include <QFont>

namespace autoviz::ui::theme {

class UiScaleManager {
public:
    static UiScaleManager& instance();

    void initialize();

    double uiScale() const;
    double fontScale() const;
    double layoutScale() const;
    int fontSizeSmall() const;
    int fontSizeNormal() const;
    int fontSizeTitle() const;
    int spacingSmall() const;
    int spacingNormal() const;
    int marginNormal() const;
    int scaled(int value) const;
    QFont font(int pointSize, int weight = QFont::Normal) const;

private:
    UiScaleManager() = default;

    double m_uiScale = 1.0;
    double m_fontScale = 1.0;
    double m_layoutScale = 1.0;
};

}  // namespace autoviz::ui::theme
