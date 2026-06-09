#include "plcmonitor.h"
#include "ui_plcmonitor.h"
#include <QDateTime>
#include <QDebug>
#include <QModbusDataUnit>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <QMessageBox>
#include "PLCStatusEnums.h"
#include "AirTightnessParams.h"
#include "AirTightnessParamsDao.h"
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include "SystemSettingDao.h"
#include <QDir>
#include <QFileInfo>
#include "Logger.h"
#include "VolumeEncoding.h"
// 添加泄漏单位类型头文件，支持单位编码转换
#include "LeakUnitType.h"

PlcMonitor::PlcMonitor(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PlcMonitor),
    m_timer(nullptr),
    m_okCount(1234), // 初始化为示例数据
    m_ngCount(1234), // 初始化为示例数据
    m_modbusClient(nullptr),
    m_airtightModbusClient(nullptr),
    m_pressureModbusClient(nullptr),
    m_isConnected(false),
    m_isAirtightConnected(false),
    m_isPressureConnected(false),
    m_deviceAddress(2), // 默认设备地址为2
    m_airtightDeviceAddress(1), // 默认气密仪设备地址为1
    m_pressureDeviceAddress(1) // 默认调压装置设备地址为1
{
    ui->setupUi(this);
    initUI();
    initConnections();
    readPlcData();
    // 初始化气密仪启动重试定时器（非阻塞、周期重试）
    m_airtightStartRetryTimer = new QTimer(this);
    m_airtightStartRetryTimer->setSingleShot(false);
    m_airtightStartRetryTimer->setInterval(1000); // 每秒重试一次
    connect(m_airtightStartRetryTimer, &QTimer::timeout, this, &PlcMonitor::attemptStartAirtight);
    m_airtightStartRetryActive = false;
}

PlcMonitor::~PlcMonitor()
{
    if (m_timer) {
        m_timer->stop();
        delete m_timer;
    }
    delete ui;
}

void PlcMonitor::initUI()
{
    // 初始化界面设置
    setWindowTitle("PLC监控");
    
    // 初始化通道值显示
    ui->channel1Value->setText("---- ----");
    ui->channel2Value->setText("---- ----");
    ui->channel3Value->setText("---- ----");
    
    // 初始化统计数据显示
    updateStatistics();
    
    // 初始化日志显示
    logMessage("PLC监控系统启动", "info");
}

void PlcMonitor::initConnections()
{
    // 创建定时器用于更新PLC数据
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        // 只有当PLC连接成功时才更新PLC数据
        if (m_isConnected) {
            updatePlcData(); // 实时更新PLC数据
        }
    });
    m_timer->start(1500); // 每1.5秒更新一次，降低频率避免拥塞
    
    // 按钮槽函数使用Qt自动连接机制（on_<objectName>_<signal>）
}

void PlcMonitor::readPlcData()
{
    // 直接更新界面显示，不再从数据库获取数据
    updatePlcData();
}

