#include "ClevoFanControl.h"

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
    profilesGroup=new QActionGroup(this);
    for(int i=0;i<cfgMgr->profileCount;i++) {
        QAction *curAction=new QAction(this);
        curAction->setCheckable(1);
        curAction->setText(cfgMgr->fanProfiles[i].name);
        profilesGroup->addAction(curAction);
        menus[1]->addAction(curAction);
        profileActions.append(curAction);
        QObject::connect(curAction,&QAction::triggered,this,&ClevoFanControl::trayUpdated);
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
    controllerShouldRun=1;
    cpuFan=new CpuFanController(&controllerShouldRun,&controllerIsRunning[0],cfgMgr,this);
    gpuFan=new GpuFanController(&controllerShouldRun,&controllerIsRunning[1],cfgMgr,this);
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
    controllerShouldRun=0;
    while(controllerIsRunning[0] || controllerIsRunning[1])
        QThread::msleep(10);
    cpuFan->quit();
    gpuFan->quit();
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
        profileActions[i]->setChecked(i == cfgMgr->profileInUse);
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

