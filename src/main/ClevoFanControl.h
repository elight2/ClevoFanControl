#ifndef CLEVO_FAN_CONTROL_H
#define CLEVO_FAN_CONTROL_H

#include <QtWidgets/qaction.h>
#include <QtCore/qlist.h>
#include <QtGui/qicon.h>
#include <QtWidgets/qsystemtrayicon.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qtablewidget.h>
#include <QtWidgets/qmessagebox.h>
#include <QtWidgets/qactiongroup.h>
#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qdebug.h>
#include <QtWidgets/qapplication.h>
#include <qaction.h>
#include <qkeysequence.h>

#include "ConfigManager.h"
#include "CFCmonitor.h"
#include "CFCconfig.h"
#include "FanController.h"

using json = nlohmann::json;

class ClevoFanControl : public QWidget {
Q_OBJECT

public:
    ClevoFanControl(QWidget *parent = nullptr);
    ~ClevoFanControl();

private:
    QDir CFCpath = QDir::current();
    ConfigManager *cfgMgr=nullptr;
    //ui
    QSystemTrayIcon *TrayIcon = NULL;
    QMenu * trayMainMenu=nullptr;
    QMenu * trayProfilesMenu=nullptr;
    QMenu * trayCommandsMenu=nullptr;
    QAction *trayMonitorAction=nullptr;
    QAction *trayConfigAction=nullptr;
    QAction *trayMaxSpeedAction=nullptr;
    QAction *traySpeedLimitAction=nullptr;
    QAction *trayStaticSpeedAction=nullptr;
    QAction *trayClevoAutoAction=nullptr;
    QAction *trayMonitorGpuAction=nullptr;
    QAction *trayExitAction=nullptr;
    QList<QAction*> profileActions;
    QList<QAction*>commandAcions;
    QActionGroup *profilesGroup;
    //window
    CFCmonitor *monitor=nullptr;
    CFCconfig *configWindow=nullptr;
    //fan
    CpuFanController *cpuFan=nullptr;
    GpuFanController *gpuFan=nullptr;

    void buildUi();
    void initTrayEntry(QAction *&action,QString text, bool checkable);
    void executeCommand();
    //ui
    void updateMonitorSlot(int index, int speed, int rpm, int temperature);
    void cfgTrayToRam();
    void cfgRamToTray();
    void trayUpdated();
};

#endif
