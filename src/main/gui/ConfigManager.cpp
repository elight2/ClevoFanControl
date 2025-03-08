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
    for(auto i:configJson["profiles"]) {
        fanProfile curProfile;
        curProfile.name=QString::fromStdString(i["name"]);
        for(int j=0;j<2;j++) {
            curProfile.args[j].operateInterval=i["fans"][j]["operateInterval"];
            curProfile.args[j].speedStep=i["fans"][j]["speedStep"];
            curProfile.args[j].minSpeed=i["fans"][j]["minSpeed"];
            curProfile.args[j].speedUpTemp=i["fans"][j]["speedUpTemp"];
            curProfile.args[j].slowDownTemp=i["fans"][j]["slowDownTemp"];
        }
        fanProfiles.append(curProfile);
    }

    //commands
    this->commandCount=configJson["commands"].size();
    for(auto &i : configJson["commands"].items()) {
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
    this->monitorIntervals[0]=configJson["monitorIntervals"][0];
    this->monitorIntervals[1]=configJson["monitorIntervals"][1];
    //
    this->useClevoAuto=configJson["useClevoAuto"];
    //
    this->maxSpeed=configJson["maxSpeed"];
    //
    this->monitorGpu=configJson["gpuDetect"]["monitorGpu"];
    this->gpuAutoDetectEnabled=configJson["gpuDetect"]["autoDetectEnabled"];
    this->gpuSysDir=((std::string)configJson["gpuDetect"]["gpuSysDir"]).c_str();

    qDebug()<<"readFromJson finish";
}

void ConfigManager::saveToJson() {
    qDebug()<<"saveConfigJson";

    //build profiles
    nlohmann::json profileArray=this->configJson["profiles"];

    //create json
    nlohmann::json oldConfigJson=configJson;
    configJson.clear();
    configJson={
        {"profiles",profileArray},
        {"commands",oldConfigJson["commands"]},
        {"profileInUse",this->profileInUse},
        {"staticSpeed",{this->useStaticSpeed,this->staticSpeed[0],this->staticSpeed[1]}},
        {"speedLimit",{this->useSpeedLimit,speedLimit[0],speedLimit[1]}},
        {"monitorIntervals",monitorIntervals},
        {"useClevoAuto",this->useClevoAuto},
        {"maxSpeed",this->maxSpeed},
        {"gpuDetect",{
            {"monitorGpu",this->monitorGpu},
            {"autoDetectEnabled",oldConfigJson["gpuDetect"]["autoDetectEnabled"]}, 
            {"gpuSysDir",oldConfigJson["gpuDetect"]["gpuSysDir"]}
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
