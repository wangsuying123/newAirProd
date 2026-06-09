#ifndef REALTIMEMONITOR_H
#define REALTIMEMONITOR_H

#include <QWidget>
#include <QModbusClient>
#include <QTimer>
#include <QElapsedTimer>
#include "TestResultStruct.h"
#include "User.h"

class QShowEvent; // forward declaration

namespace Ui {
class RealtimeMonitor;
}

class RealtimeMonitor : public QWidget
{
    Q_OBJECT

public:
    explicit RealtimeMonitor(QWidget *parent = nullptr);
    ~RealtimeMonitor();

    void setModbusClient(QModbusClient *client); // 设置气密仪连接
    void setPlcModbusClient(QModbusClient *client); // 设置PLC连接
    void updateConnectionStatus(bool isPlc, bool connected);
    void updatePressureConnectionStatus(bool connected); // 更新调压装置连接状态
    void setAirtightSlaveId(quint8 id); // 设置气密仪从站地址
    void setCurrentUser(const User &user); // 设置当前登录用户

    // 信号：发送PLC寄存器100-103的值
    // 信号：测试完成信号，当测试进程等于1024时发送
signals:
    void registerValuesChanged(quint16 reg100, quint16 reg101, quint16 reg102, quint16 reg103);
    // 新增：通道结果更新信号，用于通知PlcMonitor刷新通道结果
    void updateChannelResult();
    // 新增：警告按钮状态变化，通知DebugPage更新报警按钮样式（1=警告，0=清除）
    void warningButtonStateChanged(int state);
    // 新增：产品编号变化信号，供AirtightTestResult接收
    void productNumberChanged(const QString &code);
    // 新增：PLC线圈200值变化（用于外部订阅）
    void plcCoil200Changed(quint16 value);
    // 新增：气密仪实时数据信号，供TcpServerController接收
    void realtimeDataUpdated(double pressure, double leak, const QString& pressureUnit, const QString& leakUnit, const QString& testProcess);
    // 新增：测试结果寄存器数据信号（8961-8972），供AirtightTestResult接收
    void testResultRegisterDataReady(const QModbusDataUnit &data);


public slots:
    // 接收程序号信号
    void handleProgramNumberReceived(int programNumber);
    // 接收AirtightTestResult页面的测试结果信号
    void handleTestResultReady(const TestResult &result);
    void handleTestResultReady2(const TestResult &result);
    void handleTestResultReady3(const TestResult &result);
    
private slots:
    void on_refreshRateSpinBox_valueChanged(int value);
    void readRealTimeData();
    // void on_startButton_clicked();  // 按钮已从UI移除
    // void on_resetButton_clicked();  // 按钮已从UI移除
    // 新增：产品编号回车确认槽
    void onProductNumberEntered();
    // 新增：清空产品编号按钮槽
    void on_clearProductNumberButton_clicked();

protected:
    void showEvent(QShowEvent *event) override;

private:
    Ui::RealtimeMonitor *ui;
    QModbusClient *modbusClient; // 气密仪连接
    QModbusClient *plcModbusClient; // PLC连接
    quint8 slaveId;
    bool isMonitoring;
    bool isConnected;
    QTimer *dataRefreshTimer;
    // 合并：统一PLC轮询定时器
    QTimer *plcPollTimer;
    // 轮询节流与分频状态
    bool plcRequestBusy = false;
    bool airtightRequestBusy = false; // 气密仪请求忙标志，防止请求堆积
    int plcPollCounter = 0;
    quint16 lastCoil200Value = 0;
    quint16 m_lastRegister519Value = 1; // 初始化为1，避免程序启动时519持续为1触发误复位
    quint16 m_lastCoil49418Value = 1;   // 初始化为1，避免启动时误触发
    quint16 register8707Value; // 保存寄存器8707的值，用于全局访问

    // 新增：产品编码缓存
    QString productCode;
    
    // 新增：OK 和 NG 计数
    quint16 m_okCount = 0;
    quint16 m_ngCount = 0;

    // 新增：updateSummaryResult 防抖/去重标志，避免短时间内重复执行
    bool m_summaryUpdateInProgress = false;

