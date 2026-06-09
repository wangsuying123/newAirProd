#include "paramsetting.h"
#include "paramsetting.h"
#include "ui_paramsetting.h"
#include "pressureunit.h"
#include "FillType.h"
#include "LeakUnitType.h"
#include "VolumeUnit.h"
#include "VolumeEncoding.h"
#include "Logger.h"
#include "AirTightnessParamsDao.h"
#include <QModbusClient>
#include <QModbusReply>
#include <QModbusDevice>
#include <QMessageBox>
#include <QDateTime>
#include <QModbusDataUnit>
#include <QSettings>
#include <QDebug>
#include <QThread>
#include <QEventLoop>
#include <QTimer>
#include <QElapsedTimer>
#include <QScrollBar>
#include <QDataStream>
#include <QListWidgetItem>
#include <QInputDialog>

// 全局压力变量，用于保存最近一次发送的填充压力值
quint16 g_lastSentFillPressure = 0;

ParamSetting::ParamSetting(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ParamSetting),
    modbusClient(nullptr),
    pressureModbusClient(nullptr),
    slaveId(1),
    currentParamId(-1),
    connectionDelayTimer(nullptr)
{
    ui->setupUi(this);
    initUi();
    updateParamsList();
    
    // 初始化定时器
    connectionDelayTimer = new QTimer(this);
    
    // 连接程序号组合框的信号槽
    connect(ui->programNumberComboBox, &QComboBox::currentTextChanged, this, &ParamSetting::onProgramNumberChanged);
    connectionDelayTimer->setSingleShot(true); // 设置为单次触发

    // 初始化时间参数最大值
    ui->fillTimeSpinBox->setMaximum(9999.99);
    ui->stabilizationTimeSpinBox->setMaximum(9999.99);
    ui->testTimeSpinBox->setMaximum(9999.99);
    ui->dumpTimeSpinBox->setMaximum(9999.99);
    
    // 尝试从数据库加载程序号1的参数
    AirTightnessFullParams params = AirTightnessParamsDao::getParamsByProgramNumber(1);
    if (params.id != -1) {
        // 数据库中有程序号1的参数，加载到界面
        loadParamsToUI(params);
        currentParamId = params.id;
        qDebug() << "已从数据库加载程序1的参数";
    } else {
        // 数据库中没有程序号1的参数，使用默认值
        // 删除日志：数据库中没有找到程序1的参数，使用默认值
        qDebug() << "数据库中没有找到程序1的参数，使用默认值";
        AirTightnessFullParams defaultParams;
        defaultParams.paramName = "默认参数程序1";
        defaultParams.programNumber = 1;
        
        // 周期时间默认值
        defaultParams.cycleTime.fillTime = 10.0;
        defaultParams.cycleTime.stabilizationTime = 5.0;
        defaultParams.cycleTime.testTime = 10.0;
        defaultParams.cycleTime.dumpTime = 5.0;
        
        // 压力默认值
        defaultParams.pressure.unitIndex = 0; // Pa
        defaultParams.pressure.maximum = 2000.0;
        defaultParams.pressure.minimum = 0.0;
        defaultParams.pressure.setFill = 1000.0;
        defaultParams.pressure.fillTypeIndex = 0; // 快速
        defaultParams.pressure.regulatorPressure = 0.0;

        // 泄漏默认值
        defaultParams.leak.leakUnitIndex = 0; // Pa/s
        defaultParams.leak.testReject = 50.0;
        defaultParams.leak.refReject = 20.0;
        defaultParams.leak.offset = 0.0;
        defaultParams.leak.volume = 100.0;
        defaultParams.leak.volumeUnitIndex = 0; // cm³
        defaultParams.leak.rejectCalcIndex = 0; // 标准泄漏率
        defaultParams.leak.stdAtm = 1013.25;
        defaultParams.leak.stdTemp = 25.0;
        
        // 加载默认参数到界面
        loadParamsToUI(defaultParams);
    }
}

ParamSetting::~ParamSetting()
{
    // 清理定时器资源
    if (connectionDelayTimer) {
        connectionDelayTimer->stop();
        delete connectionDelayTimer;
        connectionDelayTimer = nullptr;
    }
    delete ui;
}

void ParamSetting::setModbusClient(QModbusClient *client)
{
    modbusClient = client;
    if (modbusClient) {
        connect(modbusClient, &QModbusDevice::errorOccurred, this, &ParamSetting::onModbusError);
        // 新增：跟踪连接状态变化以更新按钮和标签
        connect(modbusClient, &QModbusDevice::stateChanged, this, [this](QModbusDevice::State state){
            updateConnectionStatus(true, state == QModbusDevice::ConnectedState);
        });
        // 初始化时同步UI状态
        updateConnectionStatus(true, modbusClient->state() == QModbusDevice::ConnectedState);
    }
}

