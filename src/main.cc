#include <iostream>
#include <ios>
#include <istream>

#include <QSettings>
#include <QStandardPaths>
#include <QDir>

#include <creeper-qt/creeper-qt.hh>
#include <creeper-qt/core/application.hh>
#include <creeper-qt/utility/theme/preset/blue-miku.hh>
#include <creeper-qt/utility/theme/preset/gloden-harvest.hh>
#include <creeper-qt/utility/theme/preset/green.hh>
#include <creeper-qt/utility/animation/animatable.hh>
#include <creeper-qt/utility/animation/transition.hh>
#include <creeper-qt/utility/animation/state/linear.hh>

#include "gui/command_card.h"
#include "gui/ring_countdown.h"
#include "gui/queue_io.h"
#include "gui/executor.h"
#include "gui/icon.h"
#include "gui/icon_codes.h"
#include "gui/sound.h"
#include "gui/theme_transition.h"

#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QTimer>
#include <QWidget>

using namespace creeper;
namespace mwpro = main_window::pro;
namespace capro = card::pro;
namespace lnpro = linear::pro;
namespace wdpro = widget::pro;

static const ThemePack kPresetPacks[] = {
    kBlueMikuThemePack,
    kGoldenHarvestThemePack,
    kGreenThemePack,
};

static auto switchTheme(ThemeManager& mgr,
                        const ThemePack& newPack,
                        ColorMode newMode,
                        creeper::Animatable* anim) -> void {
    auto oldScheme = mgr.color_scheme();
    mgr.set_theme_pack(newPack);
    mgr.set_color_mode(newMode);
    auto newScheme = mgr.color_scheme();
    if (anim) {
        auto state = std::make_shared<ThemeTransitionState>(
            oldScheme, newScheme,
            [&mgr](const ColorScheme& s) {
                mgr.apply_theme();
            });
        auto running = std::make_shared<bool>(true);
        anim->push_transition_task(
            std::make_unique<creeper::TransitionTask<
                ThemeTransitionState>>(state, running));
    } else {
        mgr.apply_theme();
    }
};

// ══════════════════════════════════════════════════════════════════════════
//  main
// ══════════════════════════════════════════════════════════════════════════

