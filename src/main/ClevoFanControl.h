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

    QSystemTrayIcon *TrayIcon = NULL;
    QMenu *menus[3];
    QAction *actions[8];
    QList<QAction*> profileActions;
    QList<QAction*>commandAcions;
    QActionGroup *profilesGroup;

    CFCmonitor *monitor=nullptr;
    CFCconfig *configWindow=nullptr;

    CpuFanController *cpuFan=nullptr;
    GpuFanController *gpuFan=nullptr;
    std::atomic_bool controllerShouldRun;
    std::atomic_bool controllerIsRunning[2];

    void updateMonitorSlot(int index, int speed, int rpm, int temperature);
    void cfgTrayToRam();
    void cfgRamToTray();
    void trayUpdated();
    void executeCommand();
};
