#ifndef TCPSERVERMANAGER_H
#define TCPSERVERMANAGER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <QList>
#include <QMap>
#include "TestResultStruct.h"

// 设备运行状态枚举
enum DeviceStatus {
    Status_Idle,       // 空闲
    Status_Running,    // 运行中
    Status_Error,      // 故障
    Status_Paused      // 暂停
};

// 客户端信息结构体
struct ClientInfo {
    QTcpSocket* socket;
    QString address;
    quint16 port;
    QDateTime connectTime;
    QByteArray recvBuffer;  // 接收缓冲区（处理粘包）
};

class TcpServerManager : public QObject
{
    Q_OBJECT
public:
    explicit TcpServerManager(QObject *parent = nullptr);
    ~TcpServerManager();

    // 服务器控制
    bool startServer(quint16 port = 8888);
    void stopServer();
    bool isRunning() const;
    
    // 获取连接的客户端数量
    int clientCount() const;

    // 数据推送接口
    void sendDeviceStatus(DeviceStatus status, float pressure, const QString& pressureUnit, 
                          float leakValue, const QString& leakUnit, 
                          const QString& testProcess, const QString& errorMsg = "");
    void sendDeviceParams(const QString& programNumber, const QString& fillTime, 
                          const QString& stabilizationTime, const QString& testTime, 
                          const QString& dumpTime, const QString& leakThreshold, 
                          const QString& fillPressure);
    void sendTestResult(const TestResult& result);

signals:
    // 接收到远程指令信号
    void sigRecvRemoteCmd(const QString& cmdCode, const QJsonObject& params);
    // 客户端连接/断开信号
    void sigClientConnected(const QString& address, quint16 port);
    void sigClientDisconnected(const QString& address, quint16 port);

private slots:
    void onNewConnection();
    void onClientDisconnected();
    void onReadyRead();
    void onTimerPushStatus();

private:
    // 广播数据给所有客户端
    void broadcastData(const QString& data);
    // 发送数据给指定客户端
    void sendToClient(QTcpSocket* client, const QString& data);
    // 回复客户端
    void replyToClient(QTcpSocket* client, int code, const QString& msg, const QString& cmdCode = "");
    
    // JSON序列化
    QString packStatusData(DeviceStatus status, float pressure, const QString& pressureUnit, 
                          float leakValue, const QString& leakUnit,
                          const QString& testProcess, const QString& errorMsg);
    QString packParamsData(const QString& programNumber, const QString& fillTime, 
                          const QString& stabilizationTime, const QString& testTime, 
                          const QString& dumpTime, const QString& leakThreshold, 
                          const QString& fillPressure);
    QString packTestResultData(const TestResult& result);
    
    // 指令解析
    void parseClientCmd(const QString& data, QTcpSocket* client);
    void processRecvBuffer(QTcpSocket* client);

private:
    QTcpServer* m_tcpServer;
    QMap<QTcpSocket*, ClientInfo> m_clients;  // 使用Map管理客户端
    QTimer* m_statusPushTimer;
    
    // 缓存当前设备状态
    DeviceStatus m_currentStatus;
    float m_currentPressure;
    QString m_currentPressureUnit;
    float m_currentLeakValue;
    QString m_currentLeakUnit;
    QString m_currentTestProcess;
    QString m_currentErrorMsg;
};

#endif // TCPSERVERMANAGER_H
