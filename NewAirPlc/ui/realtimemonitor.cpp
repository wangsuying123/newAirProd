#include "realtimemonitor.h"
#include "ui_realtimemonitor.h"
#include <QModbusDataUnit>
#include <QDateTime>
#include <QMessageBox>
#include <QDebug>
#include <QTextCursor>
#include <QRandomGenerator>
#include <QIterator>
#include <QMetaObject>
#include <QFileDialog>
#include <QFile>
#include <QDir>
#include <QPointer>
#include <functional>
#include "PressureUnit.h"
#include "LeakUnitType.h"
#include "PLCStatusEnums.h"
#include "SystemSettingDao.h"
#include <QFileInfo>
#include "Logger.h"
#include <QTimer>
#include <QEventLoop>
#include "AirTightnessParamsDao.h"
#include "VolumeUnit.h"
#include <QLineEdit>
#include <QShowEvent>
#include <memory>


// 全局变量，用于存储当前程序号
int programNumber = 0;

RealtimeMonitor::RealtimeMonitor(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::RealtimeMonitor),
    modbusClient(nullptr),
    plcModbusClient(nullptr),
    slaveId(1),
    isMonitoring(false),
    isConnected(false),
    dataRefreshTimer(nullptr),
    plcPollTimer(nullptr),
    plcRequestBusy(false),
    plcPollCounter(0),
    lastCoil200Value(0),
    register8707Value(0)
{
    ui->setupUi(this);
    initUI();

    // MonitorDataDao现在使用静态方法，不需要实例化
}

RealtimeMonitor::~RealtimeMonitor() {
    // 停止定时器并清理资源
    stopPLCRegister519Check();
    stopPLCRegister200Check();
    if (plcPollTimer) {
        delete plcPollTimer;
        plcPollTimer = nullptr;
    }
    if (dataRefreshTimer) {
        delete dataRefreshTimer;
        dataRefreshTimer = nullptr;
    }
    if (reg9088Timer) {
        delete reg9088Timer;
        reg9088Timer = nullptr;
    }
    if (progressTimer) {
        delete progressTimer;
        progressTimer = nullptr;
    }
    // MonitorDataDao使用静态方法，不需要释放
    delete ui;
}

void RealtimeMonitor::setModbusClient(QModbusClient *client)
{
    if (modbusClient == client){
        return;
    }

    // 断开旧连接的信号槽
    if (modbusClient) {
        disconnect(modbusClient, &QModbusClient::stateChanged, this, nullptr);
    }

    modbusClient = client;

    if (modbusClient){
        // 使用lambda表达式适配参数，传递正确的isPlc值（false表示气密仪）
        connect(modbusClient, &QModbusClient::stateChanged, this, [this](QModbusDevice::State state) {
            updateConnectionStatus(false, state == QModbusClient::ConnectedState);
        });
        updateConnectionStatus(false, modbusClient->state() == QModbusClient::ConnectedState);
    } else {
        updateConnectionStatus(false, false);
    }
}

void RealtimeMonitor::setPlcModbusClient(QModbusClient *client)
{
    if (plcModbusClient == client){
        return;
    }

    // 断开旧连接的信号槽
    if (plcModbusClient) {
        disconnect(plcModbusClient, &QModbusClient::stateChanged, this, nullptr);
        // 停止旧连接的定时器（统一）
        stopPLCRegister519Check();
        stopPLCRegister200Check();
    }

    plcModbusClient = client;

    if (plcModbusClient){
        // 使用lambda表达式适配参数，传递正确的isPlc值（true表示PLC）
        connect(plcModbusClient, &QModbusClient::stateChanged, this, [this](QModbusDevice::State state) {
            updateConnectionStatus(true, state == QModbusClient::ConnectedState);
        });
        updateConnectionStatus(true, plcModbusClient->state() == QModbusClient::ConnectedState);
        
        // 如果新客户端已经处于连接状态，立即启动检测
        if (plcModbusClient->state() == QModbusDevice::ConnectedState) {
            // 合并后：统一启动
            startPLCRegister519Check();
            startPLCRegister200Check();
        }
    } else {
        updateConnectionStatus(true, false);
    }
}

void RealtimeMonitor::updateConnectionStatus(bool isPlc, bool connected)
{
    qDebug() << "RealtimeMonitor收到updateConnectionStatus：isPlc=" << isPlc << " connected=" << connected;

    if (isPlc) {
        // 处理PLC连接状态
        if (connected) {
            ui->plcConnectionStatusLabel->setText("PLC: 已连接");
            ui->plcConnectionStatusLabel->setStyleSheet(
                "font-weight: bold;\n"
                "color: green;\n"
                "background-color: #e8f5e8;\n"
                "padding: 4px 12px;\n"
                "border-radius: 12px;"
                );
            logMessage("PLC连接成功");
            // 合并后：统一启动PLC轮询
            startPLCRegister519Check();
            startPLCRegister200Check();
        } else {
            ui->plcConnectionStatusLabel->setText("PLC: 未连接");
            ui->plcConnectionStatusLabel->setStyleSheet(
                "font-weight: bold;\n"
                "color: red;\n"
                "background-color: #ffebee;\n"
                "padding: 4px 12px;\n"
                "border-radius: 12px;"
                );
            logMessage("PLC连接断开");
            // 合并后：统一停止PLC轮询
            stopPLCRegister519Check();
            stopPLCRegister200Check();
        }
    } else {
        // 处理气密仪连接状态
        // 先判断modbusClient是否有效
        if (!modbusClient) {
            ui->airConnectionStatusLabel->setText("气密仪: 未初始化");
            ui->airConnectionStatusLabel->setStyleSheet(
                "font-weight: bold;\n"
                "color: orange;\n"
                "background-color: #fff3e0;\n"
                "padding: 4px 12px;\n"
                "border-radius: 12px;"
                );
            logMessage("气密仪Modbus客户端未初始化", true);
            return;
        }

        isConnected = connected; // 更新连接状态标志
        if (connected) {
            ui->airConnectionStatusLabel->setText("气密仪: 已连接");
            ui->airConnectionStatusLabel->setStyleSheet(
                "font-weight: bold;\n"
                "color: green;\n"
                "background-color: #e8f5e8;\n"
                "padding: 4px 12px;\n"
                "border-radius: 12px;"
                );
            // 设备连接成功，自动开始监测
            if (!isMonitoring && modbusClient->state() == QModbusClient::ConnectedState) {
                isMonitoring = true;
                ui->refreshRateSpinBox->setEnabled(false);
                // 在开始监测前，确保测试结果显示为未开始状态
                ui->testResultValueLabel->setText("--");
                ui->testResultFrame->setStyleSheet(
                    "background-color: #f5f5f5;\n"
                    "border: 2px solid #9e9e9e;\n"
                    "border-radius: 8px;"
                    );
                ui->testResultValueLabel->setStyleSheet(
                    "font-weight: bold;\n"
                    "color: #616161;\n"
                    "font-size: 18px;"
                    );
                // 初始化通道1-3的压力/泄漏值
                ui->channel1PassPressureLabel->setText("0.00");
                ui->channel1PassLeakLabel->setText("0.000");
                ui->channel2PassPressureLabel->setText("0.00");
                ui->channel2PassLeakLabel->setText("0.000");
                ui->channel3PassPressureLabel->setText("0.00");
                ui->channel3PassLeakLabel->setText("0.000");
                // 初始化测试结果显示
                ui->testResultValueLabel->setText("--");
                ui->testResultFrame->setStyleSheet(
                    "background-color: #f5f5f5;\n"
                    "border: 2px solid #9e9e9e;\n"
                    "border-radius: 8px;"
                    );
                ui->testResultValueLabel->setStyleSheet(
                    "font-weight: bold;\n"
                    "color: #616161;\n"
                    "font-size: 18px;"
                    );
                readRealTimeData();
                dataRefreshTimer->start(ui->refreshRateSpinBox->value());
                // 启动9088高频轮询（100ms间隔）
                reg9088Timer->start(100);
                logMessage("开始实时监测");
            }
        } else {
            ui->airConnectionStatusLabel->setText("气密仪: 未连接");
            ui->airConnectionStatusLabel->setStyleSheet(
                "font-weight: bold;\n"
                "color: red;\n"
                "background-color: #ffebee;\n"
                "padding: 4px 12px;\n"
                "border-radius: 12px;"
                );
            if (isMonitoring) {
                // 停止监控
                isMonitoring = false;
                ui->refreshRateSpinBox->setEnabled(true);
                dataRefreshTimer->stop();
                // 停止9088高频轮询
                reg9088Timer->stop();
                logMessage("停止实时监测");
            }
            logMessage("气密仪连接已断开");
        }
    }
}

void RealtimeMonitor::updatePressureConnectionStatus(bool connected)
{
    qDebug() << "RealtimeMonitor收到updatePressureConnectionStatus：connected=" << connected;

    if (connected) {
        ui->pressureConnectionStatusLabel->setText("🟢 调压装置: 已连接");
        ui->pressureConnectionStatusLabel->setStyleSheet(
            "font-weight: 600;\n"
            "color: #16a34a;\n"
            "background-color: #dcfce7;\n"
            "padding: 8px 14px;\n"
            "border-radius: 16px;\n"
            "font-size: 13px;\n"
            "border: 2px solid #86efac;"
            );
        logMessage("调压装置连接成功");
    } else {
        ui->pressureConnectionStatusLabel->setText("🔴 调压装置: 未连接");
        ui->pressureConnectionStatusLabel->setStyleSheet(
            "font-weight: 600;\n"
            "color: #dc2626;\n"
            "background-color: #fef2f2;\n"
            "padding: 8px 14px;\n"
            "border-radius: 16px;\n"
            "font-size: 13px;\n"
            "border: 2px solid #fecaca;"
            );
        logMessage("调压装置连接断开");
    }
}

void RealtimeMonitor::setAirtightSlaveId(quint8 id)
{
    if (id >= 1 && id <= 255) {
        slaveId = id;
        logMessage(QString("气密仪从站地址已设置为: %1").arg(id), false);
    } else {
        logMessage(QString("无效的气密仪从站地址: %1，范围应为1-255").arg(id), true);
    }
}

void RealtimeMonitor::setCurrentUser(const User &user)
{
    // 更新当前用户显示
    QString userName = user.realName.isEmpty() ? user.username : user.realName;
    if (ui->currentUserLabel) {
        ui->currentUserLabel->setText(userName);
    }
}

