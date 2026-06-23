#include "command_card.h"
#include "gui/icon.h"
#include "gui/icon_codes.h"

#include <creeper-qt/creeper-qt.hh>
#include <QFont>
#include <QFontMetrics>
#include <QJsonArray>
#include <QJsonObject>
#include <QEvent>
#include <QVBoxLayout>

using namespace creeper;
namespace mwpro = main_window::pro;
namespace capro = card::pro;
namespace lnpro = linear::pro;
namespace wdpro = widget::pro;

// Available width for command text inside a 700px window:
//   700 - 28(card padding) - 16(scroll+mask padding) - 32(switch) ≈ 624
// Leave headroom: use 550.
static constexpr int kCmdMaxWidth = 550;

enum class Danger { Safe, Warning, Danger };

static Danger detectDanger(const QString& cmd) {
    auto c = cmd.toLower();
    if (c.contains("rm -rf") || c.contains("rm -r") || c.contains("dd if=")
        || c.contains("mkfs") || c.contains("/dev/sd") || c.contains("> /dev/")
        || c.contains("fork bomb") || c.contains(":(){ :|:& };:"))
        return Danger::Danger;
    if (c.contains("systemctl stop") || c.contains("kill") || c.contains("pkill")
        || c.contains("chmod") || c.contains("chown") || c.contains("mount ")
        || c.contains("umount") || c.contains("iptables"))
        return Danger::Warning;
    return Danger::Safe;
}

static const char* dangerIcon(Danger d) {
    switch (d) {
        case Danger::Danger:  return "warning";
        case Danger::Warning: return "error";
        case Danger::Safe:    return "check_circle";
    }
    return "";
}

static auto dangerLabel(Danger d, const QColor& color) -> QLabel* {
    return IconProvider::iconLabel(
        QString::fromUtf8(dangerIcon(d)), color, 16);
}

struct HoverBorder : QObject {
    HoverBorder(FilledCard* c, QColor normal, QColor hover)
        : card(c), normal_(normal), hover_(hover) {}
    bool eventFilter(QObject*, QEvent* e) override {
        if (e->type() == QEvent::Enter)
            card->set_border_color(hover_);
        else if (e->type() == QEvent::Leave)
            card->set_border_color(normal_);
        return false;
    }
    void setColors(QColor normal, QColor hover) {
        normal_ = normal; hover_ = hover;
        card->set_border_width(1.5);
        card->set_border_color(normal);
    }
    FilledCard* card;
    QColor normal_, hover_;
};

QWidget* buildCommandCards(const QJsonArray& items,
                           ThemeManager* manager,
                           QVector<Switch*>& switches) {
    auto* widget = new QWidget;
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);
    widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    const QFont commandFont("Courier", 9);
    const QFontMetrics commandFm(commandFont);
    auto scheme = manager->color_scheme();

    switches.reserve(items.size());

    for (int i = 0; i < items.size(); ++i) {
        auto obj    = items[i].toObject();
        int id      = obj["id"].toInt();
        QString reason  = obj["reason"].toString();
        QString command = obj["command"].toString();
        QString elidedCmd = commandFm.elidedText(command, Qt::ElideRight,
                                                  kCmdMaxWidth);

        Switch* sw = nullptr;

        auto* titleRow = new Row {
            lnpro::Margin { 0 },
            lnpro::Spacing { 8 },
            lnpro::Item<QWidget> {
                IconProvider::iconLabel(icon::kTerminal,
                    manager->color_scheme().on_surface, 16) },
            lnpro::Item<QWidget> {
                dangerLabel(detectDanger(command),
                    manager->color_scheme().on_surface) },
            lnpro::Item<Text> {
                text::pro::ThemeManager { *manager },
                wdpro::Font { QFont("sans-serif", 12) },
                text::pro::Text { QString("#%1 — %2").arg(id).arg(reason) },
            },
            lnpro::Stretch { 1 },
            lnpro::Item<Switch> {
                _switch::pro::ThemeManager { *manager },
                _switch::pro::Checked { true },
                wdpro::Bind { sw },
                wdpro::FixedSize { 60, 36 },
            },
        };

        auto* card = new FilledCard {
            card::pro::ThemeManager { *manager },
            card::pro::Layout<Col> {
                lnpro::Margin { 8 },
                lnpro::Spacing { 4 },
                lnpro::Item<Row> { titleRow },
                lnpro::Item<Text> {
                    text::pro::ThemeManager { *manager },
                    text::pro::Text { "→ " + elidedCmd },
                    wdpro::Font { commandFont },
                },
            },
        };
        card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        card->set_border_width(1.5);
        card->set_border_color(scheme.outline);
        card->setAttribute(Qt::WA_Hover, true);
        auto* hover = new HoverBorder(card, scheme.outline, scheme.primary);
        card->installEventFilter(hover);
        manager->append_handler(hover, [hover](const ThemeManager& m) {
            auto s = m.color_scheme();
            hover->setColors(s.outline, s.primary);
        });

        switches.append(sw);
        layout->addWidget(card);
    }

    layout->addStretch();
    return widget;
}
