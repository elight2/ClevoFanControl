#include "ConfigManager.h"
#include "nlohmann/json_fwd.hpp"

#include <QtCore/qdebug.h>
#include <QtCore/qdir.h>
#include <qlogging.h>

void ConfigManager::readFromJson() {
    qDebug()<<"readFromJson";

    //create if not exist
    if(!configFile.exists())
        createConfigJson();

    configJson=readJsonFile(configFile);

    //profiles
    this->profileCount=configJson["profiles"].size();
    for(int i=0;i<this->profileCount;i++) {
        fanProfile curProfile;
        curProfile.name=QString::fromStdString(configJson["profiles"][i][0]);
        for(int j=0;j<2;j++) {
            curProfile.inUse[j]=configJson["profiles"][i][j+1][0];
            for(int k=0;k<4;k++)
                curProfile.MTconfig[j][k]=configJson["profiles"][i][j+1][1][k];
            for(int k=0;k<10;k++) {
                curProfile.TStempList[j][k]=configJson["profiles"][i][j+1][2][0][k];
                curProfile.TSspeedList[j][k]=configJson["profiles"][i][j+1][2][1][k];
            }
        }
        fanProfiles.append(curProfile);
    }

    //commands
    this->commandCount=configJson["commands"].size();
    for(auto &i :configJson["commands"].items()) {
        commandEntry curCommand;
        curCommand.name=i.key().c_str();
        curCommand.content=((std::string)i.value()).c_str();
        commands.append(curCommand);
    }

    //others
    this->profileInUse=configJson["profileInUse"];
    //
    this->useStaticSpeed=configJson["staticSpeed"][0];
    this->staticSpeed[0]=configJson["staticSpeed"][1];
    this->staticSpeed[1]=configJson["staticSpeed"][2];
    //
    this->useSpeedLimit=configJson["speedLimit"][0];
    this->speedLimit[0]=configJson["speedLimit"][1];
    this->speedLimit[1]=configJson["speedLimit"][2];
    //
    this->timeIntervals[0]=configJson["timeIntervals"][0];
    this->timeIntervals[1]=configJson["timeIntervals"][1];
    //
    this->useClevoAuto=configJson["useClevoAuto"];
    //
    this->maxSpeed=configJson["maxSpeed"];
    //
    this->monitorGpu=configJson["gpuDetect"]["monitorGpu"];
    this->gpuAutoDetectEnabled=configJson["gpuDetect"]["autoDetectEnabled"];
    this->gpuSysDir=((std::string)configJson["gpuDetect"]["gpuSysDir"]).c_str();
    this->gpuDevDir=((std::string)configJson["gpuDetect"]["gpuDevDir"]).c_str();
    for(auto i : configJson["gpuDetect"]["procExclude"])
        this->gpuLsofExcludeProc.push_back(((std::string)i).c_str());

    qDebug()<<"readFromJson finish";
}

void ConfigManager::saveToJson() {
    qDebug()<<"saveConfigJson";

    //build profiles
    nlohmann::json profileArray=nlohmann::json::array();
    for(int i=0;i<profileCount;i++) {
        nlohmann::json curProfile={
            fanProfiles[i].name.toStdString(),
            {
                fanProfiles[i].inUse[0],
                fanProfiles[i].MTconfig[0],
                {
                    fanProfiles[i].TStempList[0],
                    fanProfiles[i].TSspeedList[0]
                }
            },
            {
                fanProfiles[i].inUse[1],
                fanProfiles[i].MTconfig[1],
                {
                    fanProfiles[i].TStempList[1],
                    fanProfiles[i].TSspeedList[1]
                }
            }
        };
        profileArray+=curProfile;
    }

    //create json
    nlohmann::json oldConfigJson=configJson;
    configJson.clear();
    configJson={
        {"profiles",profileArray},
        {"commands",oldConfigJson["commands"]},
        {"profileInUse",this->profileInUse},
        {"staticSpeed",{this->useStaticSpeed,this->staticSpeed[0],this->staticSpeed[1]}},
        {"speedLimit",{this->useSpeedLimit,speedLimit[0],speedLimit[1]}},
        {"timeIntervals",timeIntervals},
        {"useClevoAuto",this->useClevoAuto},
        {"maxSpeed",this->maxSpeed},
        {"gpuDetect",{
            {"monitorGpu",this->monitorGpu},
            {"autoDetectEnabled",oldConfigJson["gpuDetect"]["autoDetectEnabled"]}, 
            {"gpuSysDir",oldConfigJson["gpuDetect"]["gpuSysDir"]},
            {"gpuDevDir",oldConfigJson["gpuDetect"]["gpuDevDir"]},
            {"procExclude",oldConfigJson["gpuDetect"]["procExclude"]}
        }}
    };
    
    writeJsonFile(configJson,configFile);

    qDebug()<<"saveConfigJson finish";
}

ConfigManager::ConfigManager() {
    configFile.setFileName((QDir::currentPath() + QDir::separator() + configFileName));
}

void ConfigManager::createConfigJson() {
    QFile defaultConfigFile;
    defaultConfigFile.setFileName((QDir::currentPath() + QDir::separator() + defaultConfigFileName));
    nlohmann::json defaultConfigJson=readJsonFile(defaultConfigFile);

    writeJsonFile(defaultConfigJson, configFile);
}

void ConfigManager::writeJsonFile(nlohmann::json &content, QFile &file) {
    file.open(QIODevice::WriteOnly);
    file.write(content.dump(4).c_str());
    file.close();
}

nlohmann::json ConfigManager::readJsonFile(QFile &file) {
    file.open(QIODevice::ReadOnly);
    std::string jsonStr=file.readAll().toStdString();
    file.close();
    nlohmann::json result;
    result.clear();
    result=nlohmann::json::parse(jsonStr);
    return result;
}
