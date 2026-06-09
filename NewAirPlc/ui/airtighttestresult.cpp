#include "airtighttestresult.h"
#include "ui_airtighttestresult.h"
#include <QModbusDataUnit>
#include <QDateTime>
#include <QDebug>
#include <QTextCursor>
#include "PressureUnit.h"
#include "LeakUnitType.h"
#include "AirtightTestResultDao.h"
#include "TestResultStruct.h"
#include <QStringConverter>
#include "SystemSettingDao.h"
#include <QDir>
#include <QFileInfo>
#include "Logger.h"
#include "StyledMessageBox.h"
#include "UserDao.h"

AirtightTestResult::AirtightTestResult(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AirtightTestResult),
    modbusClient(nullptr),
    plcModbusClient(nullptr),
    slaveId(1),
    isConnected(false),
    isLogPaused(false),
    tableModel(nullptr),
    proxyModel(nullptr),
    monitorTimer(nullptr),
    instrumentMonitorTimer(nullptr),
    autoRefreshTimer(nullptr),
    currentProgramNumber(-1),
    lastSavedPressureValue(0.0),
    m_testResultState(TestResultState::Idle),
    currentPage(1),
    pageSize(20),
    totalRecords(0),
    totalPages(1)
{
    ui->setupUi(this);
    initUI();
    setupTableModel();

    // 初始化/迁移测试结果表（包含程序号字段）
    AirtightTestResultDao::initTable();
    
    // 从数据库加载历史测试结果
    loadTestResultsFromDatabase();
    
    // 不再使用自动刷新定时器，改为有新的测试结果时刷新
    // autoRefreshTimer = new QTimer(this);
    // connect(autoRefreshTimer, &QTimer::timeout, this, &AirtightTestResult::onAutoRefreshTimer);
    // autoRefreshTimer->start(3000);
}

AirtightTestResult::~AirtightTestResult()
{
    // 停止定时器
    if (monitorTimer) {
        monitorTimer->stop();
        delete monitorTimer;
        monitorTimer = nullptr;
    }
    if (instrumentMonitorTimer) {
        instrumentMonitorTimer->stop();
        instrumentMonitorTimer->deleteLater();
        instrumentMonitorTimer = nullptr;
    }
    if (autoRefreshTimer) {
        autoRefreshTimer->stop();
        delete autoRefreshTimer;
        autoRefreshTimer = nullptr;
    }
    
    delete ui;
}

void AirtightTestResult::setPlcModbusClient(QModbusClient *client)
{
    if (plcModbusClient == client) {
        return;
    }

    // 断开旧连接的信号槽
    if (plcModbusClient) {
        disconnect(plcModbusClient, &QModbusClient::stateChanged, this, nullptr);
    }

    plcModbusClient = client;

    if (plcModbusClient) {
        // 连接状态变化信号
        connect(plcModbusClient, &QModbusClient::stateChanged, this, [this](QModbusDevice::State state) {
            updateConnectionStatus(true, state == QModbusDevice::ConnectedState);
        });
    }
}

void AirtightTestResult::setModbusClient(QModbusClient *client)
{
    // 断开旧连接的状态变化信号
    if (modbusClient) {
        disconnect(modbusClient, &QModbusClient::stateChanged, this, nullptr);
    }

    modbusClient = client;

    // 监听气密仪客户端状态变化，更新连接状态与UI
    if (modbusClient) {
        connect(modbusClient, &QModbusClient::stateChanged, this, [this](QModbusDevice::State state){
            updateConnectionStatus(false, state == QModbusDevice::ConnectedState);
        });
    }

    // 初始状态设置并同步isConnected
    bool connected = modbusClient && modbusClient->state() == QModbusDevice::ConnectedState;
    isConnected = connected;
}

void AirtightTestResult::updateConnectionStatus(bool isPlc, bool connected)
{
    qDebug() << "AirtightTestResult收到updateConnectionStatus：isPlc=" << isPlc << " connected=" << connected;
    // 只处理气密仪的连接状态
    if (!isPlc) {
        isConnected = connected;
        if (connected) {
            logMessage("设备已连接，等待RealtimeMonitor发送测试结果数据");
            
            // 设备断开连接，停止监控定时器
            if (monitorTimer && monitorTimer->isActive()) {
                monitorTimer->stop();
            }
        }
    }
}

void AirtightTestResult::initUI()
{

    // 设置表格字体
    ui->dataTableView->setFont(QFont("SimHei"));

    // 连接信号和槽
    // 注意：使用 on_<objectName>_<signalName> 命名的槽函数会被 Qt 自动连接
    // 不需要手动连接，否则会导致信号被连接两次
    // connect(ui->exportButton, &QPushButton::clicked, this, &AirtightTestResult::on_exportButton_clicked);
    // connect(ui->clearQueryButton, &QPushButton::clicked, this, &AirtightTestResult::on_clearQueryButton_clicked);
    // connect(ui->deleteAllButton, &QPushButton::clicked, this, &AirtightTestResult::on_deleteAllButton_clicked);
    
    // 连接筛选条件变化信号（自动刷新）
    connect(ui->startDateTimeEdit, &QDateTimeEdit::dateTimeChanged, this, &AirtightTestResult::onFilterChanged);
    connect(ui->endDateTimeEdit, &QDateTimeEdit::dateTimeChanged, this, &AirtightTestResult::onFilterChanged);
    connect(ui->resultComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AirtightTestResult::onFilterChanged);
    connect(ui->productNumberLineEdit, &QLineEdit::textChanged, this, &AirtightTestResult::onFilterChanged);
    connect(ui->inspectorComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AirtightTestResult::onFilterChanged);

    // 初始化连接状态
    isConnected = false;
    
    // 创建并配置监控定时器（已废弃，保留用于兼容性）
    // 数据现在由RealtimeMonitor统一轮询，通过testResultRegisterDataReady信号接收
    monitorTimer = new QTimer(this);
    monitorTimer->setInterval(100);
    // 不再连接信号，定时器不会启动
    // connect(monitorTimer, &QTimer::timeout, this, &AirtightTestResult::on_monitorTimer_timeout);
    
    // 设置默认时间范围为当天的0时到23时59分
    QDate today = QDate::currentDate();
    ui->startDateTimeEdit->setDateTime(QDateTime(today, QTime(0, 0, 0)));
    ui->endDateTimeEdit->setDateTime(QDateTime(today, QTime(23, 59, 59)));
    
    // 连接分页按钮信号
    connect(ui->firstPageButton, &QPushButton::clicked, this, &AirtightTestResult::onFirstPageClicked);
    connect(ui->prevPageButton, &QPushButton::clicked, this, &AirtightTestResult::onPrevPageClicked);
    connect(ui->nextPageButton, &QPushButton::clicked, this, &AirtightTestResult::onNextPageClicked);
    connect(ui->lastPageButton, &QPushButton::clicked, this, &AirtightTestResult::onLastPageClicked);
    connect(ui->pageSizeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AirtightTestResult::onPageSizeChanged);
}

