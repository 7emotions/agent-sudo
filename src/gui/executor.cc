#include "executor.h"
#include "queue_io.h"
#include <QJsonDocument>
#include <QProcess>
#include <QSet>
#include <iostream>

int execCommands(const QString& password,
                 const QString& bashScript,
                 const QJsonArray& selectedItems,
                 const QJsonArray& allItems)
{
    QProcess proc;
    proc.start("sudo", {"-k", "-S", "bash", "-c", bashScript});
    if (!proc.waitForStarted(5000)) {
        QString pw = password;
        scrubPassword(pw);
        return 127;
    }
    proc.write(password.toUtf8() + "\n");
    proc.closeWriteChannel();
    {
        QString pw = password;
        scrubPassword(pw);
    }

    if (!proc.waitForFinished(30000)) {
        proc.kill();
        proc.waitForFinished(5000);
        std::cerr << "ERROR: execution timed out" << std::endl;
        return 124;
    }

    auto out = QString::fromUtf8(proc.readAllStandardOutput());
    auto err = QString::fromUtf8(proc.readAllStandardError());
    int rc = proc.exitCode();

    // Print output (filter noise)
    QStringList noise = {"[sudo]", "sudo:"};
    for (auto& line : out.split('\n')) {
        auto ls = line.trimmed();
        if (ls.isEmpty()) continue;
        bool skip = false;
        for (auto& p : noise)
            if (ls.startsWith(p)) { skip = true; break; }
        if (!skip) std::cout << ls.toStdString() << std::endl;
    }
    if (!err.trimmed().isEmpty()) {
        for (auto& line : err.split('\n')) {
            auto ls = line.trimmed();
            if (ls.isEmpty()) continue;
            bool skip = false;
            for (auto& p : noise)
                if (ls.startsWith(p)) { skip = true; break; }
            if (!skip) std::cerr << ls.toStdString() << std::endl;
        }
    }

    // Auth failure → retry once
    if (rc == 1 && isAuthFail(out + "\n" + err)) {
        std::cerr << "ERROR: authentication failed" << std::endl;
        clearQueue();
        return 126;
    }

    if (rc == 0) {
        QJsonArray ex;
        QSet<int> sids;
        for (auto sv : selectedItems) sids.insert(sv.toObject()["id"].toInt());
        QString now = nowIso();
        for (auto v : allItems) {
            auto o = v.toObject();
            if (sids.contains(o["id"].toInt())) {
                o["status"] = "executed";
                o["completed_at"] = now;
                ex.append(o);
            }
        }
        clearQueue();
        appendHistory(ex);
        std::cout << "EXECUTED: " << ex.size() << " commands" << std::endl;
        return 0;
    }

    // Build structured error info for LLM consumption
    QJsonArray failedItems;
    QSet<int> sids;
    for (auto sv : selectedItems) sids.insert(sv.toObject()["id"].toInt());
    for (auto v : allItems) {
        auto o = v.toObject();
        if (sids.contains(o["id"].toInt())) {
            o["status"] = "failed";
            o["exit_code"] = rc;
            failedItems.append(o);
        }
    }
    QJsonObject errInfo;
    errInfo["failed"] = failedItems;
    errInfo["queue_cleared"] = true;
    errInfo["error"] = "commands failed with exit code " + QString::number(rc);
    std::cout << "AGENT_SUDO_ERROR: "
              << QString(QJsonDocument(errInfo).toJson(QJsonDocument::Compact))
                     .toStdString()
              << std::endl;

    clearQueue();
    return rc;
}
