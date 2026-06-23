#pragma once
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QString>

namespace tr {

inline QMap<QString, QString> map;

inline auto load(const QString& lang) -> void {
    map.clear();
    auto path = QString(":/lang/%1.json").arg(lang);
    if (!QFile::exists(path)) {
        path = QString(":/lang/%1.json").arg(lang.left(2));
        if (!QFile::exists(path)) return;
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    auto obj = QJsonDocument::fromJson(f.readAll()).object();
    for (auto it = obj.begin(); it != obj.end(); ++it)
        map[it.key()] = it.value().toString();
}

inline auto t(const QString& key) -> QString {
    return map.value(key, key);
}

} // namespace tr
