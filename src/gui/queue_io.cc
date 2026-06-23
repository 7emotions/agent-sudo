#include "queue_io.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>

// ---------------------------------------------------------------------------
// readQueue
// ---------------------------------------------------------------------------
QJsonObject readQueue() {
    QFile f(QFILE);
    if (!f.exists() || !f.open(QIODevice::ReadOnly))
        return {{"version", 1}, {"items", QJsonArray()}};
    auto doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject() || doc.object().value("version").toInt() != 1)
        return {{"version", 1}, {"items", QJsonArray()}};
    return doc.object();
}

// ---------------------------------------------------------------------------
// writeQueue
// ---------------------------------------------------------------------------
void writeQueue(const QJsonObject& d) {
    QDir().mkpath(QDIR);
    QFile f(QFILE);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    f.write(QJsonDocument(d).toJson(QJsonDocument::Indented));
    f.flush();
    f.close();
}

// ---------------------------------------------------------------------------
// clearQueue
// ---------------------------------------------------------------------------
void clearQueue() {
    writeQueue({{"version", 1}, {"items", QJsonArray()}});
}

// ---------------------------------------------------------------------------
// appendHistory
// ---------------------------------------------------------------------------
void appendHistory(const QJsonArray& entries) {
    if (entries.isEmpty()) return;
    QDir().mkpath(QDIR);
    QJsonObject hist;
    {
        QFile f(HFILE);
        if (f.exists() && f.open(QIODevice::ReadOnly)) {
            auto doc = QJsonDocument::fromJson(f.readAll());
            if (doc.isObject()) hist = doc.object();
            f.close();
        }
    }
    if (hist.value("version").toInt() != 1)
        hist = {{"version", 1}, {"entries", QJsonArray()}};
    auto arr = hist.value("entries").toArray();
    for (const auto& e : entries) arr.append(e);
    hist["entries"] = arr;
    QFile f(HFILE);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        f.write(QJsonDocument(hist).toJson(QJsonDocument::Indented));
        f.flush();
        f.close();
    }
}

// ---------------------------------------------------------------------------
// nowIso
// ---------------------------------------------------------------------------
QString nowIso() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

// ---------------------------------------------------------------------------
// isAuthFail
// ---------------------------------------------------------------------------
bool isAuthFail(const QString& t) {
    auto lo = t.toLower();
    return lo.contains("incorrect password")
        || lo.contains("authentication failure")
        || lo.contains("sorry, try again")
        || t.contains(QString::fromUtf8("密码错误"));
}

// ---------------------------------------------------------------------------
// scrubPassword
// ---------------------------------------------------------------------------
void scrubPassword(QString& pw) {
    std::fill(pw.begin(), pw.end(), QChar(0));
    pw.clear();
}
