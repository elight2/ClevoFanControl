#ifndef CFC_CONFIG_H
#define CFC_CONFIG_H

#include <QtWidgets/qwidget.h>

#include "ConfigManager.h"
#include "build/ui_config.h"

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

#endif