void AirtightTestResult::setupTableModel()
{
    // 创建表格模型
    tableModel = new QStandardItemModel(this);

    // 设置表头（加入产品编号列，并新增报警代码列）
    QStringList headers;
    headers << "创建时间" << "程序号" << "产品编号" << "报警信息" << "报警代码" << "压力值" << "压力单位" \
            << "泄露值" << "泄露单位" << "质检人";
    tableModel->setHorizontalHeaderLabels(headers);

    // 创建代理模型用于筛选
    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(tableModel);
    proxyModel->setFilterKeyColumn(-1); // 搜索所有列

    // 设置表格视图使用代理模型
    ui->dataTableView->setModel(proxyModel);

    // 启用水平滚动条（内容超出宽度时自动显示）
    ui->dataTableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // 各列根据内容自适应宽度
    ui->dataTableView->horizontalHeader()->setStretchLastSection(false);
    ui->dataTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    // 设置表格不可编辑
    ui->dataTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

// 从数据库加载测试结果
void AirtightTestResult::loadTestResultsFromDatabase()
{
    // 读取所有测试结果
    QList<AirtightTestResultData> results = AirtightTestResultDao::getAllResults();
    
    // 按时间戳降序排序，确保最新的数据排在前面
    std::sort(results.begin(), results.end(), [](const AirtightTestResultData& a, const AirtightTestResultData& b) {
        return a.timestamp > b.timestamp;
    });
    
    // 清空当前表格数据
    tableModel->clear();
    
    // 重新设置表头（新增报警代码列）
    QStringList headers;
    headers << "创建时间"<< "程序号" << "产品编号" << "报警信息" << "报警代码" << "压力值" << "压力单位" \
            << "泄露值" << "泄露单位" << "质检人";
    tableModel->setHorizontalHeaderLabels(headers);
    
    // 重新设置列宽（与 setupTableModel 中的设置保持一致）
    ui->dataTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    
    // 添加结果到表格
    for (const auto& result : results) {
        addTestResultRow(result.timestamp, result.alarmInfo, result.pressureValue, result.pressureUnit,
                         result.leakValue, result.leakUnit, result.programNumber, result.productNumber, result.alarmCode, result.inspector);
    }
    
    // logMessage(QString("从数据库加载了 %1 条测试结果记录").arg(results.size()));
    
    // 应用筛选条件
    applyFilter();
    
    // 更新记录数显示
    updateRecordCount();
    
    // 更新最后刷新时间
    // 刷新质检人下拉列表
    { QString cur = ui->inspectorComboBox->currentText();
      ui->inspectorComboBox->blockSignals(true);
      ui->inspectorComboBox->clear();
      ui->inspectorComboBox->addItem("全部");
      UserDao userDao; for (const User &u : userDao.getAllUsers()) { if (u.isActive) { QString name = u.realName.isEmpty() ? u.username : u.realName; ui->inspectorComboBox->addItem(name); } }
      int idx = ui->inspectorComboBox->findText(cur); ui->inspectorComboBox->setCurrentIndex(idx >= 0 ? idx : 0);
      ui->inspectorComboBox->blockSignals(false); }
    ui->lastUpdateLabel->setText(QString("最后更新: %1").arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
}

void AirtightTestResult::readTestResultData()
{
    if (!isConnected || !modbusClient || modbusClient->state() != QModbusDevice::ConnectedState) {
        return;
    }

    QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, 8961, 12);
    QModbusReply *reply = modbusClient->sendReadRequest(readUnit, static_cast<quint8>(slaveId));
    if (!reply) {
        logMessage(QString("发送读取请求失败: %1").arg(modbusClient->errorString()), true);
        return;
    }

    QTimer *timeout = new QTimer(this);
    timeout->setSingleShot(true);
    timeout->setInterval(300);

    struct CallbackState { bool done = false; };
    CallbackState *state = new CallbackState();

    connect(timeout, &QTimer::timeout, this, [this, reply, state]() {
        if (state->done) return;
        state->done = true;
        logMessage("读取测试数据超时", true);
        reply->deleteLater();
    });

    connect(reply, &QModbusReply::finished, this, [this, reply, timeout, state]() {
        if (state->done) { reply->deleteLater(); return; }
        state->done = true;
        timeout->stop();
        timeout->deleteLater();

        if (reply->error() == QModbusDevice::NoError) {
            const QModbusDataUnit result = reply->result();
            if (result.valueCount() >= 12) {
                // 8962：报警信息（Alarm）
                quint16 alarmInfoCode = result.value(1);
                QString alarmInfo = "通过";
                if (alarmInfoCode == 4864) {
                    alarmInfo = "不通过(压力过高)";
                } else if (alarmInfoCode == 5120) {
                    alarmInfo = "不通过(压力过低)";
                } else if (alarmInfoCode != 0) {
                    alarmInfo = QString("不通过(未知报警: %1)").arg(alarmInfoCode);
                }

                // 8963/8964：Pressure值（低位8963，高位8964）- 16位字节交换；根据高位是否为0选择缩放因子
                 auto swap16 = [](quint16 v) -> quint16 { return ((v & 0x00FF) << 8) | ((v & 0xFF00) >> 8); };
                 quint16 lowCode = result.value(2);
                 quint16 highCode = result.value(3);
                 quint16 lowRaw = swap16(lowCode);
                 quint16 highRaw = swap16(highCode);
                 double pressureValue = 0.0;
                quint32 rawValue = (static_cast<quint32>(highRaw) << 16) | static_cast<quint32>(lowRaw);
                pressureValue = static_cast<double>(rawValue) / 1000.0;
                 
                 
                 
                 // 8965：Pressure单位
                quint16 pressureUnitCode = result.value(4);
                PressureUnit::Unit pressureUnitEnum = PressureUnit::fromCode(pressureUnitCode);
                QString pressureUnit = PressureUnit::toString(pressureUnitEnum);

                // 8968：Leak单位
                quint16 leakUnitCode = result.value(7);
                LeakUnitType::Unit leakUnitEnum = LeakUnitType::getUnitFromCode(leakUnitCode, LeakUnitType::CodeType1);
                QString leakUnit = LeakUnitType::getUnitString(leakUnitEnum);

                // 8966/8967：泄漏值（两个寄存器字节交换后相减，根据单位选择缩放因子）
                quint16 r1Code = result.value(5); // 8966
                quint16 r2Code = result.value(6); // 8967
                quint16 r1Swapped = swap16(r1Code);
                quint16 r2Swapped = swap16(r2Code);
                // 根据泄漏单位选择缩放因子：Pa单位使用10，其他单位使用1000
                double leakScale = (leakUnitEnum == LeakUnitType::Pa) ? 10.0 : 1000.0;
                double leakValue = static_cast<double>(static_cast<int32_t>(r1Swapped) - static_cast<int32_t>(r2Swapped)) / leakScale;

                // 8969：标准大气压
                quint16 atmPressureCode = result.value(8);
                double atmPressure = 1006.0746484375 + atmPressureCode / 25600.0;

                // 8970：大气压单位
                quint16 atmPressureUnitCode = result.value(9);
                QString atmPressureUnit = "hPa";
                if (atmPressureUnitCode == 1) atmPressureUnit = "kPa";
                else if (atmPressureUnitCode == 2) atmPressureUnit = "MPa";

                // 8971：温度
                quint16 temperatureCode = result.value(10);
                double temperature = 31.4853515625 + temperatureCode / 25600.0;

                // 8972：温度单位
                quint16 temperatureUnitCode = result.value(11);
                QString temperatureUnit = "°C";
                if (temperatureUnitCode == 1) temperatureUnit = "°F";
                else if (temperatureUnitCode == 2) temperatureUnit = "K";

                QDateTime currentTime = QDateTime::currentDateTime();

                // 去重判断：压力值必须大于0，且（压力值与上次不同 或 距离上次保存超过5秒）
                bool shouldSave = pressureValue > 0 && 
                    (qAbs(pressureValue - lastSavedPressureValue) > 0.001 || 
                     !lastSavedTimestamp.isValid() || 
                     lastSavedTimestamp.secsTo(currentTime) > 5);
                
                if (shouldSave) {
                    addTestResultRow(currentTime, alarmInfo, pressureValue, pressureUnit,
                                     leakValue, leakUnit, currentProgramNumber, currentProductNumber, alarmInfoCode);

                    AirtightTestResultData resultData;
                    resultData.timestamp = currentTime;
                    resultData.alarmInfo = alarmInfo;
                    resultData.pressureValue = pressureValue;
                    resultData.pressureUnit = pressureUnit;
                    resultData.leakValue = leakValue;
                    resultData.leakUnit = leakUnit;
                    resultData.atmPressure = atmPressure;
                    resultData.atmPressureUnit = atmPressureUnit;
                    resultData.temperature = temperature;
                    resultData.temperatureUnit = temperatureUnit;
                    resultData.programNumber = currentProgramNumber;
                    resultData.productNumber = currentProductNumber;
 
                     if (AirtightTestResultDao::saveResult(resultData)) {
                        lastSavedTimestamp = currentTime; // 更新上次保存时间
                        lastSavedPressureValue = pressureValue; // 更新上次保存的压力值
                        logMessage("测试结果已保存到数据库");
                    } else {
                        logMessage("测试结果保存到数据库失败", true);
                    }
                }
            } else {
                logMessage("读取数据失败：返回值数量不足", true);
            }
        } else {
            logMessage(QString("读取数据失败: %1").arg(reply->errorString()), true);
        }
        reply->deleteLater();
        delete state;
    });

    timeout->start();
}

void AirtightTestResult::on_monitorTimer_timeout()
{
    // 实时读取测试结果数据，不再依赖8706寄存器的值
    if (!isConnected || !modbusClient || modbusClient->state() != QModbusDevice::ConnectedState) {
        return;
    }

    readTestResultData();
}



bool AirtightTestResult::readDeviceData(quint16 address, quint16 &value, QModbusDataUnit::RegisterType type)
{
    // 使用PLC连接而不是气密仪连接
    if (!plcModbusClient || plcModbusClient->state() != QModbusDevice::ConnectedState) {
        logMessage(QString("无法读取PLC数据：PLC未连接，地址: %1").arg(address), true);
        return false;
    }

    // 使用功能码03（Read Holding Registers）读取单个寄存器
    QModbusDataUnit readUnit(type, address, 1);
    
    // 发送读取请求
    QModbusReply *reply = plcModbusClient->sendReadRequest(readUnit, static_cast<quint8>(2));
    if (!reply) {
        logMessage(QString("发送PLC读取请求失败: %1").arg(plcModbusClient->errorString()), true);
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
            const QModbusDataUnit result = reply->result();
            if (result.valueCount() > 0) {
                value = result.value(0);
                success = true;
            } else {
                logMessage(QString("读取数据失败：无返回值，地址: %1").arg(address), true);
            }
        } else {
            logMessage(QString("读取PLC数据失败: %1, 地址: %2").arg(reply->errorString()).arg(address), true);
        }
        
        reply->deleteLater();
    } else {
        // 超时
        logMessage(QString("读取PLC数据超时，地址: %1").arg(address), true);
        disconnect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
        reply->deleteLater();
    }

    return success;
}

