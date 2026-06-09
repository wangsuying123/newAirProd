#include "connect.h"
#include "ui_connect.h"
#include "ConnectionParameters.h"
#include "ConnectionParamsDao.h"
#include <QMessageBox>
#include <QRandomGenerator>
#include <QLabel>
#include <QDateTime>
#include <QSerialPortInfo>
#include <QSerialPort>
#include <QModbusTcpClient>
#include <QModbusRtuSerialClient>
#include <QDebug>
#include <QTimer>
#include "Logger.h"

// 自动更新串口号下拉列表
void ConnectWidget::updateSerialPorts()
{
    // 清空现有串口号选项
    ui->plcSerialCombo->clear();
    ui->airSerialCombo->clear();
    ui->serialPortCombo->clear();

    // 获取系统可用串口信息
    QList<QSerialPortInfo> serialPorts = QSerialPortInfo::availablePorts();

    if (serialPorts.isEmpty()) {
        addLog("未检测到可用串口");
        // 添加一个默认选项，避免下拉框为空
        ui->plcSerialCombo->addItem("无可用串口");
        ui->airSerialCombo->addItem("无可用串口");
        ui->serialPortCombo->addItem("无可用串口");
    } else {
        QString portInfo;
        foreach (const QSerialPortInfo &info, serialPorts) {
            QString portName = info.portName();
            ui->plcSerialCombo->addItem(portName);
            ui->airSerialCombo->addItem(portName);
            ui->serialPortCombo->addItem(portName);

            // 添加日志信息
            portInfo = QString("检测到串口: %1, 制造商: %2, 描述: %3")
                           .arg(portName)
                           .arg(info.manufacturer())
                           .arg(info.description());
            addLog(portInfo);
        }
    }
}

// 刷新串口按钮槽函数
void ConnectWidget::on_refreshSerialPortBtn_clicked()
{
    addLog("🔄 手动刷新串口列表...");
    updateSerialPorts();
    addLog("✅ 串口列表刷新完成");
}

ConnectWidget::ConnectWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ConnectWidget),
    airModbusClient(nullptr),
    plcModbusClient(nullptr),
    pressureModbusClient(nullptr),
    plcConnected(false),
    airConnected(false),
    pressureConnected(false),
    pressureWasConnected(false),
    pressureConnectPromptShown(false),
    plcTestTimer(nullptr),
    airTestTimer(nullptr),
    autoConnectTimer(nullptr),
    plcSlaveId(2), // 默认PLC从站地址为2
    airtightSlaveId(1), // 默认气密仪从站地址为1
    pressureSlaveId(1), // 默认调压装置从站地址为1
    autoConnectEnabled(true),
    autoConnectRetryCount(0)
{
    // 首先初始化UI组件
    ui->setupUi(this);

    // 初始化日志
    addLog("系统启动，开始初始化设备连接");

    // 自动检测并更新串口列表
    updateSerialPorts();

    // 加载PLC参数
    loadPLCParameters();
    addLog("PLC连接参数加载完成");

    // 加载气密仪参数
    loadAirTightnessParameters();
    addLog("气密仪连接参数加载完成");

    // 加载调压装置参数
    loadPressureParameters();
    addLog("调压装置连接参数加载完成");

    // 连接PLC协议选择变化信号
    connect(ui->plcProtocolCombo, &QComboBox::currentIndexChanged, this, &ConnectWidget::onPlcProtocolChanged);

    // 连接气密仪连接方式变化信号
    connect(ui->airConnectionTypeCombo, &QComboBox::currentIndexChanged, this, &ConnectWidget::onAirConnectionTypeChanged);
    
    // 连接调压装置协议选择变化信号
    connect(ui->protocolComboBox, &QComboBox::currentIndexChanged, this, &ConnectWidget::on_protocolComboBox_currentIndexChanged);
    
    // 连接调压装置连接按钮
    connect(ui->connectButton, &QPushButton::clicked, this, &ConnectWidget::on_connectButton_clicked);
    connect(ui->disconnectButton, &QPushButton::clicked, this, &ConnectWidget::on_disconnectButton_clicked);

    // 默认隐藏串口号选项
    onPlcProtocolChanged(ui->plcProtocolCombo->currentIndex());
    onAirConnectionTypeChanged(ui->airConnectionTypeCombo->currentIndex());
    
    // 初始化调压装置UI和Modbus客户端
    // on_protocolComboBox_currentIndexChanged会根据当前协议创建相应的客户端
    on_protocolComboBox_currentIndexChanged(ui->protocolComboBox->currentIndex());
    
    // 初始化调压装置状态
    updatePressureDeviceStatus(false);
    
    // 延迟执行自动连接（等待UI完全初始化）
    autoConnectTimer = new QTimer(this);
    autoConnectTimer->setSingleShot(true);
    connect(autoConnectTimer, &QTimer::timeout, this, &ConnectWidget::performAutoConnect);
    autoConnectTimer->start(1000); // 1秒后执行自动连接

}


ConnectWidget::~ConnectWidget()
{
    // 断开并释放Modbus客户端
    if (plcModbusClient) {
        plcModbusClient->disconnectDevice();
        delete plcModbusClient;
    }
    if (airModbusClient) {
        airModbusClient->disconnectDevice();
        delete airModbusClient;
    }
    if (pressureModbusClient) {
        pressureModbusClient->disconnectDevice();
        delete pressureModbusClient;
    }

    // 释放定时器
    if (plcTestTimer) {
        delete plcTestTimer;
    }
    if (airTestTimer) {
        delete airTestTimer;
    }
    if (autoConnectTimer) {
        delete autoConnectTimer;
    }

    delete ui;
}

