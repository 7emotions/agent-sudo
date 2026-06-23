#include "command_card.h"
#include "gui/icon.h"

#include <creeper-qt/creeper-qt.hh>
#include <QFont>
#include <QFontMetrics>
#include <QJsonArray>
#include <QJsonObject>
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

QWidget* buildCommandCards(const QJsonArray& items,
                           ThemeManager* manager,
                           QVector<Switch*>& switches) {
    auto* widget = new QWidget;
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);
    // Prevent vertical expansion so cards pack at top of scroll viewport
    widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    const QFont commandFont("Courier", 9);
    const QFontMetrics commandFm(commandFont);

    switches.reserve(items.size());

    for (int i = 0; i < items.size(); ++i) {
        auto obj    = items[i].toObject();
        int id      = obj["id"].toInt();
        QString reason  = obj["reason"].toString();
        QString command = obj["command"].toString();

        // Elide long commands; the reason is what the user cares about
        QString elidedCmd = commandFm.elidedText(command, Qt::ElideRight,
                                                 kCmdMaxWidth);

        Switch* sw = nullptr;

        auto* titleRow = new Row {
            lnpro::Margin { 0 },
            lnpro::Spacing { 8 },
            lnpro::Item<Text> {
                text::pro::ThemeManager { *manager },
                text::pro::Text {
                    QString::fromUtf8(material::icon::kTerminal) },
                wdpro::Font {
                    QFont(material::regular::font, material::size::_1) },
            },
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

        switches.append(sw);
        layout->addWidget(card);
    }

    // Push cards to the top of the scroll area
    layout->addStretch();
    return widget;
}