void RealtimeMonitor::initUI()
{
    // 初始化定时器
    dataRefreshTimer = new QTimer(this);
    connect(dataRefreshTimer, &QTimer::timeout, this, &RealtimeMonitor::readRealTimeData);

    // 初始化9088高频轮询定时器（100ms间隔）
    reg9088Timer = new QTimer(this);
    connect(reg9088Timer, &QTimer::timeout, this, &RealtimeMonitor::readRegister9088);
    
    // 初始化进度条更新定时器（100ms间隔）
    progressTimer = new QTimer(this);
    connect(progressTimer, &QTimer::timeout, this, &RealtimeMonitor::updateProgressBar);
    progressTimer->start(100);
    
    // 启动阶段计时器，确保elapsed()返回有效值
    phaseElapsedTimer.start();

    // 设置默认刷新间隔为500ms（避免请求堆积）
    ui->refreshRateSpinBox->setValue(500); // 默认500毫秒

    // 初始化状态标签
    ui->airConnectionStatusLabel->setText("气密仪: 未连接");
    ui->airConnectionStatusLabel->setStyleSheet(
        "font-weight: bold;\n"
        "color: red;\n"
        "background-color: #ffebee;\n"
        "padding: 4px 12px;\n"
        "border-radius: 12px;"
        );
    
    ui->plcConnectionStatusLabel->setText("PLC: 未连接");
    ui->plcConnectionStatusLabel->setStyleSheet(
        "font-weight: bold;\n"
        "color: red;\n"
        "background-color: #ffebee;\n"
        "padding: 4px 12px;\n"
        "border-radius: 12px;"
        );

    // 初始化数据显示
    ui->pressureValueLabel->setText("0.00");
    ui->leakValueLabel->setText("0.00");

    // 初始化测试结果为未开始状态
    ui->testResultValueLabel->setText("--");
    ui->testResultFrame->setStyleSheet(
        "background-color: #f5f5f5;\n"
        "border: 2px solid #9e9e9e;\n"
        "border-radius: 8px;"
        );
    ui->testResultValueLabel->setStyleSheet(
        "font-weight: bold;\n"
        "color: #616161;\n"
        "font-size: 20px;"
        );

    // 新增：产品编号输入框的基本设置与信号连接
    if (auto lineEdit = this->findChild<QLineEdit*>("productNumberLineEdit")) {
        lineEdit->setMaxLength(128);
        lineEdit->setClearButtonEnabled(true);
        // 初始化时清空输入框，避免扫码时与默认值拼接
        lineEdit->clear();
        // 扫码枪通常回车结束，使用returnPressed捕获
        connect(lineEdit, &QLineEdit::returnPressed, this, &RealtimeMonitor::onProductNumberEntered);
        // 页面加载后即聚焦，方便直接扫码
        lineEdit->setFocus();
        QTimer::singleShot(0, this, [this]() {
            if (auto le = this->findChild<QLineEdit*>("productNumberLineEdit")) {
                le->selectAll();
            }
        });
    }

    // 是否扫码复选框：勾选写1，取消写0，PLC寄存器49417，功能码05（Coils）
    connect(ui->scanBarcodeCheckBox, &QCheckBox::stateChanged, this, [this](int state) {
        if (!plcModbusClient || plcModbusClient->state() != QModbusDevice::ConnectedState) {
            logMessage("PLC未连接，无法写入扫码状态", true);
            return;
        }
        quint16 value = (state == Qt::Checked) ? 1 : 0;

        // 取消勾选时清空产品编号
        if (value == 0) {
            productCode.clear();
            ui->productNumberLineEdit->clear();
            if (auto label = this->findChild<QLabel*>("currentProductNumberLabel")) {
                label->setText("");
            }
        }

        QModbusDataUnit writeUnit(QModbusDataUnit::Coils, 49417, 1);
        writeUnit.setValue(0, value);
        if (auto *reply = plcModbusClient->sendWriteRequest(writeUnit, 2)) {
            if (!reply->isFinished()) {
                connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            } else {
                delete reply;
            }
            logMessage(QString("写入扫码状态到PLC寄存器49417: %1").arg(value));
        } else {
            logMessage("写入PLC寄存器49417失败: " + plcModbusClient->errorString(), true);
        }
    });

}// 更新进程状态显示
enum ProcessStatus {
    STANDBY = 0,
    FILL = 256,
    STB = 512,
    TEST = 768,
    DUMP = 1024
};

void RealtimeMonitor::updateTestStatus(int processValue)
{
    QString processName;
    QString frameStyle;
    QString labelStyle;
    QString valueStyle;

    // 当检测到充气、保压或测试状态时，标记测试已真正开始
    // 这样可以确保只有经历过这些状态后，才会在排气状态时处理结果
    if (processValue == FILL || processValue == STB || processValue == TEST) {
        if (!testActuallyStarted) {
            testActuallyStarted = true;
            logMessage(QString("检测到测试状态 %1，标记测试已真正开始").arg(processValue));
        }
    }

    // 根据processValue设置不同的状态名称和样式
    switch (processValue) {
    case STANDBY: {
        processName = "待机";
        frameStyle = "background-color: #1976d2;\n"
                     "border: 2px solid #1565c0;\n"
                     "border-radius: 8px;";
        labelStyle = "font-weight: bold;\n"
                     "color: white;\n"
                     "font-size: 16px;";
        valueStyle = "font-weight: normal;\n"
                     "color: #e3f2fd;\n"
                     "font-size: 12px;";
        // 当处于待机状态时，重置测试结果为初始状态
        // 临时保存isMonitoring状态
        bool tempMonitoringState = isMonitoring;
        // 设置为false以确保进入初始状态分支
        isMonitoring = false;
        updateTestResult(false);
        // 恢复原始状态
        isMonitoring = tempMonitoringState;
        break;
    }
    case FILL:
        processName = "充气";
        frameStyle = "background-color: #1976d2;\n"
                     "border: 2px solid #1565c0;\n"
                     "border-radius: 8px;";
        labelStyle = "font-weight: bold;\n"
                     "color: white;\n"
                     "font-size: 16px;";
        valueStyle = "font-weight: normal;\n"
                     "color: #e3f2fd;\n"
                     "font-size: 12px;";
        break;
    case STB:
        processName = "保压";
        frameStyle = "background-color: #f57c00;\n"
                     "border: 2px solid #ef6c00;\n"
                     "border-radius: 8px;";
        labelStyle = "font-weight: bold;\n"
                     "color: white;\n"
                     "font-size: 16px;";
        valueStyle = "font-weight: normal;\n"
                     "color: #fff3e0;\n"
                     "font-size: 12px;";
        break;
    case TEST:
        processName = "测试";
        frameStyle = "background-color: #0097a7;\n"
                     "border: 2px solid #00838f;\n"
                     "border-radius: 8px;";
        labelStyle = "font-weight: bold;\n"
                     "color: white;\n"
                     "font-size: 16px;";
        valueStyle = "font-weight: normal;\n"
                     "color: #e0f7fa;\n"
                     "font-size: 12px;";
        break;
    case DUMP:
        processName = "排气";
        frameStyle = "background-color: #546e7a;\n"
                     "border: 2px solid #455a64;\n"
                     "border-radius: 8px;";
        labelStyle = "font-weight: bold;\n"
                     "color: white;\n"
                     "font-size: 16px;";
        valueStyle = "font-weight: normal;\n"
                     "color: #eceff1;\n"
                     "font-size: 12px;";
        break;
    default:
        processName = "未知";
        frameStyle = "background-color: #9e9e9e;\n"
                     "border: 2px solid #757575;\n"
                     "border-radius: 8px;";
        labelStyle = "font-weight: bold;\n"
                     "color: white;\n"
                     "font-size: 16px;";
        valueStyle = "font-weight: normal;\n"
                     "color: #f5f5f5;\n"
                     "font-size: 12px;";
        break;
    }

    // 更新当前测试进程显示
    ui->currentProcessFrame->setStyleSheet(frameStyle);
    ui->currentProcessLabel->setText(processName);
    ui->currentProcessLabel->setStyleSheet(labelStyle);
    
    // 检测阶段变化，重置阶段计时器
    if (processValue != lastPhase) {
        lastPhase = processValue;
        currentPhase = processValue;
        phaseElapsedTimer.restart();
    }
}

// 更新进度条（根据时间逐渐增加）
void RealtimeMonitor::updateProgressBar() {
    // 计算总时间和各阶段的进度范围
    // 待机: 0%, 充气: 0-25%, 保压: 25-50%, 测试: 50-75%, 排气: 75-100%
    double elapsedSec = phaseElapsedTimer.elapsed() / 1000.0;
    int progressValue = 0;
    QString progressStyle;
    
    switch (currentPhase) {
    case STANDBY:
        progressValue = 0;
        lastProgressValue = 0; // 待机时重置进度
        break;
    case FILL:
        if (fillTime > 0) {
            double ratio = qMin(elapsedSec / fillTime, 1.0);
            progressValue = static_cast<int>(ratio * 25);
        } else {
            progressValue = 25;
        }
        break;
    case STB:
        if (stabilizeTime > 0) {
            double ratio = qMin(elapsedSec / stabilizeTime, 1.0);
            progressValue = 25 + static_cast<int>(ratio * 25);
        } else {
            progressValue = 50;
        }
        break;
    case TEST:
        if (testTime > 0) {
            double ratio = qMin(elapsedSec / testTime, 1.0);
            progressValue = 50 + static_cast<int>(ratio * 25);
        } else {
            progressValue = 75;
        }
        break;
    case DUMP:
        if (dumpTime > 0) {
            double ratio = qMin(elapsedSec / dumpTime, 1.0);
            progressValue = 75 + static_cast<int>(ratio * 25);
        } else {
            progressValue = 100;
        }
        break;
    default:
        progressValue = 0;
        break;
    }
    
    // 确保进度只增不减（除了待机状态）
    if (currentPhase != STANDBY && progressValue < lastProgressValue) {
        progressValue = lastProgressValue;
    }
    
    // 更新上一次的进度值
    lastProgressValue = progressValue;
    
    ui->testProgressBar->setValue(progressValue);
    
    // 根据进度更新进度条颜色
    if (progressValue >= 75) {
        progressStyle = "QProgressBar { border: 2px solid #cbd5e1; border-radius: 12px; background-color: #e2e8f0; text-align: center; } "
                       "QProgressBar::chunk { border-radius: 10px; background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #22c55e, stop:1 #4ade80); }";
    } else if (progressValue >= 25) {
        progressStyle = "QProgressBar { border: 2px solid #cbd5e1; border-radius: 12px; background-color: #e2e8f0; text-align: center; } "
                       "QProgressBar::chunk { border-radius: 10px; background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #f97316, stop:1 #fb923c); }";
    } else {
        progressStyle = "QProgressBar { border: 2px solid #cbd5e1; border-radius: 12px; background-color: #e2e8f0; text-align: center; } "
                       "QProgressBar::chunk { border-radius: 10px; background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3b82f6, stop:1 #60a5fa); }";
    }
    ui->testProgressBar->setStyleSheet(progressStyle);
}