// 加载PLC参数
void ConnectWidget::loadPLCParameters()
{
    PLCConnectionParams params = ConnectionParamsDao::loadPLCParams();

    // 设置协议类型
    int protocolIndex = ui->plcProtocolCombo->findText(params.protocol);
    if (protocolIndex >= 0) {
        ui->plcProtocolCombo->setCurrentIndex(protocolIndex);
    }

    // 设置串口参数
    ui->plcSerialCombo->setCurrentText(params.serialPort);

    // 设置波特率
    int baudIndex = ui->plcBaudrateCombo->findText(QString::number(params.baudRate));
    if (baudIndex >= 0) {
        ui->plcBaudrateCombo->setCurrentIndex(baudIndex);
    }

    // 设置数据位
    int dataBitsIndex = ui->plcDatabitsCombo->findText(QString::number(params.dataBits));
    if (dataBitsIndex >= 0) {
        ui->plcDatabitsCombo->setCurrentIndex(dataBitsIndex);
    }

    // 设置校验位
    if (params.parity == QSerialPort::OddParity) {
        ui->plcParityCombo->setCurrentText("奇校验");
    } else if (params.parity == QSerialPort::EvenParity) {
        ui->plcParityCombo->setCurrentText("偶校验");
    } else {
        ui->plcParityCombo->setCurrentText("无");
    }

    // 设置停止位
    ui->plcStopbitsCombo->setCurrentText(params.stopBits == QSerialPort::OneStop ? "1" : "2");

    // 设置网络参数
    ui->plcIpEdit->setText(params.ipAddress);
    ui->plcPortSpin->setValue(params.port);

    // 保存从站地址到对象中，以便后续使用
    // 注意：从站地址在UI中不直接显示，使用默认值2
    plcSlaveId = params.slaveId;
    qDebug() << "加载PLC从站地址: " << plcSlaveId;
}

// 加载气密仪参数
void ConnectWidget::loadAirTightnessParameters()
{
    AirTightnessParams params = ConnectionParamsDao::loadAirTightnessParams();

    // 设置连接类型
    int typeIndex = ui->airConnectionTypeCombo->findText(params.connectionType);
    if (typeIndex >= 0) {
        ui->airConnectionTypeCombo->setCurrentIndex(typeIndex);
    }

    // 设置串口参数
    ui->airSerialCombo->setCurrentText(params.serialPort);

    // 设置波特率
    int baudIndex = ui->airBaudrateCombo->findText(QString::number(params.baudRate));
    if (baudIndex >= 0) {
        ui->airBaudrateCombo->setCurrentIndex(baudIndex);
    }

    // 设置数据位
    int dataBitsIndex = ui->airDatabitsCombo->findText(QString::number(params.dataBits));
    if (dataBitsIndex >= 0) {
        ui->airDatabitsCombo->setCurrentIndex(dataBitsIndex);
    }

    // 设置校验位
    if (params.parity == QSerialPort::OddParity) {
        ui->airParityCombo->setCurrentText("奇校验");
    } else if (params.parity == QSerialPort::EvenParity) {
        ui->airParityCombo->setCurrentText("偶校验");
    } else {
        ui->airParityCombo->setCurrentText("无");
    }

    // 设置停止位
    ui->airStopbitsCombo->setCurrentText(params.stopBits == QSerialPort::OneStop ? "1" : "2");

    // 设置网络参数
    ui->airIpEdit->setText(params.ipAddress);
    ui->airPortSpin->setValue(params.port);
}

// 加载调压装置参数
void ConnectWidget::loadPressureParameters()
{
    PressureDeviceParams params = ConnectionParamsDao::loadPressureParams();

    // 设置协议类型
    int protocolIndex = ui->protocolComboBox->findText(params.protocol);
    if (protocolIndex >= 0) {
        ui->protocolComboBox->setCurrentIndex(protocolIndex);
    }

    // 设置串口参数
    ui->serialPortCombo->setCurrentText(params.serialPort);

    // 设置波特率
    int baudIndex = ui->baudrateCombo->findText(QString::number(params.baudRate));
    if (baudIndex >= 0) {
        ui->baudrateCombo->setCurrentIndex(baudIndex);
    }

    // 设置数据位
    int dataBitsIndex = ui->databitsCombo->findText(QString::number(params.dataBits));
    if (dataBitsIndex >= 0) {
        ui->databitsCombo->setCurrentIndex(dataBitsIndex);
    }

    // 设置校验位
    if (params.parity == QSerialPort::OddParity) {
        ui->parityCombo->setCurrentText("奇校验");
    } else if (params.parity == QSerialPort::EvenParity) {
        ui->parityCombo->setCurrentText("偶校验");
    } else {
        ui->parityCombo->setCurrentText("无");
    }

    // 设置停止位
    ui->stopbitsCombo->setCurrentText(params.stopBits == QSerialPort::OneStop ? "1" : "2");

    // 设置网络参数
    ui->ipLineEdit->setText(params.ipAddress);
    ui->portSpinBox->setValue(params.port);

    // 保存从站地址到对象中
    pressureSlaveId = params.slaveId;
    qDebug() << "加载调压装置从站地址: " << pressureSlaveId;
}

// 保存调压装置参数
void ConnectWidget::savePressureParameters()
{
    PressureDeviceParams params;

    // 获取协议类型
    params.protocol = ui->protocolComboBox->currentText();

    if (params.protocol == "Modbus RTU") {
        // 对于Modbus RTU协议，保存串口相关参数
        params.serialPort = ui->serialPortCombo->currentText();
        params.baudRate = ui->baudrateCombo->currentText().toInt();
        params.dataBits = ui->databitsCombo->currentText().toInt();

        // 处理校验位
        if (ui->parityCombo->currentText() == "奇校验") {
            params.parity = QSerialPort::OddParity;
        } else if (ui->parityCombo->currentText() == "偶校验") {
            params.parity = QSerialPort::EvenParity;
        } else {
            params.parity = QSerialPort::NoParity;
        }

        // 处理停止位
        params.stopBits = (ui->stopbitsCombo->currentText() == "1") ?
                              QSerialPort::OneStop : QSerialPort::TwoStop;
    } else {
        // 对于Modbus TCP协议，保存网络相关参数
        params.ipAddress = ui->ipLineEdit->text();
        params.port = ui->portSpinBox->value();
    }

    // 设置从站地址，使用当前保存的值
    params.slaveId = pressureSlaveId;
    qDebug() << "保存调压装置从站地址: " << pressureSlaveId;

    // 保存到数据库
    if (ConnectionParamsDao::savePressureParams(params)) {
        QString message = QString("调压装置参数已保存 - 协议: %1").arg(params.protocol);
        if (params.protocol == "Modbus RTU") {
            message += QString(", 串口号: %1, 波特率: %2, 数据位: %3, 校验位: %4, 停止位: %5")
                           .arg(params.serialPort)
                           .arg(params.baudRate)
                           .arg(params.dataBits)
                           .arg(ui->parityCombo->currentText())
                           .arg(ui->stopbitsCombo->currentText());
        } else {
            message += QString(", IP: %1, 端口: %2")
                           .arg(params.ipAddress)
                           .arg(params.port);
        }

        addLog(message);
    } else {
        addLog("调压装置参数保存失败: " + DatabaseManager::getInstance().getLastError());
    }
}