void AirtightTestResult::writePlcRegister(quint16 address, quint16 value, QModbusDataUnit::RegisterType type, const std::function<void(bool)>& callback) {
    // 使用PLC连接而不是气密仪连接
    if (!plcModbusClient || plcModbusClient->state() != QModbusDevice::ConnectedState) {
        logMessage(QString("无法写入PLC寄存器: PLC未连接，地址: %1, 值: %2").arg(address).arg(value), true);
        if (callback) callback(false);
        return;
    }

    // 创建写入单元
    QModbusDataUnit writeUnit(type, address, 1);
    writeUnit.setValue(0, value);

    // 发送写入请求，使用从站地址2
    QModbusReply *reply = plcModbusClient->sendWriteRequest(writeUnit, 2);
    if (!reply) {
        logMessage(QString("向PLC寄存器%1发送写入请求失败: %2").arg(address).arg(plcModbusClient->errorString()), true);
        if (callback) callback(false);
        return;
    }

    // 设置一次性定时器处理超时
    QTimer *timeoutTimer = new QTimer(reply);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(2000); // 2秒超时

    // 使用QPointer保护对象生命周期，避免悬空指针
    QPointer<AirtightTestResult> self(this);

    connect(timeoutTimer, &QTimer::timeout, this, [self, reply, address, callback, timeoutTimer]() {
        timeoutTimer->deleteLater();
        if (self) {
            self->logMessage(QString("写入PLC寄存器超时，地址: %1").arg(address), true);
        }
        if (callback) callback(false);
        reply->deleteLater();
    });

    connect(reply, &QModbusReply::finished, this, [self, reply, address, value, callback, timeoutTimer]() {
        timeoutTimer->stop();
        timeoutTimer->deleteLater();

        bool success = false;
        if (reply->error() == QModbusDevice::NoError) {
            if (self) {
                self->logMessage(QString("成功向PLC寄存器%1写入%2").arg(address).arg(value), false);
            }
            success = true;
        } else {
            if (self) {
                self->logMessage(QString("写入PLC寄存器失败: %1, 地址: %2, 值: %3").arg(reply->errorString()).arg(address).arg(value), true);
            }
        }

        if (callback) callback(success);
        reply->deleteLater();
    });

    timeoutTimer->start();
}

