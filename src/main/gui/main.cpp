#include <QtWidgets/qapplication.h>
#include <QtGui/qfont.h>
#include <QtCore/qdebug.h>
#include <QtWidgets/qstylefactory.h>
#include <qlogging.h>
#include "ClevoFanControl.h"

#ifdef _WIN32
#include <Windows.h>
#endif

int main(int argc, char *argv[])
{
    qDebug()<<"app main()";
#ifdef __linux__
    QCoreApplication::setSetuidAllowed(true);
#endif
    QApplication app(argc, argv);
    ClevoFanControl *cfc=new ClevoFanControl();
    int ret = 0;
    
    ret = app.exec();

    delete cfc;
    return ret;
}