// 保存PLC参数
void ConnectWidget::savePLCParameters()
{
    PLCConnectionParams params;

    // 获取协议类型
    params.protocol = ui->plcProtocolCombo->currentText();

    if (params.protocol == "Modbus RTU") {
        // 对于Modbus RTU协议，保存串口相关参数
        params.serialPort = ui->plcSerialCombo->currentText();
        params.baudRate = ui->plcBaudrateCombo->currentText().toInt();
        params.dataBits = ui->plcDatabitsCombo->currentText().toInt();

        // 处理校验位
        if (ui->plcParityCombo->currentText() == "奇校验") {
            params.parity = QSerialPort::OddParity;
        } else if (ui->plcParityCombo->currentText() == "偶校验") {
            params.parity = QSerialPort::EvenParity;
        } else {
            params.parity = QSerialPort::NoParity;
        }

        // 处理停止位
        params.stopBits = (ui->plcStopbitsCombo->currentText() == "1") ?
                              QSerialPort::OneStop : QSerialPort::TwoStop;
    } else {
        // 对于Modbus TCP协议，保存网络相关参数
        params.ipAddress = ui->plcIpEdit->text();
        params.port = ui->plcPortSpin->value();
    }

    // 设置从站地址，使用当前保存的值
    params.slaveId = plcSlaveId;

    // 保存到数据库
    if (ConnectionParamsDao::savePLCParams(params)) {
        addLog("PLC参数已自动保存");
    } else {
        addLog("PLC参数保存失败: " + DatabaseManager::getInstance().getLastError());
    }
}

// 保存气密仪参数
void ConnectWidget::saveAirTightnessParameters()
{
    AirTightnessParams params;

    // 获取连接类型
    params.connectionType = ui->airConnectionTypeCombo->currentText();

    if (params.connectionType == "串口") {
        // 串口连接参数
        params.serialPort = ui->airSerialCombo->currentText();
    } else {
        // 以太网连接参数
        params.ipAddress = ui->airIpEdit->text();
        params.port = ui->airPortSpin->value();
    }

    // 通用串口参数（无论哪种连接类型都可能需要）
    params.baudRate = ui->airBaudrateCombo->currentText().toInt();
    params.dataBits = ui->airDatabitsCombo->currentText().toInt();

    // 处理校验位
    if (ui->airParityCombo->currentText() == "奇校验") {
        params.parity = QSerialPort::OddParity;
    } else if (ui->airParityCombo->currentText() == "偶校验") {
        params.parity = QSerialPort::EvenParity;
    } else {
        params.parity = QSerialPort::NoParity;
    }

    // 处理停止位
    params.stopBits = (ui->airStopbitsCombo->currentText() == "1") ?
                          QSerialPort::OneStop : QSerialPort::TwoStop;

    // 保存到数据库
    if (ConnectionParamsDao::saveAirTightnessParams(params)) {
        addLog("气密仪参数已自动保存");
    } else {
        addLog("气密仪参数保存失败: " + DatabaseManager::getInstance().getLastError());
    }
}


// 测试PLC连接
void ConnectWidget::on_plcConnectBtn_clicked()
{
    ui->plcConnectBtn->setEnabled(false);
    ui->plcConnectBtn->setText("连接中...");
    addLog("开始连接PLC...");

    // 先断开可能存在的连接（先通知外部清空，再删除）
    if (plcModbusClient) {
        emit modbusClientUpdated(true, nullptr);
        plcModbusClient->disconnectDevice();
        delete plcModbusClient;
        plcModbusClient = nullptr;
    }

    QString protocol = ui->plcProtocolCombo->currentText();

    if (protocol == "Modbus RTU") {
        // 创建Modbus RTU客户端
        plcModbusClient = new QModbusRtuSerialClient(this);

        if (!plcModbusClient) {
            ui->plcConnectBtn->setEnabled(true);
            ui->plcConnectBtn->setText("连接");
            showMessage(ui->plcMessageLabel, "无法创建Modbus RTU客户端", false);
            addLog("PLC连接失败: 无法创建Modbus RTU客户端");
            return;
        }

        // 设置串口参数
        QString portName = ui->plcSerialCombo->currentText();
        int baudRate = ui->plcBaudrateCombo->currentText().toInt();
        int dataBits = ui->plcDatabitsCombo->currentText().toInt();

        QSerialPort::Parity parity = QSerialPort::NoParity;
        if (ui->plcParityCombo->currentText() == "奇校验")
            parity = QSerialPort::OddParity;
        else if (ui->plcParityCombo->currentText() == "偶校验")
            parity = QSerialPort::EvenParity;

        QSerialPort::StopBits stopBits = (ui->plcStopbitsCombo->currentText() == "1") ?
                                             QSerialPort::OneStop : QSerialPort::TwoStop;

        // 配置连接参数
        plcModbusClient->setConnectionParameter(QModbusDevice::SerialPortNameParameter, portName);
        plcModbusClient->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, baudRate);
        plcModbusClient->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, dataBits);
        plcModbusClient->setConnectionParameter(QModbusDevice::SerialParityParameter, parity);
        plcModbusClient->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, stopBits);

    } else if (protocol == "Modbus TCP") {
        // 创建Modbus TCP客户端
        plcModbusClient = new QModbusTcpClient(this);

        if (!plcModbusClient) {
            ui->plcConnectBtn->setEnabled(true);
            ui->plcConnectBtn->setText("连接");
            showMessage(ui->plcMessageLabel, "无法创建Modbus TCP客户端", false);
            addLog("PLC连接失败: 无法创建Modbus TCP客户端");
            return;
        }

        // 设置网络参数
        QString ip = ui->plcIpEdit->text();
        int port = ui->plcPortSpin->value();

        plcModbusClient->setConnectionParameter(QModbusDevice::NetworkAddressParameter, ip);
        plcModbusClient->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
    }

    if (plcModbusClient) {
        // 为Modbus RTU通信设置合适的超时和重试设置
        plcModbusClient->setTimeout(3000); // 3秒超时，适应Modbus RTU通信
        plcModbusClient->setNumberOfRetries(2); // 2次重试

        // 连接信号槽
        connect(plcModbusClient, &QModbusDevice::stateChanged,
                this, &ConnectWidget::onPlcStateChanged);

        // 连接错误信号
        connect(plcModbusClient, &QModbusDevice::errorOccurred,
                this, [this](QModbusDevice::Error error) {
                    if (error != QModbusDevice::NoError && plcConnected) {
                        addLog(QString("PLC通信错误: %1").arg(plcModbusClient->errorString()));
                        plcConnected = false;
                        updateConnectionStatus(true, false);
                        ui->plcConnectBtn->setEnabled(true);
                        ui->plcConnectBtn->setText("连接");
                    }
                });

        // 尝试连接设备
        if (!plcModbusClient->connectDevice()) {
            ui->plcConnectBtn->setEnabled(true);
            ui->plcConnectBtn->setText("连接");
            showMessage(ui->plcMessageLabel, plcModbusClient->errorString(), false);
            addLog("PLC连接失败: " + plcModbusClient->errorString());
        }
    }
}