int main(int argc, char* argv[]) {
    // ── CLI mode: invoked as `agent-sudo` (not `agent-sudo-flush`) ──
    {
        std::string prog = argv[0];
        auto slash = prog.find_last_of('/');
        std::string base = (slash != std::string::npos)
                               ? prog.substr(slash + 1) : prog;
        if (base == "agent-sudo") {
            std::string reason;
            std::string command;
            bool inReason = false, inCmd = false;
            for (int i = 1; i < argc; ++i) {
                std::string a = argv[i];
                if (a == "--reason") { inReason = true; inCmd = false; continue; }
                if (a == "--") { inReason = false; inCmd = true; continue; }
                if (inReason) { reason = a; inReason = false; }
                else if (inCmd)
                    command += (command.empty() ? "" : " ") + a;
            }
            if (reason.empty() || command.empty()) {
                std::cerr << "Usage: agent-sudo --reason \"...\" -- <command>"
                          << std::endl;
                return 127;
            }
            auto q = readQueue();
            auto items = q.value("items").toArray();
            int nextId = 1;
            for (const auto& v : items)
                nextId = std::max(nextId,
                                  v.toObject().value("id").toInt() + 1);
            QJsonObject entry;
            entry["id"]      = nextId;
            entry["reason"]  = QString::fromStdString(reason);
            entry["command"] = QString::fromStdString(command);
            entry["cwd"]     = QDir::currentPath();
            items.append(entry);
            q["items"] = items;
            writeQueue(q);
            return 0;
        }
    }

    if (qEnvironmentVariableIsEmpty("DISPLAY")) {
        std::cerr << "ERROR: DISPLAY not set" << std::endl;
        return 127;
    }
    auto queue = readQueue();
    auto items = queue.value("items").toArray();
    if (items.isEmpty()) {
        std::cerr << "ERROR: queue is empty" << std::endl;
        return 127;
    }

    // ---- Config & theme ----
    QSettings cfg("agent-sudo", "agent-sudo");
    int presetIdx = cfg.value("theme/preset", 0).toInt();
    if (presetIdx < 0 || presetIdx > 2) presetIdx = 0;
    ColorMode mode = cfg.value("theme/mode", 0).toInt() == 0
                         ? ColorMode::LIGHT : ColorMode::DARK;

    app::init { app::pro::Complete { argc, argv } };
    IconProvider::initFont();
    QApplication::setWindowIcon(IconProvider::appIcon());

    auto manager = ThemeManager { kPresetPacks[presetIdx], mode };

    auto soundConfigPath = QDir(
        QStandardPaths::writableLocation(
            QStandardPaths::ConfigLocation))
        .filePath("agent-sudo/theme.conf");
    auto soundMgr = SoundManager { soundConfigPath };

    // ---- State ----
    int  remaining   = 60;
    bool paused      = false;
    bool done        = false;
    bool llmOpen     = false;
    int  exitCode    = 126;
    QJsonArray allItems = items;

    QString    sudoPw;
    QString    bashScr;
    QJsonArray selItems;

    // ---- Pointers ----
    auto* ringW      = new RingCountdown;
    std::unique_ptr<creeper::Animatable> anim;
    OutlinedTextField* pwF   = nullptr;
    OutlinedTextField* llmF  = nullptr;
    TextButton*        llmB  = nullptr;
    FilledButton*      execB = nullptr;
    OutlinedButton*    rejB  = nullptr;
    OutlinedButton*    pauseB= nullptr;
    QVector<Switch*>   sws;

    // ---- Callbacks ----
    std::function<void()> updateExec;
    updateExec = [&] {
        int n = 0;
        for (auto* s : sws) if (s->checked()) ++n;
        if (execB) execB->setEnabled(n > 0);
    };
    std::function<void(bool)> setAll;
    setAll = [&](bool on) {
        for (auto* s : sws) s->set_checked(on);
        updateExec();
    };

    // ---- Build command cards (delegated to command_card module) ----
    auto* cmdWidget = buildCommandCards(items, &manager, sws);

    // ---- Main window ----
    creeper::ShowWindow<MainWindow> {
        [&](MainWindow& window) noexcept {
            window.setWindowTitle(QString::fromUtf8("Agent 特权命令审批"));
            window.setFixedSize(700, 550);
            auto* scr = QApplication::primaryScreen();
            if (scr) {
                auto g = scr->availableGeometry();
                window.move((g.width() - 700) / 2,
                            (g.height() - 550) / 2);
            }

            anim = std::make_unique<creeper::Animatable>(window);

            pwF->setEchoMode(QLineEdit::Password);
            pwF->setPlaceholderText(QString::fromUtf8(""));
            llmF->setMaximumHeight(0);
            llmF->setVisible(false);
            QTimer::singleShot(0, pwF, qOverload<>(&QWidget::setFocus));
            QTimer::singleShot(100, [&] {
                soundMgr.play(SoundManager::Event::Open);
            });

            updateExec();
            for (auto* s : sws)
                QObject::connect(s, &QAbstractButton::clicked, updateExec);

            // Manual theme safety net
            for (auto* s : sws)
                manager.append_handler(s, [s](const ThemeManager& m) {
                    s->set_color_scheme(m.color_scheme());
                });
            if (pwF) manager.append_handler(pwF, [pwF](const ThemeManager& m) {
                pwF->set_color_scheme(m.color_scheme());
            });
            if (llmF) manager.append_handler(llmF, [llmF](const ThemeManager& m) {
                llmF->set_color_scheme(m.color_scheme());
            });
            if (execB) manager.append_handler(execB, [execB](const ThemeManager& m) {
                execB->set_color_scheme(m.color_scheme());
            });
            if (rejB) manager.append_handler(rejB, [rejB](const ThemeManager& m) {
                rejB->set_color_scheme(m.color_scheme());
            });
            if (llmB) manager.append_handler(llmB, [llmB](const ThemeManager& m) {
                llmB->set_color_scheme(m.color_scheme());
            });
            if (pauseB) manager.append_handler(pauseB, [pauseB](const ThemeManager& m) {
                pauseB->set_color_scheme(m.color_scheme());
            });
            if (ringW) manager.append_handler(ringW, [ringW](const ThemeManager& m) {
                ringW->set_color_scheme(m.color_scheme());
            });

            // Execute
            QObject::connect(execB, &QAbstractButton::clicked, [&] {
                int n = 0;
                for (auto* s : sws) if (s->checked()) ++n;
                if (n == 0) return;
                sudoPw = pwF->text();
                if (sudoPw.isEmpty()) return;
                soundMgr.play(SoundManager::Event::Executed);
                done = true; exitCode = 0;
                auto prompt = llmF->text().trimmed();
                if (!prompt.isEmpty())
                    std::cout << "LLM_PROMPT: "
                              << prompt.toStdString() << std::endl;
                QStringList sl;
                sl << "set -e";
                for (int i = 0; i < items.size(); ++i) {
                    if (!sws[i]->checked()) continue;
                    auto o = items[i].toObject();
                    selItems.append(o);
                    QString cwd = o["cwd"].toString();
                    cwd.replace('\'', "'\\''");
                    sl << QString("echo '=== [%1] %2 ==='")
                              .arg(o["id"].toInt())
                              .arg(o["reason"].toString());
                    sl << QString("cd '%1' && %2")
                              .arg(cwd, o["command"].toString());
                }
                bashScr = sl.join('\n');
                window.hide();
                app::quit();
            });

            // Reject
            QObject::connect(rejB, &QAbstractButton::clicked, [&] {
                done = true; exitCode = 126;
                auto p = llmF->text().trimmed();
                if (!p.isEmpty())
                    std::cout << "LLM_PROMPT: "
                              << p.toStdString() << std::endl;
                soundMgr.play(SoundManager::Event::Rejected);
                clearQueue();
                app::quit();
            });

            // Pause
            QObject::connect(pauseB, &QAbstractButton::clicked, [&] {
                paused = !paused;
                pauseB->setText(paused ? "\u25B6" : "||");
            });

            // Enter key
            QObject::connect(pwF, &QLineEdit::returnPressed,
                             execB, &QAbstractButton::click);

            // Countdown
            auto* timer = new QTimer(&window);
            QObject::connect(timer, &QTimer::timeout, [&] {
                if (done || paused) return;
                --remaining;
                ringW->setRemaining(remaining);
                if (remaining == 10) {
                    soundMgr.play(SoundManager::Event::Warning);
                }
                if (remaining <= 0) {
                    done = true; exitCode = 126;
                    auto p = llmF->text().trimmed();
                    if (!p.isEmpty())
                        std::cout << "LLM_PROMPT: "
                                  << p.toStdString() << std::endl;
                    soundMgr.play(SoundManager::Event::Rejected);
                    clearQueue();
                    app::quit();
                }
            });
            timer->start(1000);
        },

        mwpro::MinimumSize { 700, 550 },
        mwpro::MoveCenter {},
        mwpro::Central<FilledCard> {
            capro::ThemeManager { manager },
            capro::Layout<Col> {
                lnpro::Margin { 0 },
                lnpro::Spacing { 0 },

                // ── Summary bar ──
                lnpro::Item<Row> {
                    lnpro::Margin { 10 },
                    lnpro::Spacing { 8 },
                    lnpro::Item<QWidget> {
                        IconProvider::iconLabel(icon::kChecklist,
                            manager.color_scheme().on_surface, 18) },
                    lnpro::Item<Text> {
                        text::pro::ThemeManager { manager },
                        text::pro::Text {
                            QString::fromUtf8("共 %1 条待审批命令")
                                .arg(items.size()) },
                    },
                    lnpro::Stretch { 1 },
                    lnpro::Item { ringW },
                    lnpro::Item<OutlinedButton> {
                        outlined_button::pro::ThemeManager { manager },
                        wdpro::Font {
                            QFont("Material Icons", 16) },
                        button::pro::Text {
                            QString::fromUtf8(icon::kPalette) },
                        wdpro::FixedSize { 32, 32 },
                        button::pro::Clickable { [&] {
                            presetIdx = (presetIdx + 1) % 3;
                            cfg.setValue("theme/preset", presetIdx);
                            cfg.sync();
                            switchTheme(manager, kPresetPacks[presetIdx],
                                        manager.color_mode(), anim.get());
                        }},
                    },
                    lnpro::Item<OutlinedButton> {
                        outlined_button::pro::ThemeManager { manager },
                        wdpro::Font {
                            QFont("Material Icons", 16) },
                        button::pro::Text {
                            mode == ColorMode::LIGHT
                                ? QString::fromUtf8(icon::kLightMode)
                                : QString::fromUtf8(icon::kDarkMode) },
                        wdpro::FixedSize { 32, 32 },
                        button::pro::Clickable { [&] {
                            ColorMode newMode =
                                mode == ColorMode::LIGHT
                                    ? ColorMode::DARK
                                    : ColorMode::LIGHT;
                            mode = newMode;
                            cfg.setValue("theme/mode",
                                newMode == ColorMode::LIGHT ? 0 : 1);
                            cfg.sync();
                            switchTheme(manager, kPresetPacks[presetIdx],
                                        newMode, anim.get());
                        }},
                    },
                    lnpro::Item<OutlinedButton> {
                        outlined_button::pro::ThemeManager { manager },
                        wdpro::Bind { pauseB },
                        wdpro::Font {
                            QFont("Material Icons", 16) },
                        button::pro::Text {
                            QString::fromUtf8(icon::kPause) },
                        wdpro::FixedSize { 32, 32 },
                    },
                },

                // ── Command list with its own mask ──
                lnpro::Item<FilledCard> {
                    capro::ThemeManager { manager },
                    capro::Layout<Col> {
                        lnpro::Margin { 0 },
                        lnpro::Item<ScrollArea> {
                            scroll::pro::ThemeManager { manager },
                            scroll::pro::Item<QWidget> { cmdWidget },
                        },
                    },
                },

                // ── LLM header (clickable disclosure) ──
                lnpro::Item<Row> {
                    lnpro::Margin { 12 },
                    lnpro::Item<TextButton> {
                        text_button::pro::ThemeManager { manager },
                        wdpro::Bind { llmB },
                        wdpro::FixedSize { 380, 28 },
                        button::pro::Text {
                            QString::fromUtf8(
                                "▶ 备注本次执行上下文 (LLM Prompt)") },
                        button::pro::Clickable { [&] {
                            llmOpen = !llmOpen;
                            llmB->setText(
                                QString(llmOpen ? "▼" : "▶")
                                + QString::fromUtf8(
                                    " 备注本次执行上下文 (LLM Prompt)"));
                            llmF->setMaximumHeight(llmOpen ? 80 : 0);
                            llmF->setVisible(llmOpen);
                        }},
                    },
                },

                // ── LLM text field ──
                lnpro::Item<Row> {
                    lnpro::Margin { 0 },
                    lnpro::Item<OutlinedTextField> {
                        text_field::pro::ThemeManager { manager },
                        wdpro::Bind { llmF },
                    },
                },

                // ── Password ──
                lnpro::Item<Row> {
                    lnpro::Margin { 8 },
                    lnpro::Spacing { 8 },
                    lnpro::Item<OutlinedTextField> {
                        { 1 },
                        text_field::pro::ThemeManager { manager },
                        text_field::pro::LabelText {
                            QString::fromUtf8("sudo 密码") },
                        wdpro::Bind { pwF },
                    },
                },

                // ── Button bar ──
                lnpro::Item<Row> {
                    lnpro::Margin { 12 },
                    lnpro::Spacing { 8 },
                    lnpro::Item<OutlinedButton> {
                        outlined_button::pro::ThemeManager { manager },
                        button::pro::Text { QString::fromUtf8("全选") },
                        wdpro::FixedSize { 60, 32 },
                        button::pro::Clickable { [&] { setAll(true); } },
                    },
                    lnpro::Item<OutlinedButton> {
                        outlined_button::pro::ThemeManager { manager },
                        button::pro::Text {
                            QString::fromUtf8("取消全选") },
                        wdpro::FixedSize { 90, 32 },
                        button::pro::Clickable { [&] { setAll(false); } },
                    },
                    lnpro::Stretch { 1 },
                    lnpro::Item<OutlinedButton> {
                        outlined_button::pro::ThemeManager { manager },
                        wdpro::Bind { rejB },
                        wdpro::FixedSize { 80, 32 },
                        button::pro::Text {
                            QString::fromUtf8("\u62D2\u7EDD") },
                    },
                    lnpro::Item<FilledButton> {
                        filled_button::pro::ThemeManager { manager },
                        wdpro::Bind { execB },
                        wdpro::FixedSize { 120, 36 },
                        button::pro::Text {
                            QString::fromUtf8("\u6267\u884C (Enter)") },
                    },
                },
            },
        },
    };

    manager.apply_theme();
    app::exec();

    // ---- Post-GUI: execute approved commands ----
    if (exitCode == 0 && !bashScr.isEmpty()) {
        return execCommands(sudoPw, bashScr, selItems, allItems);
    }

    if (!done) { clearQueue(); exitCode = 126; }
    if (exitCode == 126)
        std::cout << "REJECTED: user dismissed" << std::endl;
    return exitCode;
}