void ParamSetting::setPressureModbusClient(QModbusClient *client)
{
    // 如果已经有旧的客户端，断开连接信号
    if (pressureModbusClient) {
        disconnect(pressureModbusClient, &QModbusDevice::errorOccurred, this, &ParamSetting::onModbusError);
        // 可选：断开旧的stateChanged连接，避免重复更新
        disconnect(pressureModbusClient, &QModbusDevice::stateChanged, this, nullptr);
    }
    
    pressureModbusClient = client;
    
    // 连接新客户端的错误和状态信号
    if (pressureModbusClient) {
        connect(pressureModbusClient, &QModbusDevice::errorOccurred, this, &ParamSetting::onModbusError);
        connect(pressureModbusClient, &QModbusDevice::stateChanged, this, [this](QModbusDevice::State state){
            updatePressureConnectionStatus(state == QModbusDevice::ConnectedState);
        });
        // 初始化时同步UI状态（调压装置）
        updateConnectionStatus(false, pressureModbusClient->state() == QModbusDevice::ConnectedState);
    }
}

void ParamSetting::updateConnectionStatus(bool isPlc, bool connected)
{
    if (connected) {
        if (isPlc) {
            ui->sendToDeviceButton->setEnabled(true);
        }
    } else {
        if (isPlc) {
            ui->sendToDeviceButton->setEnabled(false);
        }
    }
}

void ParamSetting::updatePressureConnectionStatus(bool connected)
{
    updateConnectionStatus(false, connected);
}

void ParamSetting::on_sendToDeviceButton_clicked()
{
    // 收集参数并发送到设备
    if (sendParamsToDevice()) {
        // 需要把压力值发送调压装置
        // 使用功能码06向调压装置写入压力值到寄存器80，从站地址1
        if (pressureModbusClient) {
            if (pressureModbusClient->state() == QModbusDevice::ConnectedState) {
                QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, 80, 1);
                writeUnit.setValue(0, g_lastSentFillPressure);
                if (QModbusReply *reply = pressureModbusClient->sendWriteRequest(writeUnit, 1)) {
                    QEventLoop loop;
                    QTimer timer; timer.setSingleShot(true); timer.start(1000);
                    connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
                    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
                    loop.exec();
                    // 成功与失败在界面用提示框提示，移除日志
                    if (!(timer.isActive() && reply->error() == QModbusDevice::NoError)) {
                        // 写入失败，无需日志
                    }
                    reply->deleteLater();
                } else {
                    // 发送请求失败，无需日志
                }
            } else {
                // 未连接到调压装置，无需日志
            }
        } else {
            // 调压装置客户端未初始化，无需日志
        }
        
        QMessageBox::information(this, "成功", "参数已成功发送到设备");
    } else {
        QMessageBox::critical(this, "失败", "参数发送失败，请检查连接和设备状态");
    }
}

void ParamSetting::on_saveParametersButton_clicked()
{
    // 从UI收集所有参数
    AirTightnessFullParams params = collectParamsFromUI();

    // 根据程序号判断是否存在，存在则更新，不存在则新增
    AirTightnessFullParams existing = AirTightnessParamsDao::getParamsByProgramNumber(params.programNumber);
    bool success = false;
    if (existing.id != -1) {
        // 覆盖原始数据
        params.id = existing.id;
        params.createdAt = existing.createdAt;
        params.updatedAt = QDateTime::currentDateTime();
        success = AirTightnessParamsDao::updateParams(params);
        if (success) {
            currentParamId = params.id;
        }
    } else {
        // 新增一条数据
        params.createdAt = QDateTime::currentDateTime();
        params.updatedAt = params.createdAt;
        success = AirTightnessParamsDao::saveParams(params);
    }

    if (success) {
        QMessageBox::information(this, "成功", "参数已成功保存");
        updateParamsList();
    } else {
        QMessageBox::critical(this, "失败", "参数保存失败");
    }
}

void ParamSetting::on_loadParametersButton_clicked()
{
    // 从数据库加载参数
    // 这里可以实现一个对话框让用户选择要加载的参数
    QList<AirTightnessFullParams> paramsList = AirTightnessParamsDao::getAllParams();
    
    if (paramsList.isEmpty()) {
        QMessageBox::information(this, "提示", "数据库中没有保存的参数");
        return;
    }
    
    // 简单实现：加载第一个参数
    loadParamsToUI(paramsList.first());
    currentParamId = paramsList.first().id;
}

