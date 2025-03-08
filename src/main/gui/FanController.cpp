#include "FanController.h"
#include "CFCmonitor.h"
#include "ConfigManager.h"

#include <QtCore/qprocess.h>
#include <QtCore/qdebug.h>
#include <QtCore/qdatetime.h>
#include <algorithm>
#include <qcontainerfwd.h>
#include <qcoreapplication.h>
#include <qlogging.h>
#include <qthread.h>

#ifdef _WIN32
#include "../winRing0Api.h"
#endif

//for cpu temp in Windows
#define IA32_PACKAGE_THERM_STATUS_MSR 0x1B1

void CpuPowerMonitor::rdmsr(int pos, char *dest) {
    memset(dest, 0, 8);
#ifdef __linux__
    try {
        FILE *fp=fopen(cpuMsrDir,"rb");
        if(fp==NULL)
            return;
        fseek(fp, pos, SEEK_SET);
        fread(dest,8,1,fp);
        fclose(fp);
        return;
    } catch(std::exception &exc) {
        return;
    }
#elif _WIN32
    DWORD eax=0;
	DWORD edx=0;
    Rdmsr(pos,&eax,&edx);
    memcpy(dest,&eax,4);
    memcpy(dest+4,&edx,4);
#endif
}

CpuPowerMonitor::CpuPowerMonitor(int index) {
    this->cpuIndex=index;
    this->lastQueryTime=std::chrono::system_clock::now().time_since_epoch().count();
    std::string msrDir="/dev/cpu/"+std::to_string(cpuIndex)+"/msr";
    strcpy(cpuMsrDir, msrDir.c_str());
    this->lastEnergy=getCurEnergy();
}

double CpuPowerMonitor::getCurEnergy() {
    char buff[8];
    rdmsr(MSR_RAPL_POWER_UNIT, buff);
    char times=0;
    memcpy(&times, buff+1, 1);
    rdmsr(MSR_PKG_ENERGY_STATUS, buff);
    uint32_t oriEnergy;
    memcpy(&oriEnergy,buff,4);
    double realEnergy=(double)oriEnergy*1000/std::pow(2,times); // in mwatt
    return realEnergy;
}

double CpuPowerMonitor::getPower() {
    long curTime=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    double curEnergy=getCurEnergy();
    double pwr=(curEnergy-lastEnergy)/(curTime-lastQueryTime);
    lastQueryTime=curTime;
    lastEnergy=curEnergy;
    return pwr;
}

HardwareMonitor::HardwareMonitor(int index, ConfigManager *cfg, QObject *parent) : QThread(parent) {
    this->index=index;
    this->cfg=cfg;
}

void HardwareMonitor::stop() {
    this->shouldRun=false;
    while(this->running)
        QThread::msleep(100);
}

void HardwareMonitor::run() {
    qDebug()<<"HardwareMonitor start fan: "<<index;
    this->running=true;

    if(index==1)
        this->cmonitor=new CpuPowerMonitor(0);

    while(this->shouldRun) {
        if(index==1) {
            this->temperature=getcTemp();
            this->power=getcPower();
        } else if(index==2) {
            this->shouldMonitorGpu=checkShouldMonitorGpu();
            if(this->shouldMonitorGpu) {
                this->temperature=getgTemp();
                this->power=getgPower();
            } else {
                this->temperature=0;
                this->power=0;
            }
        }
        emit requireUpdateMonitor2(this->index, this->temperature, this->power);
        QThread::msleep(cfg->monitorIntervals[this->index-1]);
    }

    if(index==1)
        delete this->cmonitor;

    this->running=false;
    qDebug()<<"HardwareMonitor finish fan: "<<index;
}

int HardwareMonitor::getcTemp() {
    //qDebug()<<"get cpu temp";
    int temperature=0;
#ifdef _WIN32
    DWORD eax=0;
	DWORD edx=0;
    Rdmsr(IA32_PACKAGE_THERM_STATUS_MSR,&eax,&edx);
    temperature=100-((eax & 0x007F0000) >> 16);
#elif __linux__
    QProcess bash;
    bash.start("bash",{"-c","paste <(cat /sys/class/thermal/thermal_zone*/type) <(cat /sys/class/thermal/thermal_zone*/temp) |grep x86_pkg_temp"});
    bash.waitForFinished();
    QString output=bash.readAllStandardOutput();
    QString tempStr=output.mid(13, output.size() - 13);
    temperature=tempStr.toInt();
    temperature/=1000;
#endif
    //qDebug()<<"get cpu temp finish: "<<temperature;
    return temperature;
}

double HardwareMonitor::getcPower() {
    return this->cmonitor->getPower();
}

int HardwareMonitor::getgTemp() {
    QStringList list;
    try {
        list=nvsmiOutputParser({"-q","--display=TEMPERATURE"}, "GPU Current Temp");
        return list[0].mid(0,list[0].size()-1).toInt();
    } catch (const char* exc) {
        return 0;
    }
}

