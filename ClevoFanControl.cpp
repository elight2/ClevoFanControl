#include "ClevoFanControl.h"

#ifdef _WIN32
#include "OlsApiInit.h"
#endif

using namespace std;

QJsonObject configData;
QFile configFile;
atomic<bool> stillRun = 0;
atomic<bool> controllerRunning[2]={0,0};
mutex EClock;

#define IA32_PACKAGE_THERM_STATUS_MSR 0x1B1

//std EC
#define EC_SC_REG 0x66
#define EC_DATA_REG 0x62
#define EC_READ_CMD 0x80
#define EC_WRITE_CMD 0x81
#define EC_SC_IBF_INDEX 1
#define EC_SC_OBF_INDEX 0
//clevo
#define EC_CPU_FAN_RPM_HI_ADDR 0xD0
#define EC_CPU_FAN_RPM_LO_ADDR 0xD1
#define EC_GPU_FAN_RPM_HI_ADDR 0xD2
#define EC_GPU_FAN_RPM_LO_ADDR 0xD3
#define EC_SET_FAN_SPEED_CMD 0x99
#define EC_SET_FAN_AUTO_ADDR 0xFF

#define C_PROFILE_INDEX 0
#define G_PROFILE_INDEX 1

void ioOutByte(unsigned short port, unsigned char value)
{
	#ifdef _WIN32
    WriteIoPortByte(port, value);
    #elif __linux__
    outb(value, port);
    #endif
	return;
}

unsigned char ioInByte(unsigned short port)
{
	unsigned char ret=0;
	#ifdef _WIN32
	ret = ReadIoPortByte(port);
	#elif __linux__
	ret = inb(port);
	#endif
	return ret;
}

void waitEc(unsigned short port, unsigned char index, unsigned char value)
{
	unsigned char data=0;
	do
	{
		data=ioInByte(port);
		#ifdef __linux__
		usleep(1000);
		#endif
	}while(((data>>index)&((unsigned char)1))!=value);
	return;
}

// unsigned char readEc(unsigned char addr)
// {
//     //qDebug()<<"read ec";
//     EClock.lock();
// 	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
// 	ioOutByte(EC_SC_REG,EC_READ_CMD);
// 	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
// 	ioOutByte(EC_DATA_REG,addr);
// 	waitEc(EC_SC_REG,EC_SC_OBF_INDEX,1);
// 	unsigned char value=0;
// 	value=ioInByte(EC_DATA_REG);
//     //qDebug()<<"read ec finish";
//     EClock.unlock();
// 	return value;
// }

unsigned char readEc_1(unsigned char addr)
{
    //qDebug()<<"read ec";
    EClock.lock();
	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
	ioOutByte(EC_SC_REG,EC_READ_CMD);
	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
	ioOutByte(EC_DATA_REG,addr);
    Sleep(1);
	//waitEc(EC_SC_REG,EC_SC_OBF_INDEX,1);//???
	unsigned char value=0;
	value=ioInByte(EC_DATA_REG);
    //qDebug()<<"read ec finish";
    EClock.unlock();
	return value;
}

// void writeEc(unsigned char addr, unsigned char value)
// {
// 	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
// 	ioOutByte(EC_SC_REG,EC_WRITE_CMD);
// 	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
// 	ioOutByte(EC_DATA_REG,addr);
// 	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
// 	ioOutByte(EC_DATA_REG,value);
// 	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
// 	return;
// }

void doEc(unsigned char cmd, unsigned char addr, unsigned char value)
{
    EClock.lock();
	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);//not in doc, but for safety reason
	ioOutByte(EC_SC_REG,cmd);
	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
	ioOutByte(EC_DATA_REG,addr);
	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
	ioOutByte(EC_DATA_REG,value);
	waitEc(EC_SC_REG,EC_SC_IBF_INDEX,0);
    EClock.unlock();
	return;
}

int getCpuFanRpm() {
	int data = ((readEc_1(EC_CPU_FAN_RPM_HI_ADDR) << 8) + (readEc_1(EC_CPU_FAN_RPM_LO_ADDR)));
	return data==0 ? 0 : (2156220 / data);
}