void PlcMonitor::updatePlcData()
{
    QString deviceRunningStatus = "未连接";
    QString deviceStatusText = "未连接";
    QString detectionResultText = "未连接";
    
    // 只有当PLC连接成功时才读取寄存器值
    if (m_isConnected) {
        quint16 d400Value = 0; // 设备状态值 (D400)
        quint16 d402Value = 0; // 设备运行值 (D402)
        quint16 d404Value = 0; // 检验结果值 (D404)
        quint16 okCountValue = 0; // OK计数 (位地址0120.00)
        quint16 ngCountValue = 0; // NG计数 (字寄存器0122)
        quint16 register105Value = 0; // 寄存器105的值
        quint16 register106Value = 0; // 寄存器106的值
        
        // 从寄存器读取值
        bool d400Success = readRegister(400, d400Value);
        bool d402Success = readRegister(402, d402Value);
        bool d404Success = readRegister(404, d404Value);
        // 使用功能码01（Read Coils）读取寄存器105
        bool register105Success = readRegister(105, register105Value, QModbusDataUnit::Coils);
        // 使用功能码01（Read Coils）读取寄存器106
        bool register106Success = readRegister(106, register106Value, QModbusDataUnit::Coils);
        
        // 使用功能码2 (Discrete Inputs) 读取位地址0120.00
        bool okCountSuccess = readRegister(120, okCountValue, QModbusDataUnit::HoldingRegisters);
        
        // 使用功能码3 (Holding Registers) 读取字寄存器0122
        bool ngCountSuccess = readRegister(122, ngCountValue, QModbusDataUnit::HoldingRegisters);
        
        // 更新OK和NG计数值
        if (okCountSuccess) {
            m_okCount = okCountValue;
        }
        if (ngCountSuccess) {
            m_ngCount = ngCountValue;
        }
        
        // 处理寄存器105的值，控制气密仪启动（上升沿检测，防止重复触发）
        if (register105Success && register105Value == 1 && m_lastRegister105Value == 0) {
            qDebug() << "检测到寄存器105上升沿（0->1），启动气密仪";
            startAirtightInstrument();
        }
        m_lastRegister105Value = register105Value;
        
        // 处理寄存器106的值，控制气密仪复位（上升沿检测，防止重复触发）
        if (register106Success && register106Value == 1 && m_lastRegister106Value == 0) {
            qDebug() << "检测到寄存器106上升沿（0->1），复位气密仪";
            resetAirtightInstrument();
        }
        m_lastRegister106Value = register106Value;

        // 读取寄存器1013（功能码01，Coils），上升沿检测
        quint16 register1013Value = 0;
        bool register1013Success = readRegister(1013, register1013Value, QModbusDataUnit::Coils);
        if (register1013Success && register1013Value == 1 && m_lastRegister1013Value == 0) {
            qDebug() << "检测到寄存器1013上升沿（0->1），根据测试使能选择封堵";
            // 读取三路测试使能状态（49408/49409/49410，Coils）
            quint16 en1 = 0, en2 = 0, en3 = 0;
            readRegister(49408, en1, QModbusDataUnit::Coils);
            readRegister(49409, en2, QModbusDataUnit::Coils);
            readRegister(49410, en3, QModbusDataUnit::Coils);
            if (en1 == 1) {
                // 测试1使能：开封堵1，关封堵2/3
                writePlcCoilBlocking(49411, 1);
                writePlcCoilBlocking(49412, 0);
                writePlcCoilBlocking(49413, 0);
                logMessage("寄存器1013触发：测试1使能，开封堵1，关封堵2/3", "info");
            } else if (en2 == 1) {
                // 测试2使能：开封堵2，关封堵1/3
                writePlcCoilBlocking(49411, 0);
                writePlcCoilBlocking(49412, 1);
                writePlcCoilBlocking(49413, 0);
                logMessage("寄存器1013触发：测试2使能，开封堵2，关封堵1/3", "info");
            } else if (en3 == 1) {
                // 测试3使能：开封堵3，关封堵1/2
                writePlcCoilBlocking(49411, 0);
                writePlcCoilBlocking(49412, 0);
                writePlcCoilBlocking(49413, 1);
                logMessage("寄存器1013触发：测试3使能，开封堵3，关封堵1/2", "info");
            } else {
                logMessage("寄存器1013触发：所有测试使能均未开启，不操作封堵", "warning");
            }
        }
        if (register1013Success) {
            m_lastRegister1013Value = register1013Value;
        }

        // 读取寄存器1027（功能码01，Coils），上升沿检测
        // 若封堵1已开启，检查测试2使能，开封堵2（封堵1保持，封堵3关）
        quint16 register1026Value = 0;
        bool register1026Success = readRegister(1027, register1026Value, QModbusDataUnit::Coils);
        if (register1026Success && register1026Value == 1 && m_lastRegister1026Value == 0) {
            qDebug() << "检测到寄存器1026上升沿（0->1），检查封堵1状态及测试2/3使能";
            quint16 plug1 = 0, en2 = 0, en3 = 0;
            readRegister(49411, plug1, QModbusDataUnit::Coils); // 封堵1地址
            readRegister(49409, en2, QModbusDataUnit::Coils);   // 测试2使能地址
            readRegister(49410, en3, QModbusDataUnit::Coils);   // 测试3使能地址
            if (plug1 == 1 && en2 == 1) {
                writePlcCoilBlocking(49411, 0); // 关封堵1
                writePlcCoilBlocking(49412, 1); // 开封堵2
                writePlcCoilBlocking(49413, 0); // 关封堵3
                logMessage("寄存器1026触发：封堵1已开且测试2使能，开封堵2，关封堵3", "info");
            } else if (plug1 == 1 && en2 == 0 && en3 == 1) {
                writePlcCoilBlocking(49411, 0); // 关封堵1
                writePlcCoilBlocking(49412, 0); // 关封堵2
                writePlcCoilBlocking(49413, 1); // 开封堵3
                logMessage("寄存器1026触发：封堵1已开，测试2未使能，测试3使能，开封堵3", "info");
            } else {
                logMessage(QString("寄存器1026触发：条件不满足（封堵1=%1，测试2使能=%2，测试3使能=%3），不操作").arg(plug1).arg(en2).arg(en3), "warning");
            }
        }
        if (register1026Success) {
            m_lastRegister1026Value = register1026Value;
        }

        // 读取寄存器1047（功能码01，Coils），上升沿检测
        // 若封堵2已开启，检查测试3使能，开封堵3（封堵1/2保持）
        quint16 register1046Value = 0;
        bool register1046Success = readRegister(1047, register1046Value, QModbusDataUnit::Coils);
        if (register1046Success && register1046Value == 1 && m_lastRegister1046Value == 0) {
            qDebug() << "检测到寄存器1046上升沿（0->1），检查封堵2状态及测试3使能";
            quint16 plug2 = 0, en3 = 0;
            readRegister(49412, plug2, QModbusDataUnit::Coils); // 封堵2地址
            readRegister(49410, en3, QModbusDataUnit::Coils);   // 测试3使能地址
            if (plug2 == 1 && en3 == 1) {
                writePlcCoilBlocking(49411, 0); // 关封堵1
                writePlcCoilBlocking(49412, 0); // 开封堵2
                writePlcCoilBlocking(49413, 1); // 开封堵3
                logMessage("寄存器1046触发：封堵2已开且测试3使能，开封堵3", "info");
            } else {
                logMessage(QString("寄存器1046触发：条件不满足（封堵2=%1，测试3使能=%2），不操作").arg(plug2).arg(en3), "warning");
            }
        }
        if (register1046Success) {
            m_lastRegister1046Value = register1046Value;
        }

        // 同步测试按钮状态
        quint16 test1En = 0, test2En = 0, test3En = 0;
        if (readRegister(49408, test1En, QModbusDataUnit::Coils)) {
            ui->test1Button->setStyleSheet(test1En == 1
                ? "background-color: #4CAF50; color: white; border: 1px solid #000000; padding: 2px 15px; font-weight: bold;"
                : "background-color: #F44336; color: white; border: 1px solid #000000; padding: 2px 15px; font-weight: bold;");
            ui->test1OffButton->setStyleSheet(test1En == 1
                ? "background-color: #F44336; color: white; border: 1px solid #000000; padding: 2px 15px; font-weight: bold;"
                : "background-color: #4CAF50; color: white; border: 1px solid #000000; padding: 2px 15px; font-weight: bold;");
        }
        if (readRegister(49409, test2En, QModbusDataUnit::Coils)) {
            ui->test2Button->setStyleSheet(test2En == 1
                ? "background-color: #4CAF50; color: white; border: 1px solid #000000; padding: 2px 15px; font-weight: bold;"
                : "background-color: #F44336; color: white; border: 1px solid #000000; padding: 2px 15px; font-weight: bold;");
            ui->test2OffButton->setStyleSheet(test2En == 1
                ? "background-color: #F44336; color: white; border: 1px solid #000000; padding: 2px 15px; font-weight: bold;"
                : "background-color: #4CAF50; color: white; border: 1px solid #000000; padding: 2px 15px; font-weight: bold;");
        }
        if (readRegister(49410, test3En, QModbusDataUnit::Coils)) {
            ui->test3Button->setStyleSheet(test3En == 1
                ? "background-color: #4CAF50; color: white; border: 1px solid #000000; padding: 2px 15px; font-weight: bold;"
                : "background-color: #F44336; color: white; border: 1px solid #000000; padding: 2px 15px; font-weight: bold;");
            ui->test3OffButton->setStyleSheet(test3En == 1
                ? "background-color: #F44336; color: white; border: 1px solid #000000; padding: 2px 15px; font-weight: bold;"
                : "background-color: #4CAF50; color: white; border: 1px solid #000000; padding: 2px 15px; font-weight: bold;");
        }
        
        // 如果读取成功，使用PLCStatusEnums类将值转换为中文显示
        if (d400Success) {
            // 对于设备状态值，我们直接显示数值和对应的中文描述
            PLCStatusEnums::DeviceStatus status = PLCStatusEnums::deviceStatusFromCode(d400Value);
            deviceStatusText = QString("%1 (%2)").arg(PLCStatusEnums::deviceStatusToString(status)).arg(d400Value);
        } else {
            deviceStatusText = "读取失败";
        }
        
        if (d402Success) {
            // 对于设备运行值，我们直接显示数值和对应的中文描述
            PLCStatusEnums::DeviceOperation operation = PLCStatusEnums::deviceOperationFromCode(d402Value);
            deviceRunningStatus = QString("%1 (%2)").arg(PLCStatusEnums::deviceOperationToString(operation)).arg(d402Value);
        } else {
            deviceRunningStatus = "读取失败";
        }
        
        if (d404Success) {
            // 对于检验结果值，我们直接显示数值和对应的中文描述
            PLCStatusEnums::InspectionResult result = PLCStatusEnums::inspectionResultFromCode(d404Value);
            detectionResultText = QString("%1 (%2)").arg(PLCStatusEnums::inspectionResultToString(result)).arg(d404Value);
        } else {
            detectionResultText = "读取失败";
        }
    }
    
    // 更新设备运行状态显示
    ui->deviceRunningValue->setText(deviceRunningStatus);
    
    // 更新设备状态显示
    ui->deviceStatusValue->setText(deviceStatusText);
    
    // 更新检测结果显示
    ui->detectionResultValue->setText(detectionResultText);
    
    // 更新统计数据显示
    updateStatistics();

}

void PlcMonitor::updateStatistics()
{
    // 更新OK和NG计数显示
    ui->okCountValue->setText(QString("OK计数: %1").arg(m_okCount));
    ui->ngCountValue->setText(QString("NG计数: %1").arg(m_ngCount));
}



