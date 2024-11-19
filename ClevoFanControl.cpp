#include "ClevoFanControl.h"
#include <atomic>
#include <csignal>
#include <qaction.h>
#include <qcombobox.h>
#include <qcoreapplication.h>
#include <qdebug.h>
#include <qguiapplication.h>
#include <qkeysequence.h>
#include <qlogging.h>
#include <qmenu.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qthread.h>
#include <string>
#include <unistd.h>

bool havePrivilege=1;

#ifdef _WIN32
#include "OlsApiInit.h"
#endif

mutex EClock;

#define IA32_PACKAGE_THERM_STATUS_MSR 0x1B1

//std EC
#define EC_SC_REG 0x66
#define EC_DATA_REG 0x62
#define EC_READ_CMD 0x80
#define EC_WRITE_CMD 0x81
#define EC_SC_IBF_INDEX 1
#define EC_SC_OBF_INDEX 0
//clevo
#define EC_CPU_FAN_RPM_HI_ADDR 0xD0
#define EC_CPU_FAN_RPM_LO_ADDR 0xD1
#define EC_GPU_FAN_RPM_HI_ADDR 0xD2
#define EC_GPU_FAN_RPM_LO_ADDR 0xD3
#define EC_SET_FAN_SPEED_CMD 0x99
#define EC_SET_FAN_AUTO_ADDR 0xFF

#define C_PROFILE_INDEX 0
#define G_PROFILE_INDEX 1

void ioOutByte(unsigned short port, unsigned char value) {
	#ifdef _WIN32
    WriteIoPortByte(port, value);
    #elif __linux__
    outb(value, port);
    #endif
	return;
}

unsigned char ioInByte(unsigned short port) {
	unsigned char ret=0;
	#ifdef _WIN32
	ret = ReadIoPortByte(port);
	#elif __linux__
	ret = inb(port);
	#endif
	return ret;
}

void waitEc(unsigned short port, unsigned char index, unsigned char value) {
	unsigned char data=0;
	do
	{
		data=ioInByte(port);
		#ifdef __linux__
		usleep(1000);
		#endif
	}while(((data>>index)&((unsigned char)1))!=value);
	return;
}

// unsigned char readEc(unsigned char addr)
// {
//     //qDebug()<<"read ec";
//     EClock.lock();
// 	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
// 	ioOutByte(EC_SC_REG,EC_READ_CMD);
// 	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
// 	ioOutByte(EC_DATA_REG,addr);
// 	waitEc(EC_SC_REG,EC_SC_OBF_INDEX,1);
// 	unsigned char value=0;
// 	value=ioInByte(EC_DATA_REG);
//     //qDebug()<<"read ec finish";
//     EClock.unlock();
// 	return value;
// }

unsigned char readEc_1(unsigned char addr) {
    //qDebug()<<"read ec";
    EClock.lock();
    waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
    ioOutByte(EC_SC_REG,EC_READ_CMD);
    waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
    ioOutByte(EC_DATA_REG,addr);
    #ifdef _WIN32
    Sleep(1);
    #elif __linux__
    waitEc(EC_SC_REG,EC_SC_OBF_INDEX,1);
    #endif
    unsigned char value=0;
    value=ioInByte(EC_DATA_REG);
    //qDebug()<<"read ec finish";
    EClock.unlock();
	return value;
}

// void writeEc(unsigned char addr, unsigned char value)
// {
// 	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
// 	ioOutByte(EC_SC_REG,EC_WRITE_CMD);
// 	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
// 	ioOutByte(EC_DATA_REG,addr);
// 	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
// 	ioOutByte(EC_DATA_REG,value);
// 	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
// 	return;
// }

void doEc(unsigned char cmd, unsigned char addr, unsigned char value) {
    EClock.lock();
    waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);//not in doc, but for safety reason
    ioOutByte(EC_SC_REG,cmd);
    waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
    ioOutByte(EC_DATA_REG,addr);
    waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
    ioOutByte(EC_DATA_REG,value);
    waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
    EClock.unlock();
	return;
}