// 更新测试结果显示
void RealtimeMonitor::updateTestResult(bool isPass){
    // 只有当测试进程为排气（DUMP=1024）时才显示测试结果
    if (register8707Value != DUMP) {
        // 非排气状态，保持初始显示
        ui->testResultValueLabel->setText("-");
        ui->testResultFrame->setStyleSheet(
            "background-color: #f5f5f5;\n"
            "border: 2px solid #bdbdbd;\n"
            "border-radius: 8px;"
            );
        ui->testResultValueLabel->setStyleSheet(
            "font-weight: bold;\n"
            "color: #616161;\n"
            "font-size: 18px;"
            );
        return;
    }
    
    // 根据测试结果更新显示
    if (isPass) {
        // 测试通过
        ui->testResultValueLabel->setText("通过");
        ui->testResultFrame->setStyleSheet(
            "background-color: #e8f5e9;\n"
            "border: 2px solid #4caf50;\n"
            "border-radius: 8px;"
            );
        ui->testResultValueLabel->setStyleSheet(
        "font-weight: bold;\n"
        "color: #2e7d32;\n"
        "font-size: 20px;"
        );
    } else if (!isPass && isMonitoring) {
        // 测试失败
        ui->testResultValueLabel->setText("失败");
        ui->testResultFrame->setStyleSheet(
            "background-color: #ffebee;\n"
            "border: 2px solid #f44336;\n"
            "border-radius: 8px;"
            );
        ui->testResultValueLabel->setStyleSheet(
        "font-weight: bold;\n"
        "color: #c62828;\n"
        "font-size: 20px;"
        );
    } else {
        // 初始状态或待机状态，重置测试结果显示
        ui->testResultValueLabel->setText("-");
        ui->testResultFrame->setStyleSheet(
            "background-color: #f5f5f5;\n"
            "border: 2px solid #bdbdbd;\n"
            "border-radius: 8px;"
            );
        ui->testResultValueLabel->setStyleSheet(
            "font-weight: bold;\n"
            "color: #616161;\n"
            "font-size: 18px;"
            );
    }
}

void RealtimeMonitor::readRealTimeData()
{
    // 检查modbusClient是否为空，防止崩溃
    if (!modbusClient) {
        logMessage("Modbus客户端未初始化，无法读取数据", true);
        return;
    }

    if (!isConnected) {
        logMessage("未连接到设备", true);
        return;
    }

    // 防止请求堆积：如果上一次请求还未完成，跳过本次
    if (airtightRequestBusy) {
        return;
    }
    airtightRequestBusy = true;

    // 只读取主要数据（寄存器8705-8717），9088由独立定时器高频读取
    readDeviceDataAsync(8705, 13, QModbusDataUnit::HoldingRegisters,
                        [this](bool success, const QModbusDataUnit &data) {
                            if (success) {
                                processMainRegisterData(data);
                            } else {
                                logMessage("无法读取设备数据：读取主要寄存器失败", true);
                                QMetaObject::invokeMethod(this, [this]() {
                                    ui->airConnectionStatusLabel->setStyleSheet(
                                        "font-weight: bold;\n"
                                        "color: green;\n"
                                        "background-color: #e8f5e9;\n"
                                        "padding: 4px 12px;\n"
                                        "border-radius: 12px;"
                                        );
                                    ui->timestampLabel->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
                                }, Qt::QueuedConnection);
                            }
                            airtightRequestBusy = false;
                        });
}

// 高频读取寄存器9088，检查是否需要读取测试结果
void RealtimeMonitor::readRegister9088()
{
    // 增加空指针检查和连接状态验证
    if (!modbusClient) {
        logMessage("readRegister9088: Modbus客户端未初始化", true);
        return;
    }
    
    if (!isConnected || modbusClient->state() != QModbusDevice::ConnectedState) {
        return;
    }
    
    // 检查测试是否真正开始：必须经历过非排气状态（充气/保压/测试）
    // 这样可以确保不会读取到上一次测试的旧结果
    if (!testActuallyStarted) {
        return;
    }
    
    // 只有当测试进程为排气（DUMP=1024）时才读取测试结果
    if (register8707Value != DUMP) {
        return;
    }
    
    // 如果当前测试周期已经处理过结果，不再重复处理
    if (currentTestResultProcessed) {
        return;
    }
    
    // 防止请求堆积
    if (reg9088RequestBusy) {
        return;
    }
    reg9088RequestBusy = true;
    
    readDeviceDataAsync(9088, 1, QModbusDataUnit::HoldingRegisters,
        [this](bool success, const QModbusDataUnit &data) {
            // 增加数据有效性检查
            if (success && data.valueCount() > 0 && data.value(0) == 256) {
                // 9088=256时，读取8961-8972寄存器
                readDeviceDataAsync(8961, 12, QModbusDataUnit::HoldingRegisters,
                    [this](bool success, const QModbusDataUnit &data) {
                        if (success && data.valueCount() >= 12) {
                            processTestResultData(data);
                            emit testResultRegisterDataReady(data);
                            // 标记当前测试周期已处理过结果
                            currentTestResultProcessed = true;
                            logMessage("测试结果已处理，标记当前测试周期完成");
                        } else {
                            if (!success) {
                                logMessage("readRegister9088: 读取测试结果寄存器失败", true);
                            } else {
                                logMessage(QString("readRegister9088: 测试结果数据不足，期望12个寄存器，收到%1个").arg(data.valueCount()), true);
                            }
                        }
                        reg9088RequestBusy = false;
                    });
            } else {
                if (!success) {
                    logMessage("readRegister9088: 读取寄存器9088失败", true);
                }
                reg9088RequestBusy = false;
            }
        });
}

void RealtimeMonitor::processTestResultData(const QModbusDataUnit &data)
{
    // 使用主线程更新UI
    // 现在数据来自8961-8972寄存器（12个寄存器）
    QMetaObject::invokeMethod(this, [this, data]() {
        if (data.valueCount() >= 12) {
            // 8962：报警信息（索引1，因为起始地址是8961）
            quint16 alarmInfoCode = data.value(1);
            
            // 根据报警信息设置测试结果
            if (alarmInfoCode == 0) {
                // 值为0，表示测试通过
                updateTestResult(true);
            } else if (alarmInfoCode == 4864) {
                // 值为4864，表示压力过高，测试不通过
                updateTestResult(false);
            } else if (alarmInfoCode == 5120) {
                // 值为5120，表示压力过低，测试不通过
                updateTestResult(false);
            }
            
            // 当8707等于1024时，根据报警信息向PLC寄存器写入数据
            // 使用全局保存的register8707Value成员变量
            if (this->register8707Value == 1024) {
                if (alarmInfoCode != 0) {
                    // 当报警码不等于0时，向PLC寄存器104写1，使用功能码05
                    if (plcModbusClient && plcModbusClient->state() == QModbusDevice::ConnectedState) {
                        quint16 plcSlaveId = 2; // 从站地址2
                        
                        // 向寄存器104发送1
                        QModbusDataUnit writeUnit104(QModbusDataUnit::Coils, 104, 1);
                        writeUnit104.setValue(0, 1);
                        if (auto *reply = plcModbusClient->sendWriteRequest(writeUnit104, plcSlaveId)) {
                            if (!reply->isFinished()) {
                                connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
                                logMessage("向PLC寄存器104发送1成功");
                                // 发送信号给debugpage更新警告按钮状态
                                emit warningButtonStateChanged(1);
                            } else {
                                delete reply;
                                logMessage("向PLC寄存器104发送1失败", true);
                            }
                        } else {
                            logMessage("向PLC寄存器104发送写入请求失败", true);
                        }
                        
                    } else {
                        logMessage("PLC未连接，无法发送命令", true);
                    }
                }
            }
        } else {
            logMessage(QString("读取测试结果数据失败：返回值数量不足，只收到 %1 个值").arg(data.valueCount()), true);
        }
    }, Qt::QueuedConnection);
}

void RealtimeMonitor::on_refreshRateSpinBox_valueChanged(int value)
{
    if (isMonitoring) {
        dataRefreshTimer->setInterval(value);
        logMessage(QString("刷新间隔已调整为: %1 ms").arg(value));
    }
}

void RealtimeMonitor::readDeviceDataAsync(quint16 address, quint16 count, QModbusDataUnit::RegisterType type, const std::function<void(bool, const QModbusDataUnit&)>& callback)
{
    // 检查modbusClient是否为空
    if (!modbusClient) {
        logMessage(QString("无法读取设备数据：Modbus客户端未初始化，地址: %1").arg(address), true);
        callback(false, QModbusDataUnit());
        return;
    }

    // 检查连接状态 - 添加更稳健的检查，避免因临时状态变化导致连接中断
    if (modbusClient->state() != QModbusDevice::ConnectedState) {
        // 只在状态真正变化时更新UI和日志
        if (isConnected) {
            isConnected = false;
            updateConnectionStatus(false, false);
            logMessage(QString("设备连接已断开，无法读取数据，地址: %1").arg(address), true);
        }
        callback(false, QModbusDataUnit());
        return;
    }

    // 使用功能码03（Read Holding Registers）读取寄存器
    QModbusDataUnit readUnit(type, address, count);

    // 发送读取请求
    QModbusReply *reply = modbusClient->sendReadRequest(readUnit, static_cast<quint8>(slaveId));
    if (!reply) {
        logMessage(QString("发送读取请求失败: %1, 地址: %2, 从站: %3")
                   .arg(modbusClient->errorString()).arg(address).arg(slaveId), true);
        callback(false, QModbusDataUnit());
        return;
    }

    // 设置一次性定时器处理超时
    QTimer *timeoutTimer = new QTimer(reply);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(2000); // 2秒超时，避免通信繁忙时误报
    
    // 使用共享标志确保callback只被调用一次
    auto callbackCalled = std::make_shared<bool>(false);

    connect(timeoutTimer, &QTimer::timeout, this, [reply, address, callback, timeoutTimer, callbackCalled]() {
        if (*callbackCalled) {
            return;
        }
        *callbackCalled = true;
        qDebug() << "读取数据超时，地址: " << address;
        callback(false, QModbusDataUnit());
        // reply会自动被父对象清理
    });

    connect(reply, &QModbusReply::finished, this, [this, reply, address, callback, timeoutTimer, callbackCalled]() {
        timeoutTimer->stop();
        if (*callbackCalled) {
            return;
        }
        *callbackCalled = true;

        // 处理回复
        if (reply->error() == QModbusDevice::NoError) {
            const QModbusDataUnit result = reply->result();
            if (result.valueCount() > 0) {
                callback(true, result);
            } else {
                logMessage(QString("读取数据失败：无返回值，地址: %1").arg(address), true);
                callback(false, QModbusDataUnit());
            }
        } else {
            logMessage(QString("读取数据失败: %1, 地址: %2").arg(reply->errorString()).arg(address), true);
            callback(false, QModbusDataUnit());
        }
    });

    timeoutTimer->start();
}

