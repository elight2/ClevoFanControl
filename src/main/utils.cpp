#include "utils.h"
#include "defines.h"
#include <qlogging.h>
#include <qdebug.h>
#include <qdatetime.h>
#include <qobject.h>

bool cfcUtils::logFirstTime=true;

int cfcUtils::calcTable(const curvePoint table[],int count,int value) {
    if (count==1)
        return table[0].y;

    int index1,index2;
    for (int i=0;i<count;i++) {
        if (i==count-1) {
            index1=i-1;
            index2=i;
            break;
        } else if (table[i].x==value) {
            index1=i;
            index2=i;
            break;
        } else {
            if (value<table[i+1].x) {
                index1=i;
                index2=i+1;
                break;
            }
        }
    }

    return table[index1].y+float(table[index2].y-table[index1].y)*(value-table[index1].x)/(table[index2].x-table[index1].x);
}

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
