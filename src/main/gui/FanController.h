#ifndef FAN_CONTROLLER_H
#define FAN_CONTROLLER_H

#include <atomic>

#include "ConfigManager.h"
#include "../ClevoEcAccessor.h"
#include "CFCmonitor.h"
#include "../defines.h"
#include "ExternalFan.h"

#include <QtCore/qthread.h>
#include <qcontainerfwd.h>
#include <qlist.h>
#include <qtmetamacros.h>

class CpuPowerMonitor {
public:
    CpuPowerMonitor(int index);
    float getPower();

private:
    float getCurEnergy();
    void rdmsr(int pos, char *dest);

    float lastEnergy;
    long lastQueryTime;
    int cpuIndex;
    char cpuMsrDir[1024];
    const int MSR_RAPL_POWER_UNIT=0x606;
    const int MSR_PKG_ENERGY_STATUS=0x611;
};

class HardwareMonitor : public QThread {
Q_OBJECT

public:
    HardwareMonitor(int index, ConfigManager *cfg, QObject *parent);
    ~HardwareMonitor();

    std::atomic_int temperature=0;
    std::atomic<float> power=0;
    static std::atomic_bool shouldMonitorGpu;
    QList<float> lastPower;

private:
    void run();
    int getcTemp();
    float getcPower();
    int getgTemp();
    float getgPower();

    QStringList nvsmiOutputParser(QStringList args, QString flag);
    bool checkShouldMonitorGpu();
    // bool checkDevFile();
    bool checkSysFile();
    bool checkNvsmiProc();

    const int nvsmiPauseInterval=12;

    int index;
    ConfigManager *cfg;
    CpuPowerMonitor *cmonitor;
    qint64 gpuCheckPauseTime=0;
    bool gpuCheckPaused=false;
    QFile cpuTempFile;

signals:
    void requireUpdateMonitor2(int index, int temperature, float power);
};

class FanController : public QThread {
Q_OBJECT

public:
    FanController(ConfigManager *config, QObject *parent, int index, CFCmonitor *appMonitor, ExternalFan *exFan);
    ~FanController();
    void stop();

    ConfigManager *config;
    int index;

private:
    void run();
    int getRpm();
    int getMinSpeed();

    ExternalFan *exFan;
    HardwareMonitor *hwMonitor;
    CFCmonitor *appMonitor;
    ClevoEcAccessor accessor;
    qint64 lastControlTime = 0;
    qint64 currentTime = 0;
    int curSpeed=CfcDef::DEFAULT_SPEED;
    int curMinSafeSpeed=0;
    int rpm=0;
    float power=0;

signals:
    void requireUpdateMonitor1(int index, int speed, int rpm);
};

#endif