void RealtimeMonitor::processMainRegisterData(const QModbusDataUnit &data)
{
    // 使用主线程更新UI
    QMetaObject::invokeMethod(this, [this, data]() {
        if (data.valueCount() < 13) {
            logMessage(QString("处理数据失败：返回值数量不足，只收到 %1 个值").arg(data.valueCount()), true);
            return;
        }

        // 从寄存器读取数据
        quint16 deviceStatus = 0;
        quint16 processValue = 0;
        quint16 pressureValue = 0;
        quint16 pressureHighValue = 0; // 高16位（寄存器8709）
        quint16 pressureUnit = 0;
        quint16 leakValue = 0;
        quint16 leakValue2 = 0; // 8712 第二寄存器
        quint16 leakUnit = 0;
        quint16 atmPressureValue = 0;
        quint16 atmPressureUnit = 0;
        quint16 temperatureValue = 0;
        quint16 temperatureUnit = 0;

        // 记录每个参数的读取状态
        bool deviceStatusRead = false;
        bool processValueRead = false;
        bool pressureValueRead = false;
        bool pressureHighValueRead = false; // 新增：压力高位读取状态
        bool pressureUnitRead = false;
        bool leakValueRead = false;
        bool leakValue2Read = false; // 8712 第二寄存器读取状态
        bool leakUnitRead = false;
        bool atmPressureValueRead = false;
        bool atmPressureUnitRead = false;
        bool temperatureValueRead = false;
        bool temperatureUnitRead = false;
        bool allReadSuccess = true;

        // 提取各个寄存器的值
        try {
            if (data.valueCount() > 1) { deviceStatus = data.value(1); deviceStatusRead = true; } // 设备状态 - 地址8706
            if (data.valueCount() > 2) { processValue = data.value(2); this->register8707Value = processValue; processValueRead = true; } // 测试进程 - 地址8707，同时保存到全局变量
            if (data.valueCount() > 3) { pressureValue = data.value(3); pressureValueRead = true; } // 压力低位编码 - 地址8708
            if (data.valueCount() > 4) { pressureHighValue = data.value(4); pressureHighValueRead = true; } // 压力高位编码 - 地址8709
            if (data.valueCount() > 5) { pressureUnit = data.value(5); pressureUnitRead = true; } // Pressure单位 - 地址8710
            if (data.valueCount() > 6) { leakValue = data.value(6); leakValueRead = true; } // 泄露值 - 地址8711
            if (data.valueCount() > 7) { leakValue2 = data.value(7); leakValue2Read = true; } // 泄露值第二寄存器 - 地址8712
            if (data.valueCount() > 8) { leakUnit = data.value(8); leakUnitRead = true; } // Leak单位 - 地址8713
            if (data.valueCount() > 9) { atmPressureValue = data.value(9); atmPressureValueRead = true; } // 实时标准大气压 - 地址8714
            if (data.valueCount() > 10) { atmPressureUnit = data.value(10); atmPressureUnitRead = true; } // 单位 - 地址8715
            if (data.valueCount() > 11) { temperatureValue = data.value(11); temperatureValueRead = true; } // 实时温度 - 地址8716
            if (data.valueCount() > 12) { temperatureUnit = data.value(12); temperatureUnitRead = true; } // 单位 - 地址8717
        } catch (const std::exception &e) {
            logMessage(QString("提取寄存器值时发生异常: %1").arg(e.what()), true);
        }

        // 检查是否所有必要的参数都读取成功
        allReadSuccess = deviceStatusRead && processValueRead && pressureValueRead &&
                         pressureHighValueRead && pressureUnitRead && leakValueRead && leakValue2Read && leakUnitRead &&
                         atmPressureValueRead && atmPressureUnitRead &&
                         temperatureValueRead && temperatureUnitRead;

        // 初始化默认值，防止读取失败时变量未定义
        double pressure = 0.0;
        double leak = 0.0;
        QString pressureUnitStr = "KPa"; // 默认压力单位
        QString leakUnitStr = "cc/min"; // 默认泄漏单位

        // 只有当读取成功时，才转换寄存器值并更新UI
            if (allReadSuccess) {
                // 将寄存器值转换为实际数值
                // 1. 压力值参数解码：低位(8708)和高位(8709)分别字节交换，根据高位是否为0选择缩放因子
                auto swap16 = [](quint16 v) -> quint16 { return static_cast<quint16>((v << 8) | (v >> 8)); };
                quint16 lowOrig = swap16(pressureValue);      // 8708 低位原始值
                quint16 highOrig = swap16(pressureHighValue); // 8709 高位原始值
                // 合成32位值后按1000缩放
                quint32 raw = (static_cast<quint32>(highOrig) << 16) | static_cast<quint32>(lowOrig);
                
                // 压力值范围校验：防止通信异常（如寄存器全为0xFFFF）导致显示异常大值
                double tempPressure = static_cast<double>(raw) / 1000.0;
                if (tempPressure > MAX_PRESSURE_KPA || tempPressure < 0) {
                    // 异常值：使用上次有效压力值，避免UI跳变
                    logMessage(QString("检测到异常压力值: %1 KPa (原始值: 0x%2)，使用上次有效值 %3 KPa")
                               .arg(tempPressure).arg(raw, 0, 16).arg(lastValidPressure), true);
                    pressure = lastValidPressure;
                } else {
                    // 有效值：更新缓存并使用
                    pressure = tempPressure;
                    lastValidPressure = pressure;
                }

                // 温度值处理 - 使用公式 V=31.4853515625+R/25600（用于后续扩展）
                Q_UNUSED(temperatureValue);
                // 大气压值处理 - 使用公式 V=1006.0746484375+R/25600（用于后续扩展）
                Q_UNUSED(atmPressureValue);

                // 1. 压力单位 - 使用PressureUnit类中的辅助方法
                PressureUnit::Unit pressureUnitEnum = PressureUnit::fromCode(pressureUnit);
                pressureUnitStr = PressureUnit::toString(pressureUnitEnum);

                // 2. 泄漏单位 - 使用LeakUnitType类中的辅助方法
                LeakUnitType::Unit leakUnitEnum = LeakUnitType::getUnitFromCode(leakUnit, LeakUnitType::CodeType1);
                leakUnitStr = LeakUnitType::getUnitString(leakUnitEnum);

                // 3. 泄漏值处理 - 使用两个寄存器进行字节交换后相减，根据单位选择缩放因子
                quint16 r1 = leakValue;   // 寄存器8711
                quint16 r2 = leakValue2;  // 寄存器8712
                quint16 r1Swapped = swap16(r1);
                quint16 r2Swapped = swap16(r2);
                // 根据泄漏单位选择缩放因子：Pa单位使用10，其他单位使用1000
                double leakScale = (leakUnitEnum == LeakUnitType::Pa) ? 10.0 : 1000.0;
                leak = static_cast<double>(static_cast<int32_t>(r1Swapped) - static_cast<int32_t>(r2Swapped)) / leakScale;

                // 更新UI显示
                ui->pressureValueLabel->setText(QString::number(pressure, 'f', 2));
                ui->leakValueLabel->setText(QString::number(leak, 'f', 3));
                
                // 更新UI显示单位
                ui->pressureUnitLabel->setText(pressureUnitStr);
                ui->leakUnitLabel->setText(leakUnitStr);
            }

        // 更新连接状态显示 - 只在状态变化时更新UI，避免频繁刷新导致的闪烁
        bool newConnectionState = (modbusClient && modbusClient->state() == QModbusDevice::ConnectedState && allReadSuccess);
        if (newConnectionState != isConnected) {
            isConnected = newConnectionState;
            updateConnectionStatus(false, isConnected);
        }

        // 设置泄漏值的样式
        ui->leakValueLabel->setStyleSheet("font-size: 20pt; font-weight: bold; color: #f57c00; background-color: #fff3e0; border: 1px solid #ffcc80; border-radius: 6px;");

        // 更新测试进程状态
        if (processValueRead) {
            updateTestStatus(processValue);
        }
        
        // 如果读取失败，记录错误但不使用默认值，保持上次成功读取的值
        if (!allReadSuccess) {
            // 记录具体哪些参数读取失败（包含寄存器地址）
            QStringList failedParams;
            if (!deviceStatusRead) failedParams.append("设备状态 (寄存器8706)");
            if (!processValueRead) failedParams.append("测试进程 (寄存器8707)");
            if (!pressureValueRead) failedParams.append("压力值 (寄存器8708)");
            if (!pressureUnitRead) failedParams.append("压力单位 (寄存器8710)");
            if (!leakValueRead) failedParams.append("泄露值 (寄存器8711)");
            if (!leakValue2Read) failedParams.append("泄露值2 (寄存器8712)");
            if (!leakUnitRead) failedParams.append("泄漏单位 (寄存器8713)");
            if (!atmPressureValueRead) failedParams.append("标准大气压 (寄存器8714)");
            if (!atmPressureUnitRead) failedParams.append("大气压单位 (寄存器8715)");
            if (!temperatureValueRead) failedParams.append("温度值 (寄存器8716)");
            if (!temperatureUnitRead) failedParams.append("温度单位 (寄存器8717)");

            logMessage(QString("以下参数读取失败，保持上次读取的值: %1").arg(failedParams.join(", ")), true);
            
            // 只有在processValue读取成功时才更新测试进程状态
            if (!processValueRead) {
                return; // 如果测试进程状态也读取失败，则直接返回，不更新任何UI
            }
        }

        // 输出寄存器8707的原始值到日志
        //logMessage(QString("寄存器8707原始值: %1").arg(processValue));

        // 更新测试进程UI
        updateTestStatus(processValue);

        // 更新时间戳
        ui->timestampLabel->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));

        // 初始化状态字符串，确保在所有情况下都有定义
        QString statusStr = "Unknown"; // 默认状态
        
        // 将processValue转换为测试进程字符串
        QString processName;
        switch (processValue) {
        case STANDBY: processName = "待机"; break;
        case FILL: processName = "充气"; break;
        case STB: processName = "保压"; break;
        case TEST: processName = "测试"; break;
        case DUMP: processName = "排气"; break;
        default: processName = "未知"; break;
        }
        
        // 处理寄存器8706的值，写入PLC寄存器100-103
        if (allReadSuccess && isConnected) {
            // 初始化PLC寄存器值
            quint16 plcRegister100 = 0; // 仪器实时OK
            quint16 plcRegister101 = 0; // 仪器实时RUN
            quint16 plcRegister102 = 0; // 仪器实时TD
            quint16 plcRegister103 = 0; // 仪器实时RD

            // 根据deviceStatus（寄存器8706的值）设置对应的PLC寄存器
            switch(deviceStatus) {
            case 3328: // 仪器实时OK
                plcRegister100 = 1;
                break;
            case 3584: // 仪器实时TD
                plcRegister102 = 1;
                break;
            case 3840: // 仪器实时RD
                plcRegister103 = 1;
                break;
            default:
                // 其他值，所有寄存器设为0
                break;
            }

            if(processValue == 0){
                // 仪器实时RUN
                plcRegister101 = 1;
            }
            // 使用功能码05（Write Single Coil）逐个写入PLC寄存器
            // 检查PLC连接是否有效
            if (!plcModbusClient || plcModbusClient->state() != QModbusDevice::ConnectedState) {
                logMessage(tr("PLC连接无效，无法写入寄存器"), true);
                return;
            }

            // 逐个写入寄存器100-103
            QModbusDataUnit writeUnit100(QModbusDataUnit::Coils, 100, 1);
            writeUnit100.setValue(0, plcRegister100);
            QModbusReply *reply100 = plcModbusClient->sendWriteRequest(writeUnit100, 2);
            if (reply100) {
                connect(reply100, &QModbusReply::finished, reply100, &QModbusReply::deleteLater);
            }

            QModbusDataUnit writeUnit101(QModbusDataUnit::Coils, 101, 1);
            writeUnit101.setValue(0, plcRegister101);
            QModbusReply *reply101 = plcModbusClient->sendWriteRequest(writeUnit101, 2);
            if (reply101) {
                connect(reply101, &QModbusReply::finished, reply101, &QModbusReply::deleteLater);
            }

            QModbusDataUnit writeUnit102(QModbusDataUnit::Coils, 102, 1);
            writeUnit102.setValue(0, plcRegister102);
            QModbusReply *reply102 = plcModbusClient->sendWriteRequest(writeUnit102, 2);
            if (reply102) {
                connect(reply102, &QModbusReply::finished, reply102, &QModbusReply::deleteLater);
            }

            // 根据设备状态和进程值设置状态字符串
            switch(deviceStatus) {
            case 3328:
                statusStr = "OK";
                break;
            case 3584:
                statusStr = "TD";
                break;
            case 3840:
                statusStr = "RD";
                break;
            default:
                statusStr = "";
                break;
            }
            if(processValue == 0){
                // 仪器实时RUN
                statusStr = "RUN";
            }
            
            QModbusDataUnit writeUnit103(QModbusDataUnit::Coils, 103, 1);
            writeUnit103.setValue(0, plcRegister103);
            QModbusReply *reply103 = plcModbusClient->sendWriteRequest(writeUnit103, 2);
            if (reply103) {
                connect(reply103, &QModbusReply::finished, reply103, &QModbusReply::deleteLater);

                // 发送信号，将寄存器值传递给DebugPage
                emit registerValuesChanged(plcRegister100, plcRegister101, plcRegister102, plcRegister103);
            }
        }
        
        // 发送气密仪实时数据信号，供TcpServerController接收
        emit realtimeDataUpdated(pressure, leak, pressureUnitStr, leakUnitStr, processName);
    }, Qt::QueuedConnection);
}

