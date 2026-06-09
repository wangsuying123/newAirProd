#include "debugpage.h"
#include "ui_debugpage.h"
#include <QMessageBox>
#include <QDateTime>
#include <QListWidget>
#include <QScrollBar>
#include <QModbusDataUnit>
#include <QEventLoop>
#include <QTimer>
#include <QDebug>
#include <QThread>
#include <QTextCursor>
#include "AirTightnessParamsDao.h"
#include "VolumeEncoding.h"
// 添加泄漏单位类型头文件，支持单位编码转换
#include "LeakUnitType.h"

// 初始化静态成员变量
int DebugPage::m_flashCounter = 0;

DebugPage::DebugPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DebugPage)
    , timeTimer(new QTimer(this))
    , m_plcModbusClient(nullptr)
    , m_airtightModbusClient(nullptr)
    , m_isPlcConnected(false)
    , m_isAirtightConnected(false)
    , m_plcDeviceAddress(2)
    , m_airtightDeviceAddress(1)
    , m_isProcessingButtonClick(false)
    , m_reg100(0)
    , m_reg101(0)
    , m_reg102(0)
    , m_reg103(0)
    , m_flashTimer(new QTimer(this))
    , m_plcSyncTimer(new QTimer(this))
    , m_isWarningActive(false)
    , m_pressureModbusClient(nullptr)
    , m_isPressureConnected(false)
    , m_pressureDeviceAddress(1)
{
    ui->setupUi(this);
    initUI();

    // 连接按钮信号槽（左列“开”与右列“关”分离事件）
    connect(ui->trayInBtn, &QPushButton::clicked, this, &DebugPage::on_trayInBtn_clicked);
    connect(ui->trayInOffBtn, &QPushButton::clicked, this, &DebugPage::on_trayInOffBtn_clicked);
    connect(ui->trayOutBtn, &QPushButton::clicked, this, &DebugPage::on_trayOutBtn_clicked);
    connect(ui->trayOutOffBtn, &QPushButton::clicked, this, &DebugPage::on_trayOutOffBtn_clicked);
    connect(ui->mainCylinderInBtn, &QPushButton::clicked, this, &DebugPage::on_mainCylinderInBtn_clicked);
    connect(ui->mainCylinderInOffBtn, &QPushButton::clicked, this, &DebugPage::on_mainCylinderInOffBtn_clicked);
    connect(ui->mainCylinderOutBtn, &QPushButton::clicked, this, &DebugPage::on_mainCylinderOutBtn_clicked);
    connect(ui->mainCylinderOutOffBtn, &QPushButton::clicked, this, &DebugPage::on_mainCylinderOutOffBtn_clicked);
    connect(ui->block1Btn, &QPushButton::clicked, this, &DebugPage::on_block1Btn_clicked);
    connect(ui->block1OffBtn, &QPushButton::clicked, this, &DebugPage::on_block1OffBtn_clicked);
    connect(ui->block2Btn, &QPushButton::clicked, this, &DebugPage::on_block2Btn_clicked);
    connect(ui->block2OffBtn, &QPushButton::clicked, this, &DebugPage::on_block2OffBtn_clicked);
    connect(ui->block3Btn, &QPushButton::clicked, this, &DebugPage::on_block3Btn_clicked);
    connect(ui->block3OffBtn, &QPushButton::clicked, this, &DebugPage::on_block3OffBtn_clicked);
    connect(ui->instrumentStartBtn, &QPushButton::clicked, this, &DebugPage::on_instrumentStartBtn_clicked);
    connect(ui->instrumentResetBtn, &QPushButton::clicked, this, &DebugPage::on_instrumentResetBtn_clicked);
    connect(ui->inflateValve1Btn, &QPushButton::clicked, this, &DebugPage::on_inflateValve1Btn_clicked);
    connect(ui->inflateValve1OffBtn, &QPushButton::clicked, this, &DebugPage::on_inflateValve1OffBtn_clicked);
    connect(ui->exhaustValve1Btn, &QPushButton::clicked, this, &DebugPage::on_exhaustValve1Btn_clicked);
    connect(ui->exhaustValve1OffBtn, &QPushButton::clicked, this, &DebugPage::on_exhaustValve1OffBtn_clicked);
    connect(ui->inflateValve2Btn, &QPushButton::clicked, this, &DebugPage::on_inflateValve2Btn_clicked);
    connect(ui->inflateValve2OffBtn, &QPushButton::clicked, this, &DebugPage::on_inflateValve2OffBtn_clicked);
    connect(ui->exhaustValve2Btn, &QPushButton::clicked, this, &DebugPage::on_exhaustValve2Btn_clicked);
    connect(ui->exhaustValve2OffBtn, &QPushButton::clicked, this, &DebugPage::on_exhaustValve2OffBtn_clicked);
    connect(ui->inflateValve3Btn, &QPushButton::clicked, this, &DebugPage::on_inflateValve3Btn_clicked);
    connect(ui->inflateValve3OffBtn, &QPushButton::clicked, this, &DebugPage::on_inflateValve3OffBtn_clicked);
    connect(ui->exhaustValve3Btn, &QPushButton::clicked, this, &DebugPage::on_exhaustValve3Btn_clicked);
    connect(ui->exhaustValve3OffBtn, &QPushButton::clicked, this, &DebugPage::on_exhaustValve3OffBtn_clicked);
    connect(ui->manualBtn, &QPushButton::clicked, this, &DebugPage::on_manualBtn_clicked);
    connect(ui->manualOffBtn, &QPushButton::clicked, this, &DebugPage::on_manualOffBtn_clicked);
    connect(ui->totalSealOnBtn, &QPushButton::clicked, this, &DebugPage::on_totalSealOnBtn_clicked);
    connect(ui->totalSealOffBtn, &QPushButton::clicked, this, &DebugPage::on_totalSealOffBtn_clicked);
    connect(ui->okBtn, &QPushButton::clicked, this, &DebugPage::on_okBtn_clicked);
    connect(ui->tdBtn, &QPushButton::clicked, this, &DebugPage::on_tdBtn_clicked);
    connect(ui->rdBtn, &QPushButton::clicked, this, &DebugPage::on_rdBtn_clicked);
    connect(ui->alBtn, &QPushButton::clicked, this, &DebugPage::on_alBtn_clicked);
    connect(ui->runBtn, &QPushButton::clicked, this, &DebugPage::on_runBtn_clicked);
}

