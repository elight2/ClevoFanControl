#include "ClevoFanControl.h"

#include "../utils.h"
#include "ExternalFan.h"
#include <qobject.h>

ClevoFanControl::ClevoFanControl(QWidget *parent) :QWidget(parent) {
    qDebug()<<"cfc construct";
    config=new ConfigManager;
    config->readFromJson();

    //ui
    buildUi();
    TrayIcon->show();

    //ex fan
    #ifdef CFC_USE_EX_FAN
    exFan=new ExternalFan();
    exFan->start();
    QObject::connect(exFan,&ExternalFan::adjustFanSig,exFan,&ExternalFan::recordData);
#endif
    
    cfgMgrToTray();

    //start controllers

    cpuFan=new FanController(config,this,1,monitor,exFan);
    gpuFan=new FanController(config,this,2,monitor,exFan);
    cpuFan->start();
    gpuFan->start();
   
    qDebug()<<"cfc construct finish";
    return;
}

ClevoFanControl::~ClevoFanControl() {
    CfcLogMgr::LOG_MGR->writeLog("cfc deconstructing");

    //stop controller
    cpuFan->requestInterruption();
    gpuFan->requestInterruption();
    cpuFan->wait();
    gpuFan->wait();
    cpuFan->quit();
    gpuFan->quit();
    delete cpuFan;
    delete gpuFan;
    CfcLogMgr::LOG_MGR->writeLog("fan controllers stopped");

    //stop ex fan
#ifdef CFC_USE_EX_FAN
    exFan->requestInterruption();
    exFan->wait();
    exFan->quit();
    delete exFan;
    CfcLogMgr::LOG_MGR->writeLog("ex fan controllers stopped");
#endif

    //delete profiles and commands
    for(QAction *i : profileActions)
        delete i;
    for(auto i : commandAcions)
        delete i;

    config->saveToJson();

    CfcLogMgr::LOG_MGR->writeLog("cfc deconstruction finish");
    return;
}

void ClevoFanControl::buildUi() {
    emit emit CfcLogMgr::LOG_MGR->writeLog("building ui");

    //tray main ui build
    TrayIcon = new QSystemTrayIcon(QIcon("ClevoFanControl.ico"), this);
    trayMainMenu = new QMenu(this);
    trayProfilesMenu=new QMenu("Profiles", this);
    trayCommandsMenu = new QMenu("Commands", this);
    initTrayEntry(trayMonitorAction, "Monitor...", false);
    initTrayEntry(trayConfigAction, "Config...", false);
    initTrayEntry(trayMaxSpeedAction, "Max Speed", true);
    initTrayEntry(traySpeedLimitAction, "Speed Limit", true);
    initTrayEntry(trayStaticSpeedAction, "Static Speed", true);
    initTrayEntry(trayClevoAutoAction, "Clevo Auto", true);
    initTrayEntry(trayMonitorGpuAction, "Monitor GPU", true);
    initTrayEntry(trayExitAction, "Exit", false);
    //add in an order
    trayMainMenu->addAction(trayMonitorAction);
    trayMainMenu->addAction(trayConfigAction);
    trayMainMenu->addMenu(trayProfilesMenu);
    trayMainMenu->addMenu(trayCommandsMenu);
    trayMainMenu->addAction(trayMaxSpeedAction);
    trayMainMenu->addAction(traySpeedLimitAction);
    trayMainMenu->addAction(trayStaticSpeedAction);
    trayMainMenu->addAction(trayClevoAutoAction);
    trayMainMenu->addAction(trayMonitorGpuAction);
    trayMainMenu->addAction(trayExitAction);

    //tray profile ui build and connect
    profilesGroup=new QActionGroup(this);
    for(int i=0;i<config->profileCount;i++) {
        QAction *curAction=new QAction(this);
        curAction->setCheckable(1);
        curAction->setText(config->fanProfiles[i].name);
        profilesGroup->addAction(curAction);
        trayProfilesMenu->addAction(curAction);
        profileActions.append(curAction);
        QObject::connect(curAction,&QAction::triggered,this,&ClevoFanControl::trayUpdated);
    }

    //tray command ui build and connect
    for(int i=0;i<config->commandCount;i++) {
        QAction *curAction=new QAction(config->commands[i].name,this);
        trayCommandsMenu->addAction(curAction);
        commandAcions.append(curAction);
        QObject::connect(curAction,&QAction::triggered,this,&ClevoFanControl::executeCommand);
    }

    //init windows
    monitor=new CFCmonitor(NULL);
    configWindow = new CFCconfig(nullptr, config);

    //other connects
    QObject::connect(trayExitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
    QObject::connect(trayMonitorAction, &QAction::triggered, this, [this]() { monitor->show(); });
    QObject::connect(trayConfigAction, &QAction::triggered, this, [this]() { configWindow->show(); });
    QObject::connect(configWindow, &CFCconfig::cfgWindowUpdate, this, &ClevoFanControl::cfgMgrToTray);

    //final
    TrayIcon->setContextMenu(trayMainMenu);
    TrayIcon->setToolTip("Clevo Fan Control");

    emit emit CfcLogMgr::LOG_MGR->writeLog("build ui finish");
}

void ClevoFanControl::initTrayEntry(QAction *&action,QString text, bool checkable) {
    action=new QAction(text,this);
    action->setCheckable(checkable);
    QObject::connect(action,&QAction::triggered,this,&ClevoFanControl::trayUpdated);
}

void ClevoFanControl::trayUpdated() {
    cfgTrayToMgr();
    configWindow->setOptions();
    config->saveToJson();
}

void ClevoFanControl::cfgTrayToMgr() {
    qDebug()<<"cfgTrayToRam";
    for (int i=0;i<config->profileCount;i++)
        if(profileActions[i]->isChecked())
            config->profileInUse=i;
    config->useStaticSpeed=trayStaticSpeedAction->isChecked();
    config->useSpeedLimit=traySpeedLimitAction->isChecked();
    config->maxSpeed=trayMaxSpeedAction->isChecked();
    config->useClevoAuto=trayClevoAutoAction->isChecked();
    config->monitorGpu=trayMonitorGpuAction->isChecked();
    qDebug()<<"cfgTrayToRam finish";
}

void ClevoFanControl::cfgMgrToTray() {
    qDebug()<<"cfgRamToTray";
    for(int i=0;i<config->profileCount;i++)
        profileActions[i]->setChecked(i == config->profileInUse);
    trayStaticSpeedAction->setChecked(config->useStaticSpeed);
    traySpeedLimitAction->setChecked(config->useSpeedLimit);
    trayMaxSpeedAction->setChecked(config->maxSpeed);
    trayClevoAutoAction->setChecked(config->useClevoAuto);
    trayMonitorGpuAction->setChecked(config->monitorGpu);
    qDebug()<<"cfgRamToTray finish";
}

void ClevoFanControl::executeCommand() {
    qDebug()<<"executeCommand()";

    QString name=((QAction*)sender())->text();
    for(int i=0;i<config->commandCount;i++) {
        if(config->commands[i].name==name)
            system(config->commands[i].content.toStdString().c_str());
    }

    qDebug()<<"executeCommand() finish";
}
