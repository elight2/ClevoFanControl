#include <QtWidgets/qapplication.h>
#include <QtGui/qfont.h>
#include <QtCore/qdebug.h>
#include <QtWidgets/qstylefactory.h>
#include <qcontainerfwd.h>
#include <qdir.h>
#include <qlogging.h>
#include <qexception.h>
#include <exception>
#include "ClevoFanControl.h"
#include "../utils.h"

#ifdef _WIN32
#include <Windows.h>
#endif

int main(int argc, char *argv[])
{
    try {
        QDir().mkpath(CfcDef::DATA_DIR);
        CfcLogMgr::LOG_MGR=new CfcLogMgr;
        emit CfcLogMgr::LOG_MGR->writeLog("cfc launching");

        //suid
        #ifdef __linux__
        QCoreApplication::setSetuidAllowed(true);
        #endif

        QApplication app(argc, argv);
        ClevoFanControl *cfc=new ClevoFanControl();
        int ret = 0;

        emit CfcLogMgr::LOG_MGR->writeLog("cfc init finish, entering loop");
        ret = app.exec();

        delete cfc;

        emit CfcLogMgr::LOG_MGR->writeLog("cfc exiting");
        delete CfcLogMgr::LOG_MGR;

        return ret;
    } catch (QException &exc) {
        emit CfcLogMgr::LOG_MGR->writeLog(QString("QException: ")+exc.what());
    } catch (std::exception &exc) {
        emit CfcLogMgr::LOG_MGR->writeLog(QString("std::exception: ")+exc.what());
    }
}
