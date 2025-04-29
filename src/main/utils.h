#ifndef UTILS_H
#define UTILS_H

#include "defines.h"
#include <qfile.h>
#include <qobject.h>
#include <qstring.h>
#include <qtmetamacros.h>

class CfcUtils {
public:
    struct curvePoint {
        int x;
        int y;
    };

    static int calcTable(const curvePoint table[],int count,int value);
};

class CfcLogMgr : public QObject {
Q_OBJECT

public:
    CfcLogMgr();
    ~CfcLogMgr();
    void writeLogFunc(QString info);

    static CfcLogMgr *LOG_MGR;

private:
    QFile logFile;

signals:
    void writeLog(QString info);
};

#endif