int getGpuFanRpm() {
	int data = ((readEc_1(EC_GPU_FAN_RPM_HI_ADDR) << 8) + (readEc_1(EC_GPU_FAN_RPM_LO_ADDR)));
	return data==0 ? 0 : (2156220 / data);
}

int getCpuTemperature()
{
    //qDebug()<<"get cpu temp";
    int temperature=0;
    DWORD eax=0;
	DWORD edx=0;
    Rdmsr(IA32_PACKAGE_THERM_STATUS_MSR,&eax,&edx);
    temperature=100-((eax & 0x007F0000) >> 16);
    //qDebug()<<"get cpu temp finish: "<<temperature;
    return temperature;
}

int getGpuTemperature()
{
    //qDebug()<<"get gpu temp";
    QProcess nvsmi;
    nvsmi.start("nvidia-smi",{"-q","-d=TEMPERATURE"});
    nvsmi.waitForFinished();
    QString output=nvsmi.readAllStandardOutput();
    int temperature=0;
    int index=output.indexOf("GPU Current Temp");
    int index2=output.indexOf(":",index);
    int index3=output.indexOf(" ",index2+2);
    temperature=output.mid(index2+2,index3-index2-2).toInt();
    //qDebug()<<"get gpu temp finish: "<<temperature;
    return temperature;
}

void saveConfigJson()
{
    qDebug()<<"saveConfigJson";
    configFile.open(QIODevice::WriteOnly);
    configFile.write(QJsonDocument(configData).toJson());
    configFile.close();
    return;
}

ClevoFanControl::ClevoFanControl(QWidget *parent) :QWidget(parent)
{
    qDebug()<<"cfc construct";
    configFile.setFileName((CFCpath.path() + QDir::separator() + "config.json"));
    loadConfigJson();

    #ifdef _WIN32
    bool WinRing0result=InitOpenLibSys_m();
    qDebug()<<"InitOpenLibSys result: "<<WinRing0result;
    //error handle
    #elif __linux__
    ioperm(0x62, 1, 1);
    ioperm(0x66, 1, 1);
    #endif

    //tray
    TrayIcon = new QSystemTrayIcon(QIcon("ClevoFanControl.ico"), this);
    TrayIcon->setToolTip("Clevo Fan Control");
    menus[0] = new QMenu(this);
    actions[0] = new QAction("Monitor...", this);
    actions[1] = new QAction("Config...", this);
    actions[2] = new QAction("Max Speed", this);
    actions[2]->setCheckable(1);
    actions[2]->setChecked(configData["maxSpeed"].toBool());
    actions[3] = new QAction("Speed Limit", this);
    actions[3]->setCheckable(1);
    actions[3]->setChecked(configData["speedLimit"].toArray()[0].toBool());
    actions[4] = new QAction("Static Speed", this);
    actions[4]->setCheckable(1);
    actions[4]->setChecked(configData["staticSpeed"].toArray()[0].toBool());
    actions[5] = new QAction("Clevo Auto", this);
    actions[5]->setCheckable(1);
    actions[5]->setChecked(configData["useClevoAuto"].toBool());
    actions[6] = new QAction("Exit", this);
    for (int i = 0; i < 7; i++)
        menus[0]->addAction(actions[i]);
    for(int i=2;i<6;i++)
        QObject::connect(actions[i],&QAction::triggered,this,&ClevoFanControl::updateDataFromContext);

    monitor=new CFCmonitor(NULL);
    configWindow = new CFCconfig(NULL);

    QObject::connect(actions[6], &QAction::triggered, qApp, QApplication::quit);
    QObject::connect(actions[0], &QAction::triggered, this, [this]() { monitor->show(); });
    QObject::connect(actions[1], &QAction::triggered, this, [this]() { configWindow->show(); });
    QObject::connect(configWindow, &CFCconfig::updateConfigData, this, &ClevoFanControl::updateConfigDataSlot);

    //final
    TrayIcon->setContextMenu(menus[0]);
    TrayIcon->show();

    stillRun=1;
    fan1=new FanController(1,this);
    fan2=new FanController(2,this);
    fan1->setConfig(configData);
    fan2->setConfig(configData);
    QObject::connect(fan1, &FanController::updateValue, this, &ClevoFanControl::updateMonitorValueSlot);
    QObject::connect(fan2, &FanController::updateValue, this, &ClevoFanControl::updateMonitorValueSlot);
    fan1->start();
    fan2->start();
   
    qDebug()<<"cfc construct finish";
    return;
}