bool RealtimeMonitor::writeDeviceData(quint16 address, quint16 value, QModbusDataUnit::RegisterType type)
{
    Q_UNUSED(type); // 参数保留以保持接口兼容性，但当前实现不使用
    
    // 检查modbusClient是否为空
    if (!modbusClient) {
        logMessage("Modbus客户端未初始化，无法写入数据", true);
        return false;
    }

    if (!isConnected || modbusClient->state() != QModbusDevice::ConnectedState) {
        return false;
    }

    // 强制使用功能码16（Write Multiple Registers）
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, address, 2);
    writeUnit.setValue(0, value);
    writeUnit.setValue(1, 0); // 第二个寄存器设为0，确保使用功能码16

    if (auto *reply = modbusClient->sendWriteRequest(writeUnit, slaveId)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            return true;
        } else {
            delete reply;
            return false;
        }
    } else {
        return false;
    }
}

// 向PLC寄存器510-516及封堵选择49411/49412/49413写入0的方法
bool RealtimeMonitor::writeZeroToPLCRegisters() {
    if (!plcModbusClient) {
        logMessage("PLC连接未初始化", true);
        return false;
    }
    
    if (plcModbusClient->state() != QModbusDevice::ConnectedState) {
        logMessage("PLC未连接", true);
        return false;
    }
    
    // PLC通信的从站ID
    quint16 plcSlaveId = 2;
    
    // 优化：使用功能码15（Write Multiple Coils）批量写入
    // 把原来的10次独立请求合并为2次批量请求：
    //   请求1：地址510开始，连续7个线圈（510-516）
    //   请求2：地址49411开始，连续3个线圈（49411-49413）
    // 这样大幅减少串口通信开销，避免总线拥塞
    
    struct BatchWrite {
        quint16 startAddr;
        quint16 count;
    };
    QList<BatchWrite> batches = {
        {510, 7},    // 510,511,512,513,514,515,516
        {49411, 3}   // 49411,49412,49413
    };
    
    bool plcWriteSuccess = true;
    int pendingBatches = batches.size();
    
    for (const auto& batch : batches) {
        QModbusDataUnit writeUnit(QModbusDataUnit::Coils, batch.startAddr, batch.count);
        // 全部设置为0
        for (quint16 i = 0; i < batch.count; ++i) {
            writeUnit.setValue(i, 0);
        }
        
        QModbusReply *reply = plcModbusClient->sendWriteRequest(writeUnit, plcSlaveId);
        if (reply) {
            // 异步完成回调，避免阻塞主线程
            connect(reply, &QModbusReply::finished, reply, [this, reply, batch, &plcWriteSuccess, &pendingBatches]() {
                if (reply->error() != QModbusDevice::NoError) {
                    logMessage(QString("批量写入PLC寄存器失败: 地址%1开始%2个线圈, 错误=%3")
                              .arg(batch.startAddr).arg(batch.count).arg(reply->errorString()), true);
                    plcWriteSuccess = false;
                }
                pendingBatches--;
                reply->deleteLater();
            });
        } else {
            logMessage(QString("发送批量写请求失败: 地址%1开始%2个线圈").arg(batch.startAddr).arg(batch.count), true);
            plcWriteSuccess = false;
            pendingBatches--;
        }
    }
    
    // 注意：这里是异步写入，立即返回成功状态。
    // 真正的写入结果由回调记录日志，不再阻塞等待。
    // 这样避免了当 PLC 通信繁忙时调用方被卡住导致的崩溃风险。
    
    if (!plcWriteSuccess) {
        QMessageBox::warning(this, "警告", "部分PLC寄存器写入失败，请检查连接");
    }
    
    return plcWriteSuccess;
}

// 检查PLC寄存器519的值的方法
void RealtimeMonitor::checkPLCRegister519() {
    if (!plcModbusClient || plcModbusClient->state() != QModbusDevice::ConnectedState) {
        plcRequestBusy = false;
        return;
    }
    
    quint16 plcSlaveId = 2; // 从站地址2
    
    // 使用功能码01（Read Coils）读取寄存器519的值
    QModbusDataUnit readUnit(QModbusDataUnit::Coils, 519, 1);
    
    if (auto *reply = plcModbusClient->sendReadRequest(readUnit, plcSlaveId)) {
        connect(reply, &QModbusReply::finished, this, [this, reply]() {
            if (reply->error() == QModbusDevice::NoError) {
                const QModbusDataUnit data = reply->result();
                if (!data.values().isEmpty()) {
                    quint16 value = data.value(0);
                    // 仅上升沿触发（0->1），避免519持续为1时反复清空
                    if (value == 1 && m_lastRegister519Value == 0) {
                        writeZeroToPLCRegisters();
                        logMessage("寄存器519触发复位");
                    }
                    m_lastRegister519Value = value;
                }
            } else {
                logMessage(QString("读取PLC寄存器519失败: %1").arg(reply->errorString()), true);
            }
            plcRequestBusy = false;
            reply->deleteLater();
        });
    } else {
        logMessage(QString("向PLC发送读取寄存器519请求失败: %1").arg(plcModbusClient->errorString()), true);
        plcRequestBusy = false;
    }
}

// 合并后：统一启动PLC轮询定时器
void RealtimeMonitor::startPLCRegister519Check() {
    if (!plcPollTimer) {
        plcPollTimer = new QTimer(this);
        connect(plcPollTimer, &QTimer::timeout, this, &RealtimeMonitor::pollPlcOnce);
    }
    if (plcModbusClient && plcModbusClient->state() == QModbusDevice::ConnectedState) {
        if (!plcPollTimer->isActive()) {
            plcPollTimer->start(800); // 统一800ms轮询，降低频率避免拥塞
            logMessage("已启动PLC统一轮询定时器");
        }
    } else {
        logMessage("PLC未连接，无法启动轮询", true);
    }
}

// 合并后：统一停止PLC轮询定时器
void RealtimeMonitor::stopPLCRegister519Check() {
    if (plcPollTimer && plcPollTimer->isActive()) {
        plcPollTimer->stop();
        logMessage("已停止PLC统一轮询定时器");
    }
}

// 统一轮询tick：优先读取线圈200，每3次读取一次寄存器519，每7次读取一次寄存器0
void RealtimeMonitor::pollPlcOnce() {
    if (!plcModbusClient || plcModbusClient->state() != QModbusDevice::ConnectedState) {
        return;
    }
    if (plcRequestBusy) {
        return; // 上一次请求尚未结束，跳过本次
    }
    
    plcPollCounter++;
    if (plcPollCounter % 7 == 0) {
        // 每7次轮询读取一次线圈0，约2100ms频率
        plcRequestBusy = true; // 先设置为忙，防止多个请求同时发出
        checkPLCRegister2000();
    } else if (plcPollCounter % 3 == 0) {
        // 每3次轮询读取一次519，约900ms频率
        plcRequestBusy = true;
        checkPLCRegister519();
    } else if (plcPollCounter % 5 == 0) {
        // 每5次轮询读取一次 OK/NG 计数，约1500ms频率
        plcRequestBusy = true;
        readOkNgCount();
    } else if (plcPollCounter % 2 == 0) {
        // 每2次轮询读取一次49418，约600ms频率
        plcRequestBusy = true;
        checkCoil49418();
    } else {
        // 其他周期读取线圈200，约300ms频率
        plcRequestBusy = true;
        checkPLCRegister200();
    }
}

// 监测线圈49418，下降沿（1->0）时清空产品编号
void RealtimeMonitor::checkCoil49418() {
    if (!plcModbusClient || plcModbusClient->state() != QModbusDevice::ConnectedState) {
        plcRequestBusy = false;
        return;
    }
    QModbusDataUnit readUnit(QModbusDataUnit::Coils, 49418, 1);
    QModbusReply *reply = plcModbusClient->sendReadRequest(readUnit, 2);
    if (!reply) {
        logMessage("读取线圈49418请求发送失败", true);
        plcRequestBusy = false;
        return;
    }
    connect(reply, &QModbusReply::finished, this, [this, reply]() {
        if (reply->error() == QModbusDevice::NoError) {
            const QModbusDataUnit unit = reply->result();
            if (unit.valueCount() >= 1) {
                const quint16 value = unit.value(0);
                // 下降沿检测：1->0 时清空产品编号
                if (value == 0 && m_lastCoil49418Value == 1) {
                    productCode.clear();
                    if (auto label = this->findChild<QLabel*>("currentProductNumberLabel")) {
                        label->setText("");
                    }
                    emit productNumberChanged("");
                    logMessage("线圈49418下降沿触发，已清空产品编号");
                }
                m_lastCoil49418Value = value;
            }
        } else {
            logMessage(QString("读取线圈49418失败: %1").arg(reply->errorString()), true);
        }
        plcRequestBusy = false;
        reply->deleteLater();
    });
}