DebugPage::~DebugPage()
{
    // 先停止所有定时器，防止定时器回调访问已删除的UI组件
    if (m_flashTimer) { m_flashTimer->stop(); }
    if (timeTimer) { timeTimer->stop(); }
    if (m_plcSyncTimer) { m_plcSyncTimer->stop(); }
    
    // 断开所有信号连接
    disconnect();
    
    // 删除UI组件
    delete ui;
    
    // 清理定时器
    if (timeTimer) { delete timeTimer; }
    if (m_flashTimer) { delete m_flashTimer; }
    if (m_plcSyncTimer) { delete m_plcSyncTimer; }
    
    // 断开Modbus连接
    if (m_plcModbusClient && m_plcModbusClient->state() != QModbusDevice::UnconnectedState) {
        m_plcModbusClient->disconnectDevice();
    }
    if (m_airtightModbusClient && m_airtightModbusClient->state() != QModbusDevice::UnconnectedState) {
        m_airtightModbusClient->disconnectDevice();
    }
}

// 设置PLC客户端并强化连接逻辑
void DebugPage::setPlcModbusClient(QModbusClient *client)
{
    if (m_plcModbusClient) {
        disconnect(m_plcModbusClient, nullptr, this, nullptr);
    }

    m_plcModbusClient = client;
    m_isPlcConnected = false;

    if (m_plcModbusClient) {
        // 连接状态变化时的处理
        connect(m_plcModbusClient, &QModbusDevice::stateChanged, this, [this](QModbusDevice::State state) {
            bool newConnected = (state == QModbusDevice::ConnectedState);
            m_isPlcConnected = newConnected;
            onConnectionStatusChanged(true, newConnected);
        });

        // 错误信号连接
        connect(m_plcModbusClient, &QModbusDevice::errorOccurred, this, [this](QModbusDevice::Error error) {
            qDebug() << "PLC错误发生:" << m_plcModbusClient->errorString() << "错误码:" << error;
            showFeedback("PLC通讯错误: " + m_plcModbusClient->errorString());
        });

        m_isPlcConnected = (m_plcModbusClient->state() == QModbusDevice::ConnectedState);
        onConnectionStatusChanged(true, m_isPlcConnected);
    } else {
        onConnectionStatusChanged(true, false);
    }
}

void DebugPage::setAirtightModbusClient(QModbusClient *client)
{
    if (m_airtightModbusClient) {
        disconnect(m_airtightModbusClient, nullptr, this, nullptr);
    }

    m_airtightModbusClient = client;
    m_isAirtightConnected = false;

    if (m_airtightModbusClient) {
        connect(m_airtightModbusClient, &QModbusDevice::stateChanged, this, [this](QModbusDevice::State state) {
            m_isAirtightConnected = (state == QModbusDevice::ConnectedState);
            onConnectionStatusChanged(false, m_isAirtightConnected);
        });
        m_isAirtightConnected = (m_airtightModbusClient->state() == QModbusDevice::ConnectedState);
        onConnectionStatusChanged(false, m_isAirtightConnected);
    } else {
        onConnectionStatusChanged(false, false);
    }
}

void DebugPage::setPlcDeviceAddress(int address)
{
    if (address >= 1 && address <= 255) {
        m_plcDeviceAddress = address;
        qDebug() << "PLC设备地址已设置为:" << address;
    }
}

void DebugPage::setAirtightDeviceAddress(int address)
{
    if (address >= 1 && address <= 255) {
        m_airtightDeviceAddress = address;
    }
}

void DebugPage::setPressureModbusClient(QModbusClient *client)
{
    if (m_pressureModbusClient) {
        disconnect(m_pressureModbusClient, nullptr, this, nullptr);
    }
    m_pressureModbusClient = client;
    m_isPressureConnected = (client && client->state() == QModbusDevice::ConnectedState);

    if (m_pressureModbusClient) {
        connect(m_pressureModbusClient, &QModbusDevice::stateChanged, this, [this](QModbusDevice::State state){
            bool connected = (state == QModbusDevice::ConnectedState);
            if (m_isPressureConnected != connected) {
                m_isPressureConnected = connected;
                qDebug() << "调试页面：调压装置连接状态变化:" << connected;
            }
        });
        qDebug() << "调试页面：已设置调压装置Modbus客户端，当前连接状态:" << m_isPressureConnected;
    } else {
        qDebug() << "调试页面：未设置调压装置Modbus客户端（nullptr）";
        m_isPressureConnected = false;
    }
}

void DebugPage::setPressureDeviceAddress(int address)
{
    m_pressureDeviceAddress = address;
}

bool DebugPage::sendPressureToRegulator(const AirTightnessFullParams& params)
{
    // 检查调压装置连接状态
    if (!m_pressureModbusClient || m_pressureModbusClient->state() != QModbusDevice::ConnectedState) {
        recordOperation("无法向调压装置发送压力值: 调压装置未连接");
        return false;
    }

    // 准备压力值，将参数中的调压阀压力值写入到寄存器80
    quint16 pressureValue = static_cast<quint16>((params.pressure.regulatorPressure + 3) / 0.18);
    recordOperation(QString("准备向调压装置发送压力值: %1").arg(pressureValue));

    // 使用功能码06（Write Single Register）写入单个寄存器
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, 80, 1);
    writeUnit.setValue(0, pressureValue);

    // 发送写入请求到调压装置
    QModbusReply *reply = m_pressureModbusClient->sendWriteRequest(writeUnit, m_pressureDeviceAddress);
    if (!reply) {
        recordOperation(QString("向调压装置发送压力值请求失败: %1").arg(m_pressureModbusClient->errorString()));
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

    bool success = false;
    if (timer.isActive()) {
        timer.stop();
        if (reply->error() == QModbusDevice::NoError) {
            recordOperation(QString("成功向调压装置发送压力值: %1").arg(pressureValue));
            success = true;
        } else {
            recordOperation(QString("向调压装置发送压力值失败: %1, 值: %2").arg(reply->errorString()).arg(pressureValue));
        }
        reply->deleteLater();
    } else {
        recordOperation(QString("向调压装置发送压力值超时，值: %1").arg(pressureValue));
        disconnect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
        reply->deleteLater();
    }

    return success;
}