ClevoFanControl::~ClevoFanControl()
{
    qDebug()<<"cfc deconstructing";

    stillRun=0;
    while(controllerRunning[0] || controllerRunning[1])
        QThread::msleep(10);
    fan1->quit();
    fan2->quit();
    delete fan1;
    delete fan2;

    saveConfigJson();

    #ifdef _WIN32
    bool WinRing0result=DeinitOpenLibSys_m();
    qDebug()<<"DeinitOpenLibSys result: "<<WinRing0result;
    #endif

    return;
}

BOOL ClevoFanControl::InitOpenLibSys_m()
{
    return InitOpenLibSys(&WinRing0m);
}

BOOL ClevoFanControl::DeinitOpenLibSys_m()
{
    return DeinitOpenLibSys(&WinRing0m);
}

void ClevoFanControl::loadConfigJson()
{
    //create
    if(!configFile.exists())
    {
        configData=
        {
            {"configs",QJsonArray({
                QJsonArray({
                1,
                QJsonArray({80,75,20,5}),
                QJsonArray({QJsonArray({58, 60, 63, 65, 68, 70, 73, 75, 78, 80}),QJsonArray({20, 25, 30, 40, 50, 60, 70, 80, 90, 100})})
                }),
                QJsonArray({
                1,
                QJsonArray({80,75,20,5}),
                QJsonArray({QJsonArray({58, 60, 63, 65, 68, 70, 73, 75, 78, 80}),QJsonArray({20, 25, 30, 40, 50, 60, 70, 80, 90, 100})})
                })
            })
            },
            {"timeIntervals",QJsonArray({2000,2000})},
            {"speedLimit",QJsonArray({false,80,80})},
            {"staticSpeed",QJsonArray({false,80,80})},
            {"useClevoAuto",false},
            {"maxSpeed",false}
        };
        configFile.open(QIODevice::ReadWrite);
        configFile.write(QJsonDocument(configData).toJson());
        configFile.close();
    }

    configFile.open(QIODevice::ReadWrite);
    configData=QJsonDocument::fromJson(configFile.readAll()).object();
    configFile.close();

    return;
}

void ClevoFanControl::updateMonitorValueSlot(int index, int speed, int rpm, int temperature)
{
    //qDebug()<<"updateMonitorValueSlot";
    if(index==1)
    {
        monitor->ui.label_2->setText(to_string(temperature).c_str());
        if(speed==-2)
            monitor->ui.label_6->setText("Auto");
        else
            monitor->ui.label_6->setText(to_string(speed).c_str());
        monitor->ui.label_11->setText(to_string(rpm).c_str());
    }
    else if(index==2)
    {
        monitor->ui.label_4->setText(to_string(temperature).c_str());
        if(speed==-2)
            monitor->ui.label_8->setText("Auto");
        else
            monitor->ui.label_8->setText(to_string(speed).c_str());
        monitor->ui.label_12->setText(to_string(rpm).c_str());
    }
    //qDebug()<<"updateMonitorValueSlot finish";
    return;
}

void ClevoFanControl::updateConfigDataSlot()
{
    qDebug()<<"updateConfigData";
    fan1->setConfig(configData);
    fan2->setConfig(configData);
}

void ClevoFanControl::updateDataFromContext()
{
    qDebug()<<"updateDataFromContext";
    QJsonArray sl=configData["speedLimit"].toArray();
    QJsonArray ss=configData["staticSpeed"].toArray();
    sl[0]=actions[3]->isChecked();
    ss[0]=actions[4]->isChecked();
    configData["speedLimit"]=sl;
    configData["staticSpeed"]=ss;
    configData["maxSpeed"]=actions[2]->isChecked();
    configData["useClevoAuto"]=actions[5]->isChecked();
    updateConfigDataSlot();
    configWindow->setOptionsFromJson();
}