// 使用功能码01读取PLC线圈地址0，值为1时清空所有结果显示
void RealtimeMonitor::checkPLCRegister2000() {
    if (!plcModbusClient || plcModbusClient->state() != QModbusDevice::ConnectedState) {
        plcRequestBusy = false;
        return;
    }
    QModbusDataUnit readUnit(QModbusDataUnit::Coils, 0, 1);
    QModbusReply *reply = plcModbusClient->sendReadRequest(readUnit, 2);
    if (!reply) {
        logMessage("PLC线圈0读取请求发送失败", true);
        plcRequestBusy = false;
        return;
    }
    connect(reply, &QModbusReply::finished, this, [this, reply]() {
        if (reply->error() == QModbusDevice::NoError) {
            const QModbusDataUnit unit = reply->result();
            if (unit.valueCount() >= 1 && unit.value(0) == 1) {
                logMessage("PLC线圈0值为1，清空所有结果显示");

                // 通道1结果清空
                ui->test1ResultValueLabel->setText("--");
                ui->test1ResultFrame->setStyleSheet(
                    "background-color: #f5f5f5;\n"
                    "border: 2px solid #9e9e9e;\n"
                    "border-radius: 8px;"
                );
                ui->test1ResultValueLabel->setStyleSheet(
                    "font-weight: bold;\n"
                    "color: #616161;\n"
                    "font-size: 20px;"
                );

                // 通道2结果清空
                ui->test2ResultValueLabel->setText("--");
                ui->test2ResultFrame->setStyleSheet(
                    "background-color: #f5f5f5;\n"
                    "border: 2px solid #9e9e9e;\n"
                    "border-radius: 8px;"
                );
                ui->test2ResultValueLabel->setStyleSheet(
                    "font-weight: bold;\n"
                    "color: #616161;\n"
                    "font-size: 20px;"
                );

                // 通道3结果清空
                ui->test3ResultValueLabel->setText("--");
                ui->test3ResultFrame->setStyleSheet(
                    "background-color: #f5f5f5;\n"
                    "border: 2px solid #9e9e9e;\n"
                    "border-radius: 8px;"
                );
                ui->test3ResultValueLabel->setStyleSheet(
                    "font-weight: bold;\n"
                    "color: #616161;\n"
                    "font-size: 20px;"
                );

                // 当前测试结果清空
                ui->testResultValueLabel->setText("--");
                ui->testResultFrame->setStyleSheet(
                    "background-color: #f5f5f5;\n"
                    "border: 2px solid #9e9e9e;\n"
                    "border-radius: 8px;"
                );
                ui->testResultValueLabel->setStyleSheet(
                    "font-weight: bold;\n"
                    "color: #616161;\n"
                    "font-size: 18px;"
                );

                // 汇总结果清空（重置通道状态后调用updateSummaryResult）
                ch1HasResult = false;
                ch2HasResult = false;
                ch3HasResult = false;
                ch1Pass = false;
                ch2Pass = false;
                ch3Pass = false;
                updateSummaryResult();

                logMessage("寄存器0触发复位，已清空结果显示");
            }
        } else {
            logMessage(QString("读取PLC线圈0失败：%1").arg(reply->errorString()), true);
        }
        plcRequestBusy = false;
        reply->deleteLater();
    });
}

// 使用功能码01实时读取PLC线圈地址200
void RealtimeMonitor::checkPLCRegister200() {
    if (!plcModbusClient || plcModbusClient->state() != QModbusDevice::ConnectedState) {
        plcRequestBusy = false;
        return;
    }
    QModbusDataUnit readUnit(QModbusDataUnit::Coils, 200, 1);
    QModbusReply *reply = plcModbusClient->sendReadRequest(readUnit, 2);
    if (!reply) {
        logMessage("PLC线圈200读取请求发送失败", true);
        plcRequestBusy = false;
        return;
    }
    connect(reply, &QModbusReply::finished, this, [this, reply]() {
        if (reply->error() == QModbusDevice::NoError) {
            const QModbusDataUnit unit = reply->result();
            if (unit.valueCount() >= 1) {
                const quint16 value = unit.value(0);
                const bool valueChanged = (value != lastCoil200Value);
                if (valueChanged) {
                    lastCoil200Value = value;
                    emit plcCoil200Changed(value);
                    logMessage(QString("PLC线圈200值变更为: %1").arg(value));
                }
                // 当线圈200读到值为1时，立即将通道面板回到初始状态（仅上升沿触发，避免重复清空）
                if (value == 1 && valueChanged) {
                    // 发送信号给plcmonitor,更新通道结果
                    emit updateChannelResult();
                    
                    // 通道1恢复初始状态
                    ui->test1ResultValueLabel->setText("--");
                    ui->test1ResultFrame->setStyleSheet(
                        "background-color: #f5f5f5;\n"
                        "border: 2px solid #9e9e9e;\n"
                        "border-radius: 8px;"
                    );
                    ui->test1ResultValueLabel->setStyleSheet(
                        "font-weight: bold;\n"
                        "color: #616161;\n"
                        "font-size: 20px;"
                    );

                    // 测试2恢复初始状态
                    ui->test2ResultValueLabel->setText("--");
                    ui->test2ResultFrame->setStyleSheet(
                        "background-color: #f5f5f5;\n"
                        "border: 2px solid #9e9e9e;\n"
                        "border-radius: 8px;"
                    );
                    ui->test2ResultValueLabel->setStyleSheet(
                        "font-weight: bold;\n"
                        "color: #616161;\n"
                        "font-size: 20px;"
                    );

                    // 测试3恢复初始状态
                    ui->test3ResultValueLabel->setText("--");
                    ui->test3ResultFrame->setStyleSheet(
                        "background-color: #f5f5f5;\n"
                        "border: 2px solid #9e9e9e;\n"
                        "border-radius: 8px;"
                    );
                    ui->test3ResultValueLabel->setStyleSheet(
                        "font-weight: bold;\n"
                        "color: #616161;\n"
                        "font-size: 20px;"
                    );

                    // 参数初始化
                    ui->fillTimeValueLabelAirtight->setText("--");
                    ui->stabilizeTimeValueLabelAirtight->setText("--");
                    ui->testTimeValueLabelAirtight->setText("--");
                    ui->dumpTimeValueLabelAirtight->setText("--");
                    ui->fillPressureValueLabelAirtight->setText("--");

                    // 同步重置通道1/2/3结果面板中的压力和泄漏显示为初始值
                    ui->channel1PassPressureLabel->setText("--");
                    ui->channel1PassLeakLabel->setText("--");
                    ui->channel2PassPressureLabel->setText("--");
                    ui->channel2PassLeakLabel->setText("--");
                    ui->channel3PassPressureLabel->setText("--");
                    ui->channel3PassLeakLabel->setText("--");

                    // 清空通道结果标志，避免上一轮结果影响本轮汇总
                    ch1HasResult = false;
                    ch2HasResult = false;
                    ch3HasResult = false;
                    ch1Pass = false;
                    ch2Pass = false;
                    ch3Pass = false;

                    updateSummaryResult();
                }
            }
        } else {
            logMessage(QString("读取PLC线圈200失败：%1").arg(reply->errorString()), true);
        }
        plcRequestBusy = false;
        reply->deleteLater();
    });
}

void RealtimeMonitor::startPLCRegister200Check() {
    // 兼容旧调用：统一启动PLC轮询
    startPLCRegister519Check();
}

void RealtimeMonitor::stopPLCRegister200Check() {
    // 兼容旧调用：统一停止PLC轮询
    stopPLCRegister519Check();
}

void RealtimeMonitor::logMessage(const QString message, bool isError) {
    QString level = isError ? "error" : "info";
    Logger::log(level, message);
}

void RealtimeMonitor::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, [this]() {
        if (auto lineEdit = this->findChild<QLineEdit*>("productNumberLineEdit")) {
            lineEdit->clear();
            lineEdit->setFocus();
            lineEdit->selectAll();
        }
    });
}

/*
// 按钮已从UI移除，保留代码以备将来使用
void RealtimeMonitor::on_startButton_clicked()
{
    // 检查modbusClient是否为空（用于设备控制）
    if (!modbusClient) {
        QMessageBox::warning(this, "错误", "Modbus客户端未初始化");
        return;
    }

    if (modbusClient->state() != QModbusDevice::ConnectedState) {
        QMessageBox::warning(this, "错误", "请先连接设备");
        return;
    }
    
    // 检查plcModbusClient是否为空（用于PLC通信）
    if (!plcModbusClient) {
        QMessageBox::warning(this, "错误", "PLC Modbus客户端未初始化");
        return;
    }
    
    if (plcModbusClient->state() != QModbusDevice::ConnectedState) {
        QMessageBox::warning(this, "错误", "请先连接PLC");
        return;
    }

    // 需要给plc寄存器516写入0，使用功能码05
     QModbusDataUnit writeUnit(QModbusDataUnit::Coils, 516, 1);
        writeUnit.setValue(0, 0); // 写入0
        
        if (auto *reply = plcModbusClient->sendWriteRequest(writeUnit, 2)) {
            if (!reply->isFinished()) {
                connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            } else {
                delete reply;
                logMessage("向PLC寄存器516写入0失败", "error");
            }
        } else {
            logMessage("向PLC寄存器516发送写入请求失败", "error");
        }
    
    // 启动定时器定期检查PLC寄存器519的值
    startPLCRegister519Check(); // 确保在启动测试时开始监测
    
    // 发送启动命令到设备（向寄存器9472发送值4608）
    if (writeDeviceData(9472, 4608)) {
        logMessage("发送启动命令成功");
        QMessageBox::information(this, "提示", "启动命令已发送");
        
        // 新增：启动时自动聚焦产品编号输入框，确保扫码枪输入直接写入
        if (auto lineEdit = this->findChild<QLineEdit*>("productNumberLineEdit")) {
            lineEdit->clear();
            lineEdit->setFocus();
        }
    } else {
        QMessageBox::warning(this, "错误", "启动命令发送失败");
    }
}
*/

// 新增：产品编号回车确认处理
void RealtimeMonitor::onProductNumberEntered()
{
    auto lineEdit = this->findChild<QLineEdit*>("productNumberLineEdit");
    if (!lineEdit) return;

    QString code = lineEdit->text().trimmed();
    lineEdit->clear();
    lineEdit->setFocus();

    if (code.isEmpty()) return;

    code = code.remove(QRegularExpression("^-+"));
    if (code.isEmpty()) return;

    productCode = code;
    logMessage(QString("扫码产品编号: %1").arg(productCode));

    // 显示到当前编号 Label
    if (auto label = this->findChild<QLabel*>("currentProductNumberLabel")) {
        label->setText(productCode);
    }

    // 发出产品编号变化信号，供其他页面使用
    emit productNumberChanged(productCode);

    // 产品编号非空时：使用PLC连接向寄存器49418写1（功能码05，写单线圈）
    if (!plcModbusClient) {
        logMessage("PLC Modbus客户端未初始化，无法写入寄存器49418", true);
    } else if (plcModbusClient->state() != QModbusDevice::ConnectedState) {
        logMessage("PLC未连接，无法写入寄存器49418", true);
    } else {
        QModbusDataUnit writeUnit(QModbusDataUnit::Coils, 49418, 1);
        writeUnit.setValue(0, 1);
        if (auto *reply = plcModbusClient->sendWriteRequest(writeUnit, 2)) {
            if (!reply->isFinished()) {
                connect(reply, &QModbusReply::finished, reply, &QModbusReply::deleteLater);
            } else {
                reply->deleteLater();
            }
            logMessage("已向PLC寄存器49418写入1（功能码05）");
        } else {
            logMessage(QString("向PLC寄存器49418发送写入请求失败: %1").arg(plcModbusClient->errorString()), true);
        }
    }

    // 清空输入框，准备下次扫码，避免二次扫码拼接
    lineEdit->clear();
    lineEdit->setFocus();
}

void RealtimeMonitor::on_clearProductNumberButton_clicked()
{
    productCode.clear();

    if (auto label = this->findChild<QLabel*>("currentProductNumberLabel")) {
        label->setText("");
    }
    if (auto lineEdit = this->findChild<QLineEdit*>("productNumberLineEdit")) {
        lineEdit->clear();
        lineEdit->setFocus();
    }

    emit productNumberChanged("");
    logMessage("手动清空产品编号");
}

