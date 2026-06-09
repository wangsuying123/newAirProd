#include "TcpServerController.h"
#include "plcmonitor.h"
#include "pressureunit.h"
#include "LeakUnitType.h"
#include "connect.h"
#include <QDebug>
#include <QString>
#include <QModbusClient>
#include <QModbusDataUnit>
#include <QModbusDevice>
#include <QEventLoop>
#include <QTimer>
#include "AirTightnessParamsDao.h"

TcpServerController::TcpServerController(QObject *parent)
    : QObject(parent),
      m_tcpServer(nullptr),
      m_plcMonitorPage(nullptr),
      m_connectPage(nullptr),
      m_dataCollectTimer(nullptr),
      m_currentProgramNumber(0),
      m_currentPressure(0.0),
      m_currentLeak(0.0),
      m_pressureUnit("Pa"),
      m_leakUnit("Pa/s"),
      m_testProcess("")
{
}

TcpServerController::~TcpServerController()
{
    // 清理资源
    if (m_tcpServer) {
        delete m_tcpServer;
        m_tcpServer = nullptr;
    }
    
    if (m_dataCollectTimer) {
        delete m_dataCollectTimer;
        m_dataCollectTimer = nullptr;
    }
}

// 初始化TCP服务器
void TcpServerController::initTcpServer()
{
    // 初始化TCP服务器
    m_tcpServer = new TcpServerManager(this);
    // 启动服务器（端口8888）
    m_tcpServer->startServer(8888);

    // 绑定远程指令接收信号（处理指令）
    connect(m_tcpServer, &TcpServerManager::sigRecvRemoteCmd, this, [=](const QString& cmdCode, const QJsonObject& params){
        // 根据指令码执行对应操作（示例）
        if (cmdCode == "start_test") {
            // 执行启动气密测试逻辑
            int timeout = params["timeout"].toInt(10);
            Q_UNUSED(timeout);
            float pressure = params["pressure_set"].toDouble(2.5);
            Q_UNUSED(pressure); // 标记变量为已使用，避免警告
            if (m_plcMonitorPage) {
                // 这里可以根据需要设置压力值并启动气密仪
                m_plcMonitorPage->attemptStartAirtight();
            }
        } else if (cmdCode == "stop_test") {
            // 停止测试的逻辑
            if (m_plcMonitorPage) {
                // 让气密仪复位就可以了
                m_plcMonitorPage->resetAirtightInstrument();
            }
        }
    });

    // 定时采集气密仪/PLC状态
    m_dataCollectTimer = new QTimer(this);
    connect(m_dataCollectTimer, &QTimer::timeout, this, &TcpServerController::onTimerCollectData);
    m_dataCollectTimer->start(1000); // 1秒采集一次
}

// 设置PLC监控页面指针
void TcpServerController::setPlcMonitorPage(PlcMonitor *page)
{
    m_plcMonitorPage = page;
}

// 设置连接页面指针
void TcpServerController::setConnectPage(ConnectWidget *page)
{
    m_connectPage = page;
}

// 获取TCP服务器实例
TcpServerManager *TcpServerController::tcpServer() const
{
    return m_tcpServer;
}

// 接收程序号变化信号槽函数
void TcpServerController::onInstrumentSelectionChanged(int programNumber)
{
    m_currentProgramNumber = programNumber;
    qDebug() << "TcpServerController: 收到程序号/仪器评选号: " << programNumber;
    
    // 根据当前程序号去数据库中进行查询出相应的参数值
    m_currentAirtightParams = AirTightnessParamsDao::getParamsByProgramNumber(m_currentProgramNumber);
    
    qDebug() << "TcpServerController: 获取到气密仪参数 - 填充时间:" << m_currentAirtightParams.cycleTime.fillTime 
             << " 稳定时间:" << m_currentAirtightParams.cycleTime.stabilizationTime
             << " 测试时间:" << m_currentAirtightParams.cycleTime.testTime
             << " 排气时间:" << m_currentAirtightParams.cycleTime.dumpTime
             << " 泄漏阈值:" << m_currentAirtightParams.leak.testReject
             << " 填充压力:" << m_currentAirtightParams.pressure.setFill
             << " 压力单位:" << PressureUnit::toString(PressureUnit::fromCode(m_currentAirtightParams.pressure.unitIndex))
             << " 泄漏单位:" << LeakUnitType::getUnitString(LeakUnitType::getUnitFromCode(m_currentAirtightParams.leak.leakUnitIndex, LeakUnitType::CodeType1))
             ;
    
    // 发送参数给所有连接的客户端
    if (m_tcpServer) {
        m_tcpServer->sendDeviceParams(QString::number(m_currentAirtightParams.programNumber),
                                     QString::number(m_currentAirtightParams.cycleTime.fillTime) + "s",
                                     QString::number(m_currentAirtightParams.cycleTime.stabilizationTime) + "s",
                                     QString::number(m_currentAirtightParams.cycleTime.testTime) + "s",
                                     QString::number(m_currentAirtightParams.cycleTime.dumpTime) + "s",
                                     QString::number(m_currentAirtightParams.leak.testReject) + LeakUnitType::getUnitString(LeakUnitType::getUnitFromCode(m_currentAirtightParams.leak.leakUnitIndex, LeakUnitType::CodeType1)),
                                     QString::number(m_currentAirtightParams.pressure.setFill) + PressureUnit::toString(PressureUnit::fromCode(m_currentAirtightParams.pressure.unitIndex)));
    }
}

