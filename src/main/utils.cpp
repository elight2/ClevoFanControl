#include "utils.h"
#include "defines.h"
#include <qlogging.h>
#include <qdebug.h>
#include <qdatetime.h>

void writeLog(QString info) {
    QFile logFile(LOG_DIR);
    if(!logFile.exists()) {
        logFile.open(QIODeviceBase::WriteOnly);
        logFile.close();
    }

    logFile.open(QIODeviceBase::Append);
    logFile.write((QDateTime::currentDateTime().toString()+" "+info+"\n").toUtf8());
    logFile.close();

    qDebug()<<"LOG: "+info;
}