// PLC状态变化处理
void ConnectWidget::onPlcStateChanged(QModbusDevice::State state)
{
    if (state == QModbusDevice::ConnectedState) {
        // 连接成功
        plcConnected = true;
        ui->plcConnectBtn->setEnabled(false);
        ui->plcConnectBtn->setText("已连接");
        showMessage(ui->plcMessageLabel, "PLC连接成功", true);
        addLog("PLC连接成功");
        updateConnectionStatus(true, true);
        
        // 自动保存PLC参数
        savePLCParameters();
        
        // 设置自动连接标志
        ConnectionParamsDao::setAutoConnect("PLC", true);
        
        // 更新连接状态和时间戳
        ConnectionParamsDao::updateConnectionStatus("PLC", true, "连接成功");
        
        // 保存连接历史
        ConnectionParamsDao::saveConnectionHistory("PLC", "success", "");
        
        // 发送Modbus客户端对象更新信号
        emit modbusClientUpdated(true, plcModbusClient);

        // 如果是连接所有流程，继续连接气密仪
        // connect-all 逻辑已移除
    } else if (state == QModbusDevice::UnconnectedState) {
        if (plcConnected) {
            // 如果之前是连接状态，现在断开了，记录日志
            plcConnected = false;
            addLog("PLC连接意外断开");
            updateConnectionStatus(true, false);
            ui->plcConnectBtn->setEnabled(true);
            ui->plcConnectBtn->setText("连接");
            
            // 更新连接状态
            ConnectionParamsDao::updateConnectionStatus("PLC", false, "连接断开");
        } else if (!plcConnected && plcModbusClient) {
            // 连接尝试失败
            ui->plcConnectBtn->setEnabled(true);
            ui->plcConnectBtn->setText("连接");
            showMessage(ui->plcMessageLabel, plcModbusClient->errorString(), false);
            addLog("PLC连接失败: " + plcModbusClient->errorString());
            
            // 更新连接状态
            ConnectionParamsDao::updateConnectionStatus("PLC", false, plcModbusClient->errorString());
            
            // 保存连接历史
            ConnectionParamsDao::saveConnectionHistory("PLC", "failed", plcModbusClient->errorString());

            // 连接失败：恢复按钮状态（connect-all已移除）
        }
    }
}

// 测试气密仪连接
void ConnectWidget::on_airConnectBtn_clicked()
{
    ui->airConnectBtn->setEnabled(false);
    ui->airConnectBtn->setText("连接中...");
    addLog("开始连接气密仪...");

    // 先断开可能存在的连接（先通知外部清空，再删除）
    if (airModbusClient) {
        emit modbusClientUpdated(false, nullptr);
        airModbusClient->disconnectDevice();
        delete airModbusClient;
        airModbusClient = nullptr;
    }

    QString type = ui->airConnectionTypeCombo->currentText();

    if (type == "串口") {
        // 创建Modbus RTU客户端
        airModbusClient = new QModbusRtuSerialClient(this);

        if (!airModbusClient) {
            ui->airConnectBtn->setEnabled(true);
            ui->airConnectBtn->setText("连接");
            showMessage(ui->airMessageLabel, "无法创建Modbus RTU客户端", false);
            addLog("气密仪连接失败: 无法创建Modbus RTU客户端");
            return;
        }

        // 设置串口参数
        QString portName = ui->airSerialCombo->currentText();
        int baudRate = ui->airBaudrateCombo->currentText().toInt();
        int dataBits = ui->airDatabitsCombo->currentText().toInt();

        QSerialPort::Parity parity = QSerialPort::NoParity;
        if (ui->airParityCombo->currentText() == "奇校验")
            parity = QSerialPort::OddParity;
        else if (ui->airParityCombo->currentText() == "偶校验")
            parity = QSerialPort::EvenParity;

        QSerialPort::StopBits stopBits = (ui->airStopbitsCombo->currentText() == "1") ?
                                             QSerialPort::OneStop : QSerialPort::TwoStop;

        // 配置连接参数
        airModbusClient->setConnectionParameter(QModbusDevice::SerialPortNameParameter, portName);
        airModbusClient->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, baudRate);
        airModbusClient->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, dataBits);
        airModbusClient->setConnectionParameter(QModbusDevice::SerialParityParameter, parity);
        airModbusClient->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, stopBits);

    } else if (type == "以太网") {
        // 创建Modbus TCP客户端
        airModbusClient = new QModbusTcpClient(this);

        if (!airModbusClient) {
            ui->airConnectBtn->setEnabled(true);
            ui->airConnectBtn->setText("连接");
            showMessage(ui->airMessageLabel, "无法创建Modbus TCP客户端", false);
            addLog("气密仪连接失败: 无法创建Modbus TCP客户端");
            return;
        }

        // 设置网络参数
        QString ip = ui->airIpEdit->text();
        int port = ui->airPortSpin->value();

        airModbusClient->setConnectionParameter(QModbusDevice::NetworkAddressParameter, ip);
        airModbusClient->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
    }

    if (airModbusClient) {
        // 为Modbus通信设置合适的超时和重试设置
        airModbusClient->setTimeout(1000); // 3秒超时，适应Modbus RTU通信
        airModbusClient->setNumberOfRetries(3); // 3次重试

        // 连接信号槽
        connect(airModbusClient, &QModbusDevice::stateChanged,
                this, &ConnectWidget::onAirStateChanged);

        // 连接错误信号
        connect(airModbusClient, &QModbusDevice::errorOccurred,
                this, [this](QModbusDevice::Error error) {
                    if (error != QModbusDevice::NoError && airConnected) {
                        addLog(QString("气密仪通信错误: %1").arg(airModbusClient->errorString()));
                        airConnected = false;
                        updateConnectionStatus(false, false);
                        ui->airConnectBtn->setEnabled(true);
                        ui->airConnectBtn->setText("连接");
                    }
                });

        // 尝试连接设备
        if (!airModbusClient->connectDevice()) {
            ui->airConnectBtn->setEnabled(true);
            ui->airConnectBtn->setText("连接");
            showMessage(ui->airMessageLabel, airModbusClient->errorString(), false);
            addLog("气密仪连接失败: " + airModbusClient->errorString());
        }
    }
}

