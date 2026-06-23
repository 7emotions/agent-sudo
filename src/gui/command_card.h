#pragma once
#include <creeper-qt/creeper-qt.hh>
#include <QJsonArray>
#include <QVector>
#include <QWidget>

/// Build a scrollable list of command-approval cards from queue items.
/// Returns the scroll content widget and populates the switches vector.
/// The caller owns the returned widget; switches are owned by the widget tree.
QWidget* buildCommandCards(const QJsonArray& items,
                           creeper::theme::ThemeManager* manager,
                           QVector<creeper::Switch*>& switches);
