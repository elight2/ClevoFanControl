#ifndef CFC_MONITOR_H
#define CFC_MONITOR_H

#include <QtWidgets/qwidget.h>
#include <QtCore/qstring.h>

#include "build/ui_monitor.h"

class CFCmonitor : public QWidget {
Q_OBJECT

public:
    CFCmonitor(QWidget *parent);
    void updateValue(int index, int speed, int rpm, int temperature, double power);

    Ui::CFCmonitorWindow ui;
};

#endif
