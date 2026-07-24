#pragma once

#include <QAbstractButton>

class ToggleSwitch : public QAbstractButton
{
    Q_OBJECT

public:
    explicit ToggleSwitch(QWidget* parent = nullptr);

    QSize sizeHint() const override;
    void setHasData(bool hasData);
    bool hasData() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void updateToolTip();

    bool m_hasData = true;
};