/*
// 按钮已从UI移除，保留代码以备将来使用
void RealtimeMonitor::on_resetButton_clicked()
{
    // 检查modbusClient是否为空
    if (!modbusClient) {
        QMessageBox::warning(this, "错误", "Modbus客户端未初始化");
        return;
    }

    if (modbusClient->state() != QModbusDevice::ConnectedState) {
        QMessageBox::warning(this, "错误", "请先连接设备");
        return;
    }

    // 发送复位命令到设备（向寄存器9472发送值4864）
    if (writeDeviceData(9472, 4864)) {
        logMessage("复位命令已发送");
    } else {
        logMessage("发送复位命令失败", true);
        QMessageBox::warning(this, "错误", "发送复位命令失败");
    }
}
*/

void RealtimeMonitor::handleProgramNumberReceived(int newProgramNumber)
{
    
    // 处理接收到的程序号或仪器评选号，更新全局变量
    programNumber = newProgramNumber;
    // 记录详细日志，包含来源信息（程序号或仪器评选号）
    qDebug() << "RealtimeMonitor收到信号，新的程序号/仪器评选号: " << newProgramNumber;
    logMessage(QString("收到程序号/仪器评选号: %1，已更新全局变量").arg(programNumber));
    
    // 重置测试状态标志：新测试开始，需要等待测试真正开始（经历非排气状态）后才能处理结果
    currentTestResultProcessed = false;
    testActuallyStarted = false;
    logMessage("新测试周期开始，重置测试状态标志");
    
    // 注意：不要重置通道结果状态！
    // 因为三个通道可能测试不同的程序号，需要保留之前的测试结果
    // 只有当所有启用的通道都测试完成后，汇总结果才显示"合格"
    
    // 读取通道使能状态（从PLC线圈寄存器49408/49409/49410）
    if (plcModbusClient && plcModbusClient->state() == QModbusDevice::ConnectedState) {
        // 使用QSharedPointer避免内存泄漏和重复释放
        QSharedPointer<int> enableReadCount = QSharedPointer<int>::create(0);
        
        // 读取通道1使能状态
        QModbusDataUnit readUnit1(QModbusDataUnit::Coils, 49408, 1);
        if (auto *reply = plcModbusClient->sendReadRequest(readUnit1, 2)) {
            connect(reply, &QModbusReply::finished, this, [this, reply, enableReadCount]() {
                if (reply->error() == QModbusDevice::NoError) {
                    ch1Enabled = reply->result().value(0) == 1;
                    logMessage(QString("通道1使能状态: %1").arg(ch1Enabled ? "启用" : "禁用"));
                }
                reply->deleteLater();
                (*enableReadCount)++;
                if (*enableReadCount == 3) {
                    // 所有通道使能状态都已读取，更新汇总结果
                    updateSummaryResult();
                }
            });
        } else {
            (*enableReadCount)++;
            if (*enableReadCount == 3) {
                updateSummaryResult();
            }
        }
        
        // 读取通道2使能状态
        QModbusDataUnit readUnit2(QModbusDataUnit::Coils, 49409, 1);
        if (auto *reply = plcModbusClient->sendReadRequest(readUnit2, 2)) {
            connect(reply, &QModbusReply::finished, this, [this, reply, enableReadCount]() {
                if (reply->error() == QModbusDevice::NoError) {
                    ch2Enabled = reply->result().value(0) == 1;
                    logMessage(QString("通道2使能状态: %1").arg(ch2Enabled ? "启用" : "禁用"));
                }
                reply->deleteLater();
                (*enableReadCount)++;
                if (*enableReadCount == 3) {
                    // 所有通道使能状态都已读取，更新汇总结果
                    updateSummaryResult();
                }
            });
        } else {
            (*enableReadCount)++;
            if (*enableReadCount == 3) {
                updateSummaryResult();
            }
        }
        
        // 读取通道3使能状态
        QModbusDataUnit readUnit3(QModbusDataUnit::Coils, 49410, 1);
        if (auto *reply = plcModbusClient->sendReadRequest(readUnit3, 2)) {
            connect(reply, &QModbusReply::finished, this, [this, reply, enableReadCount]() {
                if (reply->error() == QModbusDevice::NoError) {
                    ch3Enabled = reply->result().value(0) == 1;
                    logMessage(QString("通道3使能状态: %1").arg(ch3Enabled ? "启用" : "禁用"));
                }
                reply->deleteLater();
                (*enableReadCount)++;
                if (*enableReadCount == 3) {
                    // 所有通道使能状态都已读取，更新汇总结果
                    updateSummaryResult();
                }
            });
        } else {
            (*enableReadCount)++;
            if (*enableReadCount == 3) {
                updateSummaryResult();
            }
        }
    } else {
        // 如果PLC未连接，直接更新汇总结果
        updateSummaryResult();
    }
    
    // 根据当前程序号去数据库中进行查询出相应的参数值
    AirTightnessFullParams params = AirTightnessParamsDao::getParamsByProgramNumber(programNumber);
    if (ui) {
        if (params.id != -1) {
            ui->fillTimeValueLabelAirtight->setText(QString::number(params.cycleTime.fillTime)+"s");
            ui->stabilizeTimeValueLabelAirtight->setText(QString::number(params.cycleTime.stabilizationTime)+"s");
            ui->testTimeValueLabelAirtight->setText(QString::number(params.cycleTime.testTime)+"s");
            ui->dumpTimeValueLabelAirtight->setText(QString::number(params.cycleTime.dumpTime)+"s");
            ui->fillPressureValueLabelAirtight->setText(QString::number(params.pressure.setFill)+" "+PressureUnit::toString(PressureUnit::fromCode(params.pressure.unitIndex)));
            
            // 保存时间参数用于进度条计算
            fillTime = params.cycleTime.fillTime;
            stabilizeTime = params.cycleTime.stabilizationTime;
            testTime = params.cycleTime.testTime;
            dumpTime = params.cycleTime.dumpTime;
        } else {
            ui->fillTimeValueLabelAirtight->setText("--");
            ui->stabilizeTimeValueLabelAirtight->setText("--");
            ui->testTimeValueLabelAirtight->setText("--");
            ui->dumpTimeValueLabelAirtight->setText("--");
            ui->fillPressureValueLabelAirtight->setText("--");
            // 重置时间参数
            fillTime = 0;
            stabilizeTime = 0;
            testTime = 0;
            dumpTime = 0;
            logMessage(QString("未在数据库找到程序号 %1 对应的参数").arg(programNumber), true);
        }
    }
    
    // 重置进度条状态
    lastPhase = -1;
    currentPhase = STANDBY;
    phaseElapsedTimer.restart();
    
    // 更新界面上的程序号显示（测试进程面板）
    if (ui) {
        ui->testProgramNumberValueLabel->setText(QString::number(programNumber));
    }
    // 确保PLC寄存器519检测已经启动
    if (plcModbusClient && plcModbusClient->state() == QModbusDevice::ConnectedState) {
        startPLCRegister519Check(); // 调用此方法会检查定时器是否已运行，不会重复启动
        logMessage(QString("基于收到的值 %1 启动了PLC寄存器519检测").arg(programNumber));
    } else {
        logMessage("PLC未连接，无法启动寄存器519检测", true);
    }
}


void RealtimeMonitor::handleTestResultReady(const TestResult &result)
{
    // 更新测试1结果
    QMetaObject::invokeMethod(this, [this, result]() {
        const bool pass = result.isPassed && !result.isFailed;
        logMessage(QString("通道1测试结果: isPassed=%1, isFailed=%2, pass=%3").arg(result.isPassed).arg(result.isFailed).arg(pass));
        if (pass) {
            ui->test1ResultValueLabel->setText("通过");
            ui->test1ResultFrame->setStyleSheet(
                "background-color: #e8f5e9;\n"
                "border: 2px solid #4caf50;\n"
                "border-radius: 8px;"
            );
            ui->test1ResultValueLabel->setStyleSheet(
                "font-weight: bold;\n"
                "color: #2e7d32;\n"
                "font-size: 20px;"
            );
        } else {
            ui->test1ResultValueLabel->setText("失败");
            ui->test1ResultFrame->setStyleSheet(
            "background-color: #ffebee;\n"
                "border: 2px solid #f44336;\n"
                "border-radius: 8px;"
            );
            ui->test1ResultValueLabel->setStyleSheet(
                "font-weight: bold;\n"
                "color: #c62828;\n"
                "font-size: 20px;"
            );
        }
        // 初始化/更新通道1压力和泄漏值显示
        ui->channel1PassPressureLabel->setText(QString("%1 %2").arg(result.channelPressure, 0, 'f', 2).arg(result.pressureUnit));
        ui->channel1PassLeakLabel->setText(QString("%1 %2").arg(result.channelLeak, 0, 'f', 3).arg(result.leakUnit));
        // 新增：更新通道状态并联动汇总
        ch1Pass = pass;
        ch1HasResult = true;
        logMessage(QString("设置通道1状态: ch1Pass=%1, ch1HasResult=%2").arg(ch1Pass).arg(ch1HasResult));
        updateSummaryResult();
    }, Qt::QueuedConnection);
}

void RealtimeMonitor::handleTestResultReady2(const TestResult &result)
{
    // 更新测试2结果
    QMetaObject::invokeMethod(this, [this, result]() {
        const bool pass = result.isPassed && !result.isFailed;
        logMessage(QString("通道2测试结果: isPassed=%1, isFailed=%2, pass=%3").arg(result.isPassed).arg(result.isFailed).arg(pass));
        if (pass) {
            ui->test2ResultValueLabel->setText("通过");
            ui->test2ResultFrame->setStyleSheet(
                "background-color: #e8f5e9;\n"
                "border: 2px solid #4caf50;\n"
                "border-radius: 8px;"
            );
            ui->test2ResultValueLabel->setStyleSheet(
                "font-weight: bold;\n"
                "color: #2e7d32;\n"
                "font-size: 20px;"
            );
        } else {
            ui->test2ResultValueLabel->setText("失败");
            ui->test2ResultFrame->setStyleSheet(
                "background-color: #ffebee;\n"
                "border: 2px solid #f44336;\n"
                "border-radius: 8px;"
            );
            ui->test2ResultValueLabel->setStyleSheet(
                "font-weight: bold;\n"
                "color: #c62828;\n"
                "font-size: 20px;"
            );
        }
        ui->channel2PassPressureLabel->setText(QString("%1 %2").arg(result.channelPressure, 0, 'f', 2).arg(result.pressureUnit));
        ui->channel2PassLeakLabel->setText(QString("%1 %2").arg(result.channelLeak, 0, 'f', 3).arg(result.leakUnit));
        // 新增：更新通道状态并联动汇总
        ch2Pass = pass;
        ch2HasResult = true;
        logMessage(QString("设置通道2状态: ch2Pass=%1, ch2HasResult=%2").arg(ch2Pass).arg(ch2HasResult));
        updateSummaryResult();
    }, Qt::QueuedConnection);
}

