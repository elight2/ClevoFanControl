#include "ConfigManager.h"
#include "nlohmann/json_fwd.hpp"
#include "../defines.h"
#include "../utils.h"

#include <QtCore/qdebug.h>
#include <QtCore/qdir.h>
#include <qcontainerfwd.h>
#include <qlogging.h>
#include <qobject.h>

void ConfigManager::readFromJson() {
    cfcUtils::writeLog("loading config json");

    //create if not exist
    if(!configFile.exists()) {
        cfcUtils::writeLog("file not exist, creating config json");
        createConfigJson();
    }
    configJson=readJsonFile(configFile);

    //profiles
    this->profileCount=configJson["profiles"].size();
    cfcUtils::writeLog("loading profiles, total: "+QString::number(this->profileCount));
    for(auto &i:configJson["profiles"]) {
        //name
        fanProfile curProfile;
        curProfile.name=QString::fromStdString(i["name"]);
        cfcUtils::writeLog("loading profile: "+curProfile.name);

        //for each fan
        for(int j=0;j<2;j++) {
            //normal cfgs
            cfcUtils::writeLog("loading normal args");
            curProfile.args[j].pwrCount=i["fans"][j]["minSpeedPwrCount"];
            curProfile.args[j].operateInterval=i["fans"][j]["operateInterval"];
            curProfile.args[j].speedStep=i["fans"][j]["speedStep"];
            curProfile.args[j].speedUpTemp=i["fans"][j]["speedUpTemp"];
            curProfile.args[j].slowDownTemp=i["fans"][j]["slowDownTemp"];

            //min speed
            curProfile.args[j].minSpeedList.clear();
            nlohmann::json minSpeedData=i["fans"][j]["minSpeed"];
            cfcUtils::writeLog("loading min speed args");
            if (minSpeedData.type()==nlohmann::json::value_t::array) { // auto mode
                curProfile.args[j].minSpeed=minSpeedData.size();
                cfcUtils::writeLog("using min speed table, with size of "+QString::number(curProfile.args[j].minSpeed));
                for (auto k : minSpeedData)
                    curProfile.args[j].minSpeedList.append((cfcUtils::curvePoint){k[0],k[1]});

                cfcUtils::writeLog("no point have power of 0W. adding 0,10");
                if (curProfile.args[j].minSpeedList[0].x!=0) // add 0,10
                    curProfile.args[j].minSpeedList.prepend({0,10});
            }
            else {//normal mode
                cfcUtils::writeLog("using fixed min speed");
                curProfile.args[j].minSpeed=minSpeedData;
            }
        }
        fanProfiles.append(curProfile);
    }

    //commands
    this->commandCount=configJson["commands"].size();
    cfcUtils::writeLog("loading commands, total: "+QString::number(this->commandCount));
    commands.clear();
    for (const auto& [i,j] : configJson["commands"].items())
        commands.append({QString::fromStdString(i),QString::fromStdString(j)});

    cfcUtils::writeLog("loading other options");
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

    cfcUtils::writeLog("read config finish");
}

void ConfigManager::saveToJson() {
    cfcUtils::writeLog("saving config");

    //create json
    nlohmann::json oldConfigJson=configJson;
    configJson.clear();
    configJson={
        {"profiles",oldConfigJson["profiles"]},
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

    cfcUtils::writeLog("save config finish");
}

ConfigManager::ConfigManager() {
    configFile.setFileName((QDir::currentPath() + QDir::separator() + cfcDef::CFG_DIR));
}

void ConfigManager::createConfigJson() {
    QFile defaultConfigFile(QDir::currentPath() + QDir::separator() + cfcDef::DEFAULT_CFG_DIR);
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
