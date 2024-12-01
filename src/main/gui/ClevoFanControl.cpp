#include "ClevoFanControl.h"

ClevoFanControl::ClevoFanControl(QWidget *parent) :QWidget(parent) {
    qDebug()<<"cfc construct";
    config=new ConfigManager;
    config->readFromJson();

    //ui
    buildUi();
    TrayIcon->show();

    //start controllers
    cpuFan=new CpuFanController(config,this);
    gpuFan=new GpuFanController(config,this);
    QObject::connect(cpuFan, &CpuFanController::updateMonitor, monitor, &CFCmonitor::updateValue, Qt::BlockingQueuedConnection);
    QObject::connect(gpuFan, &GpuFanController::updateMonitor, monitor, &CFCmonitor::updateValue, Qt::BlockingQueuedConnection);
    cpuFan->start();
    gpuFan->start();
    
    cfgMgrToTray();
   
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

    config->saveToJson();

    qInfo()<<"cfc deconstruction finish";
    return;
}

void ClevoFanControl::buildUi() {
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
