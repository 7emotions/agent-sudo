#include "sound.h"
#include <QApplication>
#include <QFileInfo>
#include <QUrl>

static constexpr auto kGroupSound = "sound";

auto SoundManager::eventKey(Event event) -> const char* {
    switch (event) {
        case Event::Open:     return "sound/event/open";
        case Event::Warning:  return "sound/event/warning";
        case Event::Executed: return "sound/event/executed";
        case Event::Rejected: return "sound/event/rejected";
    }
    return "";
}

SoundManager::SoundManager(const QString& configPath)
    : settings_(configPath, QSettings::IniFormat) {
    enabled_ = settings_.value("sound/enabled", true).toBool();
}

auto SoundManager::setEnabled(bool enabled) -> void {
    enabled_ = enabled;
    settings_.setValue("sound/enabled", enabled);
    settings_.sync();
}

auto SoundManager::resolveSource(Event event) const -> QString {
    return settings_.value(eventKey(event), "default").toString();
}

auto SoundManager::play(Event event) -> void {
    if (!enabled_) return;

    auto source = resolveSource(event);
    if (source == "none") return;

    // "default" → system beep (works without Qt6::Multimedia)
    if (source == "default") {
        QApplication::beep();
        return;
    }

    // User-specified WAV file path → QSoundEffect (needs Qt6::Multimedia)
    auto it = effects_.find(event);
    if (it == effects_.end()) {
        if (!QFileInfo::exists(source)) return;
        auto effect = std::make_unique<QSoundEffect>();
        effect->setSource(QUrl::fromLocalFile(source));
        effect->setVolume(0.8);
        effects_[event] = std::move(effect);
        it = effects_.find(event);
    }

    auto* effect = it->second.get();
    if (effect && effect->status() == QSoundEffect::Ready) {
        effect->play();
    }
}