bool AirtightTestResult::writePlcRegisterBlocking(quint16 address, quint16 value, QModbusDataUnit::RegisterType type) {
    // 使用PLC连接而不是气密仪连接
    if (!plcModbusClient || plcModbusClient->state() != QModbusDevice::ConnectedState) {
        logMessage(QString("无法写入PLC寄存器: PLC未连接，地址: %1, 值: %2").arg(address).arg(value), true);
        return false;
    }

    // 创建写入单元
    QModbusDataUnit writeUnit(type, address, 1);
    writeUnit.setValue(0, value);

    // 发送写入请求，使用从站地址2
    QModbusReply *reply = plcModbusClient->sendWriteRequest(writeUnit, 2);
    if (!reply) {
        logMessage(QString("向PLC寄存器%1发送写入请求失败: %2").arg(address).arg(plcModbusClient->errorString()), true);
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
            logMessage(QString("成功向PLC寄存器%1写入%2").arg(address).arg(value), false);
            success = true;
        } else {
            logMessage(QString("写入PLC寄存器失败: %1, 地址: %2, 值: %3").arg(reply->errorString()).arg(address).arg(value), true);
        }
        
        reply->deleteLater();
    } else {
        // 超时
        logMessage(QString("写入PLC寄存器超时，地址: %1").arg(address), true);
        disconnect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
        reply->deleteLater();
    }

    return success;
}

void AirtightTestResult::onInstrumentSelectionChanged(int instrumentSelection)
{
    // 处理接收到的仪器评选号
    qDebug() << "========= AirtightTestResult::onInstrumentSelectionChanged 方法被调用 =========";
    qDebug() << "AirtightTestResult: 收到仪器评选号:" << instrumentSelection;
    
    // 更新当前程序号
    currentProgramNumber = instrumentSelection;
    
    // 判断程序号不能为0
    if (instrumentSelection == 0) {
        logMessage("程序号为0，不启动监控", false);
        
        // 如果之前已经有定时器在运行，先停止并清理
        if (instrumentMonitorTimer) {
            instrumentMonitorTimer->stop();
            instrumentMonitorTimer->deleteLater();
            instrumentMonitorTimer = nullptr;
        }
        
        return; // 直接返回，不执行后续逻辑
    }
    
    logMessage(QString("收到仪器评选号: %1，读取通道配置").arg(instrumentSelection), false);
    
    // 读取并缓存通道配置
    readDeviceData(49408, cachedTest1Enabled, QModbusDataUnit::Coils);
    readDeviceData(49409, cachedTest2Enabled, QModbusDataUnit::Coils);
    readDeviceData(49410, cachedTest3Enabled, QModbusDataUnit::Coils);
    readDeviceData(101, cachedTestChannel1, QModbusDataUnit::HoldingRegisters);
    readDeviceData(102, cachedTestChannel2, QModbusDataUnit::HoldingRegisters);
    readDeviceData(103, cachedTestChannel3, QModbusDataUnit::HoldingRegisters);
    
    logMessage(QString("通道配置 - 使能: %1/%2/%3, 程序号: %4/%5/%6")
        .arg(cachedTest1Enabled).arg(cachedTest2Enabled).arg(cachedTest3Enabled)
        .arg(cachedTestChannel1).arg(cachedTestChannel2).arg(cachedTestChannel3), false);
    
    // 不再创建定时器，改为接收RealtimeMonitor的信号
    // 如果之前有定时器在运行，停止并清理
    if (instrumentMonitorTimer) {
        instrumentMonitorTimer->stop();
        instrumentMonitorTimer->deleteLater();
        instrumentMonitorTimer = nullptr;
    }
}