void setFanSpeed(int index,int percentage) {
    if(!havePrivilege) {
        qWarning()<<"No permission, setting fan speed aborted!";
        return;
    }
    if(percentage!=-1)
        doEc(EC_SET_FAN_SPEED_CMD, index,percentage*255/100);
    else
        doEc(EC_SET_FAN_SPEED_CMD, EC_SET_FAN_AUTO_ADDR, index);
}

int getCpuFanRpm() {
    if(!havePrivilege) {
        qWarning()<<"No permission, getting cpu fan speed aborted!";
        return 0;
    }
	int data = ((readEc_1(EC_CPU_FAN_RPM_HI_ADDR) << 8) + (readEc_1(EC_CPU_FAN_RPM_LO_ADDR)));
	return data==0 ? 0 : (2156220 / data);
}

int getGpuFanRpm() {
    if(!havePrivilege) {
        qWarning()<<"No permission, getting gpu fan speed aborted!";
        return 0;
    }
	int data = ((readEc_1(EC_GPU_FAN_RPM_HI_ADDR) << 8) + (readEc_1(EC_GPU_FAN_RPM_LO_ADDR)));
	return data==0 ? 0 : (2156220 / data);
}

int FanController::getCpuTemperature() {
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

int FanController::getGpuTemperature() {
    //qDebug()<<"get gpu temp";
    if(!config->monitorGpu)//temporary solution
        return 0;
    QProcess nvsmi;
    nvsmi.start("nvidia-smi",{"-q","-d=TEMPERATURE"});
    nvsmi.waitForFinished();
    QString output=nvsmi.readAllStandardOutput();
    int temperature=0;
    int index=output.indexOf(static_cast<QString>("GPU Current Temp"));
    int index2=output.indexOf(static_cast<QString>(":"),index);
    int index3=output.indexOf(static_cast<QString>(" "),index2+2);
    temperature=output.mid(index2+2,index3-index2-2).toInt();
    //qDebug()<<"get gpu temp finish: "<<temperature;
    return temperature;
}

ClevoFanControl::ClevoFanControl(QWidget *parent) :QWidget(parent) {
    qDebug()<<"cfc construct";
    cfgMgr=new ConfigManager;
    cfgMgr->readFromJson();

    #ifdef _WIN32
    //check permission
    bool WinRing0result=InitOpenLibSys_m();
    qDebug()<<"InitOpenLibSys result: "<<WinRing0result;
    //error handle
    #elif __linux__
    if(getuid()!=0)
        havePrivilege=0;
    ioperm(0x62, 1, 1);
    ioperm(0x66, 1, 1);
    #endif
    if(!havePrivilege)
        qWarning()<<"The app may be running without necessary permissions!";

    //tray main ui build
    TrayIcon = new QSystemTrayIcon(QIcon("ClevoFanControl.ico"), this);
    TrayIcon->setToolTip("Clevo Fan Control");
    menus[0] = new QMenu(this);
    menus[1]=new QMenu("Profiles", this);
    menus[2] = new QMenu("Commands", this);
    actions[0] = new QAction("Monitor...", this);
    actions[1] = new QAction("Config...", this);
    actions[2] = new QAction("Max Speed", this);
    actions[2]->setCheckable(1);
    actions[3] = new QAction("Speed Limit", this);
    actions[3]->setCheckable(1);
    actions[4] = new QAction("Static Speed", this);
    actions[4]->setCheckable(1);
    actions[5] = new QAction("Clevo Auto", this);
    actions[5]->setCheckable(1);
    actions[6] = new QAction("Monitor GPU", this);
    actions[6]->setCheckable(1);
    actions[7] = new QAction("Exit", this);
    for (int i = 0; i < 2; i++)
        menus[0]->addAction(actions[i]);
    menus[0]->addMenu(menus[1]);
    menus[0]->addMenu(menus[2]);
    for (int i = 2; i < 8; i++)
        menus[0]->addAction(actions[i]);

    //tray profile ui build and connect
    profileActions=new QAction[cfgMgr->profileCount];
    profilesGroup=new QActionGroup(this);
    for(int i=0;i<cfgMgr->profileCount;i++) {
        profileActions[i].setCheckable(1);
        profileActions[i].setText(cfgMgr->fanProfiles[i].name);
        profilesGroup->addAction(&profileActions[i]);
        menus[1]->addAction(&profileActions[i]);
        QObject::connect(&profileActions[i],&QAction::triggered,this,&ClevoFanControl::trayUpdated);
    }

    //tray command ui build and connect
    for(int i=0;i<cfgMgr->commandCount;i++) {
        QAction *curAction=new QAction(cfgMgr->commands[i].name,this);
        menus[2]->addAction(curAction);
        commandAcions.append(curAction);
        QObject::connect(curAction,&QAction::triggered,this,&ClevoFanControl::executeCommand);
    }

    //connect tray main ui actions
    for(int i=2;i<7;i++)
        QObject::connect(actions[i],&QAction::triggered,this,&ClevoFanControl::trayUpdated);

    //init windows
    monitor=new CFCmonitor(NULL);
    configWindow = new CFCconfig(nullptr, cfgMgr);

    //other connects
    QObject::connect(actions[7], &QAction::triggered, qApp, &QCoreApplication::quit);
    QObject::connect(actions[0], &QAction::triggered, this, [this]() { monitor->show(); });
    QObject::connect(actions[1], &QAction::triggered, this, [this]() { configWindow->show(); });
    QObject::connect(configWindow, &CFCconfig::configUpdatedInConfigWindow, this, &ClevoFanControl::cfgRamToTray);

    //final
    TrayIcon->setContextMenu(menus[0]);
    TrayIcon->show();

    //start controllerss
    stillRun=1;
    fan1=new FanController(1,&stillRun,&controllerRunning[0],cfgMgr,this);
    fan2=new FanController(2,&stillRun,&controllerRunning[1],cfgMgr,this);
    QObject::connect(fan1, &FanController::updateMonitor, monitor, &CFCmonitor::updateValue, Qt::BlockingQueuedConnection);
    QObject::connect(fan2, &FanController::updateMonitor, monitor, &CFCmonitor::updateValue, Qt::BlockingQueuedConnection);
    fan1->start();
    fan2->start();
    
    cfgRamToTray();
   
    qDebug()<<"cfc construct finish";
    return;
}

ClevoFanControl::~ClevoFanControl() {
    qInfo()<<"cfc deconstructing";

    stillRun=0;
    while(controllerRunning[0] || controllerRunning[1])
        QThread::msleep(10);
    fan1->quit();
    fan2->quit();
    delete fan1;
    delete fan2;

    delete [] profileActions;
    for(auto i : commandAcions)
        delete i;
    cfgMgr->saveToJson();

    #ifdef _WIN32
    bool WinRing0result=DeinitOpenLibSys_m();
    qDebug()<<"DeinitOpenLibSys result: "<<WinRing0result;
    #endif

    qInfo()<<"cfc deconstruction finish";
    return;
}

#ifdef _WIN32
bool ClevoFanControl::InitOpenLibSys_m() {
    return InitOpenLibSys(&WinRing0m);
}

bool ClevoFanControl::DeinitOpenLibSys_m() {
    return DeinitOpenLibSys(&WinRing0m);
}
#endif

void ClevoFanControl::trayUpdated() {
    cfgTrayToRam();
    configWindow->setOptions();
    cfgMgr->saveToJson();
}

void ClevoFanControl::cfgTrayToRam() {
    qDebug()<<"cfgTrayToRam";
    for (int i=0;i<cfgMgr->profileCount;i++)
        if(profileActions[i].isChecked())
            cfgMgr->profileInUse=i;
    cfgMgr->useStaticSpeed=actions[4]->isChecked();
    cfgMgr->useSpeedLimit=actions[3]->isChecked();
    cfgMgr->maxSpeed=actions[2]->isChecked();
    cfgMgr->useClevoAuto=actions[5]->isChecked();
    cfgMgr->monitorGpu=actions[6]->isChecked();
    qDebug()<<"cfgTrayToRam finish";
}

void ClevoFanControl::cfgRamToTray() {
    qDebug()<<"cfgRamToTray";
    for(int i=0;i<cfgMgr->profileCount;i++)
        profileActions[i].setChecked(i == cfgMgr->profileInUse);
    actions[4]->setChecked(cfgMgr->useStaticSpeed);
    actions[3]->setChecked(cfgMgr->useSpeedLimit);
    actions[2]->setChecked(cfgMgr->maxSpeed);
    actions[5]->setChecked(cfgMgr->useClevoAuto);
    actions[6]->setChecked(cfgMgr->monitorGpu);
    qDebug()<<"cfgRamToTray finish";
}

void ClevoFanControl::executeCommand() {
    qDebug()<<"executeCommand()";

    QString name=((QAction*)sender())->text();
    for(int i=0;i<cfgMgr->commandCount;i++) {
        if(cfgMgr->commands[i].name==name)
            system(cfgMgr->commands[i].content.toStdString().c_str());
    }

    qDebug()<<"executeCommand() finish";
}

FanController::FanController(int fanIndex, atomic_bool *flag, atomic_bool *isRunningFlag, ConfigManager *cfg, QObject *parent) : QThread(parent) {
    qDebug()<<"FanController construct fan: "<<fanIndex;
    index=fanIndex;
    runFlag=flag;
    runningFlag=isRunningFlag;
    config=cfg;
    lastControlTime=QDateTime::currentMSecsSinceEpoch();
}

FanController::~FanController() {
    qDebug()<<"FanController deconstruct fan: "<<index;
}

void FanController::run() {
    qDebug()<<"FanController run fan: "<<index;
    *runningFlag=1;
    while(*runFlag)
    {
        currentTime=QDateTime::currentMSecsSinceEpoch();
        if(currentTime>lastControlTime+config->timeIntervals[index-1])
        {
            //qDebug()<<"controling speed fan: "<<index;
            int targetSpeed=-1;
            if(config->useClevoAuto) {
                if(!isAuto) {
                    isAuto=1;
                    targetSpeed=-2;
                    qDebug()<<"adjust fan: "<<index<<" auto";
                }
                else {
                    targetSpeed=-3;
                    qDebug()<<"adjust fan: "<<index<<" already auto";
                }
            }
            else if(config->maxSpeed) {
                if(isAuto)
                    isAuto=0;
                targetSpeed=100;
                //qDebug()<<"adjust fan: "<<index<<" max";
            }
            else if(config->useStaticSpeed) {
                targetSpeed=config->staticSpeed[index-1];
                //qDebug()<<"adjust fan: "<<index<<" static"<<targetSpeed;
            }
            else {
                temperature = index==1 ? getCpuTemperature() : getGpuTemperature();
                int mode=config->fanProfiles[config->profileInUse].inUse[index-1];
                targetSpeed=curSpeed;
                if(mode==1) {
                    if(temperature>(config->fanProfiles[config->profileInUse].MTconfig[index-1][0]))//> max
                        targetSpeed+=config->fanProfiles[config->profileInUse].MTconfig[index-1][3];//+=step
                    else if(temperature<(config->fanProfiles[config->profileInUse].MTconfig[index-1][1]))//<min
                        targetSpeed-=config->fanProfiles[config->profileInUse].MTconfig[index-1][3];//-=step
                    targetSpeed=max({targetSpeed,minSafeSpeed,config->fanProfiles[config->profileInUse].MTconfig[index-1][2]});
                    targetSpeed=min(targetSpeed,100);
                }
                else if(mode==2) {
                    targetSpeed=config->fanProfiles[config->profileInUse].TSspeedList[index-1][0];
                    if(temperature<(config->fanProfiles[config->profileInUse].TStempList[index-1][0]))
                        targetSpeed=config->fanProfiles[config->profileInUse].TSspeedList[index-1][0];
                    else {
                        for(int i=0;i<10;i++)
                        {
                            if(i==9)
                                targetSpeed=config->fanProfiles[config->profileInUse].TSspeedList[index-1][9];
                            else if(temperature>=(config->fanProfiles[config->profileInUse].TStempList[index-1][i]) && temperature<(config->fanProfiles[config->profileInUse].TStempList[index-1][i+1]))
                            {
                                targetSpeed=config->fanProfiles[config->profileInUse].TSspeedList[index-1][i];
                                break;
                            }
                        }
                    }
                    targetSpeed=max(targetSpeed,minSafeSpeed);
                    targetSpeed=min(targetSpeed,100);
                }
                if(config->useSpeedLimit)
                    targetSpeed=min(targetSpeed,config->speedLimit[index-1]);
                //qDebug()<<"adjust fan: "<<index<<" normal"<<targetSpeed;
            }
            if(targetSpeed==-2) //auto
                setFanSpeed(index, -1);
            else if(targetSpeed!=-3)
            {
                if(curSpeed!=targetSpeed)
                {
                    qDebug()<<"apply speed: "<<index<<" "<<targetSpeed;
                    setFanSpeed(index,targetSpeed);
                    qDebug()<<"apply speed finish: "<<index<<" "<<targetSpeed;
                    curSpeed=targetSpeed;
                }
            }
            if(index==1)
                rpm=getCpuFanRpm();
            else if(index==2)
                rpm=getGpuFanRpm();

            emit updateMonitor(index,isAuto ? -2 : curSpeed,rpm,temperature);

            if(rpm==0)
                curSpeed=0;//toggle speed adjust
            lastControlTime=currentTime;
        }
        QThread::msleep(100);
    }
    setFanSpeed(index,-1); //finalize auto
   *runningFlag=false;
    qDebug()<<"FanController run finish "<<index;
}

CFCmonitor::CFCmonitor(QWidget *parent) : QWidget(parent) {
    this->setAttribute(Qt::WA_QuitOnClose, 0);
    this->setFixedSize(200,300);
    ui.setupUi(this);
}

void CFCmonitor::updateValue(int index, int speed, int rpm, int temperature) {
    //qDebug()<<"CFCmonitor::updateValue ";
    if(index==1)
    {
        ui.label_2->setText(to_string(temperature).c_str());
        if(speed==-2)
            ui.label_6->setText("Auto");
        else
            ui.label_6->setText(to_string(speed).c_str());
        ui.label_11->setText(to_string(rpm).c_str());
    }
    else if(index==2)
    {
        ui.label_4->setText(to_string(temperature).c_str());
        if(speed==-2)
            ui.label_8->setText("Auto");
        else
            ui.label_8->setText(to_string(speed).c_str());
        ui.label_12->setText(to_string(rpm).c_str());
    }
    //qDebug()<<"CFCmonitor::updateValue finish";
    return;
}

CFCconfig::CFCconfig(QWidget *parent, ConfigManager *cfg) : QWidget(parent) {
    config=cfg;

    this->setAttribute(Qt::WA_QuitOnClose, 0);
    ui.setupUi(this);

    //profiles
    for(int i=0;i<config->profileCount;i++)
        ui.comboBox->addItem(config->fanProfiles[i].name);

    setCurProfileOptions();
    setOptions();

    QObject::connect(ui.pushButton, &QPushButton::clicked, this, &CFCconfig::ok);
    QObject::connect(ui.pushButton_2, &QPushButton::clicked, this, &CFCconfig::apply);
    QObject::connect(ui.comboBox, &QComboBox::currentTextChanged, this, &CFCconfig::setCurProfileOptions);
}

void CFCconfig::setOptions() {
    ui.checkBox->setChecked(config->useStaticSpeed);
    ui.lineEdit_35->setText(QString::number(config->staticSpeed[0]));
    ui.lineEdit_37->setText(QString::number(config->staticSpeed[1]));
    ui.checkBox_2->setChecked(config->useSpeedLimit);
    ui.lineEdit_36->setText(QString::number(config->speedLimit[0]));
    ui.lineEdit_38->setText(QString::number(config->speedLimit[1]));
    ui.lineEdit_6->setText(QString::number(config->timeIntervals[0]));
    ui.lineEdit_7->setText(QString::number(config->timeIntervals[1]));
    ui.checkBox_3->setChecked(config->useClevoAuto);
    ui.checkBox_4->setChecked(config->maxSpeed);
    ui.checkBox_5->setChecked(config->monitorGpu);
}

void CFCconfig::setCurProfileOptions() {
    qDebug()<<"setCurProfileOptions";
    int curProfileIndex=getCurProfileIndex();

    //cpu
    ui.radioButton->setChecked(config->fanProfiles[curProfileIndex].inUse[0] == 2);//ts check
    ui.radioButton_2->setChecked(config->fanProfiles[curProfileIndex].inUse[0] == 1);//mt check
    for (int i = 0; i < 10; i++)
        ui.tableWidget->removeRow(0);
    for (int i = 0; i < 10; i++) {
        ui.tableWidget->insertRow(i);
        ui.tableWidget->setItem(i, 0, new QTableWidgetItem(
                QString::number(config->fanProfiles[curProfileIndex].TStempList[0][i])));//temp list
        ui.tableWidget->setItem(i, 1, new QTableWidgetItem(
                QString::number(config->fanProfiles[curProfileIndex].TSspeedList[0][i])));//speed list
    }
    ui.lineEdit->setText(QString::number(config->fanProfiles[curProfileIndex].MTconfig[0][0]));//mt max temp
    ui.lineEdit_2->setText(QString::number(config->fanProfiles[curProfileIndex].MTconfig[0][1]));//mt min temp
    ui.lineEdit_4->setText(QString::number(config->fanProfiles[curProfileIndex].MTconfig[0][2]));//mt min speed
    ui.lineEdit_5->setText(QString::number(config->fanProfiles[curProfileIndex].MTconfig[0][3]));//mt step

    //gpu 2
    ui.radioButton_11->setChecked(config->fanProfiles[curProfileIndex].inUse[1] == 2);
    ui.radioButton_12->setChecked(config->fanProfiles[curProfileIndex].inUse[1] == 1);
    for (int i = 0; i < 10; i++)
        ui.tableWidget_6->removeRow(0);
    for (int i = 0; i < 10; i++) {
        ui.tableWidget_6->insertRow(i);
        ui.tableWidget_6->setItem(i, 0, new QTableWidgetItem(
                QString::number(config->fanProfiles[curProfileIndex].TStempList[1][i])));
        ui.tableWidget_6->setItem(i, 1, new QTableWidgetItem(
                QString::number(config->fanProfiles[curProfileIndex].TSspeedList[1][i])));
    }
    ui.lineEdit_26->setText(QString::number(config->fanProfiles[curProfileIndex].MTconfig[1][0]));
    ui.lineEdit_27->setText(QString::number(config->fanProfiles[curProfileIndex].MTconfig[1][1]));
    ui.lineEdit_29->setText(QString::number(config->fanProfiles[curProfileIndex].MTconfig[1][2]));
    ui.lineEdit_0->setText(QString::number(config->fanProfiles[curProfileIndex].MTconfig[1][3]));
    qDebug()<<"setCurProfileOptions finish";
}

int CFCconfig::getCurProfileIndex() {
    QString curProfileName=ui.comboBox->currentText();
    int curProfileIndex=0;
    for(int i=0;i<config->profileCount;i++) {
        if(config->fanProfiles[i].name==curProfileName) {
            curProfileIndex=i;
            break;
        }
    }
    return curProfileIndex;
}

void CFCconfig::syncOptions() {
    int curProfileIndex=getCurProfileIndex();
    
    for (int i = 0; i < 10; i++) {
        config->fanProfiles[curProfileIndex].TStempList[0][i]=ui.tableWidget->item(i, 0)->text().toInt();
        config->fanProfiles[curProfileIndex].TSspeedList[0][i]=ui.tableWidget->item(i, 1)->text().toInt();
    }
    for (int i = 0; i < 10; i++) {
        config->fanProfiles[curProfileIndex].TStempList[1][i]=ui.tableWidget_6->item(i, 0)->text().toInt();
        config->fanProfiles[curProfileIndex].TSspeedList[1][i]=ui.tableWidget_6->item(i, 1)->text().toInt();
    }
    
    config->fanProfiles[curProfileIndex].inUse[0]=ui.radioButton->isChecked() ? 2 : 1;
    config->fanProfiles[curProfileIndex].MTconfig[0][0]=ui.lineEdit->text().toInt();
    config->fanProfiles[curProfileIndex].MTconfig[0][1]=ui.lineEdit_2->text().toInt();
    config->fanProfiles[curProfileIndex].MTconfig[0][2]=ui.lineEdit_4->text().toInt();
    config->fanProfiles[curProfileIndex].MTconfig[0][3]=ui.lineEdit_5->text().toInt();

    config->fanProfiles[curProfileIndex].inUse[1]=ui.radioButton_11->isChecked() ? 2 : 1;
    config->fanProfiles[curProfileIndex].MTconfig[1][0]=ui.lineEdit_26->text().toInt();
    config->fanProfiles[curProfileIndex].MTconfig[1][1]=ui.lineEdit_27->text().toInt();
    config->fanProfiles[curProfileIndex].MTconfig[1][2]=ui.lineEdit_29->text().toInt();
    config->fanProfiles[curProfileIndex].MTconfig[1][3]=ui.lineEdit_0->text().toInt();

    config->useStaticSpeed=ui.checkBox->isChecked();
    config->staticSpeed[0]=ui.lineEdit_35->text().toInt();
    config->staticSpeed[1]=ui.lineEdit_37->text().toInt();
    config->useSpeedLimit=ui.checkBox_2->isChecked();
    config->speedLimit[0]=ui.lineEdit_36->text().toInt();
    config->speedLimit[1]=ui.lineEdit_38->text().toInt();
    config->timeIntervals[0]=ui.lineEdit_6->text().toInt();
    config->timeIntervals[1]=ui.lineEdit_7->text().toInt();
    config->useClevoAuto=ui.checkBox_3->isChecked();
    config->maxSpeed=ui.checkBox_4->isChecked();
    config->monitorGpu=ui.checkBox_5->isChecked();
}

void CFCconfig::ok() {
    qDebug()<<"clicked OK";
    apply();
    this->hide();
}

void CFCconfig::apply() {
    qDebug()<<"clicked apply";
    syncOptions();
    config->saveToJson();
    emit configUpdatedInConfigWindow();
}

void ConfigManager::readFromJson() {
    //create is not exist
    if(!configFile.exists()) {
        createConfigJson();
    }
    
    //read
    configFile.open(QIODevice::ReadOnly);
    std::string jsonStr=configFile.readAll().toStdString();
    configFile.close();
    configJson.clear();
    configJson=json::parse(jsonStr);

    //profiles
    this->profileCount=configJson["profiles"].size();
    if(fanProfiles!=nullptr)
        delete [] fanProfiles;
    fanProfiles=new fanProfile[this->profileCount];
    for(int i=0;i<profileCount;i++) {
        fanProfiles[i].name=QString::fromStdString(configJson["profiles"][i][0]);
        for(int j=0;j<2;j++) {
            fanProfiles[i].inUse[j]=configJson["profiles"][i][j+1][0];
            for(int k=0;k<4;k++)
                fanProfiles[i].MTconfig[j][k]=configJson["profiles"][i][j+1][1][k];
            for(int k=0;k<10;k++) {
                fanProfiles[i].TStempList[j][k]=configJson["profiles"][i][j+1][2][0][k];
                fanProfiles[i].TSspeedList[j][k]=configJson["profiles"][i][j+1][2][1][k];
            }
        }
    }

    //commands
    this->commandCount=configJson["commands"].size();
    for(auto &i :configJson["commands"].items()) {
        commandEntry curCommand;
        curCommand.name=i.key().c_str();
        curCommand.content=((string)i.value()).c_str();
        commands.append(curCommand);
    }

    //others
    this->profileInUse=configJson["profileInUse"];
    this->useStaticSpeed=configJson["staticSpeed"][0];
    this->staticSpeed[0]=configJson["staticSpeed"][1];
    this->staticSpeed[1]=configJson["staticSpeed"][2];
    this->useSpeedLimit=configJson["speedLimit"][0];
    this->speedLimit[0]=configJson["speedLimit"][1];
    this->speedLimit[1]=configJson["speedLimit"][2];
    this->timeIntervals[0]=configJson["timeIntervals"][0];
    this->timeIntervals[1]=configJson["timeIntervals"][1];
    this->useClevoAuto=configJson["useClevoAuto"];
    this->maxSpeed=configJson["maxSpeed"];
    this->monitorGpu=configJson["monitorGpu"];
}

void ConfigManager::saveToJson() {
    qDebug()<<"saveConfigJson";

    //profiles
    json profileArray=json::array();
    for(int i=0;i<profileCount;i++)
    {
        json curProfile={
            fanProfiles[i].name.toStdString(),
            {
                fanProfiles[i].inUse[0],
                fanProfiles[i].MTconfig[0],
                {
                    fanProfiles[i].TStempList[0],
                    fanProfiles[i].TSspeedList[0]
                }
            },
            {
                fanProfiles[i].inUse[1],
                fanProfiles[i].MTconfig[1],
                {
                    fanProfiles[i].TStempList[1],
                    fanProfiles[i].TSspeedList[1]
                }
            }
        };
        profileArray+=curProfile;
    }

    //save commands
    json commandsSaved=configJson["commands"];

    //create json
    configJson.clear();
    configJson={
        {"profiles",profileArray},
        {"commands",commandsSaved},
        {"profileInUse",this->profileInUse},
        {"staticSpeed",{this->useStaticSpeed,this->staticSpeed[0],this->staticSpeed[1]}},
        {"speedLimit",{this->useSpeedLimit,speedLimit[0],speedLimit[1]}},
        {"timeIntervals",timeIntervals},
        {"useClevoAuto",this->useClevoAuto},
        {"maxSpeed",this->maxSpeed},
        {"monitorGpu",this->monitorGpu}
    };
    
    //write file
    configFile.open(QIODevice::WriteOnly);
    configFile.write(configJson.dump().c_str());
    configFile.close();
    qDebug()<<"saveConfigJson finish";
}

ConfigManager::ConfigManager() {
    configFile.setFileName((QDir::currentPath() + QDir::separator() + "config.json"));
}

ConfigManager::~ConfigManager() {
    delete [] fanProfiles;
}

void ConfigManager::createConfigJson() {
    //make default profile
    json defaultProfile={
        "default1",//name
        {//cpu fan
            1,//in use
            {80,75,20,5},//mt
            {//ts
                {58, 60, 63, 65, 68, 70, 73, 75, 78, 80},//temp list
                {20, 25, 30, 40, 50, 60, 70, 80, 90, 100}//speed list
            }
         },
         {//gpu fan
            1,//in use
            {80,75,20,5},//mt
            {//ts
                {58, 60, 63, 65, 68, 70, 73, 75, 78, 80},//temp list
                {20, 25, 30, 40, 50, 60, 70, 80, 90, 100}//speed list
            }
         }
    };
    configJson.clear();
    configJson={
    {"profiles",{defaultProfile,defaultProfile}},
        {"commands",{{"CPU-ondemond","cpupower frequency-set -g ondemand"},{"CPU-conservative","cpupower frequency-set -g conservative"}}},
        {"profileInUse",0},
        {"staticSpeed",{false,80,80}},
        {"speedLimit",{false,80,80}},
        {"timeIntervals",{2000,2000}},
        {"useClevoAuto",false},
        {"maxSpeed",false},
        {"monitorGpu",false}
    };
    configJson["profiles"][1][0]="default2";

    configFile.open(QIODevice::WriteOnly);
    configFile.write(configJson.dump().c_str());
    configFile.close();
}
