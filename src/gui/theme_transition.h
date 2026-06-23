#pragma once

#include <creeper-qt/utility/animation/state/accessor.hh>
#include <creeper-qt/utility/theme/theme.hh>

/// Linear interpolation between two ColorSchemes.
/// Satisfies creeper::transition_state_trait, usable with TransitionValue.
class ThemeTransitionState : public creeper::NormalAccessor {
public:
    using ValueT = creeper::ColorScheme;

    ThemeTransitionState() = default;
    ThemeTransitionState(const creeper::ColorScheme& from,
                         const creeper::ColorScheme& to,
                         double durationSec = 0.3);

    auto update() -> bool;
    auto get_value() const -> ValueT;
    auto get_target() const -> ValueT;
    auto set_value(ValueT v) -> void;
    auto set_target(ValueT t) -> void;

private:
    creeper::ColorScheme value_{};
    creeper::ColorScheme target_{};
    double progress_ = 0.0;
    double duration_ = 0.3;
    double lastTimestamp_ = 0.0;
};

/// Per-field QColor linear interpolation for all ColorScheme fields.
auto colorSchemeLerp(const creeper::ColorScheme& a,
                     const creeper::ColorScheme& b,
                     double t) -> creeper::ColorScheme;

/// Cubic-bezier(0.4, 0, 0.2, 1) ease-out function.
auto easeOut(double t) -> double;
