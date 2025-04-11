#ifndef EXTERNAL_FAN_H
#define EXTERNAL_FAN_H

#include <qlist.h>
#include <qobject.h>
#include <qserialport.h>
#include <qstring.h>
#include <qtmetamacros.h>
#include <vector>
#include <qdatetime.h>

#include "../utils.h"

class ExternalFan : public QObject {
Q_OBJECT

public:
    ExternalFan();
    ~ExternalFan();
    void adjustFan(int index,float power);

private:
    QString EX_FAN_CFG_FILE_DIR="./ex_fan.txt";
    QSerialPort port1;
    QSerialPort port2;

    qint64 lastControlTime = 0;
    qint64 currentTime = 0;
    int controlInterval;

    int pwrListLen;
    bool disabled=false;
    cfcUtils::curvePoint *fanTables[2];
    int fanTableLens[2];

    std::vector<float> pwrList[2];

    void setSpeed(QSerialPort &port, int num, int speed);
    void initPort(QSerialPort &port,QString name);

signals:
    void adjustFanSig(int index,float power);
};

#endif
