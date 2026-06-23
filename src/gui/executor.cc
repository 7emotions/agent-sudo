#include "executor.h"
#include "queue_io.h"
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

    QString fullOut;
    QStringList noise = {"[sudo]", "sudo:", "Sorry", "password"};
    while (proc.state() == QProcess::Running || proc.bytesAvailable() > 0) {
        if (proc.waitForReadyRead(1000)) {
            while (proc.canReadLine()) {
                auto line = proc.readLine();
                auto ls = QString::fromUtf8(line).trimmed();
                bool skip = false;
                for (auto& p : noise) {
                    if (ls.startsWith(p)) { skip = true; break; }
                }
                if (!skip) {
                    std::cout << line.toStdString();
                    std::cout.flush();
                }
                fullOut += ls + "\n";
            }
        }
    }
    auto rem = proc.readAll();
    if (!rem.isEmpty()) {
        std::cout << rem.toStdString();
        fullOut += QString::fromUtf8(rem);
    }

    if (!proc.waitForFinished(300000)) {
        proc.kill();
        proc.waitForFinished(5000);
        std::cerr << "ERROR: execution timed out" << std::endl;
        return 124;
    }

    int rc = proc.exitCode();
    if (rc == 1 && isAuthFail(fullOut)) {
        std::cerr << "ERROR: authentication failed" << std::endl;
        return 126;
    }

    if (rc == 0) {
        QJsonArray ex;
        QSet<int> sids;
        for (auto sv : selectedItems) sids.insert(sv.toObject()["id"].toInt());
        QString now = nowIso();
        QJsonArray rem;
        for (auto v : allItems) {
            auto o = v.toObject();
            if (sids.contains(o["id"].toInt())) {
                o["status"] = "executed";
                o["completed_at"] = now;
                ex.append(o);
            } else {
                rem.append(v);
            }
        }
        writeQueue({{"version", 1}, {"items", rem}});
        appendHistory(ex);
        std::cout << "EXECUTED: " << ex.size() << " commands" << std::endl;
        return 0;
    }

    std::cerr << "ERROR: commands failed with exit code " << rc << std::endl;
    return rc;
}