void ParamSetting::onModbusError(QModbusDevice::Error error)
{
    QString errorMsg;
    switch (error) {
    case QModbusDevice::NoError:
        return;
    case QModbusDevice::ReadError:
        errorMsg = "读取错误";
        break;
    case QModbusDevice::WriteError:
        errorMsg = "写入错误";
        break;
    case QModbusDevice::ConnectionError:
        errorMsg = "连接错误";
        break;
    case QModbusDevice::ConfigurationError:
        errorMsg = "配置错误";
        break;
    case QModbusDevice::TimeoutError:
        errorMsg = "超时错误";
        break;
    default:
        errorMsg = "未知错误";
        break;
    }
    
    // 如果是连接错误，更新连接状态
    if (error == QModbusDevice::ConnectionError) {
        updateConnectionStatus(false, false);
    }
}

void ParamSetting::initUi()
{
    // 初始化UI控件
    // 设置程序号组合框
    ui->programNumberComboBox->setCurrentIndex(0);  // 默认选择程序号1
    
    // 设置压力单位组合框
    ui->pressureUnitComboBox->clear();
    ui->pressureUnitComboBox->addItem(PressureUnit::toString(PressureUnit::Pa), QVariant::fromValue(PressureUnit::Pa));
    ui->pressureUnitComboBox->addItem(PressureUnit::toString(PressureUnit::Kpa), QVariant::fromValue(PressureUnit::Kpa));
    ui->pressureUnitComboBox->addItem(PressureUnit::toString(PressureUnit::Mpa), QVariant::fromValue(PressureUnit::Mpa));
    ui->pressureUnitComboBox->addItem(PressureUnit::toString(PressureUnit::Bar), QVariant::fromValue(PressureUnit::Bar));
    
    // 设置填充类型组合框
    ui->fillTypeComboBox->clear();
    ui->fillTypeComboBox->addItem(FillType::toChineseString(FillType::Standard), QVariant::fromValue(FillType::Standard));
    ui->fillTypeComboBox->addItem(FillType::toChineseString(FillType::Auto), QVariant::fromValue(FillType::Auto));
    ui->fillTypeComboBox->addItem(FillType::toChineseString(FillType::Instruction), QVariant::fromValue(FillType::Instruction));
    ui->fillTypeComboBox->addItem(FillType::toChineseString(FillType::Ramp), QVariant::fromValue(FillType::Ramp));
    ui->fillTypeComboBox->addItem(FillType::toChineseString(FillType::RampControl), QVariant::fromValue(FillType::RampControl));
    
    // 设置泄漏单位组合框
    ui->leakUnitComboBox->clear();
    for (const auto& unit : LeakUnitType::getAllUnits()) {
        ui->leakUnitComboBox->addItem(LeakUnitType::getUnitString(unit), QVariant::fromValue(unit));
    }
    
    // 设置容积单位组合框
    ui->volumeUnitComboBox->clear();
    ui->volumeUnitComboBox->addItem("cm³", QVariant::fromValue(VolumeUnit::CubicCentimeter));
    ui->volumeUnitComboBox->addItem("mm³", QVariant::fromValue(VolumeUnit::CubicMillimeter));
    ui->volumeUnitComboBox->addItem("mL", QVariant::fromValue(VolumeUnit::Milliliter));
    ui->volumeUnitComboBox->addItem("L", QVariant::fromValue(VolumeUnit::Liter));
    ui->volumeUnitComboBox->addItem("inch³", QVariant::fromValue(VolumeUnit::CubicInch));
    ui->volumeUnitComboBox->addItem("feet³", QVariant::fromValue(VolumeUnit::CubicFeet));
    
    // 设置泄漏计算方式组合框
    ui->rejectCalcComboBox->clear();
    ui->rejectCalcComboBox->addItem("标准泄漏率");
    ui->rejectCalcComboBox->addItem("绝对泄漏率");
    ui->rejectCalcComboBox->addItem("相对泄漏率");
    
    // 初始禁用发送按钮
    ui->sendToDeviceButton->setEnabled(false);
}

