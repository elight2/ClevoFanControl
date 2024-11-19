#include <qaction.h>
#include <qlist.h>
#ifdef _WIN32
#include <tchar.h>
#include <Windows.h>
#elif __linux__
#include <sys/io.h>
#include <unistd.h>
#endif
#include <atomic>
#include <cmath>
#include <QtCore/qthread.h>
#include <QtWidgets/qwidget.h>
#include <QtGui/qicon.h>
#include <QtWidgets/qsystemtrayicon.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qaction.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qtablewidget.h>
#include <QtWidgets/qmessagebox.h>
#include <QtWidgets/qactiongroup.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qdir.h>
#include <QtCore/qprocess.h>
#include <QtCore/qfile.h>
#include <QtCore/qdebug.h>
#include <QtCore/qprocess.h>
#include <QtCore/qstring.h>
#include <QtWidgets/qapplication.h>
#include "build/ui_config.h"
#include "build/ui_monitor.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;
using namespace std;

struct fanProfile {
    QString name;
    int inUse[2];//c,g
    int MTconfig[2][4];
    int TStempList[2][10];
    int TSspeedList[2][10];
};

struct commandEntry {
    QString name;
    QString content;
};

class ConfigManager {
public:
    json configJson;
    int profileCount;
    int commandCount;
    fanProfile* fanProfiles=nullptr;
    QList<commandEntry> commands;
    int profileInUse;
    bool useStaticSpeed;
    int staticSpeed[2];
    bool useSpeedLimit;
    int speedLimit[2];
    int timeIntervals[2];
    bool useClevoAuto;
    bool maxSpeed;
    bool monitorGpu;

    ConfigManager();
    ~ConfigManager();
    void readFromJson();
    void saveToJson();
    void createConfigJson();

    private:
    QFile configFile;
};

class FanController : public QThread {
Q_OBJECT

public:
    FanController(int fanIndex, atomic_bool *requireRunFlag, atomic_bool *isRunningFlag, ConfigManager *cfg, QObject *parent = nullptr);
    ~FanController();

protected:
    void run();

private:
    ConfigManager *config;
    atomic_bool *runFlag;
    atomic_bool *runningFlag;
    int index;
    qint64 lastControlTime = 0;
    qint64 currentTime = 0;
    int curSpeed=30;
    const int minSafeSpeed=10;
    bool isAuto=0;
    int temperature=0;
    int rpm=0;

    int getCpuTemperature();
    int getGpuTemperature();

signals:
    void updateMonitor(int index, int speed, int rpm, int temperature);
};

class CFCmonitor : public QWidget {
Q_OBJECT

public:
    CFCmonitor(QWidget *parent);
    void updateValue(int index, int speed, int rpm, int temperature);

    Ui::CFCmonitorWindow ui;
};

class CFCconfig : public QWidget {
Q_OBJECT

public:
    CFCconfig(QWidget *parent, ConfigManager *cfg);
    void ok();
    int getCurProfileIndex();
    void syncOptions();
    void apply();
    void setCurProfileOptions();
    void setOptions();

    Ui::CFCconfigWindow ui;

private:
    ConfigManager *config;

signals:
    void configUpdatedInConfigWindow();
};

class ClevoFanControl : public QWidget {
Q_OBJECT

public:
    ClevoFanControl(QWidget *parent = nullptr);
    ~ClevoFanControl();

private:
    #ifdef _WIN32
    HMODULE WinRing0m;
    #endif
    QDir CFCpath = QDir::current();
    ConfigManager *cfgMgr=nullptr;

    QSystemTrayIcon *TrayIcon = NULL;
    QMenu *menus[3];
    QAction *actions[8];
    QAction *profileActions;
    QList<QAction*>commandAcions;
    QActionGroup *profilesGroup;

    CFCmonitor *monitor=nullptr;
    CFCconfig *configWindow=nullptr;

    FanController *fan1=nullptr;
    FanController *fan2=nullptr;
    atomic_bool stillRun;
    atomic_bool controllerRunning[2];

#ifdef _WIN32
    bool InitOpenLibSys_m();
    bool DeinitOpenLibSys_m();
#endif

    void updateMonitorSlot(int index, int speed, int rpm, int temperature);
    void cfgTrayToRam();
    void cfgRamToTray();
    void trayUpdated();
    void executeCommand();
};