void AirtightTestResult::onTestResultRegisterDataReady(const QModbusDataUnit &data)
{
    // 接收来自RealtimeMonitor的测试结果寄存器数据（8961-8972）
    if (data.valueCount() < 12) {
        logMessage("接收到的测试结果数据不完整", true);
        return;
    }
    
    // 如果程序号为0或未设置，不处理
    if (currentProgramNumber <= 0) {
        return;
    }
    
    // 状态机检查：如果正在处理或已完成，跳过重复处理
    if (m_testResultState != TestResultState::Idle) {
        logMessage(QString("测试结果正在处理中，跳过重复数据（当前状态: %1）").arg(static_cast<int>(m_testResultState)));
        return;
    }
    
    // 设置状态为处理中
    m_testResultState = TestResultState::Processing;
    
    // 解析数据
    // 8962：报警信息（索引1）
    quint16 alarmInfoCode = data.value(1);
    QString alarmInfo = "通过";
    if (alarmInfoCode == 4864) {
        alarmInfo = "不通过(压力过高)";
    } else if (alarmInfoCode == 5120) {
        alarmInfo = "不通过(压力过低)";
    } else if (alarmInfoCode != 0) {
        alarmInfo = QString("不通过(未知报警: %1)").arg(alarmInfoCode);
    }

    // 8963/8964：压力值
    auto swap16 = [](quint16 v) -> quint16 { return ((v & 0x00FF) << 8) | ((v & 0xFF00) >> 8); };
    quint16 lowCode = data.value(2);
    quint16 highCode = data.value(3);
    quint16 lowRaw = swap16(lowCode);
    quint16 highRaw = swap16(highCode);
    quint32 rawValue = (static_cast<quint32>(highRaw) << 16) | static_cast<quint32>(lowRaw);
    double pressureValue = static_cast<double>(rawValue) / 1000.0;

    // 8965：压力单位
    quint16 pressureUnitCode = data.value(4);
    PressureUnit::Unit pressureUnitEnum = PressureUnit::fromCode(pressureUnitCode);
    QString pressureUnit = PressureUnit::toString(pressureUnitEnum);

    // 8968：泄漏单位
    quint16 leakUnitCode = data.value(7);
    LeakUnitType::Unit leakUnitEnum = LeakUnitType::getUnitFromCode(leakUnitCode, LeakUnitType::CodeType1);
    QString leakUnit = LeakUnitType::getUnitString(leakUnitEnum);

    // 8966/8967：泄漏值
    quint16 r1Code = data.value(5);
    quint16 r2Code = data.value(6);
    quint16 r1Swapped = swap16(r1Code);
    quint16 r2Swapped = swap16(r2Code);
    // 根据泄漏单位选择缩放因子：Pa单位使用10，其他单位使用1000
    double leakScale = (leakUnitEnum == LeakUnitType::Pa) ? 10.0 : 1000.0;
    double leakValue = static_cast<double>(static_cast<int32_t>(r1Swapped) - static_cast<int32_t>(r2Swapped)) / leakScale;

    // 判断压力值不为0
    if (pressureValue == 0) {
        m_testResultState = TestResultState::Idle;
        return;
    }

    // 去重判断：压力值与上次不同 或 距离上次保存超过5秒
    QDateTime currentTime = QDateTime::currentDateTime();
    bool shouldProcess = (qAbs(pressureValue - lastSavedPressureValue) > 0.001 || 
                          !lastSavedTimestamp.isValid() || 
                          lastSavedTimestamp.secsTo(currentTime) > 5);
    
    if (!shouldProcess) {
        m_testResultState = TestResultState::Idle;
        return; // 重复数据，跳过处理
    }

    // 创建TestResult结构体
    TestResult testResult;
    testResult.createTime = QDateTime::currentDateTime();
    testResult.programNumber = currentProgramNumber;
    testResult.channelPressure = pressureValue;
    testResult.pressureUnit = pressureUnit;
    testResult.channelLeak = leakValue;
    testResult.leakUnit = leakUnit;
    testResult.isPassed = (alarmInfoCode == 0);
    testResult.isFailed = (alarmInfoCode != 0);

    // 使用缓存的通道配置（在onInstrumentSelectionChanged中已读取并缓存）
    // 避免在信号处理函数中同步读取PLC寄存器导致超时
    
    // 调试日志：输出当前程序号和缓存的通道配置
    logMessage(QString("通道匹配检查 - 当前程序号: %1, 通道1: %2(使能:%3), 通道2: %4(使能:%5), 通道3: %6(使能:%7)")
        .arg(testResult.programNumber)
        .arg(cachedTestChannel1).arg(cachedTest1Enabled)
        .arg(cachedTestChannel2).arg(cachedTest2Enabled)
        .arg(cachedTestChannel3).arg(cachedTest3Enabled));
    
    // 标记是否匹配到通道
    bool channelMatched = false;
    
    // 根据程序号和缓存的测试使能状态发送信号
    // 注意：如果多个通道使用相同的程序号，需要同时发送信号到所有匹配的通道
    if (testResult.programNumber == cachedTestChannel1 && cachedTest1Enabled == 1) {
        if (testResult.isPassed && !testResult.isFailed) {
            writePlcRegister(510, 1, QModbusDataUnit::Coils, nullptr);
        } else if (!testResult.isPassed && testResult.isFailed) {
            writePlcRegister(511, 1, QModbusDataUnit::Coils, nullptr);
        }
        emit testResultReady(testResult);
        logMessage(QString("测试结果已处理 - 通道1 (程序号: %1)").arg(testResult.programNumber));
        channelMatched = true;
    }
    
    if (testResult.programNumber == cachedTestChannel2 && cachedTest2Enabled == 1) {
        if (testResult.isPassed && !testResult.isFailed) {
            writePlcRegister(512, 1, QModbusDataUnit::Coils, nullptr);
        } else if (!testResult.isPassed && testResult.isFailed) {
            writePlcRegister(513, 1, QModbusDataUnit::Coils, nullptr);
        }
        emit testResultReady2(testResult);
        logMessage(QString("测试结果已处理 - 通道2 (程序号: %1)").arg(testResult.programNumber));
        channelMatched = true;
    }
    
    if (testResult.programNumber == cachedTestChannel3 && cachedTest3Enabled == 1) {
        if (testResult.isPassed && !testResult.isFailed) {
            writePlcRegister(514, 1, QModbusDataUnit::Coils, nullptr);
        } else if (!testResult.isPassed && testResult.isFailed) {
            writePlcRegister(515, 1, QModbusDataUnit::Coils, nullptr);
        }
        emit testResultReady3(testResult);
        logMessage(QString("测试结果已处理 - 通道3 (程序号: %1)").arg(testResult.programNumber));
        channelMatched = true;
    }

    // 只有匹配到通道时才保存到数据库
    if (channelMatched) {
        // 解析额外数据用于数据库保存
        // 8969：标准大气压
        quint16 atmPressureCode = data.value(8);
        double atmPressure = 1006.0746484375 + atmPressureCode / 25600.0;

        // 8970：大气压单位
        quint16 atmPressureUnitCode = data.value(9);
        QString atmPressureUnit = "hPa";
        if (atmPressureUnitCode == 1) atmPressureUnit = "kPa";
        else if (atmPressureUnitCode == 2) atmPressureUnit = "MPa";

        // 8971：温度
        quint16 temperatureCode = data.value(10);
        double temperature = 31.4853515625 + temperatureCode / 25600.0;

        // 8972：温度单位
        quint16 temperatureUnitCode = data.value(11);
        QString temperatureUnit = "°C";
        if (temperatureUnitCode == 1) temperatureUnit = "°F";
        else if (temperatureUnitCode == 2) temperatureUnit = "K";

        // 添加到表格
        addTestResultRow(currentTime, alarmInfo, pressureValue, pressureUnit,
                         leakValue, leakUnit, currentProgramNumber, currentProductNumber, alarmInfoCode,
                         m_currentUser.realName.isEmpty() ? m_currentUser.username : m_currentUser.realName);

        // 保存到数据库
        AirtightTestResultData resultData;
        resultData.timestamp = currentTime;
        resultData.alarmInfo = alarmInfo;
        resultData.pressureValue = pressureValue;
        resultData.pressureUnit = pressureUnit;
        resultData.leakValue = leakValue;
        resultData.leakUnit = leakUnit;
        resultData.atmPressure = atmPressure;
        resultData.atmPressureUnit = atmPressureUnit;
        resultData.temperature = temperature;
        resultData.temperatureUnit = temperatureUnit;
        resultData.programNumber = currentProgramNumber;
        resultData.productNumber = currentProductNumber;
        resultData.alarmCode = alarmInfoCode;
        resultData.inspector = m_currentUser.realName.isEmpty() ? m_currentUser.username : m_currentUser.realName;

        if (AirtightTestResultDao::saveResult(resultData)) {
            lastSavedTimestamp = currentTime;
            lastSavedPressureValue = pressureValue;
            logMessage("测试结果已保存到数据库");
            // 有新测试结果时刷新页面
            loadTestResultsFromDatabase();
        } else {
            logMessage("测试结果保存到数据库失败", true);
        }
        
        // 写入测试完成信号
        writePlcRegister(516, 1, QModbusDataUnit::Coils, nullptr);
    }
    
    // 重置状态机为空闲状态，允许处理下一个测试结果
    m_testResultState = TestResultState::Idle;
}