    void initUI();
    // 枚举定义测试进程状态
    enum ProcessStatus {
        STANDBY = 0,
        FILL = 256,
        STB = 512,
        TEST = 768,
        DUMP = 1024
    };
    void updateTestStatus(int processValue);
    void updateTestResult(bool isPass);
    bool readDeviceData(quint16 address, quint16 &value, QModbusDataUnit::RegisterType type = QModbusDataUnit::HoldingRegisters);
    void readDeviceDataAsync(quint16 address, quint16 count, QModbusDataUnit::RegisterType type, const std::function<void(bool, const QModbusDataUnit&)>& callback);
    void processMainRegisterData(const QModbusDataUnit &data);
    void processTestResultData(const QModbusDataUnit &data);
    bool writeDeviceData(quint16 address, quint16 value, QModbusDataUnit::RegisterType type = QModbusDataUnit::HoldingRegisters);
    void logMessage(const QString message, bool isError = false);
    void saveDataToDatabase(double pressure, double leak, double temperature, double atmPressure,
                            bool isFilling, bool isExhaust, bool isLowPressure, bool isHighPressure,
                            bool isFail, bool isPass);  // 添加保存数据到数据库的方法
    
    // PLC相关方法
    bool writeZeroToPLCRegisters(); // 向PLC寄存器510-516写入0
    void checkPLCRegister519(); // 检查PLC寄存器519的值
    void startPLCRegister519Check(); // 合并后：启动统一PLC轮询定时器
    void stopPLCRegister519Check(); // 合并后：停止统一PLC轮询定时器
    // 新增：PLC线圈200读取相关方法
    void checkPLCRegister200();
    // 新增：PLC线圈2000读取相关方法（值为1时清空结果显示）
    void checkPLCRegister2000();
    // 合并后：保留兼容函数名，统一启动/停止
    void startPLCRegister200Check();
    void stopPLCRegister200Check();
    // 合并后：统一轮询tick
    void pollPlcOnce();
    void checkCoil49418();
    // 新增：汇总结果更新方法
    void updateSummaryResult();
    // 新增：高频读取寄存器9088
    void readRegister9088();
    // 新增：读取 OK 和 NG 计数
    void readOkNgCount();
    void updateOkNgDisplay();
    // 新增：通道结果状态缓存
    bool ch1HasResult = false;
    bool ch2HasResult = false;
    bool ch3HasResult = false;
    bool ch1Pass = false;
    bool ch2Pass = false;
    bool ch3Pass = false;
    // 新增：通道使能状态缓存
    bool ch1Enabled = false;
    bool ch2Enabled = false;
    bool ch3Enabled = false;
    // 新增：9088高频轮询相关
    QTimer *reg9088Timer = nullptr;
    bool reg9088RequestBusy = false;
    // 新增：压力值范围校验相关，防止通信异常时显示异常大值
    double lastValidPressure = 0.0;          // 保存上次有效的压力值
    const double MAX_PRESSURE_KPA = 10000.0;  // 最大合理压力值（KPa）
    // 新增：标记当前测试周期是否已处理过结果（防止重复处理旧数据）
    bool currentTestResultProcessed = false;
    // 新增：标记测试是否真正开始（经历过非排气状态）
    bool testActuallyStarted = false;
    
    // 新增：进度条相关
    QTimer *progressTimer = nullptr;        // 进度条更新定时器
    QElapsedTimer phaseElapsedTimer;        // 阶段计时器
    int currentPhase = 0;                   // 当前阶段（0=待机, 256=充气, 512=保压, 768=测试, 1024=排气）
    int lastPhase = -1;                     // 上一个阶段，用于检测阶段变化
    int lastProgressValue = 0;              // 上一次的进度值，确保进度只增不减
    // 阶段时间参数（秒）
    double fillTime = 0;                    // 充气时间
    double stabilizeTime = 0;               // 保压时间
    double testTime = 0;                    // 测试时间
    double dumpTime = 0;                    // 排气时间
    void updateProgressBar();               // 更新进度条方法
};

#endif // REALTIMEMONITOR_H