void PlcMonitor::logMessage(const QString &message, const QString &type)
{
    // 获取当前时间（用于界面显示）
    QString timeStr = QDateTime::currentDateTime().toString("[yyyy-MM-dd HH:mm:ss] ");

    // 根据类型设置不同的颜色（用于界面显示）
    QString color;
    if (type == "error" || type == "NG") {
        color = "#EF4444";
    } else if (type == "success" || type == "OK") {
        color = "#10B981";
    } else if (type == "warning") {
        color = "#FBBF24";
    } else {
        color = "#94A3B8";
    }

    // 构造HTML格式的日志消息（用于界面显示）
    QString html = QString("<p style='margin:0;'><span style='color:%1;'>%2%3</span></p>")
                   .arg(color).arg(timeStr).arg(message);

    // 统一写入到按日滚动的日志文件（集中式Logger）
    QString level = type;
    if (level == "success" || level == "OK") level = "info";
    if (level == "NG") level = "error";
    Logger::log(level, message);
}
// 已移除重复的本地文件写入逻辑，统一由Logger处理

void PlcMonitor::setModbusClient(QModbusClient *client)
{
    // 断开旧客户端的连接
    if (m_modbusClient) {
        disconnect(m_modbusClient, nullptr, this, nullptr);
    }
    
    // 设置新客户端
    m_modbusClient = client;
    
    // 如果客户端不为空，检查连接状态
    if (m_modbusClient) {
        m_isConnected = (m_modbusClient->state() == QModbusDevice::ConnectedState);
        updateConnectionStatus(m_isConnected);
        
        // 初始连接已是已连接状态时，异步更新一次数据，避免在回调中阻塞
        if (m_isConnected) {
            QTimer::singleShot(0, this, &PlcMonitor::updatePlcData);
        }
        
        // 连接状态变化信号（异步触发数据更新，避免在stateChanged回调中使用阻塞事件循环）
        connect(m_modbusClient, &QModbusClient::stateChanged, this, [this](QModbusDevice::State state) {
            m_isConnected = (state == QModbusDevice::ConnectedState);
            updateConnectionStatus(m_isConnected);
            qDebug() << "PLC连接状态变化:" << (m_isConnected ? "已连接" : "未连接");
            
            // 当连接状态变化时，异步更新一次数据，防止潜在的事件循环嵌套引发崩溃
            QTimer::singleShot(0, this, &PlcMonitor::updatePlcData);
        });
        
        qDebug() << "已设置PLC Modbus客户端";
    } else {
        m_isConnected = false;
        updateConnectionStatus(false);
        qDebug() << "PLC Modbus客户端已清除";
    }
}

void PlcMonitor::setAirtightModbusClient(QModbusClient *client)
{
    // 断开旧客户端的连接
    if (m_airtightModbusClient) {
        disconnect(m_airtightModbusClient, nullptr, this, nullptr);
    }
    
    // 设置新客户端
    m_airtightModbusClient = client;
    
    // 如果客户端不为空，检查连接状态
    if (m_airtightModbusClient) {
        m_isAirtightConnected = (m_airtightModbusClient->state() == QModbusDevice::ConnectedState);
        updateAirtightConnectionStatus(m_isAirtightConnected);
        
        // 连接状态变化信号
        connect(m_airtightModbusClient, &QModbusClient::stateChanged, this, [this](QModbusDevice::State state) {
            m_isAirtightConnected = (state == QModbusDevice::ConnectedState);
            updateAirtightConnectionStatus(m_isAirtightConnected);
            qDebug() << "气密仪连接状态变化:" << (m_isAirtightConnected ? "已连接" : "未连接");
        });
        
        qDebug() << "已设置气密仪Modbus客户端";
    } else {
        m_isAirtightConnected = false;
        updateAirtightConnectionStatus(false);
        qDebug() << "气密仪Modbus客户端已清除";
    }
}

void PlcMonitor::setPressureModbusClient(QModbusClient *client)
{
    // 断开旧客户端的连接
    if (m_pressureModbusClient) {
        disconnect(m_pressureModbusClient, nullptr, this, nullptr);
    }
    
    // 设置新客户端
    m_pressureModbusClient = client;
    
    // 如果客户端不为空，检查连接状态
    if (m_pressureModbusClient) {
        m_isPressureConnected = (m_pressureModbusClient->state() == QModbusDevice::ConnectedState);
        updatePressureConnectionStatus(m_isPressureConnected);
        
        // 连接状态变化信号
        connect(m_pressureModbusClient, &QModbusClient::stateChanged, this, [this](QModbusDevice::State state) {
            m_isPressureConnected = (state == QModbusDevice::ConnectedState);
            updatePressureConnectionStatus(m_isPressureConnected);
            qDebug() << "调压装置连接状态变化:" << (m_isPressureConnected ? "已连接" : "未连接");
        });
        
        qDebug() << "已设置调压装置Modbus客户端";
    } else {
        m_isPressureConnected = false;
        updatePressureConnectionStatus(false);
        qDebug() << "调压装置Modbus客户端已清除";
    }
}

void PlcMonitor::setDeviceAddress(int address)
{
    if (address >= 1 && address <= 255) { // Modbus设备地址范围通常是1-255
        m_deviceAddress = address;
        qDebug() << QString("PLC设备地址已设置为: %1").arg(address);
    } else {
        qDebug() << QString("无效的PLC设备地址: %1，地址范围应为1-255").arg(address);
    }
}

void PlcMonitor::setAirtightDeviceAddress(int address)
{
    if (address >= 1 && address <= 255) { // Modbus设备地址范围通常是1-255
        m_airtightDeviceAddress = address;
        qDebug() << QString("气密仪设备地址已设置为: %1").arg(address);
    } else {
        qDebug() << QString("无效的气密仪设备地址: %1，地址范围应为1-255").arg(address);
    }
}

void PlcMonitor::setPressureDeviceAddress(int address)
{
    if (address >= 1 && address <= 255) { // Modbus设备地址范围通常是1-255
        m_pressureDeviceAddress = address;
        qDebug() << QString("调压装置设备地址已设置为: %1").arg(address);
    } else {
        qDebug() << QString("无效的调压装置设备地址: %1，地址范围应为1-255").arg(address);
    }
}

bool PlcMonitor::readRegister(quint16 address, quint16 &value, QModbusDataUnit::RegisterType type) {
    if (!m_modbusClient || !m_isConnected) {
        qDebug() << "无法读取寄存器: 未连接到设备";
        return false;
    }

    // 使用功能码03（Read Holding Registers）读取单个寄存器
    QModbusDataUnit readUnit(type, address, 1);
    
    // 发送读取请求
    QModbusReply *reply = m_modbusClient->sendReadRequest(readUnit, m_deviceAddress); // 使用配置的设备地址
    if (!reply) {
        qDebug() << QString("发送读取请求失败: %1").arg(m_modbusClient->errorString());
        return false;
    }

    // 等待回复或超时
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    
    connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(3000);
    loop.exec();

    // 处理回复
    bool success = false;
    if (timer.isActive()) {
        // 正常收到回复
        timer.stop();
        
        if (reply->error() == QModbusDevice::NoError) {
            const QModbusDataUnit result = reply->result();
            if (result.valueCount() > 0) {
                value = result.value(0);
                success = true;
            } else {
                qDebug() << QString("读取数据失败：无返回值，地址: %1").arg(address);
            }
        } else {
            qDebug() << QString("读取数据失败: %1, 地址: %2").arg(reply->errorString()).arg(address);
        }
        
        reply->deleteLater();
    } else {
        // 超时
        qDebug() << QString("读取数据超时，地址: %1").arg(address);
        disconnect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
        reply->deleteLater();
    }

    return success;
}

