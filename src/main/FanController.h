#ifndef FAN_CONTROLLER_H
#define FAN_CONTROLLER_H

#include "ConfigManager.h"
#include "ClevoEcAccessor.h"

#include <QtCore/qthread.h>

class FanController : public QThread {
Q_OBJECT

public:
    FanController(ConfigManager *config, QObject *parent);
    ~FanController();

    void setShouldStop();

protected:
    virtual int getTemp()=0;

    ConfigManager *config;
    int index=-1;

private:
    void run();
    int getRpm();

    ClevoEcAccessor accessor;
    std::atomic_bool shouldRun=1;
    std::atomic_bool isRunning=0;
    qint64 lastControlTime = 0;
    qint64 currentTime = 0;
    int curSpeed=30;
    const int minSafeSpeed=10;
    bool curAuto=false;
    int temperature=0;
    int rpm=0;

signals:
    void updateMonitor(int index, int speed, int rpm, int temperature);
};

class CpuFanController : public FanController {
Q_OBJECT

public:
    CpuFanController(ConfigManager *config, QObject *parent);

protected:
    int getTemp();
};

class GpuFanController : public FanController {
Q_OBJECT

public:
    GpuFanController(ConfigManager *config, QObject *parent);

protected:
    int getTemp();
};

#endif
