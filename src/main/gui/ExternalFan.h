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
    static const int portCount=2;
    QSerialPort ports[portCount];

    qint64 lastControlTime = 0;
    qint64 currentTime = 0;
    int controlInterval;

    int pwrListLen;
    bool disabled=false;
    cfcUtils::curvePoint *fanTables[3];
    int fanTableLens[3];

    std::vector<float> pwrList[2];

    void setSpeed(int index, int num, int speed);
    void initPort(int index,QString name);
    void initPort(QSerialPort &port,QString name);
    QString searchPort(int index);

signals:
    void adjustFanSig(int index,float power);
};

#endif