void PlcMonitor::updateConnectionStatus(bool isConnected)
{
    m_isConnected = isConnected;
    logMessage(QString("PLC监控页面连接状态更新: %1").arg(m_isConnected ? "已连接" : "未连接"), "info");
    if (m_isConnected) {
        logMessage("PLC连接成功", "success");
    } else {
        logMessage("PLC连接断开", "error");
    }
}

void PlcMonitor::updateAirtightConnectionStatus(bool isConnected)
{
    m_isAirtightConnected = isConnected;
    logMessage(QString("气密仪连接状态更新: %1").arg(m_isAirtightConnected ? "已连接" : "未连接"), "info");
    if (m_isAirtightConnected) {
        logMessage("气密仪连接成功", "success");
    } else {
        logMessage("气密仪连接断开", "error");
    }
}

void PlcMonitor::updatePressureConnectionStatus(bool isConnected)
{
    m_isPressureConnected = isConnected;
    
    // 记录连接状态更新日志
    logMessage(QString("调压装置连接状态更新: %1").arg(m_isPressureConnected ? "已连接" : "未连接"), "info");
    qDebug() << "PlcMonitor::updatePressureConnectionStatus 被调用，状态:" << (isConnected ? "已连接" : "未连接");
}

bool PlcMonitor::writeAirtightRegister(quint16 address, quint16 value)
{
    if (!m_airtightModbusClient || !m_isAirtightConnected) {
        qDebug() << "无法写入气密仪寄存器: 气密仪未连接";
        return false;
    }

    // 创建写入单元
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, address, 2);
    writeUnit.setValue(0, value);
    writeUnit.setValue(1, 0); // 第二个寄存器设为0，确保使用功能码16
    
    // 发送写入请求
    QModbusReply *reply = m_airtightModbusClient->sendWriteRequest(writeUnit, m_airtightDeviceAddress);
    if (!reply) {
        qDebug() << QString("发送写入气密仪请求失败: %1").arg(m_airtightModbusClient->errorString());
        return false;
    }

    // 等待回复或超时
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    
    connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    timer.start(3000);
    loop.exec();

    // 处理回复
    bool success = false;
    if (timer.isActive()) {
        // 正常收到回复
        timer.stop();
        
        if (reply->error() == QModbusDevice::NoError) {
            success = true;
        } else {
            logMessage(QString("写入气密仪数据失败: %1, 地址: %2, 值: %3").arg(reply->errorString()).arg(address).arg(value), "error");
        }
        
        reply->deleteLater();
    } else {
        // 超时
        logMessage(QString("写入气密仪数据超时，地址: %1").arg(address), "error");
        disconnect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
        reply->deleteLater();
    }

    return success;
}

int PlcMonitor::readInstrumentSelection()
{
    if (!m_modbusClient || m_modbusClient->state() != QModbusDevice::ConnectedState) {
        logMessage("PLC未连接，无法读取仪器评选数据", "error");
        return -1; // 返回-1表示读取失败
    }
    
    quint16 value;
    // 从寄存器110读取数据，使用功能码03（Holding Registers）
    if (!readRegister(110, value, QModbusDataUnit::HoldingRegisters)) {
        logMessage("读取仪器评选数据失败（寄存器110）", "error");
        return -1;
    }
    
    // 记录读取到的仪器评选数据
    logMessage(QString("读取到的仪器评选数据: %1").arg(value), "info");
    return value;
}

