#include "utils.h"
#include "defines.h"
#include <qlogging.h>
#include <qdebug.h>
#include <qdatetime.h>
#include <qobject.h>

bool cfcUtils::logFirstTime=true;

void cfcUtils::writeLog(QString info) {
    QFile logFile(cfcDef::LOG_DIR);
    if(!logFile.exists() || logFirstTime) {
        logFile.open(QIODeviceBase::WriteOnly);
        logFile.close();
        logFirstTime=false;
    }

    logFile.open(QIODeviceBase::Append);
    logFile.write(QString("%1 %2\n").arg(QDateTime::currentDateTime().toString(),info).toUtf8());
    logFile.close();

    qDebug()<<"LOG:"<<info;
}
