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
namespace oc = outlined_card::pro;
namespace mwpro = main_window::pro;
namespace capro = card::pro;
namespace lnpro = linear::pro;
namespace wdpro = widget::pro;

// Available width for command text inside a 700px window:
//   700 - 28(card padding) - 16(scroll+mask padding) - 32(switch) ≈ 624
// Leave headroom: use 550.
static constexpr int kCmdMaxWidth = 550;

/// Event filter that highlights a card on hover by swapping color schemes.
class HoverHighlighter : public QObject {
public:
    HoverHighlighter(OutlinedCard* card, const ColorScheme& normal,
                     const ColorScheme& hover)
        : card_(card), normal_(normal), hover_(hover) {}

protected:
    bool eventFilter(QObject*, QEvent* e) override {
        if (e->type() == QEvent::Enter)
            card_->set_color_scheme(hover_);
        else if (e->type() == QEvent::Leave)
            card_->set_color_scheme(normal_);
        return false;
    }

private:
    OutlinedCard* card_;
    ColorScheme normal_, hover_;
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
    // Hover variant: slightly elevated surface
    ColorScheme hoverScheme = scheme;
    hoverScheme.surface = scheme.surface_container_high;

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

        auto* card = new OutlinedCard {
            capro::ThemeManager { *manager },
            capro::Layout<Col> {
                lnpro::Margin { 6 },
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
        card->setAttribute(Qt::WA_Hover, true);
        card->set_border_color(scheme.outline);
        card->installEventFilter(
            new HoverHighlighter(card, scheme, hoverScheme));

        switches.append(sw);
        layout->addWidget(card);
    }

    layout->addStretch();
    return widget;
}
