#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <QtCore/qlist.h>
#include <QtCore/qfile.h>
#include <QtCore/qdebug.h>
#include <QtCore/qdir.h>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

struct fanProfile {
    QString name;
    int inUse[2];//c,g
    int MTconfig[2][4];
    int TStempList[2][10];
    int TSspeedList[2][10];
};

struct commandEntry {
    QString name;
    QString content;
};

class ConfigManager {
public:
    json configJson;
    int profileCount;
    int commandCount;
    fanProfile* fanProfiles=nullptr;
    QList<commandEntry> commands;
    int profileInUse;
    bool useStaticSpeed;
    int staticSpeed[2];
    bool useSpeedLimit;
    int speedLimit[2];
    int timeIntervals[2];
    bool useClevoAuto;
    bool maxSpeed;
    bool monitorGpu;

    ConfigManager();
    ~ConfigManager();
    void readFromJson();
    void saveToJson();
    void createConfigJson();

    private:
    QFile configFile;
};

#endif
