#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <QtCore/qlist.h>
#include <QtCore/qfile.h>
#include <qcontainerfwd.h>
#include "nlohmann/json.hpp"

#include "../utils.h"

struct fanArg {
    int operateInterval;
    int speedStep;
    int minSpeed; // for normal mode, speed value, for auto mode, list size
    QList<cfcUtils::curvePoint> minSpeedList;
    int pwrCount;
    int speedUpTemp;
    int slowDownTemp;
};

struct fanProfile {
    QString name;
    fanArg args[2];
};

struct commandEntry {
    QString name;
    QString content;
};

class ConfigManager {
public:
    int profileCount;
    int commandCount;
    QList<fanProfile> fanProfiles;
    QList<commandEntry> commands;
    int profileInUse;
    bool useStaticSpeed;
    int staticSpeed[2];
    bool useSpeedLimit;
    int speedLimit[2];
    int monitorIntervals[2];
    bool useClevoAuto;
    bool maxSpeed;
    bool monitorGpu;
    bool gpuAutoDetectEnabled;
    QString gpuSysDir;

    ConfigManager();
    void readFromJson();
    void saveToJson();
    void createConfigJson();

private:
    QFile configFile;
    nlohmann::json configJson;

    void writeJsonFile(nlohmann::json &content, QFile &file);
    nlohmann::json readJsonFile(QFile &file);
};

#endif