bool ParamSetting::sendModbusCommand(quint16 address, quint16 value, int timeoutMs, quint16 secondValue)
{
    if (!modbusClient) {
        return false;
    }

    if (modbusClient->state() != QModbusDevice::ConnectedState) {
        return false;
    }

    // 强制使用功能码16（Write Multiple Registers）
    // 即使只写入一个寄存器，也创建包含两个寄存器的数据单元以确保使用功能码16
    int numValues = (secondValue != 0) ? 2 : 2; // 始终至少使用2个寄存器
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, address, numValues);
    writeUnit.setValue(0, value);
    if (secondValue != 0) {
        writeUnit.setValue(1, secondValue);
    } else {
        // 对于单个寄存器写入，第二个寄存器设置为0（不影响设备）
        writeUnit.setValue(1, 0);
    }

    // 发送写入请求 - 使用功能码16
    QModbusReply *reply = modbusClient->sendWriteRequest(writeUnit, static_cast<quint8>(slaveId));
    if (!reply) {
        Logger::log(Logger::Error, QString("发送请求失败: %1").arg(modbusClient->errorString()));
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
    if (timer.isActive()) {
        // 正常收到回复
        timer.stop();

        if (reply->error() != QModbusDevice::NoError) {
            reply->deleteLater();
            return false;
        }

        reply->deleteLater();
        return true;
    } else {
        // 超时处理
        disconnect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
        reply->deleteLater();
        return false;
    }
}
AirTightnessFullParams ParamSetting::collectParamsFromUI()
{
    AirTightnessFullParams params;
    
    // 设置参数名称（由于UI中没有输入框，设置为当前时间戳作为默认名称）
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    
    // 获取当前选择的程序号
    int programNumber = ui->programNumberComboBox->currentText().toInt();
    params.paramName = QString("参数组_%1_程序%2").arg(timestamp).arg(programNumber);
    params.programNumber = programNumber;
    
    // 收集周期时间参数
    params.cycleTime.fillTime = ui->fillTimeSpinBox->value();
    params.cycleTime.stabilizationTime = ui->stabilizationTimeSpinBox->value();
    params.cycleTime.testTime = ui->testTimeSpinBox->value();
    params.cycleTime.dumpTime = ui->dumpTimeSpinBox->value();
    
    // 收集压力参数
    PressureUnit::Unit pressureUnit = ui->pressureUnitComboBox->currentData().value<PressureUnit::Unit>();
    params.pressure.unitIndex = PressureUnit::toCode(pressureUnit);
    // 压力单位编码采集完成
    
    params.pressure.maximum = ui->maxPressureSpinBox->value();
    params.pressure.minimum = ui->minPressureSpinBox->value();
    params.pressure.setFill = ui->setFillSpinBox->value();
    
    FillType::Type fillTypeSel = ui->fillTypeComboBox->currentData().value<FillType::Type>();
    params.pressure.fillTypeIndex = FillType::toCode(fillTypeSel);
    // 填充类型编码采集完成

    params.pressure.regulatorPressure = ui->regulatorPressureSpinBox->value();

    // 收集泄漏参数
    LeakUnitType::Unit leakUnitSel = ui->leakUnitComboBox->currentData().value<LeakUnitType::Unit>();
    params.leak.leakUnitIndex = LeakUnitType::getCode(leakUnitSel);
    // 泄漏单位编码采集完成
    
    params.leak.testReject = ui->testRejectSpinBox->value();
    params.leak.refReject = ui->refRejectSpinBox->value();
    params.leak.offset = ui->offsetSpinBox->value();
    params.leak.volume = ui->volumeSpinBox->value();
    VolumeUnit volumeUnitSel = ui->volumeUnitComboBox->currentData().value<VolumeUnit>();
    params.leak.volumeUnitIndex = volumeUnitToCode(volumeUnitSel);
    // 体积单位编码采集完成
    
    params.leak.rejectCalcIndex = ui->rejectCalcComboBox->currentIndex();
    params.leak.stdAtm = ui->stdAtmSpinBox->value();
    params.leak.stdTemp = ui->stdTempSpinBox->value();
    
    // 设置创建时间和更新时间
    params.createdAt = QDateTime::currentDateTime();
    params.updatedAt = QDateTime::currentDateTime();
    
    // 如果是更新现有参数，保留ID
    if (currentParamId != -1) {
        params.id = currentParamId;
    }
    
    return params;
}