void DebugPage::initUI()
{
    // 时间更新
    timeTimer->start(1000);
    connect(timeTimer, &QTimer::timeout, this, &DebugPage::updateTime);
    updateTime();

    // 按钮闪烁和状态同步
    m_flashTimer->start(500);
    connect(m_flashTimer, &QTimer::timeout, this, &DebugPage::updateButtonStyles);

    // 初始化状态标签
    // ui->feedbackLabel->setText("系统就绪，等待操作..."); // 已移除feedbackLabel
    ui->instrumentChannelEdit->setText("2");

    ui->plcStatusLabel->setText("PLC未连接");
    ui->plcStatusLabel->setStyleSheet("color: #e74c3c; background-color: #fadbd8; border-radius: 4px; padding: 4px 8px; font-size: 10px; font-weight: bold; border: 1px solid #f1948a;");

    ui->ateqStatusLabel->setText("ATEQ未连接");
    ui->ateqStatusLabel->setStyleSheet("color: #e74c3c; background-color: #fadbd8; border-radius: 4px; padding: 4px 8px; font-size: 10px; font-weight: bold; border: 1px solid #f1948a;");

    // 初始化按钮状态
    QString defaultStyle = "QPushButton { background-color: #bdc3c7; color:rgb(5, 5, 5); border: 1px solid #95a5a6; border-radius: 3px; padding: 5px; }";
    ui->okBtn->setStyleSheet(defaultStyle);
    ui->tdBtn->setStyleSheet(defaultStyle);
    ui->rdBtn->setStyleSheet(defaultStyle);
    ui->alBtn->setStyleSheet(defaultStyle);
    ui->runBtn->setStyleSheet(defaultStyle);

    // 初始化所有开关按钮为关闭状态（红色）
    initializeSwitchButtons();
    
    // PLC连接后再同步按钮状态，避免初始化时访问未连接的PLC
    // syncPlcButtonStates(); // 移到连接成功后调用
}

void DebugPage::onConnectionStatusChanged(bool isPlc, bool connected)
{
    if (isPlc) {
        if (connected) {
            ui->plcStatusLabel->setText("PLC通讯正常");
            ui->plcStatusLabel->setStyleSheet("color: #27ae60; background-color: #e8f5e9; border-radius: 4px; padding: 4px 8px; font-size: 10px; font-weight: bold; border: 1px solid #c8e6c9;");
            // PLC连接就绪后立即同步按钮颜色
            syncPlcButtonStates();
        } else {
            ui->plcStatusLabel->setText("PLC未连接");
            ui->plcStatusLabel->setStyleSheet("color: #e74c3c; background-color: #fadbd8; border-radius: 4px; padding: 4px 8px; font-size: 10px; font-weight: bold; border: 1px solid #f1948a;");
        }
    } else {
        if (connected) {
            ui->ateqStatusLabel->setText("ATEQ通讯正常");
            ui->ateqStatusLabel->setStyleSheet("color: #27ae60; background-color: #e8f5e9; border-radius: 4px; padding: 4px 8px; font-size: 10px; font-weight: bold; border: 1px solid #c8e6c9;");
        } else {
            ui->ateqStatusLabel->setText("ATEQ未连接");
            ui->ateqStatusLabel->setStyleSheet("color: #e74c3c; background-color: #fadbd8; border-radius: 4px; padding: 4px 8px; font-size: 10px; font-weight: bold; border: 1px solid #f1948a;");
        }
    }
}

void DebugPage::updateTime()
{
    ui->timeLabel->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
}

void DebugPage::onRegisterValuesChanged(quint16 reg100, quint16 reg101, quint16 reg102, quint16 reg103)
{
    m_reg100 = reg100;
    m_reg101 = reg101;
    m_reg102 = reg102;
    m_reg103 = reg103;
    updateButtonStyles();
}

// 新增：警告按钮状态变化处理（1=警告，0=清除）
void DebugPage::onWarningButtonStateChanged(int state)
{
    m_isWarningActive = (state == 1);
    updateButtonStyles();
}
void DebugPage::showFeedback(const QString &operation)
{
    // ui->feedbackLabel->setText("[" + QDateTime::currentDateTime().toString("HH:mm:ss") + "] " + operation); // 已移除feedbackLabel
    // 可以使用状态栏或日志输出代替
    qDebug() << "[" + QDateTime::currentDateTime().toString("HH:mm:ss") + "] " + operation;
}

