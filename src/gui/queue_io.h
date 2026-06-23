#pragma once
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

/// Cache directory for queue and history files.
inline const QString QDIR = QDir::homePath() + "/.cache/agent-sudo";
inline const QString QFILE = QDIR + "/queue.json";
inline const QString HFILE = QDIR + "/history.json";

/// Read and parse queue.json.  Returns a default empty queue on any error.
QJsonObject readQueue();

/// Overwrite queue.json.  Creates the cache directory if missing.
void writeQueue(const QJsonObject& data);

/// Write an empty queue (used on reject / auto-reject).
void clearQueue();

/// Append one or more entries to the persistent history log.
void appendHistory(const QJsonArray& entries);

/// UTC timestamp in ISO-8601 format.
QString nowIso();

/// Heuristic check for sudo authentication failure in stdout/stderr.
bool isAuthFail(const QString& text);

/// In-place zero-fill then clear a password string.
void scrubPassword(QString& pw);