bool PlcMonitor::sendParamsToDevice(const AirTightnessFullParams& params)
{
    if (!m_airtightModbusClient || m_airtightModbusClient->state() != QModbusDevice::ConnectedState) {
        logMessage("未连接到气密仪设备，无法发送参数", "error");
        return false;
    }

    logMessage("开始准备参数发送到设备", "info");
    
    const int timeoutMs = 3000; // 单个命令超时时间，增加到3秒避免通信繁忙时超时
    const quint16 startAddress = 8192; // 起始地址
    const quint16 endAddress = 8229;   // 结束地址
    const int numRegisters = endAddress - startAddress + 1; // 寄存器数量
    
    // 先发送地址8192,值为1的命令，通知设备准备接收参数
    logMessage("发送参数准备命令", "info");
    
    // 等待设备准备
    QThread::msleep(500);
    
    // 创建批量写入数据单元，从8192到8229共38个寄存器
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, startAddress, numRegisters);
    
    // 初始化所有寄存器值为0
    for (int i = 0; i < numRegisters; ++i) {
        writeUnit.setValue(i, 0);
    }
    
    // 发送程序号（使用未使用的地址8193）
    writeUnit.setValue(8192 - startAddress, 26112);
    writeUnit.setValue(8193 - startAddress, 0);
    logMessage(QString("程序号: %1").arg(params.programNumber), "info");
    
     // 1. 时间参数：保持编码逻辑不变，但保留无符号编码值
    auto encodeTime = [](double n) -> quint16 {  // 返回无符号类型
        if (n < 0.0) n = 0.0;
        if (n > 200.0) n = 200.0;
        quint16 value = static_cast<quint16>(std::round(n * 100.0));
        quint16 low = static_cast<quint16>(value & 0xFFu);
        quint16 high = static_cast<quint16>((value >> 8) & 0xFFu);
        return static_cast<quint16>((low << 8) | high);  // 返回无符号编码值
    };

    // 2. 编码得到无符号值（直接用于写入寄存器）
    quint16 fillTime = encodeTime(params.cycleTime.fillTime);
    quint16 stabilizationTime = encodeTime(params.cycleTime.stabilizationTime);
    quint16 testTime = encodeTime(params.cycleTime.testTime);
    quint16 dumpTime = encodeTime(params.cycleTime.dumpTime);

    // 3. 以无符号类型写入寄存器（关键修改：确保setValue接受quint16）
    // 若writeUnit.setValue支持无符号参数，直接传入：
    writeUnit.setValue(8196 - startAddress, fillTime);        // 填充时间（无符号）
    writeUnit.setValue(8197 - startAddress, stabilizationTime); // 稳定时间（无符号）
    writeUnit.setValue(8198 - startAddress, testTime);         // 测试时间（无符号）
    writeUnit.setValue(8199 - startAddress, dumpTime);         // 排放时间（无符号）

    // 4. 日志输出（保持不变，显示无符号值）
    logMessage(QString("时间参数(编码): 填充=%1, 稳定=%2, 测试=%3, 排放=%4")
        .arg(fillTime).arg(stabilizationTime).arg(testTime).arg(dumpTime));

    // 2. 压力单位
    writeUnit.setValue(8200 - startAddress, static_cast<quint16>(params.pressure.unitIndex));   // 压力单位
    logMessage(QString("压力单位代码: %1").arg(params.pressure.unitIndex), "info");

    // 3. 压力值参数（双寄存器写入：8201/8202，8203/8204，8205/8206）
    auto swap16 = [](quint16 v) -> quint16 { return static_cast<quint16>((v << 8) | (v >> 8)); };
    auto encodePressure = [&](quint16 n) -> QPair<quint16, quint16> {
        quint32 raw = static_cast<quint32>(n) * 1000u; // 例如 n=70 -> 70000 (0x00011170)
        quint16 low = static_cast<quint16>(raw & 0xFFFFu);       // 低16位，如0x1170
        quint16 high = static_cast<quint16>((raw >> 16) & 0xFFFFu); // 高16位，如0x0001
        return qMakePair(swap16(low), swap16(high)); // 字节交换：0x1170->0x7011, 0x0001->0x0100
    };
    quint16 minPressure = static_cast<quint16>(params.pressure.minimum);
    quint16 maxPressure = static_cast<quint16>(params.pressure.maximum);
    quint16 fillPressure = static_cast<quint16>(params.pressure.setFill);

    QPair<quint16, quint16> minEnc = encodePressure(minPressure);
    QPair<quint16, quint16> maxEnc = encodePressure(maxPressure);
    QPair<quint16, quint16> fillEnc = encodePressure(fillPressure);

    // 最小压力: 写入8201(低位交换) / 8202(高位交换)
    writeUnit.setValue(8201 - startAddress, minEnc.first);
    writeUnit.setValue(8202 - startAddress, minEnc.second);
    // 最大压力: 写入8203 / 8204
    writeUnit.setValue(8203 - startAddress, maxEnc.first);
    writeUnit.setValue(8204 - startAddress, maxEnc.second);
    // 填充压力: 写入8205 / 8206
    writeUnit.setValue(8205 - startAddress, fillEnc.first);
    writeUnit.setValue(8206 - startAddress, fillEnc.second);
    logMessage(QString("压力参数编码: 最小=原%1 -> 低位%2 高位%3, 最大=原%4 -> 低位%5 高位%6, 填充=原%7 -> 低位%8 高位%9")
              .arg(minPressure)
              .arg(minEnc.first)
              .arg(minEnc.second)
              .arg(maxPressure)
              .arg(maxEnc.first)
              .arg(maxEnc.second)
              .arg(fillPressure)
              .arg(fillEnc.first)
              .arg(fillEnc.second), "info");

    // 4. 填充类型
    writeUnit.setValue(8211 - startAddress, static_cast<quint16>(params.pressure.fillTypeIndex));          // 填充类型
    logMessage(QString("填充类型代码: %1").arg(params.pressure.fillTypeIndex), "info");

    // 5. 泄漏相关参数
    writeUnit.setValue(8212 - startAddress, static_cast<quint16>(params.leak.leakUnitIndex));     // 泄漏单位
    writeUnit.setValue(8213 - startAddress, 256);
    writeUnit.setValue(8214 - startAddress, static_cast<quint16>(params.leak.volumeUnitIndex));       // 容积单位
    logMessage(QString("泄漏参数: 测试阈值=%1, 单位代码=%2")
              .arg(params.leak.testReject).arg(params.leak.leakUnitIndex), "info");

    // 6. 容积和其他参数
    // 将容积值映射为枚举编码（见VolumeEncoding.h）
    {
        int volumeInt = static_cast<int>(params.leak.volume);
        quint16 volumeOrig = static_cast<quint16>(volumeInt);
        quint16 encoded8215 = VolumeEncoding::encode8215(volumeInt);
        quint16 encoded8216 = 0;
        if (VolumeEncoding::tryEncode(volumeInt, encoded8216)) {
            volumeOrig = encoded8216;
        }
        writeUnit.setValue(8215 - startAddress, encoded8215);
        writeUnit.setValue(8216 - startAddress, encoded8216);
        logMessage(QString("容积参数: 原始容积=%1 8215=%2 8216=%3").arg(volumeInt).arg(encoded8215).arg(encoded8216));
    }

    // 先用单位1编码还原单位，再取单位2编码
    LeakUnitType::Unit leakUnit = LeakUnitType::getUnitFromCode(static_cast<int>(params.leak.leakUnitIndex), LeakUnitType::CodeType1);
    quint16 leakUnitCode2 = static_cast<quint16>(LeakUnitType::getCode(leakUnit, LeakUnitType::CodeType2));
    
    // 计算允许泄露值编码：根据泄露单位使用不同的编码方式
    quint16 allowLeakEncoded8219 = 0;
    quint16 allowLeakEncoded8220 = 0;
    double allowLeakValue = static_cast<double>(params.leak.testReject);
    
    if (leakUnit == LeakUnitType::Pa) {
        // Pa单位：寄存器8219 = (2560 × 编码值) mod 65535
        quint32 allowLeakScaled = static_cast<quint32>(allowLeakValue * 2560);
        allowLeakEncoded8219 = static_cast<quint16>(allowLeakScaled % 65535);
        allowLeakEncoded8220 = 0;
        logMessage(QString("允许泄露值编码(Pa单位): 原值=%1, 8219=%2, 8220=%3").arg(allowLeakValue).arg(allowLeakEncoded8219).arg(allowLeakEncoded8220), "info");
    } else if (leakUnit == LeakUnitType::MlPerMinute) {
        // ml/min单位：
        // 寄存器8220 = floor(输入值 / 64) × 256
        // 寄存器8219 = ((输入值 × 256000) mod 65535) - 寄存器8220
        allowLeakEncoded8220 = static_cast<quint16>((static_cast<int>(allowLeakValue) / 64) * 256);
        quint32 scaled = static_cast<quint32>(allowLeakValue * 256000);
        allowLeakEncoded8219 = static_cast<quint16>((scaled % 65535) - allowLeakEncoded8220);
        logMessage(QString("允许泄露值编码(ml/min单位): 原值=%1, 8219=%2, 8220=%3").arg(allowLeakValue).arg(allowLeakEncoded8219).arg(allowLeakEncoded8220), "info");
    } else {
        // 其他单位：直接使用原值
        allowLeakEncoded8219 = static_cast<quint16>(allowLeakValue);
        allowLeakEncoded8220 = 0;
        logMessage(QString("允许泄露值编码(其他单位): 原值=%1, 8219=%2, 8220=%3").arg(allowLeakValue).arg(allowLeakEncoded8219).arg(allowLeakEncoded8220), "info");
    }
    
    writeUnit.setValue(8217 - startAddress, leakUnitCode2); 
    writeUnit.setValue(8219 - startAddress, allowLeakEncoded8219); 
    writeUnit.setValue(8220 - startAddress, allowLeakEncoded8220); 
    writeUnit.setValue(8218 - startAddress, 0); // 拒绝计算方式
    writeUnit.setValue(8224 - startAddress, 64);
    writeUnit.setValue(8225 - startAddress, 32068);          // 标准大气压
    writeUnit.setValue(8227 - startAddress, 0);         // 标准温度
    writeUnit.setValue(8229 - startAddress, static_cast<quint16>(params.leak.offset));          // 偏移量
    logMessage(QString("容积: %1, 单位代码: %2, 标准大气压: %3, 标准温度: %4, 偏移量: %5")
              .arg(params.leak.volume).arg(params.leak.volumeUnitIndex).arg(params.leak.stdAtm).arg(params.leak.stdTemp).arg(params.leak.offset), "info");

    // 发送批量写入请求
    logMessage(QString("准备批量发送从地址 %1 到 %2 的参数（共 %3 个寄存器）")
              .arg(startAddress).arg(endAddress).arg(numRegisters), "info");
    
    QModbusReply *reply = m_airtightModbusClient->sendWriteRequest(writeUnit, m_airtightDeviceAddress);
    if (!reply) {
        logMessage(QString("发送请求失败: %1").arg(m_airtightModbusClient->errorString()), "error");
        return false;
    }

    // 等待回复或超时
    QEventLoop loop;
    connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);

    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);

    loop.exec();

    // 处理回复或超时
    bool success = false;
    if (timer.isActive()) {
        // 正常收到回复
        timer.stop();

        if (reply->error() != QModbusDevice::NoError) {
            logMessage(QString("发送命令失败: %1").arg(reply->errorString()), "error");
        } else {
            logMessage(QString("批量参数发送成功 - 从地址 %1 到 %2")
                      .arg(startAddress).arg(endAddress), "success");
            success = true;
        }
        reply->deleteLater();
    } else {
        // 超时处理
        logMessage(QString("发送命令超时 - 从地址 %1 到 %2")
                  .arg(startAddress).arg(endAddress), "error");
        disconnect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
        reply->deleteLater();
    }
    
    if (success) {
        logMessage("所有参数已成功发送到设备", "success"); 
    } else {
        logMessage("发送参数完成命令失败", "warning");
    }
    return success;
}