void DebugPage::updateButtonStyles()
{
    m_flashCounter++;

    // 仪器状态按钮样式
    ui->okBtn->setStyleSheet(m_reg100 == 1 ?
                                 (m_flashCounter % 2 == 0 ? "background-color: #2ecc71; color: white; font-weight: bold;" : "background-color: #27ae60; color: white; font-weight: bold;") :
                                 "background-color: #95a5a6; color: white;");

    ui->runBtn->setStyleSheet(m_reg101 == 1 ?
                                  (m_flashCounter % 2 == 0 ? "background-color: #3498db; color: white; font-weight: bold;" : "background-color: #2980b9; color: white; font-weight: bold;") :
                                  "background-color: #95a5a6; color: white;");

    ui->tdBtn->setStyleSheet(m_reg102 == 1 ?
                                 (m_flashCounter % 2 == 0 ? "background-color: #e67e22; color: white; font-weight: bold;" : "background-color: #d35400; color: white; font-weight: bold;") :
                                 "background-color: #95a5a6; color: white;");

    ui->rdBtn->setStyleSheet(m_reg103 == 1 ?
                                 (m_flashCounter % 2 == 0 ? "background-color: #9b59b6; color: white; font-weight: bold;" : "background-color: #8e44ad; color: white; font-weight: bold;") :
                                 "background-color: #95a5a6; color: white;");

    // 新增：报警按钮样式统一在此处理（有警告时闪烁红色，无警告时绿色）
    if (m_isWarningActive) {
        ui->alBtn->setStyleSheet(m_flashCounter % 2 == 0 ?
            "background-color: #e74c3c; color: white; font-weight: bold;" :
            "background-color: #c0392b; color: white; font-weight: bold;");
    } else {
        setButtonStyle(ui->alBtn, true); // 无警告显示为绿色（如需默认灰色可改回默认样式）
    }
}



bool DebugPage::writePlcRegister(quint16 address, quint16 value, QModbusDataUnit::RegisterType type)
{
    if (!m_plcModbusClient || !m_isPlcConnected) {
        showFeedback("写入失败: PLC未连接");
        return false;
    }

    QModbusDataUnit writeUnit(type, address, 1);
    writeUnit.setValue(0, value);

    // 最多重试2次
    for (int retry = 0; retry < 2; ++retry) {
        QModbusReply *reply = m_plcModbusClient->sendWriteRequest(writeUnit, m_plcDeviceAddress);
        if (!reply) {
            qDebug() << "写入请求发送失败(重试" << retry << "):" << m_plcModbusClient->errorString();
            continue;
        }

        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        timer.start(2000); // 缩短超时时间，提高响应速度

        connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        loop.exec();

        if (timer.isActive()) {
            timer.stop();
            if (reply->error() == QModbusDevice::NoError) {
                reply->deleteLater();
                qDebug() << "写入成功(重试" << retry << "): 地址" << address << "值" << value;
                return true;
            } else {
                qDebug() << "写入失败(重试" << retry << "):" << reply->errorString() << "地址" << address;
            }
            reply->deleteLater();
        } else {
            reply->deleteLater();
            qDebug() << "写入超时(重试" << retry << "): 地址" << address;
        }
    }

    showFeedback(QString("写入PLC失败: 地址%1，值%2").arg(address).arg(value));
    return false;
}

bool DebugPage::writeAirtightRegister(quint16 address, quint16 value, QModbusDataUnit::RegisterType type)
{
    Q_UNUSED(type);
    if (m_airtightModbusClient && m_isAirtightConnected) {
        QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, address, 2);
        writeUnit.setValue(0, value);
        writeUnit.setValue(1, 0);

        QModbusReply *reply = m_airtightModbusClient->sendWriteRequest(writeUnit, m_airtightDeviceAddress);
        if (!reply) {
            showFeedback("发送气密仪请求失败");
            return false;
        }

        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        timer.start(3000);

        connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        loop.exec();

        bool success = false;
        if (timer.isActive()) {
            timer.stop();
            success = (reply->error() == QModbusDevice::NoError);
            if (!success) {
                showFeedback("气密仪写入失败: " + reply->errorString());
            }
            reply->deleteLater();
        } else {
            reply->deleteLater();
            showFeedback("气密仪写入超时");
        }
        return success;
    }

    showFeedback("气密仪未连接");
    return false;
}

void DebugPage::syncPlcButtonStates()
{
    if (!m_plcModbusClient || !m_isPlcConnected) {
        return;
    }

    // 使用功能码01读取线圈状态（QModbusDataUnit::Coils类型自动对应功能码01）
    // 范围 1516~1540，共 25 个线圈（含总封堵地址1540）
    QModbusDataUnit readUnit(QModbusDataUnit::Coils, 1516, 25);

    QModbusReply *reply = m_plcModbusClient->sendReadRequest(readUnit, m_plcDeviceAddress);
    if (!reply) {
        showFeedback("同步请求发送失败");
        return;
    }

    connect(reply, &QModbusReply::finished, this, [this, reply]() {
        if (reply->error() == QModbusDevice::NoError) {
            QModbusDataUnit result = reply->result();

            auto val = [&result](quint16 addr) -> bool {
                quint16 idx = addr - 1516;
                return idx < result.valueCount() ? (result.value(idx) == 1) : false;
            };

            // 左列“开”按钮样式（开=绿，关=红）
            setButtonStyle(ui->trayInBtn, val(1518));
            setButtonStyle(ui->trayOutBtn, val(1519));
            setButtonStyle(ui->mainCylinderInBtn, val(1522));
            setButtonStyle(ui->mainCylinderOutBtn, val(1523));
            setButtonStyle(ui->block1Btn, val(1526));
            setButtonStyle(ui->block2Btn, val(1527));
            setButtonStyle(ui->block3Btn, val(1528));
            setButtonStyle(ui->inflateValve1Btn, val(1529));
            setButtonStyle(ui->exhaustValve1Btn, val(1530));
            setButtonStyle(ui->inflateValve2Btn, val(1531));
            setButtonStyle(ui->exhaustValve2Btn, val(1532));
            setButtonStyle(ui->inflateValve3Btn, val(1533));
            setButtonStyle(ui->exhaustValve3Btn, val(1534));
            setButtonStyle(ui->manualBtn, val(1516));

            // 右列“关”按钮样式（与开相反：开=红，关=绿）
            setButtonStyle(ui->trayInOffBtn, !val(1518));
            setButtonStyle(ui->trayOutOffBtn, !val(1519));
            setButtonStyle(ui->mainCylinderInOffBtn, !val(1522));
            setButtonStyle(ui->mainCylinderOutOffBtn, !val(1523));
            setButtonStyle(ui->block1OffBtn, !val(1526));
            setButtonStyle(ui->block2OffBtn, !val(1527));
            setButtonStyle(ui->block3OffBtn, !val(1528));
            setButtonStyle(ui->inflateValve1OffBtn, !val(1529));
            setButtonStyle(ui->exhaustValve1OffBtn, !val(1530));
            setButtonStyle(ui->inflateValve2OffBtn, !val(1531));
            setButtonStyle(ui->exhaustValve2OffBtn, !val(1532));
            setButtonStyle(ui->inflateValve3OffBtn, !val(1533));
            setButtonStyle(ui->exhaustValve3OffBtn, !val(1534));
            setButtonStyle(ui->manualOffBtn, !val(1516));

            // 总封堵按钮状态同步（地址1540）
            setButtonStyle(ui->totalSealOnBtn, val(1540));
            setButtonStyle(ui->totalSealOffBtn, !val(1540));
        } else {
            showFeedback("读取PLC状态失败:" + reply->errorString());
        }
        reply->deleteLater();
    });
}

