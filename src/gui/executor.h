#pragma once
#include <QJsonArray>
#include <QString>

/// Run the queued sudo commands after the user approved in the GUI.
/// Returns the process exit code (0, 124, 126, 127, or the sudo rc).
int execCommands(const QString& password,
                 const QString& bashScript,
                 const QJsonArray& selectedItems,
                 const QJsonArray& allItems);
