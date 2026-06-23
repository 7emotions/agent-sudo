#include "ring_countdown.h"
#include <algorithm>

RingCountdown::RingCountdown(QWidget* parent) : QWidget(parent) {
    setFixedSize(36, 36);
}

void RingCountdown::setRemaining(int val) {
    remaining_ = std::max(val, 0);
    update();
}

void RingCountdown::set_color_scheme(const creeper::theme::ColorScheme& scheme) {
    colorScheme_ = scheme;
    update();
}

void RingCountdown::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background ring: surface_variant
    QPen pen(colorScheme_.surface_variant, 2.5);
    p.setPen(pen);
    p.drawEllipse(QRectF(2, 2, 32, 32));

    // Progress arc: primary
    pen.setColor(colorScheme_.primary);
    p.setPen(pen);
    double frac = static_cast<double>(remaining_) / total_;
    int span = static_cast<int>(-frac * 360 * 16);
    p.drawArc(QRectF(2, 2, 32, 32), 90 * 16, span);

    // Remaining seconds digit: on_surface
    p.setPen(colorScheme_.on_surface);
    p.setFont(QFont("sans-serif", 10, QFont::Bold));
    p.drawText(QRectF(0, 0, 36, 36), Qt::AlignCenter,
               QString::number(remaining_));
}
