#pragma once
#include <QWidget>
#include <QFont>
#include <QPainter>
#include <QPen>
#include <creeper-qt/utility/theme/color-scheme.hh>

/// Ring-countdown indicator widget (36×36). Draws a circular progress arc
/// and remaining-seconds digit. Used in the summary bar for the 60s auto-reject.
class RingCountdown : public QWidget {
public:
    explicit RingCountdown(QWidget* parent = nullptr);
    void setRemaining(int val);
    void set_color_scheme(const creeper::theme::ColorScheme& scheme);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    int remaining_ = 60;
    int total_     = 60;
    creeper::theme::ColorScheme colorScheme_;
};