double HardwareMonitor::getgPower() {
    QStringList list;
    try {
        list=nvsmiOutputParser({"-q","--display=POWER"}, "Power Draw");
        return list[0].mid(0,list[0].size()-1).toDouble();
    } catch (const char* exc) {
        return 0;
    }
}

QStringList HardwareMonitor::nvsmiOutputParser(QStringList args, QString flag) {
    QStringList result;
    QProcess nvsmi;
    nvsmi.start("nvidia-smi",args);
    nvsmi.waitForFinished();
    QString output=nvsmi.readAllStandardOutput();

    if(output.contains("NVIDIA-SMI has failed because it couldn't communicate with the NVIDIA driver.")) {
        throw "driver-error";
    }

    QStringList list=output.split('\n');
    for(auto curStr : list) {
        int index=curStr.indexOf(flag);
        if(index!=-1) {
            int index2=curStr.indexOf(":",index);
            result.push_back(curStr.mid(index2+2,curStr.size()-index2));
        }
    }
    return result;
}

// bool GpuFanController::checkDevFile() {
//     QProcess lsof;
//     lsof.start("lsof",{config->gpuDevDir});
//     lsof.waitForFinished();
//     QString output=lsof.readAllStandardOutput();

//     if(output=="") //empty list
//         return false;

//     //found proc
//     QStringList outputList=output.split('\n');
//     for(int i=1;i<outputList.size();i++) {
//         int spaceIndex=outputList[i].indexOf(' ');
//         QString procName=outputList[i].mid(0,spaceIndex);

//         //check exclude proc
//         if(!(config->gpuLsofExcludeProc.contains(procName)))
//             return true;
//     }

//     return false;
// }

bool HardwareMonitor::checkSysFile() {
    QFile sysFile(cfg->gpuSysDir);
    if(sysFile.exists()) {
        sysFile.open(QIODevice::ReadOnly);
        QString status=sysFile.readLine();
        sysFile.close();
        return status=="active\n";
    } else
        return false;
}

bool HardwareMonitor::checkNvsmiProc() {
    QStringList list;
    try {
        list=nvsmiOutputParser({"-q","--display=PIDS"}, "Name");
    } catch (const char* exc) {
        return false;
    }
    for(auto curStr : list) {
        if(curStr!="/usr/lib/Xorg")
            return true;
    }

    return false;
}

bool HardwareMonitor::checkShouldMonitorGpu() {
    if(cfg->monitorGpu) //when force enabled
        return true;

    //auto detect
#ifdef __linux__ //linux only, on windows will return 0
    if(cfg->gpuAutoDetectEnabled) {
        if(gpuCheckPaused) {
            if(QDateTime::currentSecsSinceEpoch()<=gpuCheckPauseTime+nvsmiPauseInterval)
                return false;
            else
                gpuCheckPaused=false;
        }

        if(!checkSysFile())
            return false;
        else {
            if(!checkNvsmiProc()) {
                gpuCheckPaused=true;
                gpuCheckPauseTime=QDateTime::currentSecsSinceEpoch();
            }
            return true;
        }
    }

    return false;
#elif _WIN32
    return false;
#endif
}

FanController::FanController(ConfigManager *config, QObject *parent, int index, CFCmonitor *appMonitor) : QThread(parent) {
    qDebug()<<"FanController general construct";
    this->index=index;
    this->config=config;
    this->appMonitor=appMonitor;
    this->hwMonitor=new HardwareMonitor(index,config,this);
    QObject::connect(this, &FanController::requireUpdateMonitor1, appMonitor, &CFCmonitor::updateValue1, Qt::BlockingQueuedConnection);
    QObject::connect(hwMonitor, &HardwareMonitor::requireUpdateMonitor2, appMonitor, &CFCmonitor::updateValue2, Qt::BlockingQueuedConnection);
}

FanController::~FanController() {
    qDebug()<<"FanController general deconstruct";
}

void FanController::stop() {
    this->shouldRun=false;
    while(running) {
        QThread::msleep(100);
        QCoreApplication::processEvents();
    }
}