void PlcMonitor::startAirtightInstrument()
{
    // 需要给plc寄存器516写入0，使用功能码05
     // 使用统一的写入方法，确保正确处理回复与错误
     if (!writePlcCoilBlocking(516, 0)) {
         logMessage("向PLC寄存器516写入0失败", "error");
     } else {
         logMessage("向PLC寄存器516写入0成功", "success");
     }
    // 读取仪器评选，plc寄存器110，使用功能码03
    int instrumentSelection = readInstrumentSelection();

    // 第二步：根据仪器评选号从数据库读取参数并发送到气密仪
    if (instrumentSelection > 0) {
        try {
            AirTightnessFullParams params = AirTightnessParamsDao::getParamsByProgramNumber(instrumentSelection);
            
            if (params.id != -1) {
                logMessage(QString("成功根据仪器评选号 %1 读取到参数配置，参数组ID: %2")
                          .arg(instrumentSelection).arg(params.id), "info");
                sendPressureToRegulator(params);

                bool paramsSent = sendParamsToDevice(params);
                if (!paramsSent) {
                    logMessage("参数发送失败，开始重试（最多10次）", "warning");
                    int attempt = 1;
                    const int maxAttempts = 10;
                    while (!paramsSent && attempt <= maxAttempts) {
                        logMessage(QString("参数发送失败，正在重试，第 %1 次（共 %2 次）").arg(attempt).arg(maxAttempts), "warning");
                        QThread::msleep(1500);
                        paramsSent = sendParamsToDevice(params);
                        ++attempt;
                    }
                    if (paramsSent) {
                        logMessage("参数发送成功（重试后）", "success");
                    } else {
                        logMessage(QString("参数发送失败，已达到最大重试次数 %1 次").arg(maxAttempts), "error");
                    }
                }
            } else {
                QString warningMsg = QString("警告：未找到程序号 %1 对应的参数配置，请先配置参数")
                                  .arg(instrumentSelection);
                logMessage(warningMsg, "warning");
                QMessageBox::warning(this, "参数配置警告",
                                    warningMsg + "\n\n请立即前往参数设置页面进行配置程序号：" + QString::number(instrumentSelection) + "的参数",
                                    QMessageBox::Ok, QMessageBox::Ok);
            }
        } catch (const std::exception &e) {
            logMessage(QString("根据仪器评选号 %1 读取参数配置时发生异常: %2")
                      .arg(instrumentSelection).arg(QString::fromUtf8(e.what())), "error");
        }
    } else if (instrumentSelection == 0) {
        logMessage("仪器评选号为0，无法找到对应的参数配置", "warning");
    } else {
        logMessage("警告：无法获取仪器评选数据，将使用默认配置启动气密仪", "warning");
    }

    // 第三步：发送仪器评选号信号给 realtimemonitor 和 airtighttestresult
    logMessage(QString("发送仪器评选号信号: %1").arg(instrumentSelection));
    emit instrumentSelectionChanged(instrumentSelection);

    // 然后向气密仪的寄存器地址9472发送值4608，启动气密仪
    // 写入成功后，需要等待1秒，确保指令生效
    QThread::msleep(1000);
    bool success = writeAirtightRegister(9472, 4608);
    if (success) {
        logMessage("气密仪启动成功", "success");
        // 启动成功则停止重试
        if (m_airtightStartRetryTimer) m_airtightStartRetryTimer->stop();
        m_airtightStartRetryActive = false;
    } else {
        logMessage("气密仪启动失败，开始持续重试直到成功", "error");
        if (m_airtightStartRetryTimer && !m_airtightStartRetryActive) {
            m_airtightStartRetryActive = true;
            m_airtightStartRetryTimer->start();
        }
    }
}

void PlcMonitor::handleTestResultReady(const TestResult &result)
{
    // 处理通道1的测试结果
    if (result.isPassed && !result.isFailed) {
        // 通过状态
        // 向PLC寄存器510写入1，使用功能码05
        // writePlcCoil(510, 1);
        ui->channel1Value->setText("通过");
        ui->channel1Value->setStyleSheet("background-color: #4caf50; color: white; border: 1px solid #2196f3; padding: 8px; font-weight: bold;");
    } else if (!result.isPassed && result.isFailed) {
        // 不通过状态
        // 向PLC寄存器511写入1，使用功能码05
        // writePlcCoil(511, 1);
        ui->channel1Value->setText("不通过");
        ui->channel1Value->setStyleSheet("background-color: #f44336; color: white; border: 1px solid #2196f3; padding: 8px; font-weight: bold;");
    } else if (!result.isPassed && !result.isFailed) {
        // 测试中状态
        ui->channel1Value->setText("测试中");
        ui->channel1Value->setStyleSheet("background-color: #ff9800; color: white; border: 1px solid #2196f3; padding: 8px; font-weight: bold;");
    }
    logMessage(QString("通道1测试结果更新: 通过=%1, 不通过=%2, 压力=%3, 泄漏=%4")
              .arg(result.isPassed ? "1" : "0")
              .arg(result.isFailed ? "1" : "0")
              .arg(result.channelPressure)
              .arg(result.channelLeak), "info");

    
}

void PlcMonitor::handleTestResultReady2(const TestResult &result)
{
    // 处理通道2的测试结果
    if (result.isPassed && !result.isFailed) {
        // 通过状态
        // 向PLC寄存器512写入1，使用功能码05
        // writePlcCoil(512, 1);
        ui->channel2Value->setText("通过");
        ui->channel2Value->setStyleSheet("background-color: #4caf50; color: white; border: 1px solid #2196f3; padding: 8px; font-weight: bold;");
    } else if (!result.isPassed && result.isFailed) {
        // 不通过状态
        // 向PLC寄存器513写入1，使用功能码05
        // writePlcCoil(513, 1);
        ui->channel2Value->setText("不通过");
        ui->channel2Value->setStyleSheet("background-color: #f44336; color: white; border: 1px solid #2196f3; padding: 8px; font-weight: bold;");
    } else if (!result.isPassed && !result.isFailed) {
        // 测试中状态
        ui->channel2Value->setText("测试中");
        ui->channel2Value->setStyleSheet("background-color: #ff9800; color: white; border: 1px solid #2196f3; padding: 8px; font-weight: bold;");
    }
    logMessage(QString("通道2测试结果更新: 通过=%1, 不通过=%2, 压力=%3, 泄漏=%4")
              .arg(result.isPassed ? "1" : "0")
              .arg(result.isFailed ? "1" : "0")
              .arg(result.channelPressure)
              .arg(result.channelLeak), "info");
}

