#ifndef UTILS_H
#define UTILS_H

#include "defines.h"
#include <qfile.h>
#include <qstring.h>

class cfcUtils {
public:
    struct curvePoint {
        int x;
        int y;
    };

    static void writeLog(QString info);

    static int calcTable(const curvePoint table[],int count,int value);

private:
    static bool logFirstTime;
};

#endif