void ParamSetting::loadParamsToUI(const AirTightnessFullParams& params)
{
    // 加载周期时间参数
    ui->fillTimeSpinBox->setValue(params.cycleTime.fillTime);
    ui->stabilizationTimeSpinBox->setValue(params.cycleTime.stabilizationTime);
    ui->testTimeSpinBox->setValue(params.cycleTime.testTime);
    ui->dumpTimeSpinBox->setValue(params.cycleTime.dumpTime);
    
    // 加载程序号
    ui->programNumberComboBox->setCurrentText(QString::number(params.programNumber));
    
    // 加载压力参数
    {
        PressureUnit::Unit unit = PressureUnit::fromCode(params.pressure.unitIndex);
        for (int i = 0; i < ui->pressureUnitComboBox->count(); ++i) {
            QVariant data = ui->pressureUnitComboBox->itemData(i);
            if (data.isValid() && data.value<PressureUnit::Unit>() == unit) {
                ui->pressureUnitComboBox->setCurrentIndex(i);
                break;
            }
        }
    }
    ui->maxPressureSpinBox->setValue(params.pressure.maximum);
    ui->minPressureSpinBox->setValue(params.pressure.minimum);
    ui->setFillSpinBox->setValue(params.pressure.setFill);
    FillType::Type type = FillType::fromCode(params.pressure.fillTypeIndex);
    for (int i = 0; i < ui->fillTypeComboBox->count(); ++i) {
        QVariant data = ui->fillTypeComboBox->itemData(i);
        if (data.isValid() && data.value<FillType::Type>() == type) {
            ui->fillTypeComboBox->setCurrentIndex(i);
            break;
        }
    }

    ui->regulatorPressureSpinBox->setValue(params.pressure.regulatorPressure);

    // 加载泄漏参数（按编码匹配下拉项）
    {
        LeakUnitType::Unit leakType = LeakUnitType::getUnitFromCode(params.leak.leakUnitIndex, LeakUnitType::CodeType1);
        for (int i = 0; i < ui->leakUnitComboBox->count(); ++i) {
            QVariant data = ui->leakUnitComboBox->itemData(i);
            if (data.isValid() && data.value<LeakUnitType::Unit>() == leakType) {
                ui->leakUnitComboBox->setCurrentIndex(i);
                break;
            }
        }
    }
    ui->testRejectSpinBox->setValue(params.leak.testReject);
    ui->refRejectSpinBox->setValue(params.leak.refReject);
    ui->offsetSpinBox->setValue(params.leak.offset);
    ui->volumeSpinBox->setValue(static_cast<int>(params.leak.volume));
    // 加载容积单位（按编码匹配下拉项）
    {
        VolumeUnit vUnit = volumeUnitFromCode(params.leak.volumeUnitIndex);
        for (int i = 0; i < ui->volumeUnitComboBox->count(); ++i) {
            QVariant data = ui->volumeUnitComboBox->itemData(i);
            if (data.isValid() && data.value<VolumeUnit>() == vUnit) {
                ui->volumeUnitComboBox->setCurrentIndex(i);
                break;
            }
        }
    }
    if (params.leak.rejectCalcIndex >= 0 && params.leak.rejectCalcIndex < ui->rejectCalcComboBox->count()) {
        ui->rejectCalcComboBox->setCurrentIndex(params.leak.rejectCalcIndex);
    }
    ui->stdAtmSpinBox->setValue(params.leak.stdAtm);
    ui->stdTempSpinBox->setValue(params.leak.stdTemp);
    
    // 保存当前参数ID
    currentParamId = params.id;
}

void ParamSetting::onProgramNumberChanged(const QString &programNumber)
{
    // 获取用户选择的程序号
    int progNum = programNumber.toInt();
    
    // 从数据库加载该程序号的参数
    AirTightnessFullParams params = AirTightnessParamsDao::getParamsByProgramNumber(progNum);
    
    if (params.id != -1) {
        // 数据库中有该程序号的参数，加载到界面
        loadParamsToUI(params);
        currentParamId = params.id;
        // 已加载程序参数（移除日志）
    } else {
        // 数据库中没有该程序号的参数，设置默认值
        // 数据库中没有该程序号的参数，使用默认值（移除日志）
        AirTightnessFullParams defaultParams;
        defaultParams.paramName = QString("默认参数程序%1").arg(progNum);
        defaultParams.programNumber = progNum;
        
        // 周期时间默认值
        defaultParams.cycleTime.fillTime = 10.0;
        defaultParams.cycleTime.stabilizationTime = 5.0;
        defaultParams.cycleTime.testTime = 10.0;
        defaultParams.cycleTime.dumpTime = 5.0;
        
        // 压力默认值
        defaultParams.pressure.unitIndex = 0; // Pa
        defaultParams.pressure.maximum = 2000.0;
        defaultParams.pressure.minimum = 0.0;
        defaultParams.pressure.setFill = 1000.0;
        defaultParams.pressure.fillTypeIndex = 0; // 快速
        defaultParams.pressure.regulatorPressure = 0.0;

        // 泄漏默认值
        defaultParams.leak.leakUnitIndex = 0; // Pa/s
        defaultParams.leak.testReject = 50.0;
        defaultParams.leak.refReject = 20.0;
        defaultParams.leak.offset = 0.0;
        defaultParams.leak.volume = 100.0;
        defaultParams.leak.volumeUnitIndex = 0; // cm³
        defaultParams.leak.rejectCalcIndex = 0; // 标准泄漏率
        defaultParams.leak.stdAtm = 1013.25;
        defaultParams.leak.stdTemp = 25.0;
        
        // 加载默认参数到界面
        loadParamsToUI(defaultParams);
    }
}

void ParamSetting::updateParamsList()
{
    // 获取所有参数列表并更新UI
    QList<AirTightnessFullParams> paramsList = AirTightnessParamsDao::getAllParams();
    qDebug() << "参数列表已更新，共" << paramsList.size() << "条记录";

    // 注释：原代码中使用的paramsListWidget控件在UI中不存在
    // 可以考虑添加该控件或使用其他方式展示参数列表
    qDebug() << "参数列表更新：共" << paramsList.size() << "条记录";
    for (const auto& param : paramsList) {
        qDebug() << QString("%1 (创建于: %2)")
                        .arg(param.paramName)
                        .arg(param.createdAt.toString("yyyy-MM-dd HH:mm"));
    }
}