FanController::FanController(int fanIndex, QObject *parent) : QThread(parent)
{
    qDebug()<<"FanController construct fan: "<<fanIndex;
    index=fanIndex;
    lastControlTime=QDateTime::currentMSecsSinceEpoch();
    //copy the json to normal variables
}

FanController::~FanController()
{
    qDebug()<<"FanController deconstruct fan: "<<index;
}

void FanController::run()
{
    qDebug()<<"FanController run fan: "<<index;
    controllerRunning[index-1]=1;
    while(stillRun)
    {
        currentTime=QDateTime::currentMSecsSinceEpoch();
        if(currentTime>lastControlTime+config["timeIntervals"].toArray()[index-1].toInt())
        {
            //qDebug()<<"controling speed fan: "<<index;
            int targetSpeed=-1;
            if(config["useClevoAuto"].toBool())
            {
                if(!isAuto)
                {
                    isAuto=1;
                    targetSpeed=-2;
                    qDebug()<<"adjust fan: "<<index<<" auto";
                }
                else
                {
                    targetSpeed=-3;
                    qDebug()<<"adjust fan: "<<index<<" already auto";
                }
            }
            else if(config["maxSpeed"].toBool())
            {
                if(isAuto)
                    isAuto=0;
                targetSpeed=100;
                //qDebug()<<"adjust fan: "<<index<<" max";
            }
            else if(config["staticSpeed"].toArray()[0].toBool())
            {
                targetSpeed=config["staticSpeed"].toArray()[index].toInt();
                //qDebug()<<"adjust fan: "<<index<<" static"<<targetSpeed;
            }
            else
            {
                temperature = index==1 ? getCpuTemperature() : getGpuTemperature();
                int mode=config["configs"].toArray()[index-1].toArray()[0].toInt();
                targetSpeed=curSpeed;
                if(mode==1)
                {
                    if(temperature>config["configs"].toArray()[index-1].toArray()[mode].toArray()[0].toInt())
                        targetSpeed+=config["configs"].toArray()[index-1].toArray()[mode].toArray()[3].toInt();
                    else if(temperature<config["configs"].toArray()[index-1].toArray()[mode].toArray()[1].toInt())
                        targetSpeed-=config["configs"].toArray()[index-1].toArray()[mode].toArray()[3].toInt();
                    targetSpeed=max({targetSpeed,minSafeSpeed,config["configs"].toArray()[index-1].toArray()[mode].toArray()[2].toInt()});
                    targetSpeed=min(targetSpeed,100);
                }
                else if(mode==2)
                {
                    int tempList[10];
                    int speedList[10];
                    for(int i=0;i<10;i++)
                        tempList[i]=config["configs"].toArray()[index-1].toArray()[mode].toArray()[0].toArray()[i].toInt();
                    for(int i=0;i<10;i++)
                        speedList[i]=config["configs"].toArray()[index-1].toArray()[mode].toArray()[1].toArray()[i].toInt();
                    targetSpeed=speedList[0];
                    if(temperature<tempList[0])
                        targetSpeed=speedList[0];
                    else
                    {
                        for(int i=0;i<10;i++)
                        {
                            if(i==9)
                                targetSpeed=speedList[9];
                            else if(temperature>=tempList[i] && temperature<tempList[i+1])
                            {
                                targetSpeed=speedList[i];
                                break;
                            }
                        }
                    }
                    targetSpeed=max(targetSpeed,minSafeSpeed);
                    targetSpeed=min(targetSpeed,100);
                }
                //qDebug()<<"adjust fan: "<<index<<" normal"<<targetSpeed;
            }
            if(targetSpeed==-2)
                doEc(EC_SET_FAN_SPEED_CMD, EC_SET_FAN_AUTO_ADDR, index);
            else if(targetSpeed!=-3)
            {
                if(curSpeed!=targetSpeed)
                {
                    qDebug()<<"apply speed: "<<index<<" "<<targetSpeed;
                    doEc(EC_SET_FAN_SPEED_CMD,index,targetSpeed*255/100);
                    qDebug()<<"apply speed finish: "<<index<<" "<<targetSpeed;
                    curSpeed=targetSpeed;
                }
            }
            if(index==1)
                rpm=getCpuFanRpm();
            else if(index==2)
                rpm=getGpuFanRpm();
            emit updateValue(index,isAuto ? -2 : curSpeed,rpm,temperature);
            lastControlTime=currentTime;
        }
        QThread::msleep(100);
    }
    doEc(EC_SET_FAN_SPEED_CMD, EC_SET_FAN_AUTO_ADDR, index);
    controllerRunning[index-1]=0;
    qDebug()<<"FanController run finish "<<index;
}