void AirtightTestResult::logMessage(const QString &message, bool isError)
{
    QString level = isError ? "error" : "info";
    Logger::log(level, message);
}




void AirtightTestResult::addTestResultRow(const QDateTime &timestamp, const QString &alarmInfo,
                                          double pressureValue, const QString &pressureUnit,
                                          double leakValue, const QString &leakUnit,
                                          int programNumber,
                                          const QString &productNumber,
                                          int alarmCode,
                                          const QString &inspector)
{
    if (!tableModel) return;

    // 计算报警代码文本：优先使用传入的alarmCode；未传入时基于报警信息派生
    auto deriveAlarmCodeText = [&](const QString &info) -> QString {
        if (info == "通过") return QString::number(0);
        if (info.contains("压力过高")) return QString::number(4864);
        if (info.contains("压力过低")) return QString::number(5120);
        int idx = info.indexOf("未知报警:");
        if (idx >= 0) {
            idx += QString("未知报警: ").length();
            int end = info.indexOf(')', idx);
            if (end > idx) {
                return info.mid(idx, end - idx).trimmed();
            }
        }
        return QString(); // 无法派生则留空
    };
    QString alarmCodeText = (alarmCode >= 0) ? QString::number(alarmCode) : deriveAlarmCodeText(alarmInfo);

    QList<QStandardItem*> items;
    items << new QStandardItem(timestamp.toString("yyyy-MM-dd HH:mm:ss"))
          << new QStandardItem(programNumber >= 0 ? QString::number(programNumber) : QString("未知"))
          << new QStandardItem(productNumber)
          << new QStandardItem(alarmInfo)
          << new QStandardItem(alarmCodeText)
          << new QStandardItem(QString::number(pressureValue))
          << new QStandardItem(pressureUnit)
          << new QStandardItem(QString::number(leakValue))
          << new QStandardItem(leakUnit)
          << new QStandardItem(inspector);

    tableModel->appendRow(items);
}