bool ParamSetting::sendParamsToDevice()
{
    if (!modbusClient || modbusClient->state() != QModbusDevice::ConnectedState) {
        return false;
    }

    // 开始收集参数准备发送到设备（移除日志）
    
    const int timeoutMs = 1000; // 单个命令超时时间
    const quint16 startAddress = 8192; // 起始地址
    const quint16 endAddress = 8229;   // 结束地址
    const int numRegisters = endAddress - startAddress + 1; // 寄存器数量
    
    // 先发送地址8192,值为1的命令，通知设备准备接收参数
    sendModbusCommand(8192, 26112, timeoutMs);
    
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
    
    // 发送程序号（使用未使用的地址8193）
    int programNumber = ui->programNumberComboBox->currentText().toInt();
    // writeUnit.setValue(8193 - startAddress, programNumber);
    Logger::log(Logger::Info, QString("开始发送参数到设备，程序号=%1").arg(programNumber));

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
    quint16 fillTime = encodeTime(ui->fillTimeSpinBox->value());
    quint16 stabilizationTime = encodeTime(ui->stabilizationTimeSpinBox->value());
    quint16 testTime = encodeTime(ui->testTimeSpinBox->value());
    quint16 dumpTime = encodeTime(ui->dumpTimeSpinBox->value());

    // 3. 以无符号类型写入寄存器（关键修改：确保setValue接受quint16）
    // 若writeUnit.setValue支持无符号参数，直接传入：
    writeUnit.setValue(8196 - startAddress, fillTime);        // 填充时间（无符号）
    writeUnit.setValue(8197 - startAddress, stabilizationTime); // 稳定时间（无符号）
    writeUnit.setValue(8198 - startAddress, testTime);         // 测试时间（无符号）
    writeUnit.setValue(8199 - startAddress, dumpTime);         // 排放时间（无符号）
    Logger::log(Logger::Debug, QString("时间参数编码: 填充=%1 稳定=%2 测试=%3 排放=%4").arg(fillTime).arg(stabilizationTime).arg(testTime).arg(dumpTime));

    // 4. 压力单位
    PressureUnit::Unit pressureUnit = ui->pressureUnitComboBox->currentData().value<PressureUnit::Unit>();
    quint16 pressureUnitCode = static_cast<quint16>(PressureUnit::toCode(pressureUnit));
    writeUnit.setValue(8200 - startAddress, pressureUnitCode);   // 压力单位
    // 压力单位代码日志移除

    // 3. 压力值参数
    quint16 minPressure = static_cast<quint16>(ui->minPressureSpinBox->value());
    quint16 maxPressure = static_cast<quint16>(ui->maxPressureSpinBox->value());
    quint16 setFill = static_cast<quint16>(ui->setFillSpinBox->value());
    // 将调压阀压力值直接取整数部分并做范围保护后保存到全局变量中（用于调压装置）
    {
        double __val = ui->regulatorPressureSpinBox->value();
        int __intPressure = static_cast<int>((__val + 3) / 0.18); // 直接取整（截断小数）
        if (__intPressure < 0) __intPressure = 0;
        if (__intPressure > 65535) __intPressure = 65535;
        g_lastSentFillPressure = static_cast<quint16>(__intPressure);
    }
    // 压力编码：按示例，将数值×1000得到32位，拆分为高/低16位，并分别进行字节交换
    auto swap16 = [](quint16 v) -> quint16 { return static_cast<quint16>((v << 8) | (v >> 8)); };
    auto encodePressure = [&](quint16 n) -> QPair<quint16, quint16> {
        quint32 raw = static_cast<quint32>(n) * 1000u; // 例如 n=70 -> 70000 (0x00011170)
        quint16 low = static_cast<quint16>(raw & 0xFFFFu);       // 低16位，如0x1170
        quint16 high = static_cast<quint16>((raw >> 16) & 0xFFFFu); // 高16位，如0x0001
        return qMakePair(swap16(low), swap16(high)); // 字节交换：0x1170->0x7011, 0x0001->0x0100
    };
    QPair<quint16, quint16> minEnc = encodePressure(minPressure);
    QPair<quint16, quint16> maxEnc = encodePressure(maxPressure);
    QPair<quint16, quint16> fillEnc = encodePressure(setFill);
    // 最小压力: 写入8201(低位交换) / 8202(高位交换)
    writeUnit.setValue(8201 - startAddress, minEnc.first);
    writeUnit.setValue(8202 - startAddress, minEnc.second);
    // 最大压力: 写入8203 / 8204
    writeUnit.setValue(8203 - startAddress, maxEnc.first);
    writeUnit.setValue(8204 - startAddress, maxEnc.second);
    // 填充压力: 写入8205 / 8206
    writeUnit.setValue(8205 - startAddress, fillEnc.first);
    writeUnit.setValue(8206 - startAddress, fillEnc.second);
    Logger::log(Logger::Info, QString("压力参数编码: 最小(low=%1,high=%2) 最大(low=%3,high=%4) 填充(low=%5,high=%6)")
                .arg(minEnc.first).arg(minEnc.second)
                .arg(maxEnc.first).arg(maxEnc.second)
                .arg(fillEnc.first).arg(fillEnc.second));
    // 已将填充压力（取整后）保存到全局变量（移除日志）

    // 4. 填充类型
    FillType::Type fillType = ui->fillTypeComboBox->currentData().value<FillType::Type>();
    quint16 fillTypeCode = static_cast<quint16>(FillType::toCode(fillType));
    writeUnit.setValue(8211 - startAddress, fillTypeCode);          // 填充类型
    Logger::log(Logger::Info, QString("填充类型代码=%1").arg(fillTypeCode));

    // 5. 泄漏相关参数
    LeakUnitType::Unit leakUnit = ui->leakUnitComboBox->currentData().value<LeakUnitType::Unit>();
    quint16 leakUnitCode = static_cast<quint16>(LeakUnitType::getCode(leakUnit, LeakUnitType::CodeType1));
    quint16 leakUnitCode2 = static_cast<quint16>(LeakUnitType::getCode(leakUnit, LeakUnitType::CodeType2));
    quint16 testReject = static_cast<quint16>(ui->testRejectSpinBox->value());
    quint16 refReject = static_cast<quint16>(ui->refRejectSpinBox->value());
    
    // 允许泄露值编码：根据泄露单位使用不同的编码方式
    quint16 allowLeakEncoded8219 = 0;
    quint16 allowLeakEncoded8220 = 0;
    double allowLeakValue = ui->testRejectSpinBox->value();
    
    if (leakUnit == LeakUnitType::Pa) {
        // Pa单位：寄存器8219 = (2560 × 编码值) mod 65535
        quint32 allowLeakScaled = static_cast<quint32>(allowLeakValue * 2560);
        allowLeakEncoded8219 = static_cast<quint16>(allowLeakScaled % 65535);
        allowLeakEncoded8220 = 0;
        Logger::log(Logger::Info, QString("允许泄露值编码(Pa单位): 原值=%1, 8219=%2, 8220=%3").arg(allowLeakValue).arg(allowLeakEncoded8219).arg(allowLeakEncoded8220));
    } else if (leakUnit == LeakUnitType::MlPerMinute) {
        // ml/min单位：
        // 寄存器8220 = floor(输入值 / 64) × 256
        // 寄存器8219 = ((输入值 × 256000) mod 65535) - 寄存器8220
        allowLeakEncoded8220 = static_cast<quint16>((static_cast<int>(allowLeakValue) / 64) * 256);
        quint32 scaled = static_cast<quint32>(allowLeakValue * 256000);
        allowLeakEncoded8219 = static_cast<quint16>((scaled % 65535) - allowLeakEncoded8220);
        Logger::log(Logger::Info, QString("允许泄露值编码(ml/min单位): 原值=%1, 8219=%2, 8220=%3").arg(allowLeakValue).arg(allowLeakEncoded8219).arg(allowLeakEncoded8220));
    } else {
        // 其他单位：直接使用原值
        allowLeakEncoded8219 = static_cast<quint16>(allowLeakValue);
        allowLeakEncoded8220 = 0;
        Logger::log(Logger::Info, QString("允许泄露值编码(其他单位): 原值=%1, 8219=%2, 8220=%3").arg(allowLeakValue).arg(allowLeakEncoded8219).arg(allowLeakEncoded8220));
    }
    
    writeUnit.setValue(8212 - startAddress, leakUnitCode);     // 泄漏单位
    writeUnit.setValue(8213 - startAddress, 256);      // 测试拒绝阈值
    writeUnit.setValue(8217 - startAddress, leakUnitCode2);       // 8217 泄漏单位2
    writeUnit.setValue(8219 - startAddress, allowLeakEncoded8219);       // 允许泄露值（编码后）
    writeUnit.setValue(8220 - startAddress, allowLeakEncoded8220);       // 允许泄露值高位（ml/min时使用）
    
    VolumeUnit volumeUnit = ui->volumeUnitComboBox->currentData().value<VolumeUnit>();
    quint16 volumeUnitCode = static_cast<quint16>(volumeUnitToCode(volumeUnit));
    int volumeInt = ui->volumeSpinBox->value();
    quint16 volume = static_cast<quint16>(volumeInt);
    // 需要把容积值映射到枚举编码
    {
        quint16 encoded = 0;
        if (VolumeEncoding::tryEncode(static_cast<int>(volume), encoded)) {
            volume = encoded;
        }
    }
    quint16 rejectCalc = static_cast<quint16>(ui->rejectCalcComboBox->currentIndex());
    quint16 stdAtm = static_cast<quint16>(ui->stdAtmSpinBox->value());
    quint16 stdTemp = static_cast<quint16>(ui->stdTempSpinBox->value());
    quint16 offset = static_cast<quint16>(ui->offsetSpinBox->value());
    writeUnit.setValue(8214 - startAddress, volumeUnitCode);        // 容积单位
    // 容积参数：使用新的分段函数编码
    {
        quint16 encoded8215 = VolumeEncoding::encode8215(volumeInt);
        quint16 encoded8216 = 0;
        if (VolumeEncoding::tryEncode(volumeInt, encoded8216)) {
            volume = encoded8216;
        }
        writeUnit.setValue(8215 - startAddress, encoded8215);       
        writeUnit.setValue(8216 - startAddress, encoded8216);          // 容积
        Logger::log(Logger::Info, QString("容积参数: 单位代码=%1 原始容积=%2 8215=%3 8216=%4").arg(volumeUnitCode).arg(volumeInt).arg(encoded8215).arg(encoded8216));
    }
    Logger::log(Logger::Info, QString("泄漏参数: 泄漏单位代码=%1, 测试拒绝=%2, 参考拒绝=%3")
                .arg(leakUnitCode).arg(testReject).arg(refReject));
    Logger::log(Logger::Info, QString("计算方式=%1 标准大气压=%2 标准温度=%3 偏移量=%4")
                .arg(rejectCalc).arg(stdAtm).arg(stdTemp).arg(offset));
    writeUnit.setValue(8218 - startAddress, rejectCalc); // 拒绝计算方式
    writeUnit.setValue(8224 - startAddress, 64);
    writeUnit.setValue(8225 - startAddress, 32068);          // 标准大气压
    writeUnit.setValue(8227 - startAddress, 0);         // 标准温度
    writeUnit.setValue(8229 - startAddress, offset);          // 偏移量
    // 容积及标准值日志移除

    // 发送批量写入请求
    Logger::log(Logger::Info, QString("准备批量发送从地址 %1 到 %2 的参数（共 %3 个寄存器）")
                .arg(startAddress).arg(endAddress).arg(numRegisters));
    
    QModbusReply *reply = modbusClient->sendWriteRequest(writeUnit, static_cast<quint8>(slaveId));
    if (!reply) {
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
            Logger::log(Logger::Error, QString("发送命令失败: %1").arg(reply->errorString()));
        } else {
            Logger::log(Logger::Info, QString("批量参数发送成功 - 从地址 %1 到 %2").arg(startAddress).arg(endAddress));
            success = true;
        }
        reply->deleteLater();
    } else {
        // 超时处理
        Logger::log(Logger::Error, QString("发送命令超时 - 从地址 %1 到 %2").arg(startAddress).arg(endAddress));
        disconnect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
        reply->deleteLater();
    }
    
    if (success) {
        Logger::log(Logger::Info, "所有参数已成功发送到设备");
        // 发送程序号信号
        int programNumber = ui->programNumberComboBox->currentText().toInt();
        emit programNumberSent(programNumber);
    } else {
        Logger::log(Logger::Warning, "发送参数完成命令失败");
    }
    
    return success;
}

