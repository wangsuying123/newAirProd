#ifndef TCPSERVERCONTROLLER_H
#define TCPSERVERCONTROLLER_H

#include <QObject>
#include <QTimer>
#include "TcpServerManager.h"
#include "TestResultStruct.h"
#include "AirTightnessParams.h"

class PlcMonitor;
class ConnectWidget;

class TcpServerController : public QObject
{
    Q_OBJECT
public:
    explicit TcpServerController(QObject *parent = nullptr);
    ~TcpServerController();

    // 初始化TCP服务器
    void initTcpServer();

    // 设置PLC监控页面指针
    void setPlcMonitorPage(PlcMonitor *page);

    // 设置连接页面指针
    void setConnectPage(ConnectWidget *page);

    // 获取TCP服务器实例
    TcpServerManager *tcpServer() const;

public slots:
    // 定时采集气密仪/PLC状态槽函数
    void onTimerCollectData();
    
    // 接收程序号变化信号
    void onInstrumentSelectionChanged(int programNumber);
    // 接收通道1测试结果槽函数
    void onTestResultReady(const TestResult& result);
    // 接收通道2测试结果槽函数
    void onTestResultReady2(const TestResult& result);
    // 接收通道3测试结果槽函数
    void onTestResultReady3(const TestResult& result);
    // 接收气密仪实时数据信号槽函数
    void onRealtimeDataUpdated(double pressure, double leak, const QString& pressureUnit, const QString& leakUnit, const QString& testProcess);

private:
    TcpServerManager *m_tcpServer;
    PlcMonitor *m_plcMonitorPage;
    ConnectWidget *m_connectPage;
    QTimer *m_dataCollectTimer;
    int m_currentProgramNumber;     // 当前程序号
    AirTightnessFullParams m_currentAirtightParams; // 当前气密仪参数
    
    // 实时数据存储
    double m_currentPressure;       // 当前压力值
    double m_currentLeak;           // 当前泄漏值
    QString m_pressureUnit;         // 压力单位
    QString m_leakUnit;             // 泄漏单位
    QString m_testProcess;          // 当前测试过程
};

#endif // TCPSERVERCONTROLLER_H