void PlcMonitor::handleTestResultReady3(const TestResult &result)
{
    // 处理通道3的测试结果
    if (result.isPassed && !result.isFailed) {
        // 通过状态
        // 向PLC寄存器514写入1，使用功能码05
        // writePlcCoil(514, 1);
        ui->channel3Value->setText("通过");
        ui->channel3Value->setStyleSheet("background-color: #4caf50; color: white; border: 1px solid #2196f3; padding: 8px; font-weight: bold;");
    } else if (!result.isPassed && result.isFailed) {
        // 不通过状态
        // 向PLC寄存器515写入1，使用功能码05
        // writePlcCoil(515, 1);
        ui->channel3Value->setText("不通过");
        ui->channel3Value->setStyleSheet("background-color: #f44336; color: white; border: 1px solid #2196f3; padding: 8px; font-weight: bold;");
    } else if (!result.isPassed && !result.isFailed) {
        // 测试中状态
        ui->channel3Value->setText("测试中");
        ui->channel3Value->setStyleSheet("background-color: #ff9800; color: white; border: 1px solid #2196f3; padding: 8px; font-weight: bold;");
    }
    logMessage(QString("通道3测试结果更新: 通过=%1, 不通过=%2, 压力=%3, 泄漏=%4")
              .arg(result.isPassed ? "1" : "0")
              .arg(result.isFailed ? "1" : "0")
              .arg(result.channelPressure)
              .arg(result.channelLeak), "info");
}

void PlcMonitor::on_test1Button_clicked()
{
    if (!m_isConnected) {
        QMessageBox::warning(this, "警告", "PLC未连接，无法执行测试");
        return;
    }
    int test1Num = ui->test1Value->currentText().toInt();
    bool s1 = writePlcCoilBlocking(49408, 1);
    // 写入频道号到寄存器101
    QModbusDataUnit wu(QModbusDataUnit::HoldingRegisters, 101, 1);
    wu.setValue(0, static_cast<quint16>(test1Num));
    bool s2 = false;
    if (QModbusReply *r = m_modbusClient->sendWriteRequest(wu, m_deviceAddress)) {
        QEventLoop loop; QTimer t; t.setSingleShot(true);
        connect(r, &QModbusReply::finished, &loop, &QEventLoop::quit);
        connect(&t, &QTimer::timeout, &loop, &QEventLoop::quit);
        t.start(3000); loop.exec();
        if (t.isActive()) { t.stop(); s2 = (r->error() == QModbusDevice::NoError); }
        r->deleteLater();
    }
    if (s1 && s2) {
        ui->test1Button->setStyleSheet("background-color: #4CAF50; color: white; border: 1px solid #000000; padding: 2px 15px; font-weight: bold;");
        ui->test1OffButton->setStyleSheet("background-color: #F44336; color: white; border: 1px solid #000000; padding: 2px 15px; font-weight: bold;");
        logMessage(QString("测试1开启，频道%1").arg(test1Num), "info");
    }
}

void PlcMonitor::on_test1OffButton_clicked()
{
    if (!m_isConnected) return;
    if (writePlcCoilBlocking(49408, 0)) {
        ui->test1Button->setStyleSheet("background-color: #F44336; color: white; border: 1px solid #000000; padding: 2px 15px; font-weight: bold;");
        ui->test1OffButton->setStyleSheet("background-color: #4CAF50; color: white; border: 1px solid #000000; padding: 2px 15px; font-weight: bold;");
        logMessage("测试1关闭", "info");
    }
}

void PlcMonitor::on_test2Button_clicked()
{
    if (!m_isConnected) {
        QMessageBox::warning(this, "警告", "PLC未连接，无法执行测试");
        return;
    }
    int test2Num = ui->test2Value->currentText().toInt();
    bool s1 = writePlcCoilBlocking(49409, 1);
    QModbusDataUnit wu(QModbusDataUnit::HoldingRegisters, 102, 1);
    wu.setValue(0, static_cast<quint16>(test2Num));
    bool s2 = false;
    if (QModbusReply *r = m_modbusClient->sendWriteRequest(wu, m_deviceAddress)) {
        QEventLoop loop; QTimer t; t.setSingleShot(true);
        connect(r, &QModbusReply::finished, &loop, &QEventLoop::quit);
        connect(&t, &QTimer::timeout, &loop, &QEventLoop::quit);
        t.start(3000); loop.exec();
        if (t.isActive()) { t.stop(); s2 = (r->error() == QModbusDevice::NoError); }
        r->deleteLater();
    }
    if (s1 && s2) {
        ui->test2Button->setStyleSheet("background-color: #4CAF50; color: white; border: 1px solid #000000; padding: 2px 15px; font-weight: bold;");
        ui->test2OffButton->setStyleSheet("background-color: #F44336; color: white; border: 1px solid #000000; padding: 2px 15px; font-weight: bold;");
        logMessage(QString("测试2开启，频道%1").arg(test2Num), "info");
    }
}

void PlcMonitor::on_test2OffButton_clicked()
{
    if (!m_isConnected) return;
    if (writePlcCoilBlocking(49409, 0)) {
        ui->test2Button->setStyleSheet("background-color: #F44336; color: white; border: 1px solid #000000; padding: 2px 15px; font-weight: bold;");
        ui->test2OffButton->setStyleSheet("background-color: #4CAF50; color: white; border: 1px solid #000000; padding: 2px 15px; font-weight: bold;");
        logMessage("测试2关闭", "info");
    }
}

void PlcMonitor::on_test3Button_clicked()
{
    if (!m_isConnected) {
        QMessageBox::warning(this, "警告", "PLC未连接，无法执行测试");
        return;
    }
    int test3Num = ui->test3Value->currentText().toInt();
    bool s1 = writePlcCoilBlocking(49410, 1);
    QModbusDataUnit wu(QModbusDataUnit::HoldingRegisters, 103, 1);
    wu.setValue(0, static_cast<quint16>(test3Num));
    bool s2 = false;
    if (QModbusReply *r = m_modbusClient->sendWriteRequest(wu, m_deviceAddress)) {
        QEventLoop loop; QTimer t; t.setSingleShot(true);
        connect(r, &QModbusReply::finished, &loop, &QEventLoop::quit);
        connect(&t, &QTimer::timeout, &loop, &QEventLoop::quit);
        t.start(3000); loop.exec();
        if (t.isActive()) { t.stop(); s2 = (r->error() == QModbusDevice::NoError); }
        r->deleteLater();
    }
    if (s1 && s2) {
        ui->test3Button->setStyleSheet("background-color: #4CAF50; color: white; border: 1px solid #000000; padding: 2px 15px; font-weight: bold;");
        ui->test3OffButton->setStyleSheet("background-color: #F44336; color: white; border: 1px solid #000000; padding: 2px 15px; font-weight: bold;");
        logMessage(QString("测试3开启，频道%1").arg(test3Num), "info");
    }
}

void PlcMonitor::on_test3OffButton_clicked()
{
    if (!m_isConnected) return;
    if (writePlcCoilBlocking(49410, 0)) {
        ui->test3Button->setStyleSheet("background-color: #F44336; color: white; border: 1px solid #000000; padding: 2px 15px; font-weight: bold;");
        ui->test3OffButton->setStyleSheet("background-color: #4CAF50; color: white; border: 1px solid #000000; padding: 2px 15px; font-weight: bold;");
        logMessage("测试3关闭", "info");
    }
}

