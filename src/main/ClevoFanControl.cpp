#include "ClevoFanControl.h"
#include "src/main/FanController.h"

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


ClevoFanControl::ClevoFanControl(QWidget *parent) :QWidget(parent) {
    qDebug()<<"cfc construct";
    cfgMgr=new ConfigManager;
    cfgMgr->readFromJson();

    //ui
    buildUi();
    TrayIcon->show();

    //start controllers
    cpuFan=new CpuFanController(cfgMgr,this);
    gpuFan=new GpuFanController(cfgMgr,this);
    QObject::connect(cpuFan, &CpuFanController::updateMonitor, monitor, &CFCmonitor::updateValue, Qt::BlockingQueuedConnection);
    QObject::connect(gpuFan, &GpuFanController::updateMonitor, monitor, &CFCmonitor::updateValue, Qt::BlockingQueuedConnection);
    cpuFan->start();
    gpuFan->start();
    
    cfgRamToTray();
   
    qDebug()<<"cfc construct finish";
    return;
}

ClevoFanControl::~ClevoFanControl() {
    qInfo()<<"cfc deconstructing";

    //stop controller
    cpuFan->setShouldStop();
    gpuFan->setShouldStop();
    delete cpuFan;
    delete gpuFan;

    //delete profiles and commands
    for(QAction *i : profileActions)
        delete i;
    for(auto i : commandAcions)
        delete i;

    cfgMgr->saveToJson();

    qInfo()<<"cfc deconstruction finish";
    return;
}

void ClevoFanControl::buildUi() {
    //tray main ui build
    TrayIcon = new QSystemTrayIcon(QIcon("ClevoFanControl.ico"), this);
    TrayIcon->setToolTip("Clevo Fan Control");
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
    for(int i=0;i<cfgMgr->profileCount;i++) {
        QAction *curAction=new QAction(this);
        curAction->setCheckable(1);
        curAction->setText(cfgMgr->fanProfiles[i].name);
        profilesGroup->addAction(curAction);
        trayProfilesMenu->addAction(curAction);
        profileActions.append(curAction);
        QObject::connect(curAction,&QAction::triggered,this,&ClevoFanControl::trayUpdated);
    }

    //tray command ui build and connect
    for(int i=0;i<cfgMgr->commandCount;i++) {
        QAction *curAction=new QAction(cfgMgr->commands[i].name,this);
        trayCommandsMenu->addAction(curAction);
        commandAcions.append(curAction);
        QObject::connect(curAction,&QAction::triggered,this,&ClevoFanControl::executeCommand);
    }

    //init windows
    monitor=new CFCmonitor(NULL);
    configWindow = new CFCconfig(nullptr, cfgMgr);

    //other connects
    QObject::connect(trayExitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
    QObject::connect(trayMonitorAction, &QAction::triggered, this, [this]() { monitor->show(); });
    QObject::connect(trayConfigAction, &QAction::triggered, this, [this]() { configWindow->show(); });
    QObject::connect(configWindow, &CFCconfig::configUpdatedInConfigWindow, this, &ClevoFanControl::cfgRamToTray);

    //final
    TrayIcon->setContextMenu(trayMainMenu);
}

void ClevoFanControl::initTrayEntry(QAction *&action,QString text, bool checkable) {
    action=new QAction(text,this);
    action->setCheckable(checkable);
    QObject::connect(action,&QAction::triggered,this,&ClevoFanControl::trayUpdated);
}

void ClevoFanControl::trayUpdated() {
    cfgTrayToRam();
    configWindow->setOptions();
    cfgMgr->saveToJson();
}

void ClevoFanControl::cfgTrayToRam() {
    qDebug()<<"cfgTrayToRam";
    for (int i=0;i<cfgMgr->profileCount;i++)
        if(profileActions[i]->isChecked())
            cfgMgr->profileInUse=i;
    cfgMgr->useStaticSpeed=trayStaticSpeedAction->isChecked();
    cfgMgr->useSpeedLimit=traySpeedLimitAction->isChecked();
    cfgMgr->maxSpeed=trayMaxSpeedAction->isChecked();
    cfgMgr->useClevoAuto=trayClevoAutoAction->isChecked();
    cfgMgr->monitorGpu=trayMonitorGpuAction->isChecked();
    qDebug()<<"cfgTrayToRam finish";
}

void ClevoFanControl::cfgRamToTray() {
    qDebug()<<"cfgRamToTray";
    for(int i=0;i<cfgMgr->profileCount;i++)
        profileActions[i]->setChecked(i == cfgMgr->profileInUse);
    trayStaticSpeedAction->setChecked(cfgMgr->useStaticSpeed);
    traySpeedLimitAction->setChecked(cfgMgr->useSpeedLimit);
    trayMaxSpeedAction->setChecked(cfgMgr->maxSpeed);
    trayClevoAutoAction->setChecked(cfgMgr->useClevoAuto);
    trayMonitorGpuAction->setChecked(cfgMgr->monitorGpu);
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

