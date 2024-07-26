#include <iostream>
#include <QtWidgets/qapplication.h>
#include "ClevoFanControl.h"

#ifdef _WIN32
#include <Windows.h>
#endif

int main(int argc, char *argv[])
{
    #ifdef __linux__
    QCoreApplication::setSetuidAllowed(true);
    #endif
    QApplication app(argc, argv);
    QFont AppFont("Microsoft Yahei", 9);
    app.setFont(AppFont);
    ClevoFanControl *cfc=new ClevoFanControl();
    int ret = 0;
    
    ret = app.exec();

    delete cfc;
    return ret;
}