void PlcMonitor::writePlcCoil(quint16 address, int value, const std::function<void(bool)>& callback){
    if (!m_modbusClient || !m_isConnected) {
        logMessage(QString("无法写入PLC线圈寄存器: PLC未连接，地址: %1, 值: %2").arg(address).arg(value ? 1 : 0), "error");
        if (callback) callback(false);
        return;
    }

    // 使用功能码05（Write Single Coil）
    QModbusDataUnit writeUnit(QModbusDataUnit::Coils, address, 1);
    writeUnit.setValue(0, value); // 写入线圈时，1用0xFF00表示，0用0x0000表示

    QModbusReply *reply = m_modbusClient->sendWriteRequest(writeUnit, m_deviceAddress);
    if (!reply) {
        logMessage(QString("向PLC寄存器%1发送写入请求失败: %2").arg(address).arg(m_modbusClient->errorString()), "error");
        if (callback) callback(false);
        return;
    }

    // 设置一次性定时器处理超时
    QTimer *timeoutTimer = new QTimer(reply);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(3000); // 3秒超时，避免通信繁忙时误报

    connect(timeoutTimer, &QTimer::timeout, this, [this, reply, address, callback, timeoutTimer]() {
        timeoutTimer->deleteLater();
        logMessage(QString("写入PLC线圈寄存器超时，地址: %1").arg(address), "error");
        if (callback) callback(false);
        reply->deleteLater();
    });

    connect(reply, &QModbusReply::finished, this, [this, reply, address, value, callback, timeoutTimer]() {
        timeoutTimer->stop();
        timeoutTimer->deleteLater();

        bool success = false;
        if (reply->error() == QModbusDevice::NoError) {
            logMessage(QString("成功向PLC线圈寄存器%1写入%2").arg(address).arg(value ? 1 : 0), "success");
            success = true;
        } else {
            logMessage(QString("写入PLC线圈寄存器失败: %1, 地址: %2, 值: %3").arg(reply->errorString()).arg(address).arg(value ? 1 : 0), "error");
        }

        if (callback) callback(success);
        reply->deleteLater();
    });

    timeoutTimer->start();
}

bool PlcMonitor::writePlcCoilBlocking(quint16 address, int value){
    if (!m_modbusClient || !m_isConnected) {
        logMessage(QString("无法写入PLC线圈寄存器: PLC未连接，地址: %1, 值: %2").arg(address).arg(value ? 1 : 0), "error");
        return false;
    }

    QModbusDataUnit writeUnit(QModbusDataUnit::Coils, address, 1);
    writeUnit.setValue(0, value);

    QModbusReply *reply = m_modbusClient->sendWriteRequest(writeUnit, m_deviceAddress);
    if (!reply) {
        logMessage(QString("向PLC寄存器%1发送写入请求失败: %2").arg(address).arg(m_modbusClient->errorString()), "error");
        return false;
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    
    connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    timer.start(3000);
    loop.exec();

    bool success = false;
    if (timer.isActive()) {
        timer.stop();
        
        if (reply->error() == QModbusDevice::NoError) {
            logMessage(QString("成功向PLC线圈寄存器%1写入%2").arg(address).arg(value ? 1 : 0), "success");
            success = true;
        } else {
            logMessage(QString("写入PLC线圈寄存器失败: %1, 地址: %2, 值: %3").arg(reply->errorString()).arg(address).arg(value ? 1 : 0), "error");
        }
        
        reply->deleteLater();
    } else {
        logMessage(QString("写入PLC线圈寄存器超时，地址: %1").arg(address), "error");
        disconnect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
        reply->deleteLater();
    }

    return success;
}

void PlcMonitor::resetAirtightInstrument()
{
    // 向气密仪的寄存器地址9472发送值4864，复位气密仪
    bool success = writeAirtightRegister(9472, 4864);
    if (success) {
        logMessage("气密仪复位成功", "success");
    } else {
        logMessage("气密仪复位失败", "error");
    }
}

bool PlcMonitor::sendPressureToRegulator(const AirTightnessFullParams& params)
{
    // 检查调压装置连接状态（调压装置为可选设备，未连接时跳过，不影响主流程）
    qDebug() << "PlcMonitor::sendPressureToRegulator - 调压装置客户端指针:" << (void*)m_pressureModbusClient;
    qDebug() << "PlcMonitor::sendPressureToRegulator - 调压装置连接状态:" << m_isPressureConnected;
    
    if (!m_pressureModbusClient || !m_isPressureConnected) {
        logMessage("调压装置未连接，跳过压力值发送（可选功能）", "info");
        return true; // 返回true，不阻断主流程
    }
    
    // 准备压力值，将参数中的调压阀压力值写入到寄存器80
    quint16 pressureValue = static_cast<quint16>((params.pressure.regulatorPressure + 3) / 0.18);
    logMessage(QString("准备向调压装置发送压力值: %1").arg(pressureValue), "info");
    
    // 使用功能码06（Write Single Register）写入单个寄存器
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, 80, 1);
    writeUnit.setValue(0, pressureValue);
    
    // 发送写入请求到调压装置
    QModbusReply *reply = m_pressureModbusClient->sendWriteRequest(writeUnit, m_pressureDeviceAddress);
    if (!reply) {
        logMessage(QString("向调压装置发送压力值请求失败: %1").arg(m_pressureModbusClient->errorString()), "error");
        return false;
    }
    
    // 等待回复或超时
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    
    connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    timer.start(1000); // 1秒超时
    loop.exec();
    
    // 处理回复
    bool success = false;
    if (timer.isActive()) {
        // 正常收到回复
        timer.stop();
        
        if (reply->error() == QModbusDevice::NoError) {
            logMessage(QString("成功向调压装置发送压力值: %1").arg(pressureValue), "success");
            success = true;
        } else {
            logMessage(QString("向调压装置发送压力值失败: %1, 值: %2").arg(reply->errorString()).arg(pressureValue), "error");
        }
        
        reply->deleteLater();
    } else {
        // 超时
        logMessage(QString("向调压装置发送压力值超时，值: %1").arg(pressureValue), "error");
        disconnect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
        reply->deleteLater();
    }
    
    return success;
}


void PlcMonitor::clearChannelResults()
{
    if (!ui) return;
    const QString neutralStyle = "background-color: #bdc3c7; color: black; border: 1px solid #2196f3; padding: 8px; font-weight: bold;";
    ui->channel1Value->setText("");
    ui->channel2Value->setText("");
    ui->channel3Value->setText("");
    ui->channel1Value->setStyleSheet(neutralStyle);
    ui->channel2Value->setStyleSheet(neutralStyle);
    ui->channel3Value->setStyleSheet(neutralStyle);
}


void PlcMonitor::attemptStartAirtight()
{
    // 若未连接，等待下一轮定时器触发
    if (!m_airtightModbusClient || m_airtightModbusClient->state() != QModbusDevice::ConnectedState) {
        logMessage("气密仪未连接，等待重试", "warning");
        return;
    }

    bool success = writeAirtightRegister(9472, 4608);
    if (success) {
        logMessage("气密仪启动成功（重试）", "success");
        if (m_airtightStartRetryTimer) m_airtightStartRetryTimer->stop();
        m_airtightStartRetryActive = false;
    } else {
        logMessage("气密仪启动失败，继续重试", "warning");
        // 保持定时器运行，下一次触发将再次尝试
    }
}