void RealtimeMonitor::handleTestResultReady3(const TestResult &result)
{
    // 更新测试3结果
    QMetaObject::invokeMethod(this, [this, result]() {
        const bool pass = result.isPassed && !result.isFailed;
        logMessage(QString("通道3测试结果: isPassed=%1, isFailed=%2, pass=%3").arg(result.isPassed).arg(result.isFailed).arg(pass));
        if (pass) {
            ui->test3ResultValueLabel->setText("通过");
            ui->test3ResultFrame->setStyleSheet(
                "background-color: #e8f5e9;\n"
                "border: 2px solid #4caf50;\n"
                "border-radius: 8px;"
            );
            ui->test3ResultValueLabel->setStyleSheet(
                "font-weight: bold;\n"
                "color: #2e7d32;\n"
                "font-size: 20px;"
            );
        } else {
            ui->test3ResultValueLabel->setText("失败");
            ui->test3ResultFrame->setStyleSheet(
                "background-color: #ffebee;\n"
                "border: 2px solid #f44336;\n"
                "border-radius: 8px;"
            );
            ui->test3ResultValueLabel->setStyleSheet(
                "font-weight: bold;\n"
                "color: #c62828;\n"
                "font-size: 20px;"
            );
        }
        ui->channel3PassPressureLabel->setText(QString("%1 %2").arg(result.channelPressure, 0, 'f', 2).arg(result.pressureUnit));
        ui->channel3PassLeakLabel->setText(QString("%1 %2").arg(result.channelLeak, 0, 'f', 3).arg(result.leakUnit));
        // 新增：更新通道状态并联动汇总
        ch3Pass = pass;
        ch3HasResult = true;
        logMessage(QString("设置通道3状态: ch3Pass=%1, ch3HasResult=%2").arg(ch3Pass).arg(ch3HasResult));
        updateSummaryResult();
    }, Qt::QueuedConnection);
}


// 新增：汇总结果更新逻辑（带防抖机制）
// 3个通道的 handleTestResultReady 可能短时间内连续调用此函数，
// 通过延迟30ms聚合执行，避免日志里出现重复的"========= 更新汇总结果 ========="
void RealtimeMonitor::updateSummaryResult() {
    // ====== 防抖核心 ======
    // 每次调用都重置定时器：30ms内无新调用才真正执行
    static QTimer *debounceTimer = nullptr;
    if (!debounceTimer) {
        debounceTimer = new QTimer();
        debounceTimer->setSingleShot(true);
        debounceTimer->setInterval(30);
    }
    // 断开旧连接（如果有）
    debounceTimer->disconnect();
    // 连接新的执行动作（用 QObject::connect 的函数指针方式）
    QObject::connect(debounceTimer, &QTimer::timeout, this, [this]() {
        // ========= 真正执行汇总更新 =========

        int enabledCount = 0;
        if (ch1Enabled) enabledCount++;
        if (ch2Enabled) enabledCount++;
        if (ch3Enabled) enabledCount++;

        logMessage("========= 更新汇总结果 =========");
        logMessage(QString("启用通道数: %1").arg(enabledCount));
        logMessage(QString("通道1: 配置启用=%1, 有结果=%2, 通过=%3")
            .arg(ch1Enabled).arg(ch1HasResult).arg(ch1Pass));
        logMessage(QString("通道2: 配置启用=%1, 有结果=%2, 通过=%3")
            .arg(ch2Enabled).arg(ch2HasResult).arg(ch2Pass));
        logMessage(QString("通道3: 配置启用=%1, 有结果=%2, 通过=%3")
            .arg(ch3Enabled).arg(ch3HasResult).arg(ch3Pass));

        if (enabledCount == 0) {
            ui->summaryResultValueLabel->setText("--");
            ui->summaryResultFrame->setStyleSheet(
                "QFrame#summaryResultFrame {\n"
                "    background-color: #eceff1;\n"
                "    border: 2px solid #cfd8dc;\n"
                "    border-radius: 10px;\n"
                "    min-width: 140px;\n"
                "    min-height: 50px;\n"
                "    max-height: 50px;\n"
                "}"
            );
            ui->summaryResultValueLabel->setStyleSheet(
                "font-weight: bold;\n"
                "color: #546e7a;\n"
                "font-size: 16px;"
            );
            return;
        }

        bool anyFail = false;
        if (ch1Enabled && ch1HasResult && !ch1Pass) anyFail = true;
        if (ch2Enabled && ch2HasResult && !ch2Pass) anyFail = true;
        if (ch3Enabled && ch3HasResult && !ch3Pass) anyFail = true;

        bool allEnabledHaveResult = true;
        bool allEnabledPass = true;

        if (ch1Enabled) {
            if (!ch1HasResult) allEnabledHaveResult = false;
            else if (!ch1Pass) allEnabledPass = false;
        }
        if (ch2Enabled) {
            if (!ch2HasResult) allEnabledHaveResult = false;
            else if (!ch2Pass) allEnabledPass = false;
        }
        if (ch3Enabled) {
            if (!ch3HasResult) allEnabledHaveResult = false;
            else if (!ch3Pass) allEnabledPass = false;
        }

        if (anyFail) {
            logMessage("汇总结果: 不合格 (有通道失败)");
            ui->summaryResultValueLabel->setText("不合格");
            ui->summaryResultFrame->setStyleSheet(
                "QFrame#summaryResultFrame {\n"
                "    background-color: #ffebee;\n"
                "    border: 2px solid #f44336;\n"
                "    border-radius: 10px;\n"
                "    min-width: 140px;\n"
                "    min-height: 50px;\n"
                "    max-height: 50px;\n"
                "}"
            );
            ui->summaryResultValueLabel->setStyleSheet(
                "font-weight: bold;\n"
                "color: #c62828;\n"
                "font-size: 16px;"
            );
        } else if (allEnabledHaveResult && allEnabledPass) {
            logMessage("汇总结果: 合格 (所有启用通道都通过)");
            ui->summaryResultValueLabel->setText("合格");
            ui->summaryResultFrame->setStyleSheet(
                "QFrame#summaryResultFrame {\n"
                "    background-color: #e8f5e9;\n"
                "    border: 2px solid #4caf50;\n"
                "    border-radius: 10px;\n"
                "    min-width: 140px;\n"
                "    min-height: 50px;\n"
                "    max-height: 50px;\n"
                "}"
            );
            ui->summaryResultValueLabel->setStyleSheet(
                "font-weight: bold;\n"
                "color: #2e7d32;\n"
                "font-size: 16px;"
            );
        } else {
            logMessage("汇总结果: 测试中 (还有通道没有结果)");
            logMessage(QString("allEnabledHaveResult=%1, allEnabledPass=%2")
                .arg(allEnabledHaveResult).arg(allEnabledPass));
            ui->summaryResultValueLabel->setText("测试中");
            ui->summaryResultFrame->setStyleSheet(
                "QFrame#summaryResultFrame {\n"
                "    background-color: #fff3e0;\n"
                "    border: 2px solid #ff9800;\n"
                "    border-radius: 10px;\n"
                "    min-width: 140px;\n"
                "    min-height: 50px;\n"
                "    max-height: 50px;\n"
                "}"
            );
            ui->summaryResultValueLabel->setStyleSheet(
                "font-weight: bold;\n"
                "color: #e65100;\n"
                "font-size: 14px;"
            );
        }
    });
    debounceTimer->start();
}



// 读取 OK 和 NG 计数
void RealtimeMonitor::readOkNgCount()
{
    if (!plcModbusClient || plcModbusClient->state() != QModbusDevice::ConnectedState) {
        plcRequestBusy = false;
        return;
    }
    
    // 读取寄存器 120 (OK 计数) 和 122 (NG 计数)，一次读取3个寄存器
    QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, 120, 3);
    QModbusReply *reply = plcModbusClient->sendReadRequest(readUnit, 2);
    
    if (!reply) {
        logMessage("读取 OK/NG 计数请求发送失败", true);
        plcRequestBusy = false;
        return;
    }
    
    connect(reply, &QModbusReply::finished, this, [this, reply]() {
        if (reply->error() == QModbusDevice::NoError) {
            const QModbusDataUnit unit = reply->result();
            if (unit.valueCount() >= 3) {
                // 寄存器 120: OK 计数
                m_okCount = unit.value(0);
                // 寄存器 122: NG 计数 (索引为 2，因为是从120开始读取的)
                m_ngCount = unit.value(2);
                
                // 更新显示
                updateOkNgDisplay();
                
                qDebug() << "读取 OK/NG 计数成功: OK=" << m_okCount << ", NG=" << m_ngCount;
            } else {
                logMessage("读取 OK/NG 计数返回数据不足", true);
            }
        } else {
            logMessage(QString("读取 OK/NG 计数失败: %1").arg(reply->errorString()), true);
        }
        plcRequestBusy = false;
        reply->deleteLater();
    });
}

// 更新 OK 和 NG 计数显示
void RealtimeMonitor::updateOkNgDisplay()
{
    ui->okCountLabel->setText(QString("✅ OK: %1").arg(m_okCount));
    ui->ngCountLabel->setText(QString("❌ NG: %1").arg(m_ngCount));
    
    // 计算合格率
    quint16 totalCount = m_okCount + m_ngCount;
    double passRate = 0.0;
    
    if (totalCount > 0) {
        passRate = (static_cast<double>(m_okCount) / static_cast<double>(totalCount)) * 100.0;
    }
    
    // 更新合格率显示，保留一位小数
    ui->passRateLabel->setText(QString("📊 合格率: %1%").arg(passRate, 0, 'f', 1));
    
    // 根据合格率动态改变颜色
    QString passRateStyle;
    if (passRate >= 95.0) {
        // 优秀：深绿色
        passRateStyle = "QLabel {"
                       "    color: #ffffff;"
                       "    font-size: 14px;"
                       "    font-weight: 600;"
                       "    padding: 8px 16px;"
                       "    border-radius: 10px;"
                       "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                       "        stop:0 #10b981, stop:1 #059669);"
                       "    border: 2px solid #047857;"
                       "}";
    } else if (passRate >= 85.0) {
        // 良好：蓝色
        passRateStyle = "QLabel {"
                       "    color: #ffffff;"
                       "    font-size: 14px;"
                       "    font-weight: 600;"
                       "    padding: 8px 16px;"
                       "    border-radius: 10px;"
                       "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                       "        stop:0 #3b82f6, stop:1 #2563eb);"
                       "    border: 2px solid #1e40af;"
                       "}";
    } else if (passRate >= 70.0) {
        // 一般：橙色
        passRateStyle = "QLabel {"
                       "    color: #ffffff;"
                       "    font-size: 14px;"
                       "    font-weight: 600;"
                       "    padding: 8px 16px;"
                       "    border-radius: 10px;"
                       "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                       "        stop:0 #f59e0b, stop:1 #d97706);"
                       "    border: 2px solid #b45309;"
                       "}";
    } else {
        // 较差：红色
        passRateStyle = "QLabel {"
                       "    color: #ffffff;"
                       "    font-size: 14px;"
                       "    font-weight: 600;"
                       "    padding: 8px 16px;"
                       "    border-radius: 10px;"
                       "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                       "        stop:0 #ef4444, stop:1 #dc2626);"
                       "    border: 2px solid #b91c1c;"
                       "}";
    }
    
    ui->passRateLabel->setStyleSheet(passRateStyle);
}
