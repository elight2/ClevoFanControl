#include "FanController.h"

#include <QtCore/qprocess.h>
#include <QtCore/qdebug.h>
#include <QtCore/qdatetime.h>
#include <qlogging.h>

FanController::FanController(ConfigManager *config, QObject *parent) : QThread(parent) {
    qDebug()<<"FanController general construct";
    this->config=config;
}

FanController::~FanController() {
    qDebug()<<"FanController general deconstruct";
}

void FanController::setShouldStop() {
    shouldRun=0;
    while(isRunning)
        QThread::msleep(100);
}

void FanController::run() {
    qDebug()<<"FanController run fan: "<<index;
    isRunning=1;
    while(shouldRun) {
        currentTime=QDateTime::currentMSecsSinceEpoch();
        if(currentTime>lastControlTime+config->timeIntervals[index-1]) {
            //qDebug()<<"controling speed fan: "<<index;
            int targetSpeed=-1;

            if(config->useClevoAuto) {
                targetSpeed=-2;
                qDebug()<<"adjust fan: "<<index<<" auto";
            }
            else if(config->maxSpeed) {
                targetSpeed=100;
                //qDebug()<<"adjust fan: "<<index<<" max";
            }
            else if(config->useStaticSpeed) {
                targetSpeed=config->staticSpeed[index-1];
                //qDebug()<<"adjust fan: "<<index<<" static"<<targetSpeed;
            }
            else {
                temperature = getTemp();
                int mode=config->fanProfiles[config->profileInUse].inUse[index-1];
                targetSpeed=curSpeed;
                if(mode==1) {
                    if(temperature>(config->fanProfiles[config->profileInUse].MTconfig[index-1][0]))//> max
                        targetSpeed+=config->fanProfiles[config->profileInUse].MTconfig[index-1][3];//+=step
                    else if(temperature<(config->fanProfiles[config->profileInUse].MTconfig[index-1][1]))//<min
                        targetSpeed-=config->fanProfiles[config->profileInUse].MTconfig[index-1][3];//-=step
                    targetSpeed=std::max({targetSpeed,minSafeSpeed,config->fanProfiles[config->profileInUse].MTconfig[index-1][2]});
                    targetSpeed=std::min(targetSpeed,100);
                }
                else if(mode==2) {
                    targetSpeed=config->fanProfiles[config->profileInUse].TSspeedList[index-1][0];
                    if(temperature<(config->fanProfiles[config->profileInUse].TStempList[index-1][0]))
                        targetSpeed=config->fanProfiles[config->profileInUse].TSspeedList[index-1][0];
                    else {
                        for(int i=0;i<10;i++) {
                            if(i==9)
                                targetSpeed=config->fanProfiles[config->profileInUse].TSspeedList[index-1][9];
                            else if(temperature>=(config->fanProfiles[config->profileInUse].TStempList[index-1][i]) && temperature<(config->fanProfiles[config->profileInUse].TStempList[index-1][i+1])) {
                                targetSpeed=config->fanProfiles[config->profileInUse].TSspeedList[index-1][i];
                                break;
                            }
                        }
                    }
                    targetSpeed=std::max(targetSpeed,minSafeSpeed);
                    targetSpeed=std::min(targetSpeed,100);
                }
                if(config->useSpeedLimit)
                    targetSpeed=std::min(targetSpeed,config->speedLimit[index-1]);
                //qDebug()<<"adjust fan: "<<index<<" normal"<<targetSpeed;
            }

            if(targetSpeed==-2) { //auto
                curSpeed=-2;
                if(!curAuto) {
                    accessor.setFanSpeed(-1, index);
                    curAuto=true;
                }
            }
            else {
                curAuto=false;
                if(curSpeed!=targetSpeed) {
                    qDebug()<<"apply speed: "<<index<<" "<<targetSpeed;
                    accessor.setFanSpeed(targetSpeed, index);
                    qDebug()<<"apply speed finish: "<<index<<" "<<targetSpeed;
                    curSpeed=targetSpeed;
                }
            }
            if(index==1) //cpu
                rpm=getRpm();
            else if(index==2) //gpu
                rpm=getRpm();

            //qDebug()<<"emit: "<<index<<" "<<targetSpeed<<" "<<rpm<<" "<<temperature;
            emit updateMonitor(index,targetSpeed, rpm, temperature);

            if(rpm==0)
                curSpeed=0;//toggle speed adjust
            lastControlTime=currentTime;
        }
        QThread::msleep(100);
    }
    accessor.setFanSpeed(-1,index); //finalize auto
    isRunning=false;
    qDebug()<<"FanController run finish "<<index;
}

int FanController::getRpm() {
    return accessor.getRpm(index);
}

CpuFanController::CpuFanController(ConfigManager *config, QObject *parent) : FanController(config, parent) {
    index=1;
}

int CpuFanController::getTemp() {
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

GpuFanController::GpuFanController(ConfigManager *config, QObject *parent) : FanController(config, parent) {
    index=2;
}

int GpuFanController::getTemp() {
    //qDebug()<<"get gpu temp";
    int temperature=0;
    bool check=false;
    if(config->monitorGpu)
        check=true;
    else {
        //auto detect
#ifdef  __linux__
        if(config->gpuAutoDetectEnabled) {
            //sys file check
            QFile sysFile(config->gpuSysDir);
            sysFile.open(QIODevice::ReadOnly);
            QString status=sysFile.readLine();
            sysFile.close();
            
            //only continue when active
            if(status=="active\n") {
                //dev file check
                QProcess lsof;
                lsof.start("lsof",{config->gpuDevDir});
                lsof.waitForFinished();
                QString output=lsof.readAllStandardOutput();
                QStringList list=output.split('\n');
                for(int i=1;i<list.size();i++) {
                    int index=list[i].indexOf(' ');
                    QString proc=list[i].mid(0,index);
                    if(proc != "" && proc != "Xorg" && proc != "nvidia-sm" && proc != "WebKitWeb") {
                        check=true;
                        break;
                    }
                }
            }
        }
#endif
    }

    if(check) {
        QProcess nvsmi;
        nvsmi.start("nvidia-smi",{"-q","-d=TEMPERATURE"});
        nvsmi.waitForFinished();
        QString output=nvsmi.readAllStandardOutput();
        int index=output.indexOf(static_cast<QString>("GPU Current Temp"));
        int index2=output.indexOf(static_cast<QString>(":"),index);
        int index3=output.indexOf(static_cast<QString>(" "),index2+2);
        temperature=output.mid(index2+2,index3-index2-2).toInt();
    }
    //qDebug()<<"get gpu temp finish: "<<temperature;
    return temperature;
}