void FanController::run() {
    qDebug()<<"FanController start fan: "<<index;
    running=true;
    this->hwMonitor->start();
    this->accessor.setFanSpeed(defaultSpeed, this->index);
    while(shouldRun) {
        currentTime=QDateTime::currentMSecsSinceEpoch();
        fanArg *curProfileArgs=&(config->fanProfiles[config->profileInUse].args[index-1]);
        if(currentTime>lastControlTime+curProfileArgs->operateInterval) {
            // qDebug()<<"time to adjust fan: "<<index;

            rpm=getRpm();
            //first decide speed
            int targetSpeed=-10;
            if (config->useClevoAuto)
                targetSpeed=-1;
            else if (config->useStaticSpeed)
                targetSpeed=std::clamp(config->staticSpeed[index-1],0,100);
            else if (config->maxSpeed)
                targetSpeed=100;
            else {
                int curTemp=this->hwMonitor->temperature;
                if (curTemp>curProfileArgs->speedUpTemp)
                    targetSpeed=this->curSpeed+curProfileArgs->speedStep;
                else if (curTemp<curProfileArgs->slowDownTemp)
                    targetSpeed=this->curSpeed-curProfileArgs->speedStep;
                else
                    targetSpeed=curSpeed;
                targetSpeed=std::clamp(targetSpeed,curProfileArgs->minSpeed,100);
                if (hwMonitor->shouldMonitorGpu) //prevent overheating
                    targetSpeed=std::clamp(targetSpeed,minSafeSpeedWhenGpuActive,100);
            }

            //correct the data
            if (config->useSpeedLimit && targetSpeed!=-1)
                targetSpeed=std::clamp(targetSpeed,0,config->speedLimit[index-1]);

            //then apply speed
            if (curSpeed!=targetSpeed) {
                //auto
                if (targetSpeed==-1)
                    accessor.setFanSpeed(-1, index);
                else
                    accessor.setFanSpeed(targetSpeed, index);
                curSpeed=targetSpeed;
            }
            

            // this->curMinSafeSpeed=this->hwMonitor->shouldMonitorGpu ? this->minSafeSpeedWhenGpuActive : 0;
            // rpm=getRpm();
            // int targetSpeed=-1;

            // if(config->useClevoAuto) {
            //     targetSpeed=-2;
            // }
            // else if(config->maxSpeed) {
            //     targetSpeed=100;
            // }
            // else if(config->useStaticSpeed) {
            //     targetSpeed=config->staticSpeed[index-1];
            // }
            // else {
            //     int mode=config->fanProfiles[config->profileInUse].inUse[index-1];
            //     targetSpeed=curSpeed;
            //     if(mode==1) {
            //         if(this->hwMonitor->temperature>(config->fanProfiles[config->profileInUse].MTconfig[index-1][0]))//> max
            //             targetSpeed+=config->fanProfiles[config->profileInUse].MTconfig[index-1][3];//+=step
            //         else if(this->hwMonitor->temperature<(config->fanProfiles[config->profileInUse].MTconfig[index-1][1]))//<min
            //             targetSpeed-=config->fanProfiles[config->profileInUse].MTconfig[index-1][3];//-=step
            //         targetSpeed=std::max({targetSpeed,curMinSafeSpeed,config->fanProfiles[config->profileInUse].MTconfig[index-1][2]});
            //         targetSpeed=std::min(targetSpeed,100);
            //     }
            //     else if(mode==2) {
            //         targetSpeed=config->fanProfiles[config->profileInUse].TSspeedList[index-1][0];
            //         if(this->hwMonitor->temperature<(config->fanProfiles[config->profileInUse].TStempList[index-1][0]))
            //             targetSpeed=config->fanProfiles[config->profileInUse].TSspeedList[index-1][0];
            //         else {
            //             for(int i=0;i<10;i++) {
            //                 if(i==9)
            //                     targetSpeed=config->fanProfiles[config->profileInUse].TSspeedList[index-1][9];
            //                 else if(this->hwMonitor->temperature>=(config->fanProfiles[config->profileInUse].TStempList[index-1][i]) && this->hwMonitor->temperature<(config->fanProfiles[config->profileInUse].TStempList[index-1][i+1])) {
            //                     targetSpeed=config->fanProfiles[config->profileInUse].TSspeedList[index-1][i];
            //                     break;
            //                 }
            //             }
            //         }
            //         targetSpeed=std::max(targetSpeed,curMinSafeSpeed);
            //         targetSpeed=std::min(targetSpeed,100);
            //     }
            //     if(config->useSpeedLimit)
            //         targetSpeed=std::min(targetSpeed,config->speedLimit[index-1]);
            // }

            // qDebug()<<"determine speed finish: "<<index;
            // if(targetSpeed==-2) { //auto
            //     curSpeed=-2;
            //     if(!curAuto) {
            //         accessor.setFanSpeed(-1, index);
            //         qDebug()<<"speed applied: "<<index;
            //         curAuto=true;
            //     }
            // }
            // else {
            //     curAuto=false;
            //     if(curSpeed!=targetSpeed) {
            //         accessor.setFanSpeed(targetSpeed, index);
            //         qDebug()<<"speed applied: "<<index;
            //         curSpeed=targetSpeed;
            //     }
            // }
            
            emit requireUpdateMonitor1(index,targetSpeed, rpm);
            
            lastControlTime=currentTime;
        }
        QThread::msleep(minControlInterval);
    }
    accessor.setFanSpeed(-1,index); //finalize auto
    this->hwMonitor->stop();
    running=false;
    qDebug()<<"FanController run finish "<<index;
}

int FanController::getRpm() {
    return accessor.getRpm(index);
}
