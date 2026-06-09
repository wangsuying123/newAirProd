#ifndef PLCMONITOR_H
#define PLCMONITOR_H

#include <QWidget>
#include <QTimer>
#include <QLabel>
#include <QLineEdit>
#include <QModbusClient>
#include <QModbusDataUnit>
#include "TestResultStruct.h"
#include "AirTightnessParams.h"

namespace Ui {
class PlcMonitor;
}

class PlcMonitor : public QWidget
{
    Q_OBJECT

signals:
    // 连接状态变化信号
    void connectionStatusChanged(bool isConnected);
    
    // 仪器评选号变化信号
    void instrumentSelectionChanged(int instrumentSelection);
    
public:
    explicit PlcMonitor(QWidget *parent = nullptr);
    ~PlcMonitor();

    // 设置Modbus客户端
    void setModbusClient(QModbusClient *client);
    
    // 设置气密仪Modbus客户端
    void setAirtightModbusClient(QModbusClient *client);
    
    // 设置调压装置Modbus客户端
    void setPressureModbusClient(QModbusClient *client);
    
    // 设置设备地址
    void setDeviceAddress(int address);
    
    // 设置气密仪设备地址
    void setAirtightDeviceAddress(int address);
    
    // 设置调压装置设备地址
    void setPressureDeviceAddress(int address);

public slots:
    // 更新连接状态
    void updateConnectionStatus(bool isConnected);
    
    // 更新气密仪连接状态
    void updateAirtightConnectionStatus(bool isConnected);
    
    // 更新调压装置连接状态
    void updatePressureConnectionStatus(bool isConnected);
    
    // 处理测试结果信号的槽函数
    void handleTestResultReady(const TestResult &result);
    void handleTestResultReady2(const TestResult &result);
    void handleTestResultReady3(const TestResult &result);
    
    // 新增：清空并统一通道结果样式
    void clearChannelResults();
    
    // 尝试启动气密仪
    void attemptStartAirtight();
    
    // 复位气密仪
    void resetAirtightInstrument();
private slots:
    
    // 更新PLC数据显示
    void updatePlcData();
    
    // 记录日志消息
    void logMessage(const QString &message, const QString &type = "info");
    
    // 使用功能码05向PLC线圈寄存器写入值的异步方法
    void writePlcCoil(quint16 address, int value, const std::function<void(bool)>& callback);
    // 同步阻塞版本（保留用于需要阻塞调用的场景）
    bool writePlcCoilBlocking(quint16 address, int value);

    // 测试控制槽函数
    void on_test1Button_clicked();
    void on_test1OffButton_clicked();
    void on_test2Button_clicked();
    void on_test2OffButton_clicked();
    void on_test3Button_clicked();
    void on_test3OffButton_clicked();

private:
    Ui::PlcMonitor *ui;
    
    // 定时器用于更新时间戳
    QTimer *m_timer;
    
    // OK和NG计数器
    int m_okCount;
    int m_ngCount;
    
    // Modbus客户端指针
    QModbusClient *m_modbusClient;
    
    // 气密仪Modbus客户端指针
    QModbusClient *m_airtightModbusClient;
    
    // 连接状态
    bool m_isConnected;
    bool m_isAirtightConnected;
    bool m_isPressureConnected;
    
    // PLC设备地址
    int m_deviceAddress;
    
    // 气密仪设备地址
    int m_airtightDeviceAddress;
    
    // 调压装置设备地址
    int m_pressureDeviceAddress;
    
    // 调压装置Modbus客户端指针
    QModbusClient *m_pressureModbusClient;
    
    // ========== 新增：PLC通信互斥锁，防止多请求并发堵塞总线 ==========
    bool m_plcBusy = false;
    // 上次轮询的缓存值（用于避免不必要的UI刷新 + 支持异步读取）
    quint16 m_cachedD400 = 0;
    quint16 m_cachedD402 = 0;
    quint16 m_cachedD404 = 0;
    quint16 m_cachedReg105 = 0;
    quint16 m_cachedReg106 = 0;
    quint16 m_cachedCoil1013 = 0;
    quint16 m_cachedCoil1027 = 0;
    quint16 m_cachedCoil1047 = 0;
    quint16 m_cachedEn1 = 0;
    quint16 m_cachedEn2 = 0;
    quint16 m_cachedEn3 = 0;
    // ========== 新增结束 ==========
    
    // 初始化UI
    void initUI();
    
    // 初始化连接
    void initConnections();
    
    // 从PLC读取数据
    void readPlcData();
    
    // 更新统计数据显示
    void updateStatistics();
    
    // 异步读取单个寄存器值（回调方式，不再阻塞主线程）
    void readRegisterAsync(quint16 address, QModbusDataUnit::RegisterType type,
                           const std::function<void(bool success, quint16 value)>& callback);
    
    // 同步阻塞版本读取单个寄存器（保留用于需要阻塞调用的场景）
    bool readRegisterBlocking(quint16 address, quint16& value, QModbusDataUnit::RegisterType type);
    
    // 新增：异步批量读取连续线圈（功能码01，优化多个连续Coil读取）
    void readCoilsAsync(quint16 startAddress, quint16 count,
                        const std::function<void(bool success, const QVector<quint16>& values)>& callback);
    
    // 向气密仪写入数据（异步版本）
    void writeAirtightRegister(quint16 address, quint16 value, const std::function<void(bool)>& callback);
    
    // 读取仪器评选数据（异步版本）
    void readInstrumentSelection(const std::function<void(int)>& callback);
    
    // 将参数发送到设备（异步版本）
    void sendParamsToDevice(const AirTightnessFullParams& params, const std::function<void(bool)>& callback);
    
    // 将压力值发送到调压装置（异步版本）
    void sendPressureToRegulatorAsync(const AirTightnessFullParams& params, const std::function<void(bool)>& callback);
    
    // 带重试的异步参数发送
    void sendParamsToDeviceAsyncWithRetry(const AirTightnessFullParams& params, int attempt, const std::function<void(bool)>& callback);
    
    // 同步阻塞版本（保留用于向后兼容）
    bool sendPressureToRegulator(const AirTightnessFullParams& params);
    
    // 启动气密仪
    void startAirtightInstrument();
    // 启动气密仪重试定时器与状态
    QTimer *m_airtightStartRetryTimer;
    bool m_airtightStartRetryActive;
    // 防止寄存器105重复触发
    quint16 m_lastRegister105Value = 0;
    // 防止寄存器106重复触发
    quint16 m_lastRegister106Value = 0;
    // 105触发计数器：记录本轮已触发几次，用于顺序选择封堵
    int m_testTriggerCount = 0;
    // 防止寄存器1013重复触发
    quint16 m_lastRegister1013Value = 0;
    // 防止寄存器1026重复触发
    quint16 m_lastRegister1026Value = 0;
    // 防止寄存器1046重复触发
    quint16 m_lastRegister1046Value = 0;
};

#endif // PLCMONITOR_H