// 气密仪状态变化处理
void ConnectWidget::onAirStateChanged(QModbusDevice::State state)
{
    if (state == QModbusDevice::ConnectedState) {
        // 连接成功
        airConnected = true;
        ui->airConnectBtn->setEnabled(false);
        ui->airConnectBtn->setText("已连接");
        showMessage(ui->airMessageLabel, "气密仪连接成功", true);
        addLog("气密仪连接成功");
        updateConnectionStatus(false, true);
        
        // 自动保存气密仪参数
        saveAirTightnessParameters();
        
        // 设置自动连接标志
        ConnectionParamsDao::setAutoConnect("AirTightness", true);
        
        // 更新连接状态和时间戳
        ConnectionParamsDao::updateConnectionStatus("AirTightness", true, "连接成功");
        
        // 保存连接历史
        ConnectionParamsDao::saveConnectionHistory("AirTightness", "success", "");
        
        // 发送Modbus客户端对象更新信号
        emit modbusClientUpdated(false, airModbusClient);
    } else if (state == QModbusDevice::UnconnectedState) {
        if (airConnected) {
            // 如果之前是连接状态，现在断开了，记录日志
            airConnected = false;
            addLog("气密仪连接意外断开");
            updateConnectionStatus(false, false);
            ui->airConnectBtn->setEnabled(true);
            ui->airConnectBtn->setText("连接");
            
            // 更新连接状态
            ConnectionParamsDao::updateConnectionStatus("AirTightness", false, "连接断开");
        } else if (!airConnected && airModbusClient) {
            // 连接尝试失败
            ui->airConnectBtn->setEnabled(true);
            ui->airConnectBtn->setText("连接");
            showMessage(ui->airMessageLabel, airModbusClient->errorString(), false);
            addLog("气密仪连接失败: " + airModbusClient->errorString());
            
            // 更新连接状态
            ConnectionParamsDao::updateConnectionStatus("AirTightness", false, airModbusClient->errorString());
            
            // 保存连接历史
            ConnectionParamsDao::saveConnectionHistory("AirTightness", "failed", airModbusClient->errorString());

            // 连接失败：恢复按钮状态（connect-all已移除）
        }
    }
}

// 清空日志
// 恢复按钮状态
void ConnectWidget::restoreButtonsState()
{
    // 根据当前连接状态恢复按钮可用性
    ui->plcConnectBtn->setEnabled(!plcConnected);
    ui->airConnectBtn->setEnabled(!airConnected);
}




// 获取Modbus客户端对象
QModbusClient* ConnectWidget::getModbusClient(bool isPlc)
{
    return isPlc ? plcModbusClient : airModbusClient;
}

// 获取PLC从站地址
int ConnectWidget::getPlcSlaveId() const
{
    return plcSlaveId;
}

// 获取气密仪从站地址
int ConnectWidget::getAirtightSlaveId() const
{
    return airtightSlaveId;
}

// 添加日志
void ConnectWidget::addLog(const QString &message)
{
    QString timeStr = getCurrentTimeString();
    QString logMessage = QString("[%1] %2").arg(timeStr).arg(message);
    
    // 只记录到日志文件
    Logger::log("info", message);
}

// 显示消息提示
void ConnectWidget::showMessage(QLabel *label, const QString &text, bool isSuccess)
{
    label->setText(text);
    label->setStyleSheet(QString("font-size: 12px; padding: 6px; border-radius: 4px; %1")
                             .arg(isSuccess ? "background-color: #F6FFED; color: #52C41A;"
                                            : "background-color: #FFF2F0; color: #FF4D4F;"));
    label->setHidden(false);

    // 3秒后隐藏
    QTimer::singleShot(3000, [label]() {
        label->setHidden(true);
    });
}

// 更新连接状态
void ConnectWidget::updateConnectionStatus(bool isPlc, bool connected)
{
    if (isPlc) {
        plcConnected = connected;
        ui->plcConnectStatus->setText(connected ? "已连接" : "未连接");
        ui->plcStatusIndicator->setStyleSheet(connected ?
                                                  "background-color: #52C41A; border-radius: 6px;" :
                                                  "background-color: #FF4D4F; border-radius: 6px;");

        // 更新状态栏中的PLC状态
        ui->plcStatusBarText->setText(connected ? "已连接" : "未连接");
        ui->plcStatusBarIndicator->setStyleSheet(connected ?
                                                     "background-color: #10b981; border-radius: 5px;" :
                                                     "background-color: #ef4444; border-radius: 5px;");

        if (connected) {
            QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            ui->plcLastConnect->setText(QString("最后连接：%1").arg(timeStr));
        }
        // 根据连接状态控制按钮，避免重复连接导致崩溃
        ui->plcConnectBtn->setEnabled(!connected);
        ui->plcConnectBtn->setText(connected ? "已连接" : "连接");
    } else {
        airConnected = connected;
        ui->airConnectStatus->setText(connected ? "已连接" : "未连接");
        ui->airStatusIndicator->setStyleSheet(connected ?
                                                  "background-color: #52C41A; border-radius: 6px;" :
                                                  "background-color: #FF4D4F; border-radius: 6px;");

        // 更新状态栏中的气密仪状态
        ui->airStatusBarText->setText(connected ? "已连接" : "未连接");
        ui->airStatusBarIndicator->setStyleSheet(connected ?
                                                     "background-color: #10b981; border-radius: 5px;" :
                                                     "background-color: #ef4444; border-radius: 5px;");

        if (connected) {
            QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            ui->airLastConnect->setText(QString("最后连接：%1").arg(timeStr));
        }
        // 根据连接状态控制按钮，避免重复连接导致崩溃
        ui->airConnectBtn->setEnabled(!connected);
        ui->airConnectBtn->setText(connected ? "已连接" : "连接");
    }

    // 发射连接状态变化信号
    emit connectionStatusChanged(isPlc, connected);
}

// 获取当前时间字符串
QString ConnectWidget::getCurrentTimeString()
{
    return QDateTime::currentDateTime().toString("hh:mm:ss");
}