CFCmonitor::CFCmonitor(QWidget *parent) : QWidget(parent)
{
    this->setAttribute(Qt::WA_QuitOnClose, 0);
    this->setFixedSize(375,500);
    ui.setupUi(this);
}

CFCconfig::CFCconfig(QWidget *parent) : QWidget(parent)
{
    this->setAttribute(Qt::WA_QuitOnClose, 0);
    ui.setupUi(this);

    setOptionsFromJson();

    QObject::connect(ui.pushButton, &QPushButton::clicked, this, &CFCconfig::OK);
    QObject::connect(ui.pushButton_2, &QPushButton::clicked, this, &CFCconfig::apply);
}

void CFCconfig::setOptionsFromJson() {
    //cpu
    ui.radioButton->setChecked(configData["configs"].toArray()[C_PROFILE_INDEX].toArray()[0].toInt() == 2);
    ui.radioButton_2->setChecked(configData["configs"].toArray()[C_PROFILE_INDEX].toArray()[0].toInt() == 1);
    for (int i = 0; i < 10; i++)
        ui.tableWidget->removeRow(0);
    for (int i = 0; i < 10; i++) {
        ui.tableWidget->insertRow(i);
        ui.tableWidget->setItem(i, 0, new QTableWidgetItem(
                QString::number((int) configData["configs"].toArray()[C_PROFILE_INDEX].toArray()[2].toArray()[0].toArray()[i].toInt())));
        ui.tableWidget->setItem(i, 1, new QTableWidgetItem(
                QString::number((int) configData["configs"].toArray()[C_PROFILE_INDEX].toArray()[2].toArray()[1].toArray()[i].toInt())));
    }
    ui.lineEdit->setText(QString::number((int) configData["configs"].toArray()[C_PROFILE_INDEX].toArray()[1].toArray()[0].toInt()));
    ui.lineEdit_2->setText(QString::number((int) configData["configs"].toArray()[C_PROFILE_INDEX].toArray()[1].toArray()[1].toInt()));
    ui.lineEdit_4->setText(QString::number((int) configData["configs"].toArray()[C_PROFILE_INDEX].toArray()[1].toArray()[3].toInt()));
    ui.lineEdit_5->setText(QString::number((int) configData["configs"].toArray()[C_PROFILE_INDEX].toArray()[1].toArray()[2].toInt()));

    //gpu 2
    ui.radioButton_11->setChecked(configData["configs"].toArray()[G_PROFILE_INDEX].toArray()[0].toInt() == 2);
    ui.radioButton_12->setChecked(configData["configs"].toArray()[G_PROFILE_INDEX].toArray()[0].toInt() == 1);
    for (int i = 0; i < 10; i++)
        ui.tableWidget_6->removeRow(0);
    for (int i = 0; i < 10; i++) {
        ui.tableWidget_6->insertRow(i);
        ui.tableWidget_6->setItem(i, 0, new QTableWidgetItem(
                QString::number((int) configData["configs"].toArray()[G_PROFILE_INDEX].toArray()[2].toArray()[0].toArray()[i].toInt())));
        ui.tableWidget_6->setItem(i, 1, new QTableWidgetItem(
                QString::number((int) configData["configs"].toArray()[G_PROFILE_INDEX].toArray()[2].toArray()[1].toArray()[i].toInt())));
    }
    ui.lineEdit_26->setText(QString::number((int) configData["configs"].toArray()[G_PROFILE_INDEX].toArray()[1].toArray()[0].toInt()));
    ui.lineEdit_27->setText(QString::number((int) configData["configs"].toArray()[G_PROFILE_INDEX].toArray()[1].toArray()[1].toInt()));
    ui.lineEdit_29->setText(QString::number((int) configData["configs"].toArray()[G_PROFILE_INDEX].toArray()[1].toArray()[3].toInt()));
    ui.lineEdit_0->setText(QString::number((int) configData["configs"].toArray()[G_PROFILE_INDEX].toArray()[1].toArray()[2].toInt()));

    ui.lineEdit_35->setText(QString::number((int) configData["staticSpeed"].toArray()[1].toInt()));
    ui.lineEdit_37->setText(QString::number((int) configData["staticSpeed"].toArray()[2].toInt()));
    ui.checkBox->setChecked(configData["staticSpeed"].toArray()[0].toBool());
    ui.lineEdit_36->setText(QString::number((int) configData["speedLimit"].toArray()[1].toInt()));
    ui.lineEdit_38->setText(QString::number((int) configData["speedLimit"].toArray()[2].toInt()));
    ui.checkBox_2->setChecked(configData["speedLimit"].toArray()[0].toBool());
    ui.lineEdit_6->setText(QString::number((int) configData["timeIntervals"].toArray()[0].toInt()));
    ui.lineEdit_7->setText(QString::number((int) configData["timeIntervals"].toArray()[1].toInt()));
    ui.checkBox_3->setChecked(configData["useClevoAuto"].toBool());
    ui.checkBox_4->setChecked(configData["maxSpeed"].toBool());
}

