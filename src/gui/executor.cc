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

    // Poll and stream output in real time.
    // The human approved these commands — no timeout, just wait.
    QByteArray allOut, allErr;
    while (!proc.waitForFinished(100)) {
        QByteArray chunk = proc.readAllStandardOutput();
        if (!chunk.isEmpty()) {
            std::cout << chunk.toStdString() << std::flush;
            allOut.append(chunk);
        }
        chunk = proc.readAllStandardError();
        if (!chunk.isEmpty()) {
            std::cerr << chunk.toStdString() << std::flush;
            allErr.append(chunk);
        }
    }
    // Drain any remaining output after process exits.
    allOut.append(proc.readAllStandardOutput());
    allErr.append(proc.readAllStandardError());

    int rc = proc.exitCode();
    QString out = QString::fromUtf8(allOut);
    QString err = QString::fromUtf8(allErr);

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