// PLC协议选择变化处理
void ConnectWidget::onPlcProtocolChanged(int index)
{
    QString protocol = ui->plcProtocolCombo->itemText(index);
    bool isRtu = (protocol == "Modbus RTU");

    // 显示或隐藏相关控件
    ui->plcIpLabel->setVisible(!isRtu);
    ui->plcIpEdit->setVisible(!isRtu);
    ui->plcPortLabel->setVisible(!isRtu);
    ui->plcPortSpin->setVisible(!isRtu);
    ui->plcSerialLabel->setVisible(isRtu);
    ui->plcSerialCombo->setVisible(isRtu);
    ui->plcBaudrateLabel->setVisible(isRtu);
    ui->plcBaudrateCombo->setVisible(isRtu);
    ui->plcDatabitsLabel->setVisible(isRtu);
    ui->plcDatabitsCombo->setVisible(isRtu);
    ui->plcParityLabel->setVisible(isRtu);
    ui->plcParityCombo->setVisible(isRtu);
    ui->plcStopbitsLabel->setVisible(isRtu);
    ui->plcStopbitsCombo->setVisible(isRtu);

    addLog(QString("PLC协议已切换为: %1").arg(protocol));
}

// 气密仪连接方式变化处理
void ConnectWidget::onAirConnectionTypeChanged(int index)
{
    QString type = ui->airConnectionTypeCombo->itemText(index);
    bool isSerial = (type == "串口");

    // 显示或隐藏相关控件
    ui->airSerialLabel->setVisible(isSerial);
    ui->airSerialCombo->setVisible(isSerial);
    ui->airBaudrateLabel->setVisible(isSerial);
    ui->airBaudrateCombo->setVisible(isSerial);
    ui->airDatabitsLabel->setVisible(isSerial);
    ui->airDatabitsCombo->setVisible(isSerial);
    ui->airParityLabel->setVisible(isSerial);
    ui->airParityCombo->setVisible(isSerial);
    ui->airStopbitsLabel->setVisible(isSerial);
    ui->airStopbitsCombo->setVisible(isSerial);
    ui->airIpLabel->setVisible(!isSerial);
    ui->airIpEdit->setVisible(!isSerial);
    ui->airPortLabel->setVisible(!isSerial);
    ui->airPortSpin->setVisible(!isSerial);

    addLog(QString("气密仪连接方式已切换为: %1").arg(type));
}

void ConnectWidget::on_plcDisconnectBtn_clicked()
{
    // 先通知外部模块清空客户端对象，避免下游使用已删除的指针
    emit modbusClientUpdated(true, nullptr);

    // 主动断开并清理PLC客户端
    if (plcModbusClient) {
        plcModbusClient->disconnectDevice();
        plcModbusClient->deleteLater();
        plcModbusClient = nullptr;
    }

    // 停止并清理测试定时器（如有）
    if (plcTestTimer) {
        plcTestTimer->stop();
        plcTestTimer->deleteLater();
        plcTestTimer = nullptr;
    }

    // 清除自动连接标志（手动断开不应该自动重连）
    ConnectionParamsDao::setAutoConnect("PLC", false);

    // 更新状态与UI
    updateConnectionStatus(true, false);
    ui->plcConnectBtn->setEnabled(true);
    ui->plcConnectBtn->setText("连接");
    showMessage(ui->plcMessageLabel, "PLC已断开", true);
    addLog("已断开PLC连接");
}

void ConnectWidget::on_airDisconnectBtn_clicked()
{
    // 先通知外部模块清空客户端对象，避免下游使用已删除的指针
    emit modbusClientUpdated(false, nullptr);

    // 主动断开并清理气密仪客户端
    if (airModbusClient) {
        airModbusClient->disconnectDevice();
        airModbusClient->deleteLater();
        airModbusClient = nullptr;
    }

    // 停止并清理测试定时器（如有）
    if (airTestTimer) {
        airTestTimer->stop();
        airTestTimer->deleteLater();
        airTestTimer = nullptr;
    }

    // 清除自动连接标志（手动断开不应该自动重连）
    ConnectionParamsDao::setAutoConnect("AirTightness", false);

    // 更新状态与UI
    updateConnectionStatus(false, false);
    ui->airConnectBtn->setEnabled(true);
    ui->airConnectBtn->setText("连接");
    showMessage(ui->airMessageLabel, "气密仪已断开", true);
    addLog("已断开气密仪连接");
}


// ========== 调压装置相关实现 ==========

// 获取调压装置Modbus客户端
QModbusClient* ConnectWidget::getPressureModbusClient() const
{
    return pressureModbusClient;
}

// 获取调压装置从站地址
int ConnectWidget::getPressureSlaveId() const
{
    return pressureSlaveId;
}

// 更新调压装置协议UI
void ConnectWidget::updatePressureProtocolUI()
{
    int index = ui->protocolComboBox->currentIndex();
    bool isTCP = (index == 0); // Modbus TCP
    bool isRTU = (index == 1); // Modbus RTU
    
    // 显示/隐藏TCP相关控件
    ui->ipLabel->setVisible(isTCP);
    ui->ipLineEdit->setVisible(isTCP);
    ui->portLabel->setVisible(isTCP);
    ui->portSpinBox->setVisible(isTCP);
    
    // 显示/隐藏RTU相关控件
    ui->serialPortLabel->setVisible(isRTU);
    ui->serialPortCombo->setVisible(isRTU);
    ui->baudrateLabel->setVisible(isRTU);
    ui->baudrateCombo->setVisible(isRTU);
    ui->databitsLabel->setVisible(isRTU);
    ui->databitsCombo->setVisible(isRTU);
    ui->parityLabel->setVisible(isRTU);
    ui->parityCombo->setVisible(isRTU);
    ui->stopbitsLabel->setVisible(isRTU);
    ui->stopbitsCombo->setVisible(isRTU);
    
    // 设置默认端口
    if (isTCP) {
        ui->portSpinBox->setValue(502);
    }
}

