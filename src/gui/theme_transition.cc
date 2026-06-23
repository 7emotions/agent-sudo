#include "theme_transition.h"

#include <chrono>

ThemeTransitionState::ThemeTransitionState(const creeper::ColorScheme& from,
                                           const creeper::ColorScheme& to,
                                           Callback onFrame,
                                           double durationSec)
    : value_(from)
    , target_(to)
    , onFrame_(std::move(onFrame))
    , duration_(durationSec)
    , lastTimestamp_(
          std::chrono::duration<double>(
              std::chrono::steady_clock::now().time_since_epoch())
              .count()) {}

auto ThemeTransitionState::update() -> bool {
    auto now = std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch())
        .count();
    double dt = now - lastTimestamp_;
    lastTimestamp_ = now;
    if (dt <= 0.0)
        dt = 1.0 / 90.0; // fallback for 90fps

    progress_ += dt / duration_;
    if (progress_ >= 1.0) {
        value_ = target_;
        if (onFrame_) onFrame_(value_);
        return false;
    }
    value_ = colorSchemeLerp(value_, target_, easeOut(progress_));
    if (onFrame_) onFrame_(value_);
    return true;
}

auto ThemeTransitionState::get_value() const -> ValueT { return value_; }
auto ThemeTransitionState::get_target() const -> ValueT { return target_; }
auto ThemeTransitionState::set_value(ValueT v) -> void { value_ = v; }
auto ThemeTransitionState::set_target(ValueT t) -> void {
    target_ = t;
    progress_ = 0.0;
}

// cubic-bezier(0.4, 0, 0.2, 1) — Material Design standard ease-out
auto easeOut(double t) -> double {
    if (t <= 0.0)
        return 0.0;
    if (t >= 1.0)
        return 1.0;
    double u = 1.0 - t;
    return 1.0 - u * u * u;
}

inline auto lerpColor(const QColor& a, const QColor& b, double t) -> QColor {
    return QColor(
        static_cast<int>(a.red() + (b.red() - a.red()) * t),
        static_cast<int>(a.green() + (b.green() - a.green()) * t),
        static_cast<int>(a.blue() + (b.blue() - a.blue()) * t),
        static_cast<int>(a.alpha() + (b.alpha() - a.alpha()) * t));
}

auto colorSchemeLerp(const creeper::ColorScheme& a,
                     const creeper::ColorScheme& b,
                     double t) -> creeper::ColorScheme {
    return creeper::ColorScheme{
        .primary = lerpColor(a.primary, b.primary, t),
        .on_primary = lerpColor(a.on_primary, b.on_primary, t),
        .primary_container =
            lerpColor(a.primary_container, b.primary_container, t),
        .on_primary_container =
            lerpColor(a.on_primary_container, b.on_primary_container, t),

        .secondary = lerpColor(a.secondary, b.secondary, t),
        .on_secondary = lerpColor(a.on_secondary, b.on_secondary, t),
        .secondary_container =
            lerpColor(a.secondary_container, b.secondary_container, t),
        .on_secondary_container =
            lerpColor(a.on_secondary_container, b.on_secondary_container, t),

        .tertiary = lerpColor(a.tertiary, b.tertiary, t),
        .on_tertiary = lerpColor(a.on_tertiary, b.on_tertiary, t),
        .tertiary_container =
            lerpColor(a.tertiary_container, b.tertiary_container, t),
        .on_tertiary_container =
            lerpColor(a.on_tertiary_container, b.on_tertiary_container, t),

        .error = lerpColor(a.error, b.error, t),
        .on_error = lerpColor(a.on_error, b.on_error, t),
        .error_container =
            lerpColor(a.error_container, b.error_container, t),
        .on_error_container =
            lerpColor(a.on_error_container, b.on_error_container, t),

        .background = lerpColor(a.background, b.background, t),
        .on_background = lerpColor(a.on_background, b.on_background, t),

        .surface = lerpColor(a.surface, b.surface, t),
        .on_surface = lerpColor(a.on_surface, b.on_surface, t),
        .surface_variant =
            lerpColor(a.surface_variant, b.surface_variant, t),
        .on_surface_variant =
            lerpColor(a.on_surface_variant, b.on_surface_variant, t),

        .outline = lerpColor(a.outline, b.outline, t),
        .outline_variant =
            lerpColor(a.outline_variant, b.outline_variant, t),
        .shadow = lerpColor(a.shadow, b.shadow, t),
        .scrim = lerpColor(a.scrim, b.scrim, t),

        .inverse_surface =
            lerpColor(a.inverse_surface, b.inverse_surface, t),
        .inverse_on_surface =
            lerpColor(a.inverse_on_surface, b.inverse_on_surface, t),
        .inverse_primary =
            lerpColor(a.inverse_primary, b.inverse_primary, t),

        .surface_container_highest = lerpColor(
            a.surface_container_highest, b.surface_container_highest, t),
        .surface_container_high = lerpColor(
            a.surface_container_high, b.surface_container_high, t),
        .surface_container =
            lerpColor(a.surface_container, b.surface_container, t),
        .surface_container_low =
            lerpColor(a.surface_container_low, b.surface_container_low, t),
        .surface_container_lowest = lerpColor(
            a.surface_container_lowest, b.surface_container_lowest, t),
    };
}
