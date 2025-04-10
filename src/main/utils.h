#ifndef UTILS_H
#define UTILS_H

#include "defines.h"
#include <qfile.h>
#include <qstring.h>

class cfcUtils {
public:
    static void writeLog(QString info);

private:
    static bool logFirstTime;
};

#endif
