#include <QtWidgets/qapplication.h>
#include <QtGui/qfont.h>
#include <QtCore/qdebug.h>
#include <QtWidgets/qstylefactory.h>
#include <qcontainerfwd.h>
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
    cfcUtils::writeLog("cfc launching");

    try {
        //suid
        #ifdef __linux__
        QCoreApplication::setSetuidAllowed(true);
        #endif

        QApplication app(argc, argv);
        ClevoFanControl *cfc=new ClevoFanControl();
        int ret = 0;

        cfcUtils::writeLog("cfc init finish, entering loop");
        ret = app.exec();

        delete cfc;
        return ret;
    } catch (QException &exc) {
        cfcUtils::writeLog(QString("QException: ")+exc.what());
    } catch (std::exception &exc) {
        cfcUtils::writeLog(QString("std::exception: ")+exc.what());
    }

    cfcUtils::writeLog("cfc exiting");
}
