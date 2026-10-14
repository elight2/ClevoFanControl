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
    emit CfcLogMgr::LOG_MGR->writeLog("loading config json");

    //create if not exist
    if(!configFile.exists()) {
        emit CfcLogMgr::LOG_MGR->writeLog("file not exist, creating config json");
        createConfigJson();
    }
    configJson=readJsonFile(configFile);

    //profiles
    this->profileCount=configJson["profiles"].size();
    emit CfcLogMgr::LOG_MGR->writeLog("loading profiles, total: "+QString::number(this->profileCount));
    for(auto &i:configJson["profiles"]) {
        //name
        fanProfile curProfile;
        curProfile.name=QString::fromStdString(i["name"]);
        emit CfcLogMgr::LOG_MGR->writeLog("loading profile: "+curProfile.name);

        //for each fan
        for(int j=0;j<2;j++) {
            //normal cfgs
            emit CfcLogMgr::LOG_MGR->writeLog("loading normal args");
            curProfile.args[j].pwrCount=i["fans"][j]["minSpeedPwrCount"];
            curProfile.args[j].operateInterval=i["fans"][j]["operateInterval"];
            curProfile.args[j].speedStep=i["fans"][j]["speedStep"];
            curProfile.args[j].speedUpTemp=i["fans"][j]["speedUpTemp"];
            curProfile.args[j].slowDownTemp=i["fans"][j]["slowDownTemp"];

            //min speed
            nlohmann::json minSpeedData=i["fans"][j]["minSpeed"];
            emit CfcLogMgr::LOG_MGR->writeLog("loading min speed args");
            if (minSpeedData.type()==nlohmann::json::value_t::array) { // auto mode
                curProfile.args[j].minSpeed=minSpeedData.size(); //get size
                emit CfcLogMgr::LOG_MGR->writeLog("using min speed table, with size of "+QString::number(curProfile.args[j].minSpeed));
                bool addPoint=false;
                if (minSpeedData[0][0]!=0) { // no 0,0
                    emit CfcLogMgr::LOG_MGR->writeLog("no point have power of 0W. adding 0,10");
                    curProfile.args[j].minSpeed+=1; //len++
                    addPoint=true;
                }
                curProfile.args[j].minSpeedList=new CfcUtils::curvePoint[curProfile.args[j].minSpeed]; //init table
                if (addPoint)
                    curProfile.args[j].minSpeedList[0]={0,10}; // add 0,10

                for (int k=0;k<minSpeedData.size();k++)
                    curProfile.args[j].minSpeedList[addPoint ? k+1 : k]={minSpeedData[k][0],minSpeedData[k][1]};
            }
            else {//normal mode
                emit CfcLogMgr::LOG_MGR->writeLog("using fixed min speed");
                curProfile.args[j].minSpeed=minSpeedData;
                curProfile.args[j].minSpeedList=nullptr;
            }
        }
        fanProfiles.append(curProfile);
    }

    //commands
    this->commandCount=configJson["commands"].size();
    emit CfcLogMgr::LOG_MGR->writeLog("loading commands, total: "+QString::number(this->commandCount));
    commands.clear();
    for (const auto& [i,j] : configJson["commands"].items())
        commands.append({QString::fromStdString(i),QString::fromStdString(j)});

    emit CfcLogMgr::LOG_MGR->writeLog("loading other options");
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

    emit CfcLogMgr::LOG_MGR->writeLog("read config finish");
}

void ConfigManager::saveToJson() {
    emit CfcLogMgr::LOG_MGR->writeLog("saving config");

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

    emit CfcLogMgr::LOG_MGR->writeLog("save config finish");
}

ConfigManager::ConfigManager() {
    configFile.setFileName((QDir::currentPath() + QDir::separator() + CfcDef::CFG_DIR));
}

ConfigManager::~ConfigManager() {
    for (auto i : fanProfiles) {
        delete [] i.args[0].minSpeedList;
        delete [] i.args[1].minSpeedList;
    }
}

void ConfigManager::createConfigJson() {
    QFile defaultConfigFile(QDir::currentPath() + QDir::separator() + CfcDef::DEFAULT_CFG_DIR);
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
