#include "command_card.h"
#include "gui/icon.h"
#include "gui/icon_codes.h"

#include <creeper-qt/creeper-qt.hh>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QJsonArray>
#include <QJsonObject>
#include <QVBoxLayout>

using namespace creeper;
namespace capro = card::pro;
namespace lnpro = linear::pro;
namespace wdpro = widget::pro;

static constexpr int kCmdMaxWidth = 550;

struct CardHover : QObject {
    FilledCard* card;
    QColor normal, hover;
    CardHover(FilledCard* c, QColor n, QColor h)
        : card(c), normal(n), hover(h) {}
    bool eventFilter(QObject*, QEvent* e) override {
        if (e->type() == QEvent::Enter)
            card->set_border_color(hover);
        else if (e->type() == QEvent::Leave)
            card->set_border_color(normal);
        return false;
    }
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
                    scheme.on_surface, 16) },
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
            capro::ThemeManager { *manager },
            capro::Layout<Col> {
                lnpro::Margin { 0 },
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
        card->set_border_width(1);
        card->set_border_color(scheme.outline);
        card->setAttribute(Qt::WA_Hover, true);
        auto* hover = new CardHover(card, scheme.outline, scheme.primary);
        card->installEventFilter(hover);
        manager->append_handler(card, [hover](const ThemeManager& m) {
            auto s = m.color_scheme();
            hover->normal = s.outline;
            hover->hover  = s.primary;
            hover->card->set_border_color(s.outline);
        });

        switches.append(sw);
        layout->addWidget(card);
    }

    layout->addStretch();
    return widget;
}