// 更新调压装置状态
void ConnectWidget::updatePressureDeviceStatus(bool connected)
{
    pressureConnected = connected;
    
    if (connected) {
        ui->pressureConnectStatus->setText("已连接");
        ui->pressureConnectStatus->setStyleSheet("font-size: 14px; font-weight: 600; color: #10B981; background: transparent;");
        ui->pressureStatusIndicator->setStyleSheet("background-color: #10b981; border-radius: 6px; border: 2px solid #6ee7b7;");
        
        // 更新状态栏中的调压装置状态
        ui->pressureStatusBarText->setText("已连接");
        ui->pressureStatusBarIndicator->setStyleSheet("background-color: #10b981; border-radius: 5px;");
        
        addLog(QString("[%1] 调压装置连接成功").arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
        // 已连接后重置弹窗标记
        pressureConnectPromptShown = false;
    } else {
        ui->pressureConnectStatus->setText("未连接");
        ui->pressureConnectStatus->setStyleSheet("font-size: 14px; font-weight: 600; color: #475569; background: transparent;");
        ui->pressureStatusIndicator->setStyleSheet("background-color: #ef4444; border-radius: 6px; border: 2px solid #fca5a5;");
        
        // 更新状态栏中的调压装置状态
        ui->pressureStatusBarText->setText("未连接");
        ui->pressureStatusBarIndicator->setStyleSheet("background-color: #ef4444; border-radius: 5px;");
        
        addLog(QString("[%1] 调压装置未连接").arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
    }

    // 广播连接状态变化
    qDebug() << "ConnectWidget::updatePressureDeviceStatus 发射 pressureConnectionChanged 信号，状态:" << (connected ? "已连接" : "未连接");
    emit pressureConnectionChanged(connected);
}

// 设置Modbus客户端参数
void ConnectWidget::setupPressureModbusClientParameters()
{
    if (pressureModbusClient) {
        pressureModbusClient->setTimeout(1000); // 1秒超时
        pressureModbusClient->setNumberOfRetries(3); // 3次重试
    }
}

// 连接到调压装置
void ConnectWidget::connectToPressureDevice()
{
    if (!pressureConnected) {
        int protocolIndex = ui->protocolComboBox->currentIndex();
        
        // 禁用连接按钮
        ui->connectButton->setEnabled(false);
        
        if (protocolIndex == 0) { // Modbus TCP
            QString ip = ui->ipLineEdit->text();
            quint16 port = ui->portSpinBox->value();
            pressureModbusClient->setConnectionParameter(QModbusDevice::NetworkAddressParameter, ip);
            pressureModbusClient->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
            
            if (pressureModbusClient->connectDevice()) {
                addLog(QString("正在连接调压装置 Modbus TCP %1:%2").arg(ip).arg(port));
            } else {
                addLog(QString("调压装置TCP连接失败: %1").arg(pressureModbusClient->errorString()));
                ui->connectButton->setEnabled(true);
            }
        } else if (protocolIndex == 1) { // Modbus RTU
            QString portName = ui->serialPortCombo->currentData().toString();
            if (portName.isEmpty()) {
                portName = ui->serialPortCombo->currentText().split(" ").first();
            }
            
            // 解析串口参数
            int baudrate = ui->baudrateCombo->currentText().toInt();
            int databits = ui->databitsCombo->currentText().toInt();
            
            // 校验位映射
            QSerialPort::Parity parity;
            switch (ui->parityCombo->currentIndex()) {
            case 0: parity = QSerialPort::NoParity; break;
            case 1: parity = QSerialPort::OddParity; break;
            case 2: parity = QSerialPort::EvenParity; break;
            default: parity = QSerialPort::NoParity;
            }
            
            // 停止位映射
            QSerialPort::StopBits stopbits;
            switch (ui->stopbitsCombo->currentIndex()) {
            case 0: stopbits = QSerialPort::OneStop; break;
            case 1: stopbits = QSerialPort::OneAndHalfStop; break;
            case 2: stopbits = QSerialPort::TwoStop; break;
            default: stopbits = QSerialPort::OneStop;
            }
            
            // 设置RTU参数
            pressureModbusClient->setConnectionParameter(QModbusDevice::SerialPortNameParameter, portName);
            pressureModbusClient->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, baudrate);
            pressureModbusClient->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, databits);
            pressureModbusClient->setConnectionParameter(QModbusDevice::SerialParityParameter, parity);
            pressureModbusClient->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, stopbits);
            
            // 尝试连接设备
            if (pressureModbusClient->connectDevice()) {
                addLog(QString("正在连接调压装置 Modbus RTU 串口 %1，波特率 %2，从站地址 %3")
                         .arg(portName).arg(baudrate).arg(pressureSlaveId));
            } else {
                addLog(QString("调压装置RTU连接失败: %1").arg(pressureModbusClient->errorString()));
                ui->connectButton->setEnabled(true);
            }
        }
    }
}

// 断开调压装置连接
void ConnectWidget::disconnectFromPressureDevice()
{
    if (pressureConnected) {
        pressureModbusClient->disconnectDevice();
        addLog("断开调压装置连接");
    }
}

// 调压装置状态变化处理
void ConnectWidget::onPressureStateChanged(QModbusDevice::State state)
{
    QString stateStr;
    switch(state) {
    case QModbusDevice::ConnectedState: stateStr = "ConnectedState"; break;
    case QModbusDevice::ConnectingState: stateStr = "ConnectingState"; break;
    case QModbusDevice::UnconnectedState: stateStr = "UnconnectedState"; break;
    case QModbusDevice::ClosingState: stateStr = "ClosingState"; break;
    default: stateStr = "UnknownState";
    }
    
    qDebug() << "调压装置Modbus状态变化:" << stateStr << "pressureWasConnected:" << pressureWasConnected;
    
    if (state == QModbusDevice::ConnectedState) {
        if (!pressureWasConnected) {
            pressureWasConnected = true;
            onPressureConnected();
        }
    } else if (state == QModbusDevice::UnconnectedState) {
        if (pressureWasConnected) {
            onPressureDisconnected();
            pressureWasConnected = false;
        }
    } else if (state == QModbusDevice::ConnectingState) {
        pressureWasConnected = false;
    }
}

// 调压装置连接成功
void ConnectWidget::onPressureConnected()
{
    int protocolIndex = ui->protocolComboBox->currentIndex();
    if (protocolIndex == 0) { // TCP
        addLog(QString("调压装置Modbus连接成功: %1:%2")
                      .arg(pressureModbusClient->connectionParameter(QModbusDevice::NetworkAddressParameter).toString())
                      .arg(pressureModbusClient->connectionParameter(QModbusDevice::NetworkPortParameter).toInt()));
    } else if (protocolIndex == 1) { // RTU
        QString portName = pressureModbusClient->connectionParameter(QModbusDevice::SerialPortNameParameter).toString();
        int baudrate = pressureModbusClient->connectionParameter(QModbusDevice::SerialBaudRateParameter).toInt();
        addLog(QString("调压装置Modbus RTU连接成功: %1，波特率 %2，从站地址 %3").arg(portName).arg(baudrate).arg(pressureSlaveId));
    }
    
    ui->connectButton->setEnabled(false);
    ui->disconnectButton->setEnabled(true);
    
    // 自动保存调压装置参数
    savePressureParameters();
    
    // 设置自动连接标志
    ConnectionParamsDao::setAutoConnect("Pressure", true);
    
    // 更新连接状态和时间戳
    ConnectionParamsDao::updateConnectionStatus("Pressure", true, "连接成功");
    
    // 保存连接历史
    ConnectionParamsDao::saveConnectionHistory("Pressure", "success", "");
    
    // 更新调压装置状态
    updatePressureDeviceStatus(true);
}