// 获取容积单位显示文本
QString ParamSetting::getVolumeUnitString(VolumeUnit unit)
{
    switch(unit) {
    case VolumeUnit::CubicCentimeter: return "cm³";
    case VolumeUnit::CubicMillimeter: return "mm³";
    case VolumeUnit::Milliliter: return "mL";
    case VolumeUnit::Liter: return "L";
    case VolumeUnit::CubicInch: return "inch³";
    case VolumeUnit::CubicFeet: return "feet³";
    default: return "cm³";
    }
}

bool ParamSetting::sendModbusBatchCommand(const QList<QPair<quint16, quint16>>& paramList, int timeout)
{
    if (!modbusClient || paramList.isEmpty()) {
        return false;
    }

    // 开始发送批量Modbus命令（移除日志）

    // 记录开始时间，用于统计总耗时
    QElapsedTimer timer;
    timer.start();
    
    // 发送所有参数（使用sendModbusCommand单个发送，已强制使用功能码16）
    int successCount = 0;
    
    for (int i = 0; i < paramList.size(); i++) {
        const auto& param = paramList[i];
        quint16 address = param.first;
        quint16 value = param.second;
        
        // 单个命令发送（移除日志）
        
        if (sendModbusCommand(address, value, timeout)) {
            successCount++;
            // 单个命令发送成功（移除日志）
        } else {
            // 单个命令发送失败（移除日志）
        }
        
        QThread::msleep(50); // 命令间短暂延迟
    }

    // 批量命令发送完成统计（移除日志）
    // 平均每个命令耗时统计（移除日志）

    return successCount == paramList.size();
}