// 接收通道1测试结果槽函数
void TcpServerController::onTestResultReady(const TestResult& result)
{
    qDebug() << "TcpServerController: 收到通道1测试结果 - 程序号:" << result.programNumber
             << " 压力:" << result.channelPressure << result.pressureUnit
             << " 泄漏:" << result.channelLeak << result.leakUnit
             << " 通过:" << result.isPassed << " 失败:" << result.isFailed;
    
    // 通过TCP服务器发送测试结果
    if (m_tcpServer) {
        m_tcpServer->sendTestResult(result);
    }
}

// 接收通道2测试结果槽函数
void TcpServerController::onTestResultReady2(const TestResult& result)
{
    qDebug() << "TcpServerController: 收到通道2测试结果 - 程序号:" << result.programNumber
             << " 压力:" << result.channelPressure << result.pressureUnit
             << " 泄漏:" << result.channelLeak << result.leakUnit
             << " 通过:" << result.isPassed << " 失败:" << result.isFailed;
    
    // 通过TCP服务器发送测试结果
    if (m_tcpServer) {
        m_tcpServer->sendTestResult(result);
    }
}

// 接收通道3测试结果槽函数
void TcpServerController::onTestResultReady3(const TestResult& result)
{
    qDebug() << "TcpServerController: 收到通道3测试结果 - 程序号:" << result.programNumber
             << " 压力:" << result.channelPressure << result.pressureUnit
             << " 泄漏:" << result.channelLeak << result.leakUnit
             << " 通过:" << result.isPassed << " 失败:" << result.isFailed;
    
    // 通过TCP服务器发送测试结果
    if (m_tcpServer) {
        m_tcpServer->sendTestResult(result);
    }
}

// 接收气密仪实时数据信号槽函数
void TcpServerController::onRealtimeDataUpdated(double pressure, double leak, const QString& pressureUnit, const QString& leakUnit, const QString& testProcess)
{
    // 更新实时数据存储
    m_currentPressure = pressure;
    m_currentLeak = leak;
    m_pressureUnit = pressureUnit;
    m_leakUnit = leakUnit;
    m_testProcess = testProcess;
}

// 定时采集气密仪/PLC状态槽函数
void TcpServerController::onTimerCollectData()
{
    // 采集错误信息
    QString errorMsg = ""; // 暂时返回空

    // 采集设备运行状态：读取PLC寄存器402的值（功能码03 - 保持寄存器）
    DeviceStatus status = Status_Idle; // 默认为空闲状态
    
    if (m_connectPage) {
        QModbusClient* plcClient = m_connectPage->getModbusClient(true); // 获取PLC Modbus客户端
        if (plcClient && plcClient->state() == QModbusClient::ConnectedState) {
            // 使用功能码03（QModbusDataUnit::HoldingRegisters）读取寄存器402
            QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, 402, 1);
            QModbusReply *reply = plcClient->sendReadRequest(readUnit, m_connectPage->getPlcSlaveId());
            if (reply) {
                // 等待回复
                QEventLoop loop;
                QTimer timer;
                timer.setSingleShot(true);
                connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
                connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
                timer.start(1000); // 1秒超时
                loop.exec();

                if (timer.isActive()) {
                    // 正常收到回复
                    timer.stop();
                    if (reply->error() == QModbusDevice::NoError) {
                        const QModbusDataUnit result = reply->result();
                        if (result.valueCount() > 0) {
                            quint16 registerValue = result.value(0);
                            // 根据寄存器402的值判断设备状态
                            status = (registerValue != 0) ? Status_Running : Status_Idle;
                        }
                    }
                    reply->deleteLater();
                } else {
                    // 超时
                    disconnect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
                    reply->deleteLater();
                }
            }
        }
    }
    
    // 推送状态到远程端
    if (m_tcpServer) {
        // 当前测试进程（默认"待机"）
        QString testProcess = "待机";
        
        m_tcpServer->sendDeviceStatus(status, m_currentPressure, m_pressureUnit, 
                                     m_currentLeak, m_leakUnit, 
                                     m_testProcess, errorMsg);
    }
}