void AirtightTestResult::on_exportButton_clicked()
{
    bool hasFilter = proxyModel && tableModel &&
                     (proxyModel->rowCount() != tableModel->rowCount());

    bool exportAll = true;
    if (hasFilter) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("导出数据");
        msgBox.setText(QString("当前筛选后共 %1 条记录，全部共 %2 条记录，请选择导出范围：")
                           .arg(proxyModel->rowCount())
                           .arg(tableModel->rowCount()));
        QPushButton *btnFiltered = msgBox.addButton("导出筛选数据", QMessageBox::AcceptRole);
        Q_UNUSED(btnFiltered);
        QPushButton *btnAll      = msgBox.addButton("导出全部数据", QMessageBox::ActionRole);
        msgBox.addButton("取消", QMessageBox::RejectRole);
        msgBox.exec();

        if (msgBox.clickedButton() == nullptr ||
            msgBox.buttonRole(msgBox.clickedButton()) == QMessageBox::RejectRole) {
            return;
        }
        exportAll = (msgBox.clickedButton() == btnAll);
    }

    QString defaultFileName = QString("测试数据_%1.xls").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    QString fileName = QFileDialog::getSaveFileName(this, "导出测试数据", defaultFileName, "Excel 文件 (*.xls *.xlsx);;所有文件 (*)");
    if (fileName.isEmpty()) {
        return;
    }
    exportToExcel(fileName, exportAll);
}

void AirtightTestResult::clearTestData()
{
    if (AirtightTestResultDao::clearResults()) {
        loadTestResultsFromDatabase();
        logMessage("测试数据已清空");
    }
}

void AirtightTestResult::exportToExcel(const QString &fileName, bool exportAll)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", QString("无法保存Excel文件: %1").arg(file.errorString()));
        logMessage(QString("无法导出测试数据: %1").arg(file.errorString()), true);
        return;
    }

    QTextStream out(&file);
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
    #else
    out.setCodec("UTF-8");
    #endif

    out << "<html xmlns:o=\"urn:schemas-microsoft-com:office:office\" xmlns:x=\"urn:schemas-microsoft-com:office:excel\" xmlns=\"http://www.w3.org/TR/REC-html40\">\n";
    out << "<head>\n";
    out << "<!--[if gte mso 9]><xml><x:ExcelWorkbook><x:ExcelWorksheets><x:ExcelWorksheet><x:Name>测试数据</x:Name><x:WorksheetOptions><x:DisplayGridlines/></x:WorksheetOptions></x:ExcelWorksheet></x:ExcelWorksheets></x:ExcelWorkbook></xml><![endif]-->\n";
    out << "<meta charset=\"utf-8\"/>\n";
    out << "</head>\n";
    out << "<body>\n";
    out << "<table border=\"1\">\n";

    QStringList headers = {"创建时间", "程序号", "产品编号", "报警信息", "报警代码", "压力值", "压力单位",
                          "泄露值", "泄露单位", "质检人"};
    
    out << "<tr>";
    for (const QString& header : headers) {
        out << "<th style=\"background-color:#4472C4;color:white;font-weight:bold;\">" << header.toHtmlEscaped() << "</th>";
    }
    out << "</tr>\n";

    int rowCount = 0;
    if (exportAll) {
        QList<AirtightTestResultData> allResults = AirtightTestResultDao::getAllResults();
        std::sort(allResults.begin(), allResults.end(), [](const AirtightTestResultData& a, const AirtightTestResultData& b) {
            return a.timestamp > b.timestamp;
        });
        
        for (const auto& result : allResults) {
            out << "<tr>";
            out << "<td>" << result.timestamp.toString("yyyy-MM-dd HH:mm:ss").toHtmlEscaped() << "</td>";
            out << "<td>" << (result.programNumber >= 0 ? QString::number(result.programNumber) : "未知") << "</td>";
            out << "<td style=\"mso-number-format:\\@\">" << result.productNumber.toHtmlEscaped() << "</td>";
            out << "<td>" << result.alarmInfo.toHtmlEscaped() << "</td>";
            out << "<td>" << (result.alarmCode >= 0 ? QString::number(result.alarmCode) : "") << "</td>";
            out << "<td style=\"mso-number-format:\\#\\,\\#\\#0\\.000;\">" << QString::number(result.pressureValue) << "</td>";
            out << "<td>" << result.pressureUnit.toHtmlEscaped() << "</td>";
            out << "<td style=\"mso-number-format:\\#\\,\\#\\#0\\.000;\">" << QString::number(result.leakValue) << "</td>";
            out << "<td>" << result.leakUnit.toHtmlEscaped() << "</td>";
            out << "<td>" << result.inspector.toHtmlEscaped() << "</td>";
            out << "</tr>\n";
            rowCount++;
        }
    } else {
        if (!tableModel) {
            QMessageBox::warning(this, "错误", "表格模型未初始化，无法导出。");
            file.close();
            return;
        }

        int modelRowCount = proxyModel ? proxyModel->rowCount() : tableModel->rowCount();
        for (int r = 0; r < modelRowCount; ++r) {
                out << "<tr>";
                for (int c = 0; c < tableModel->columnCount(); ++c) {
                    QModelIndex idx = proxyModel ? proxyModel->mapToSource(proxyModel->index(r, c)) : tableModel->index(r, c);
                    QString cell = tableModel->itemFromIndex(idx) ? tableModel->itemFromIndex(idx)->text() : "";
                    bool isNumeric = (c == 5 || c == 7);
                    bool isProductNumber = (c == 2);
                    if (isNumeric) {
                        out << "<td style=\"mso-number-format:\\#\\,\\#\\#0\\.000;\">" << cell.toHtmlEscaped() << "</td>";
                    } else if (isProductNumber) {
                        out << "<td style=\"mso-number-format:\\@\">" << cell.toHtmlEscaped() << "</td>";
                    } else {
                        out << "<td>" << cell.toHtmlEscaped() << "</td>";
                    }
                }
                out << "</tr>\n";
            rowCount++;
        }
    }

    out << "</table>\n";
    out << "</body>\n";
    out << "</html>\n";

    file.close();
    logMessage(QString("测试数据已成功导出到: %1（%2条）").arg(fileName).arg(rowCount), false);
    
    QMessageBox::information(this, "导出成功", QString("测试数据已成功导出到：\n%1\n共 %2 条记录").arg(fileName).arg(rowCount));
}

