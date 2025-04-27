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

struct ExFanInfo {
    int port;
    int num;
    int pwrType;
    int tableLen;
    cfcUtils::curvePoint *table;
};

class ExternalFan : public QObject {
Q_OBJECT

public:
    ExternalFan();
    ~ExternalFan();
    void adjustFan(int index,float power);

private:
    static const QString EX_FAN_CFG_FILE_DIR;
    static ExFanInfo fanInfoList[4];
    static const float portInfo[2];
    static const QString ch341Describe;
    static const QString exFanLogFlag;
    QSerialPort ports[2];

    qint64 lastControlTime = 0;
    qint64 currentTime = 0;
    int controlInterval;

    int pwrListLen;
    bool enabled=false;

    std::vector<float> pwrList[2];

    void setSpeed(int index, int num, int speed);
    void initPort(int index,QString name);
    void initPort(QSerialPort &port,QString name);
    QString searchPort(int index);

signals:
    void adjustFanSig(int index,float power);
};

#endif