// 增压进（开）按钮点击事件：显式开启
void DebugPage::on_trayInBtn_clicked()
{
    bool success = writePlcRegister(1518, 1, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->trayInBtn, true);
        setButtonStyle(ui->trayInOffBtn, false);
        recordSwitchEvent("增压进", true);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 增压进（关）按钮点击事件：显式关闭
void DebugPage::on_trayInOffBtn_clicked()
{
    bool success = writePlcRegister(1518, 0, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->trayInOffBtn, true);
        setButtonStyle(ui->trayInBtn, false);
        recordSwitchEvent("增压进", false);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 返程进缸（开）
void DebugPage::on_trayOutBtn_clicked()
{
    bool success = writePlcRegister(1519, 1, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->trayOutBtn, true);
        setButtonStyle(ui->trayOutOffBtn, false);
        recordSwitchEvent("返程进缸", true);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 返程进缸（关）
void DebugPage::on_trayOutOffBtn_clicked()
{
    bool success = writePlcRegister(1519, 0, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->trayOutOffBtn, true);
        setButtonStyle(ui->trayOutBtn, false);
        recordSwitchEvent("返程进缸", false);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 主缸进（开）
void DebugPage::on_mainCylinderInBtn_clicked()
{
    bool success = writePlcRegister(1522, 1, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->mainCylinderInBtn, true);
        setButtonStyle(ui->mainCylinderInOffBtn, false);
        recordSwitchEvent("主缸进", true);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 主缸进（关）
void DebugPage::on_mainCylinderInOffBtn_clicked()
{
    bool success = writePlcRegister(1522, 0, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->mainCylinderInOffBtn, true);
        setButtonStyle(ui->mainCylinderInBtn, false);
        recordSwitchEvent("主缸进", false);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 主缸退（开）
void DebugPage::on_mainCylinderOutBtn_clicked()
{
    bool success = writePlcRegister(1523, 1, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->mainCylinderOutBtn, true);
        setButtonStyle(ui->mainCylinderOutOffBtn, false);
        recordSwitchEvent("主缸退", true);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 主缸退（关）
void DebugPage::on_mainCylinderOutOffBtn_clicked()
{
    bool success = writePlcRegister(1523, 0, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->mainCylinderOutOffBtn, true);
        setButtonStyle(ui->mainCylinderOutBtn, false);
        recordSwitchEvent("主缸退", false);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 封堵1（开）
void DebugPage::on_block1Btn_clicked()
{
    bool success = writePlcRegister(1526, 1, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->block1Btn, true);
        setButtonStyle(ui->block1OffBtn, false);
        recordSwitchEvent("封堵1", true);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 封堵1（关）
void DebugPage::on_block1OffBtn_clicked()
{
    bool success = writePlcRegister(1526, 0, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->block1OffBtn, true);
        setButtonStyle(ui->block1Btn, false);
        recordSwitchEvent("封堵1", false);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 封堵2（开）
void DebugPage::on_block2Btn_clicked()
{
    bool success = writePlcRegister(1527, 1, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->block2Btn, true);
        setButtonStyle(ui->block2OffBtn, false);
        recordSwitchEvent("封堵2", true);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 封堵2（关）
void DebugPage::on_block2OffBtn_clicked()
{
    bool success = writePlcRegister(1527, 0, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->block2OffBtn, true);
        setButtonStyle(ui->block2Btn, false);
        recordSwitchEvent("封堵2", false);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 封堵3（开）
void DebugPage::on_block3Btn_clicked()
{
    bool success = writePlcRegister(1528, 1, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->block3Btn, true);
        setButtonStyle(ui->block3OffBtn, false);
        recordSwitchEvent("封堵3", true);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 封堵3（关）
void DebugPage::on_block3OffBtn_clicked()
{
    bool success = writePlcRegister(1528, 0, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->block3OffBtn, true);
        setButtonStyle(ui->block3Btn, false);
        recordSwitchEvent("封堵3", false);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 充气阀1（开）
void DebugPage::on_inflateValve1Btn_clicked()
{
    bool success = writePlcRegister(1529, 1, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->inflateValve1Btn, true);
        setButtonStyle(ui->inflateValve1OffBtn, false);
        recordSwitchEvent("充气阀1", true);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 充气阀1（关）
void DebugPage::on_inflateValve1OffBtn_clicked()
{
    bool success = writePlcRegister(1529, 0, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->inflateValve1OffBtn, true);
        setButtonStyle(ui->inflateValve1Btn, false);
        recordSwitchEvent("充气阀1", false);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 排气阀1（开）
void DebugPage::on_exhaustValve1Btn_clicked()
{
    bool success = writePlcRegister(1530, 1, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->exhaustValve1Btn, true);
        setButtonStyle(ui->exhaustValve1OffBtn, false);
        recordSwitchEvent("排气阀1", true);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 排气阀1（关）
void DebugPage::on_exhaustValve1OffBtn_clicked()
{
    bool success = writePlcRegister(1530, 0, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->exhaustValve1OffBtn, true);
        setButtonStyle(ui->exhaustValve1Btn, false);
        recordSwitchEvent("排气阀1", false);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 充气阀2（开）
void DebugPage::on_inflateValve2Btn_clicked()
{
    bool success = writePlcRegister(1531, 1, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->inflateValve2Btn, true);
        setButtonStyle(ui->inflateValve2OffBtn, false);
        recordSwitchEvent("充气阀2", true);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 充气阀2（关）
void DebugPage::on_inflateValve2OffBtn_clicked()
{
    bool success = writePlcRegister(1531, 0, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->inflateValve2OffBtn, true);
        setButtonStyle(ui->inflateValve2Btn, false);
        recordSwitchEvent("充气阀2", false);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 排气阀2（开）
void DebugPage::on_exhaustValve2Btn_clicked()
{
    bool success = writePlcRegister(1532, 1, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->exhaustValve2Btn, true);
        setButtonStyle(ui->exhaustValve2OffBtn, false);
        recordSwitchEvent("排气阀2", true);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 排气阀2（关）
void DebugPage::on_exhaustValve2OffBtn_clicked()
{
    bool success = writePlcRegister(1532, 0, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->exhaustValve2OffBtn, true);
        setButtonStyle(ui->exhaustValve2Btn, false);
        recordSwitchEvent("排气阀2", false);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 充气阀3（开）
void DebugPage::on_inflateValve3Btn_clicked()
{
    bool success = writePlcRegister(1533, 1, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->inflateValve3Btn, true);
        setButtonStyle(ui->inflateValve3OffBtn, false);
        recordSwitchEvent("充气阀3", true);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 充气阀3（关）
void DebugPage::on_inflateValve3OffBtn_clicked()
{
    bool success = writePlcRegister(1533, 0, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->inflateValve3OffBtn, true);
        setButtonStyle(ui->inflateValve3Btn, false);
        recordSwitchEvent("充气阀3", false);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 排气阀3（开）
void DebugPage::on_exhaustValve3Btn_clicked()
{
    bool success = writePlcRegister(1534, 1, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->exhaustValve3Btn, true);
        setButtonStyle(ui->exhaustValve3OffBtn, false);
        recordSwitchEvent("排气阀3", true);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 排气阀3（关）
void DebugPage::on_exhaustValve3OffBtn_clicked()
{
    bool success = writePlcRegister(1534, 0, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->exhaustValve3OffBtn, true);
        setButtonStyle(ui->exhaustValve3Btn, false);
        recordSwitchEvent("排气阀3", false);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 手动（开）
void DebugPage::on_manualBtn_clicked()
{
    bool success = writePlcRegister(1516, 1, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->manualBtn, true);
        setButtonStyle(ui->manualOffBtn, false);
        recordSwitchEvent("手动操作", true);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 手动（关）
void DebugPage::on_manualOffBtn_clicked()
{
    bool success = writePlcRegister(1516, 0, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->manualOffBtn, true);
        setButtonStyle(ui->manualBtn, false);
        recordSwitchEvent("手动操作", false);
        QTimer::singleShot(300, this, &DebugPage::syncPlcButtonStates);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

void DebugPage::on_sendRegulatorPressureBtn_clicked()
{
    double pressureValue = ui->regulatorPressureSpinBox->value();
    
    AirTightnessFullParams params;
    params.pressure.regulatorPressure = pressureValue;
    
    if (sendPressureToRegulator(params)) {
        recordOperation(QString("调压阀压力发送成功: %1").arg(pressureValue));
    } else {
        recordOperation(QString("调压阀压力发送失败: %1").arg(pressureValue));
    }
}

// 总封堵（开）
void DebugPage::on_totalSealOnBtn_clicked()
{
    bool success = writePlcRegister(1540, 1, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->totalSealOnBtn, true);
        setButtonStyle(ui->totalSealOffBtn, false);
        recordSwitchEvent("总封堵", true);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 总封堵（关）
void DebugPage::on_totalSealOffBtn_clicked()
{
    bool success = writePlcRegister(1540, 0, QModbusDataUnit::Coils);
    if (success) {
        setButtonStyle(ui->totalSealOnBtn, false);
        setButtonStyle(ui->totalSealOffBtn, true);
        recordSwitchEvent("总封堵", false);
    } else {
        QTimer::singleShot(100, this, &DebugPage::syncPlcButtonStates);
    }
}

// 仪器启动按钮点击事件（补齐）
void DebugPage::on_instrumentStartBtn_clicked()
{
    // 校验频道输入：只能为1-10的整数
    QString text = ui->instrumentChannelEdit->text().trimmed();
    bool ok = false;
    int channel = text.toInt(&ok);
    if (!ok || channel < 1 || channel > 10) {
        QMessageBox::warning(this, "频道无效", "请输入1-10之间的整数作为仪器频道。");
        ui->instrumentChannelEdit->setFocus();
        ui->instrumentChannelEdit->selectAll();
        return;
    }
    // 需要根据频道号在数据库里面查找相应的频道号参数
    AirTightnessFullParams params = AirTightnessParamsDao::getParamsByProgramNumber(channel);
    if (params.id == -1) {
        QMessageBox::warning(this, "参数未配置", QString("数据库中未找到频道 %1 的参数，请先在参数设置中保存。").arg(channel));
        return;
    }
    // 把压力值发送到调压装置
    bool pressureOk = sendPressureToRegulator(params);
    if (!pressureOk) {
        recordOperation("向调压装置发送压力值失败");
    }

    // 需要把参数写入到气密仪寄存器8192到8229，如果没有发送成功就继续发送
    bool sent = sendParamsToDevice(params);
    int attempt = 1;
    const int maxAttempts = 10; // 最大重试次数
    while (!sent && attempt <= maxAttempts) {
        recordOperation(QString("参数发送失败，正在重试，第 %1 次（共 %2 次）").arg(attempt).arg(maxAttempts));
        QThread::msleep(1500); // 间隔1.5秒再试
        sent = sendParamsToDevice(params);
        ++attempt;
    }
    if (!sent) {
        recordOperation(QString("参数发送失败，已达到最大重试次数 %1 次").arg(maxAttempts));
        return;
    }
    // 写入成功后，需要等待1秒，确保指令生效
    QThread::msleep(1000);
    // 启动气密仪
    bool startSuccess = writeAirtightRegister(9472, 4608, QModbusDataUnit::HoldingRegisters);
    if (!startSuccess) {
        recordOperation("启动气密仪失败");
        return;
    }
    recordOperation(QString("仪器启动，频道: %1").arg(channel));
}

// 仪器复位按钮点击事件（补齐）
void DebugPage::on_instrumentResetBtn_clicked()
{
    recordOperation("仪器复位");
    writeAirtightRegister(9472, 4864, QModbusDataUnit::HoldingRegisters);
}

void DebugPage::on_okBtn_clicked() { recordOperation("确认操作 (OK)"); }
void DebugPage::on_tdBtn_clicked() { recordOperation("测试数据 (TD)"); }
void DebugPage::on_rdBtn_clicked() { recordOperation("读取数据 (RD)"); }
void DebugPage::on_alBtn_clicked() { recordOperation("报警确认 (AL)"); }
void DebugPage::on_runBtn_clicked() { recordOperation("运行程序 (Run)"); }

bool DebugPage::sendParamsToDevice(const AirTightnessFullParams& params)
{
    if (!m_airtightModbusClient || m_airtightModbusClient->state() != QModbusDevice::ConnectedState) {
        recordOperation("未连接到气密仪设备，无法发送参数");
        return false;
    }

    recordOperation("开始准备参数发送到设备");

    const int timeoutMs = 1000; // 单个命令超时时间
    const quint16 startAddress = 8192; // 起始地址
    const quint16 endAddress = 8229;   // 结束地址（参考现有实现）
    const int numRegisters = endAddress - startAddress + 1; // 寄存器数量

    // 先发送地址8192,值为26112的命令，通知设备准备接收参数
    recordOperation("发送参数准备命令");
    if (!writeAirtightRegister(8192, 26112, QModbusDataUnit::HoldingRegisters)) {
        recordOperation("发送参数准备命令失败");
        return false;
    }

    // 等待设备准备
    QThread::msleep(500);

    // 创建批量写入数据单元，从8192到8229共38个寄存器
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, startAddress, numRegisters);

    // 初始化所有寄存器值为0
    for (int i = 0; i < numRegisters; ++i) {
        writeUnit.setValue(i, 0);
    }
    
    // 重新设置8192地址的值为26112，确保不会被覆盖为0
    writeUnit.setValue(8192 - startAddress, 26112);

    // 写入程序号（8193）
    writeUnit.setValue(8193 - startAddress, static_cast<quint16>(params.programNumber));

    // 时间参数编码
    auto encodeTime = [](double n) -> quint16 {
        if (n < 0.0) n = 0.0;
        if (n > 200.0) n = 200.0;
        quint16 value = static_cast<quint16>(std::round(n * 100.0));
        quint16 low = static_cast<quint16>(value & 0xFFu);
        quint16 high = static_cast<quint16>((value >> 8) & 0xFFu);
        return static_cast<quint16>((low << 8) | high);
    };

    quint16 fillTime = encodeTime(params.cycleTime.fillTime);
    quint16 stabilizationTime = encodeTime(params.cycleTime.stabilizationTime);
    quint16 testTime = encodeTime(params.cycleTime.testTime);
    quint16 dumpTime = encodeTime(params.cycleTime.dumpTime);

    writeUnit.setValue(8196 - startAddress, fillTime);
    writeUnit.setValue(8197 - startAddress, stabilizationTime);
    writeUnit.setValue(8198 - startAddress, testTime);
    writeUnit.setValue(8199 - startAddress, dumpTime);

    // 压力单位
    writeUnit.setValue(8200 - startAddress, static_cast<quint16>(params.pressure.unitIndex));

    // 压力值编码（双寄存器，字节交换）
    auto swap16 = [](quint16 v) -> quint16 { return static_cast<quint16>((v << 8) | (v >> 8)); };
    auto encodePressure = [&](quint16 n) -> QPair<quint16, quint16> {
        quint32 raw = static_cast<quint32>(n) * 1000u;
        quint16 low = static_cast<quint16>(raw & 0xFFFFu);
        quint16 high = static_cast<quint16>((raw >> 16) & 0xFFFFu);
        return qMakePair(swap16(low), swap16(high));
    };

    quint16 minPressure = static_cast<quint16>(params.pressure.minimum);
    quint16 maxPressure = static_cast<quint16>(params.pressure.maximum);
    quint16 fillPressure = static_cast<quint16>(params.pressure.setFill);

    QPair<quint16, quint16> minEnc = encodePressure(minPressure);
    QPair<quint16, quint16> maxEnc = encodePressure(maxPressure);
    QPair<quint16, quint16> fillEnc = encodePressure(fillPressure);

    writeUnit.setValue(8201 - startAddress, minEnc.first);
    writeUnit.setValue(8202 - startAddress, minEnc.second);
    writeUnit.setValue(8203 - startAddress, maxEnc.first);
    writeUnit.setValue(8204 - startAddress, maxEnc.second);
    writeUnit.setValue(8205 - startAddress, fillEnc.first);
    writeUnit.setValue(8206 - startAddress, fillEnc.second);

    // 填充类型
    writeUnit.setValue(8211 - startAddress, static_cast<quint16>(params.pressure.fillTypeIndex));

    // 泄漏相关参数
    writeUnit.setValue(8212 - startAddress, static_cast<quint16>(params.leak.leakUnitIndex));
    writeUnit.setValue(8213 - startAddress, 256);
    writeUnit.setValue(8214 - startAddress, static_cast<quint16>(params.leak.volumeUnitIndex));

    // 容积与标准参数
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
        recordOperation(QString("容积参数: 原始容积=%1 8215=%2 8216=%3").arg(volumeInt).arg(encoded8215).arg(encoded8216));
    }
    LeakUnitType::Unit leakUnit = LeakUnitType::getUnitFromCode(static_cast<int>(params.leak.leakUnitIndex), LeakUnitType::CodeType1);
    quint16 leakUnitCode2 = static_cast<quint16>(LeakUnitType::getCode(leakUnit, LeakUnitType::CodeType2));
    recordOperation(QString("泄漏单位编码: 单位1=%1 单位2=%2").arg(params.leak.leakUnitIndex).arg(leakUnitCode2));
    writeUnit.setValue(8217 - startAddress, leakUnitCode2); 
    writeUnit.setValue(8219 - startAddress, leakUnitCode2); 
    writeUnit.setValue(8218 - startAddress, 0); // 拒绝计算方式
    writeUnit.setValue(8224 - startAddress, 64);
    writeUnit.setValue(8225 - startAddress, 32068);
    writeUnit.setValue(8227 - startAddress, 0);
    writeUnit.setValue(8229 - startAddress, static_cast<quint16>(params.leak.offset));

    // 发送批量写入请求
    QModbusReply *reply = m_airtightModbusClient->sendWriteRequest(writeUnit, m_airtightDeviceAddress);
    if (!reply) {
        recordOperation(QString("发送请求失败: %1").arg(m_airtightModbusClient->errorString()));
        return false;
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);

    timer.start(timeoutMs);
    loop.exec();

    bool success = false;
    if (timer.isActive()) {
        timer.stop();
        if (reply->error() == QModbusDevice::NoError) {
            success = true;
            recordOperation(QString("批量参数发送成功 - 从地址 %1 到 %2").arg(startAddress).arg(endAddress));
        } else {
            recordOperation(QString("发送命令失败: %1").arg(reply->errorString()));
        }
        reply->deleteLater();
    } else {
        // 超时
        recordOperation(QString("发送命令超时 - 从地址 %1 到 %2").arg(startAddress).arg(endAddress));
        QObject::disconnect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
        reply->deleteLater();
    }

    if (success) {
        recordOperation("所有参数已成功发送到设备");
    } else {
        recordOperation("发送参数完成命令失败");
    }
    
    return success;
}

void DebugPage::initializeSwitchButtons()
{
    // 初始化所有开关按钮为关闭状态（关按钮显示绿色，开按钮显示红色）
    
    // 增压进
    setButtonStyle(ui->trayInBtn, false);
    setButtonStyle(ui->trayInOffBtn, true);
    
    // 返程进缸
    setButtonStyle(ui->trayOutBtn, false);
    setButtonStyle(ui->trayOutOffBtn, true);
    
    // 主缸进
    setButtonStyle(ui->mainCylinderInBtn, false);
    setButtonStyle(ui->mainCylinderInOffBtn, true);
    
    // 主缸退
    setButtonStyle(ui->mainCylinderOutBtn, false);
    setButtonStyle(ui->mainCylinderOutOffBtn, true);
    
    // 封堵1
    setButtonStyle(ui->block1Btn, false);
    setButtonStyle(ui->block1OffBtn, true);
    
    // 封堵2
    setButtonStyle(ui->block2Btn, false);
    setButtonStyle(ui->block2OffBtn, true);
    
    // 封堵3
    setButtonStyle(ui->block3Btn, false);
    setButtonStyle(ui->block3OffBtn, true);
    
    // 充气阀1
    setButtonStyle(ui->inflateValve1Btn, false);
    setButtonStyle(ui->inflateValve1OffBtn, true);
    
    // 排气阀1
    setButtonStyle(ui->exhaustValve1Btn, false);
    setButtonStyle(ui->exhaustValve1OffBtn, true);
    
    // 充气阀2
    setButtonStyle(ui->inflateValve2Btn, false);
    setButtonStyle(ui->inflateValve2OffBtn, true);
    
    // 排气阀2
    setButtonStyle(ui->exhaustValve2Btn, false);
    setButtonStyle(ui->exhaustValve2OffBtn, true);
    
    // 充气阀3
    setButtonStyle(ui->inflateValve3Btn, false);
    setButtonStyle(ui->inflateValve3OffBtn, true);
    
    // 排气阀3
    setButtonStyle(ui->exhaustValve3Btn, false);
    setButtonStyle(ui->exhaustValve3OffBtn, true);
    
    // 手动操作
    setButtonStyle(ui->manualBtn, false);
    setButtonStyle(ui->manualOffBtn, true);
    
    // 总封堵
    setButtonStyle(ui->totalSealOnBtn, false);
    setButtonStyle(ui->totalSealOffBtn, true);
}

void DebugPage::setButtonStyle(QPushButton *button, bool active)
{
    if (active) {
        button->setStyleSheet("background-color: #2ecc71; color: white; font-weight: bold; border-radius: 3px; padding: 5px;");
    } else {
        button->setStyleSheet("background-color: #e74c3c; color: white; font-weight: bold; border-radius: 3px; padding: 5px;");
    }
}

void DebugPage::recordOperation(const QString &operation)
{
    showFeedback(operation);
    // 操作日志面板已移除，只通过feedbackLabel显示反馈信息
}

// 开关事件记录（含时间与持续时长）
void DebugPage::recordSwitchEvent(const QString &name, bool isOn)
{
    QDateTime now = QDateTime::currentDateTime();
    if (isOn) {
        m_switchOnStartTimes[name] = now;
        recordOperation(QString("%1 开：%2").arg(name, now.toString("yyyy-MM-dd HH:mm:ss")));
    } else {
        QDateTime start = m_switchOnStartTimes.value(name);
        QString msg = QString("%1 关：%2").arg(name, now.toString("yyyy-MM-dd HH:mm:ss"));
        if (start.isValid()) {
            qint64 secs = start.secsTo(now);
            msg += QString("，持续%1秒").arg(secs);
            m_switchOnStartTimes.remove(name);
        }
        recordOperation(msg);
    }
}