void CFCconfig::getOptionsToJson() {
    QJsonArray CpuTempList;
    QJsonArray CpuSpeedList;
    for (int i = 0; i < 10; i++) {
        CpuTempList.push_back(ui.tableWidget->item(i, 0)->text().toInt());
        CpuSpeedList.push_back(ui.tableWidget->item(i, 1)->text().toInt());
    }
    QJsonArray GpuTempList;
    QJsonArray GpuSpeedList;
    for (int i = 0; i < 10; i++) {
        GpuTempList.push_back(ui.tableWidget_6->item(i, 0)->text().toInt());
        GpuSpeedList.push_back(ui.tableWidget_6->item(i, 1)->text().toInt());
    }
    QJsonObject newData=
    {
            {"configs",QJsonArray({
                QJsonArray({
                ui.radioButton->isChecked() ? 2 : 1,
                QJsonArray({ui.lineEdit->text().toInt(),ui.lineEdit_2->text().toInt(),ui.lineEdit_5->text().toInt(),ui.lineEdit_4->text().toInt()}),
                QJsonArray({CpuTempList,CpuSpeedList})
                }),
                QJsonArray({
                ui.radioButton_11->isChecked() ? 2 : 1,
                QJsonArray({ui.lineEdit_26->text().toInt(),ui.lineEdit_27->text().toInt(),ui.lineEdit_0->text().toInt(),ui.lineEdit_29->text().toInt()}),
                QJsonArray({GpuTempList,GpuSpeedList})
                })
            })
            },
            {"timeIntervals",QJsonArray({ui.lineEdit_6->text().toInt(),ui.lineEdit_7->text().toInt()})},
            {"speedLimit",QJsonArray({ui.checkBox_2->isChecked(),ui.lineEdit_36->text().toInt(),ui.lineEdit_38->text().toInt()})},
            {"staticSpeed",QJsonArray({ui.checkBox->isChecked(),ui.lineEdit_35->text().toInt(),ui.lineEdit_37->text().toInt()})},
            {"useClevoAuto",ui.checkBox_3->isChecked()},
            {"maxSpeed",ui.checkBox_4->isChecked()}
    };
    configData=newData;
}

void CFCconfig::OK()
{
    qDebug()<<"clicked OK";
    apply();
    this->hide();
}

void CFCconfig::apply()
{
    qDebug()<<"clicked apply";
    getOptionsToJson();
    emit updateConfigData();
    saveConfigJson();
}

void FanController::setConfig(QJsonObject data)
{
    config=data;
    return;
}
