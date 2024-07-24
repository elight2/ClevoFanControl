#ifdef _WIN32
#include <tchar.h>
#include <Windows.h>
#elif __linux__
#include <sys/io.h>
#endif
#include <atomic>
#include <mutex>
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
#include <QtCore/qdatetime.h>
#include <QtCore/qdir.h>
#include <QtCore/qprocess.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsonvalue.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qfile.h>
#include <QtCore/qdebug.h>
#include <QtCore/qprocess.h>
#include <QtWidgets/qapplication.h>
#include "build/ui_config.h"
#include "build/ui_monitor.h"

class FanController : public QThread {
Q_OBJECT

public:
    FanController(int fanIndex, QObject *parent = nullptr);
    ~FanController();
    void setConfig(QJsonObject data);

protected:
    void run();

private:
    QJsonObject config;
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
    void updateValue(int index, int speed, int rpm, int temperature);
};

class CFCmonitor : public QWidget {
Q_OBJECT

public:
    CFCmonitor(QWidget *parent);

    Ui::CFCmonitorWindow ui;
};

class CFCconfig : public QWidget {
Q_OBJECT

public:
    CFCconfig(QWidget *parent);
    void OK();
    void setOptionsFromJson();
    void getOptionsToJson();
    void apply();

    Ui::CFCconfigWindow ui;

signals:
    void updateConfigData();
};

class ClevoFanControl : public QWidget
{
Q_OBJECT

public:
    ClevoFanControl(QWidget *parent = nullptr);
    ~ClevoFanControl();

private:
    HMODULE WinRing0m;
    QDir CFCpath = QDir::current();

    QSystemTrayIcon *TrayIcon = NULL;
    QMenu *menus[1] = {NULL};
    QAction *actions[7] = {NULL};
    //QActionGroup *groups[4] = {NULL};

    CFCmonitor *monitor=nullptr;
    CFCconfig *configWindow=nullptr;

    FanController *fan1=nullptr;
    FanController *fan2=nullptr;

    BOOL InitOpenLibSys_m();
    BOOL DeinitOpenLibSys_m();

    void loadConfigJson();
    void updateMonitorValueSlot(int index, int speed, int rpm, int temperature);
    void updateConfigDataSlot();
    void updateDataFromContext();
};