// 筛选条件变化时自动刷新
void AirtightTestResult::onFilterChanged()
{
    applyFilter();
    updateRecordCount();
}

// 自动刷新定时器
void AirtightTestResult::onAutoRefreshTimer()
{
    loadTestResultsFromDatabase();
}

void AirtightTestResult::on_clearQueryButton_clicked()
{
    // 恢复默认查询条件
    QDate today = QDate::currentDate();
    ui->startDateTimeEdit->setDateTime(QDateTime(today, QTime(0, 0, 0)));
    ui->endDateTimeEdit->setDateTime(QDateTime(today, QTime(23, 59, 59)));
    ui->resultComboBox->setCurrentIndex(0); // 约定第一个为“全部”
     if (ui->productNumberLineEdit) ui->productNumberLineEdit->clear();
     ui->inspectorComboBox->setCurrentIndex(0); // 重置质检人为全部
 
     // 重新加载全部数据
     loadTestResultsFromDatabase();
}

// 应用筛选条件
void AirtightTestResult::applyFilter()
{
    if (!tableModel) return;
    
    QDateTime startTime = ui->startDateTimeEdit->dateTime();
    QDateTime endTime = ui->endDateTimeEdit->dateTime();
    QString resultCondition = ui->resultComboBox->currentText();
    QString productNumber = ui->productNumberLineEdit ? ui->productNumberLineEdit->text().trimmed() : QString();
    QString inspector = ui->inspectorComboBox->currentText();
    if (inspector == "全部") inspector = "";
    
    // 从数据库查询筛选后的数据
    allFilteredResults = AirtightTestResultDao::queryResults(startTime, endTime, "全部", resultCondition, productNumber, inspector);
    
    // 按时间戳降序排序
    std::sort(allFilteredResults.begin(), allFilteredResults.end(), [](const AirtightTestResultData& a, const AirtightTestResultData& b) {
        return a.timestamp > b.timestamp;
    });
    
    // 更新分页信息
    totalRecords = allFilteredResults.size();
    totalPages = (totalRecords + pageSize - 1) / pageSize;
    if (totalPages < 1) totalPages = 1;
    
    // 重置到第一页
    currentPage = 1;
    
    // 显示当前页数据
    displayCurrentPage();
    
    // 更新分页控件状态
    updatePagination();
}

// 显示当前页数据
void AirtightTestResult::displayCurrentPage()
{
    if (!tableModel) return;
    
    // 清空表格
    tableModel->removeRows(0, tableModel->rowCount());
    
    // 计算当前页的数据范围
    int startIndex = (currentPage - 1) * pageSize;
    int endIndex = qMin(startIndex + pageSize, totalRecords);
    
    // 填充当前页数据
    for (int i = startIndex; i < endIndex; ++i) {
        const auto& result = allFilteredResults[i];
        addTestResultRow(result.timestamp, result.alarmInfo, result.pressureValue, result.pressureUnit,
                         result.leakValue, result.leakUnit, result.programNumber, result.productNumber, result.alarmCode, result.inspector);
    }
}

// 更新分页控件状态
void AirtightTestResult::updatePagination()
{
    // 更新页码显示
    ui->pageInfoLabel->setText(QString("第 %1 / %2 页").arg(currentPage).arg(totalPages));
    
    // 更新按钮状态
    ui->firstPageButton->setEnabled(currentPage > 1);
    ui->prevPageButton->setEnabled(currentPage > 1);
    ui->nextPageButton->setEnabled(currentPage < totalPages);
    ui->lastPageButton->setEnabled(currentPage < totalPages);
}

// 更新记录数显示
void AirtightTestResult::updateRecordCount()
{
    ui->recordCountLabel->setText(QString("共 %1 条记录").arg(totalRecords));
}

// 分页按钮槽函数
void AirtightTestResult::onFirstPageClicked()
{
    if (currentPage > 1) {
        currentPage = 1;
        displayCurrentPage();
        updatePagination();
    }
}

void AirtightTestResult::onPrevPageClicked()
{
    if (currentPage > 1) {
        currentPage--;
        displayCurrentPage();
        updatePagination();
    }
}

void AirtightTestResult::onNextPageClicked()
{
    if (currentPage < totalPages) {
        currentPage++;
        displayCurrentPage();
        updatePagination();
    }
}

void AirtightTestResult::onLastPageClicked()
{
    if (currentPage < totalPages) {
        currentPage = totalPages;
        displayCurrentPage();
        updatePagination();
    }
}

void AirtightTestResult::onPageSizeChanged(int index)
{
    // 根据下拉框选项设置每页条数
    QStringList sizes = {"20", "50", "100", "200"};
    if (index >= 0 && index < sizes.size()) {
        pageSize = sizes[index].toInt();
        
        // 重新计算总页数
        totalPages = (totalRecords + pageSize - 1) / pageSize;
        if (totalPages < 1) totalPages = 1;
        
        // 确保当前页不超过总页数
        if (currentPage > totalPages) {
            currentPage = totalPages;
        }
        
        // 刷新显示
        displayCurrentPage();
        updatePagination();
    }
}


// 新增：接收产品编号的setter实现
void AirtightTestResult::setCurrentProductNumber(const QString &code)
{
    currentProductNumber = code.trimmed();
}

void AirtightTestResult::setCurrentUser(const User &user)
{
    m_currentUser = user;
    
    // 根据用户权限控制相关按钮的可见性
}


