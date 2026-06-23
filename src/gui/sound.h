#pragma once
#include <QSettings>
#include <QSoundEffect>
#include <QString>
#include <memory>
#include <unordered_map>

/// Configurable sound feedback manager using QSoundEffect.
/// Reads per-event paths from QSettings. Uses system beep for "default".
/// Gracefully degrades if Qt6::Multimedia is unavailable.
class SoundManager {
public:
    enum class Event { Open, Warning, Executed, Rejected };

    explicit SoundManager(const QString& configPath);

    auto play(Event event) -> void;
    auto setEnabled(bool enabled) -> void;

private:
    QSettings settings_;
    bool enabled_ = true;
    std::unordered_map<Event, std::unique_ptr<QSoundEffect>> effects_;

    auto resolveSource(Event event) const -> QString;
    static auto eventKey(Event event) -> const char*;
};