// 调压装置断开连接
void ConnectWidget::onPressureDisconnected()
{
    addLog("调压装置Modbus连接已断开");
    ui->connectButton->setEnabled(true);
    ui->disconnectButton->setEnabled(false);
    
    // 更新连接状态
    ConnectionParamsDao::updateConnectionStatus("Pressure", false, "连接断开");
    
    // 更新调压装置状态
    updatePressureDeviceStatus(false);
}

// 调压装置错误处理
void ConnectWidget::onPressureErrorOccurred(QModbusDevice::Error error)
{
    QString errorDetails;
    switch (error) {
    case QModbusDevice::ConnectionError: errorDetails = "连接错误（端口不存在或被占用）"; break;
    case QModbusDevice::ConfigurationError: errorDetails = "配置错误（参数不合法）"; break;
    case QModbusDevice::ReadError: errorDetails = "读取错误"; break;
    case QModbusDevice::WriteError: errorDetails = "写入错误"; break;
    case QModbusDevice::TimeoutError: errorDetails = "超时错误（设备未响应）"; break;
    default: errorDetails = "未知错误";
    }
    addLog(QString("调压装置Modbus错误: %1（%2）").arg(pressureModbusClient->errorString()).arg(errorDetails));
    
    // 如果是连接错误，保存连接历史
    if (error == QModbusDevice::ConnectionError || error == QModbusDevice::TimeoutError) {
        // 更新连接状态
        ConnectionParamsDao::updateConnectionStatus("Pressure", false, pressureModbusClient->errorString());
        
        // 保存连接历史
        ConnectionParamsDao::saveConnectionHistory("Pressure", "failed", pressureModbusClient->errorString());
    }
    
    // 连接失败时启用连接按钮
    ui->connectButton->setEnabled(true);
}

// 协议选择变化
void ConnectWidget::on_protocolComboBox_currentIndexChanged(int index)
{
    // 切换协议时断开当前连接
    if (pressureConnected) {
        disconnectFromPressureDevice();
    }
    
    // 销毁旧客户端
    if (pressureModbusClient) {
        disconnect(pressureModbusClient, nullptr, this, nullptr);
        pressureModbusClient->deleteLater();
        pressureModbusClient = nullptr;
    }
    
    // 创建新客户端
    if (index == 0) { // Modbus TCP
        pressureModbusClient = new QModbusTcpClient(this);
    } else if (index == 1) { // Modbus RTU
        pressureModbusClient = new QModbusRtuSerialClient(this);
    }
    
    // 配置客户端参数并连接信号
    if (pressureModbusClient) {
        setupPressureModbusClientParameters();
        connect(pressureModbusClient, &QModbusDevice::stateChanged, this, &ConnectWidget::onPressureStateChanged);
        connect(pressureModbusClient, &QModbusDevice::errorOccurred, this, &ConnectWidget::onPressureErrorOccurred);
        // 客户端对象已替换，广播更新
        emit pressureClientUpdated(pressureModbusClient);
    }
    
    updatePressureProtocolUI();
}

// 连接按钮点击
void ConnectWidget::on_connectButton_clicked()
{
    if (!pressureConnected) {
        connectToPressureDevice();
    }
}

// 断开按钮点击
void ConnectWidget::on_disconnectButton_clicked()
{
    if (pressureConnected) {
        // 清除自动连接标志（手动断开不应该自动重连）
        ConnectionParamsDao::setAutoConnect("Pressure", false);
        
        disconnectFromPressureDevice();
    }
}


// ========== 自动连接相关实现 ==========

// 检查设备是否应该自动连接
bool ConnectWidget::shouldAutoConnect(const QString &deviceType)
{
    // 检查全局自动连接开关
    if (!autoConnectEnabled) {
        return false;
    }
    
    // 从数据库获取设备的自动连接标志
    bool autoConnect = ConnectionParamsDao::getAutoConnect(deviceType);
    
    return autoConnect;
}

// 执行自动连接
void ConnectWidget::performAutoConnect()
{
    addLog("开始执行自动连接检查...");
    
    // 按顺序尝试连接三个设备
    // 1. 连接PLC
    if (shouldAutoConnect("PLC")) {
        addLog("检测到PLC需要自动连接");
        autoConnectPLC();
    } else {
        addLog("PLC不需要自动连接");
    }
    
    // 2. 连接气密仪
    if (shouldAutoConnect("AirTightness")) {
        addLog("检测到气密仪需要自动连接");
        autoConnectAirTightness();
    } else {
        addLog("气密仪不需要自动连接");
    }
    
    // 3. 连接调压装置
    if (shouldAutoConnect("Pressure")) {
        addLog("检测到调压装置需要自动连接");
        autoConnectPressure();
    } else {
        addLog("调压装置不需要自动连接");
    }
    
    addLog("自动连接检查完成");
}

// 自动连接PLC
void ConnectWidget::autoConnectPLC()
{
    if (plcConnected) {
        addLog("PLC已经连接，跳过自动连接");
        return;
    }
    
    addLog("正在自动连接PLC...");
    
    // 触发PLC连接按钮的点击事件
    on_plcConnectBtn_clicked();
}

// 自动连接气密仪
void ConnectWidget::autoConnectAirTightness()
{
    if (airConnected) {
        addLog("气密仪已经连接，跳过自动连接");
        return;
    }
    
    addLog("正在自动连接气密仪...");
    
    // 触发气密仪连接按钮的点击事件
    on_airConnectBtn_clicked();
}

// 自动连接调压装置
void ConnectWidget::autoConnectPressure()
{
    if (pressureConnected) {
        addLog("调压装置已经连接，跳过自动连接");
        return;
    }
    
    // 检查是否有调压装置参数配置
    PressureDeviceParams params = ConnectionParamsDao::loadPressureParams();
    if (params.protocol.isEmpty()) {
        addLog("调压装置参数未配置，跳过自动连接");
        return;
    }
    
    addLog("正在自动连接调压装置...");
    
    // 触发调压装置连接按钮的点击事件
    on_connectButton_clicked();
}
