#include "TcpServerManager.h"
#include "pressureunit.h"
#include "LeakUnitType.h"
#include <QDebug>
#include <QDateTime>

TcpServerManager::TcpServerManager(QObject *parent)
    : QObject(parent)
    , m_tcpServer(new QTcpServer(this))
    , m_statusPushTimer(new QTimer(this))
    , m_currentStatus(Status_Idle)
    , m_currentPressure(0.0f)
    , m_currentLeakValue(0.0f)
    , m_currentPressureUnit("Pa")
    , m_currentLeakUnit("Pa/s")
    , m_currentTestProcess("")
    , m_currentErrorMsg("")
{
    connect(m_tcpServer, &QTcpServer::newConnection, this, &TcpServerManager::onNewConnection);
    connect(m_statusPushTimer, &QTimer::timeout, this, &TcpServerManager::onTimerPushStatus);
    m_statusPushTimer->start(1000);
}

TcpServerManager::~TcpServerManager()
{
    stopServer();
}

bool TcpServerManager::startServer(quint16 port)
{
    if (m_tcpServer->listen(QHostAddress::Any, port)) {
        qInfo() << "TCP服务器启动成功，端口：" << port;
        return true;
    }
    qCritical() << "TCP服务器启动失败：" << m_tcpServer->errorString();
    return false;
}

void TcpServerManager::stopServer()
{
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        it.key()->disconnectFromHost();
        it.key()->deleteLater();
    }
    m_clients.clear();
    
    if (m_tcpServer->isListening()) {
        m_tcpServer->close();
    }
    qInfo() << "TCP服务器已停止";
}

bool TcpServerManager::isRunning() const
{
    return m_tcpServer && m_tcpServer->isListening();
}

int TcpServerManager::clientCount() const
{
    return m_clients.size();
}

// ==================== 数据广播 ====================

void TcpServerManager::broadcastData(const QString& data)
{
    QByteArray bytes = data.toUtf8() + "\n";
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        QTcpSocket* client = it.key();
        if (client->state() == QTcpSocket::ConnectedState) {
            client->write(bytes);
            client->flush();
        }
    }
}

void TcpServerManager::sendToClient(QTcpSocket* client, const QString& data)
{
    if (client && client->state() == QTcpSocket::ConnectedState) {
        client->write(data.toUtf8() + "\n");
        client->flush();
    }
}

void TcpServerManager::replyToClient(QTcpSocket* client, int code, const QString& msg, const QString& cmdCode)
{
    QJsonObject json;
    json["type"] = "response";
    json["code"] = code;
    json["msg"] = msg;
    if (!cmdCode.isEmpty()) {
        json["cmd_code"] = cmdCode;
    }
    sendToClient(client, QJsonDocument(json).toJson(QJsonDocument::Compact));
}

// ==================== 状态推送 ====================

void TcpServerManager::sendDeviceStatus(DeviceStatus status, float pressure, const QString& pressureUnit, 
                                        float leakValue, const QString& leakUnit, 
                                        const QString& testProcess, const QString& errorMsg)
{
    m_currentStatus = status;
    m_currentPressure = pressure;
    m_currentPressureUnit = pressureUnit;
    m_currentLeakValue = leakValue;
    m_currentLeakUnit = leakUnit;
    m_currentTestProcess = testProcess;
    m_currentErrorMsg = errorMsg;

    QString pressureUnitStr = PressureUnit::toString(PressureUnit::fromCode(pressureUnit.toInt()));
    QString leakUnitStr = LeakUnitType::getUnitString(LeakUnitType::getUnitFromCode(leakUnit.toInt(), LeakUnitType::CodeType1));
  
    broadcastData(packStatusData(status, pressure, pressureUnitStr, leakValue, leakUnitStr, testProcess, errorMsg));
}

void TcpServerManager::sendDeviceParams(const QString& programNumber, const QString& fillTime, 
                                        const QString& stabilizationTime, const QString& testTime, 
                                        const QString& dumpTime, const QString& leakThreshold, 
                                        const QString& fillPressure)
{
    broadcastData(packParamsData(programNumber, fillTime, stabilizationTime, testTime, 
                                 dumpTime, leakThreshold, fillPressure));
}

void TcpServerManager::sendTestResult(const TestResult& result)
{
    broadcastData(packTestResultData(result));
}

// ==================== JSON序列化 ====================

QString TcpServerManager::packStatusData(DeviceStatus status, float pressure, const QString& pressureUnit, 
                                        float leakValue, const QString& leakUnit, 
                                        const QString& testProcess, const QString& errorMsg)
{
    QJsonObject json;
    json["type"] = "status";
    
    static const char* statusStrings[] = {"idle", "running", "error", "paused"};
    json["device_status"] = statusStrings[status];
    
    json["pressure"] = pressure;
    json["pressure_unit"] = pressureUnit;
    json["leak_value"] = leakValue;
    json["leak_unit"] = leakUnit;
    json["test_process"] = testProcess;
    json["error_msg"] = errorMsg;
    json["timestamp"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

    return QJsonDocument(json).toJson(QJsonDocument::Compact);
}

QString TcpServerManager::packParamsData(const QString& programNumber, const QString& fillTime, 
                                        const QString& stabilizationTime, const QString& testTime, 
                                        const QString& dumpTime, const QString& leakThreshold, 
                                        const QString& fillPressure)
{
    QJsonObject json;
    json["type"] = "params";
    json["program_number"] = programNumber;
    json["fill_time"] = fillTime;
    json["stabilization_time"] = stabilizationTime;
    json["test_time"] = testTime;
    json["dump_time"] = dumpTime;
    json["leak_threshold"] = leakThreshold;
    json["fill_pressure"] = fillPressure;
    json["timestamp"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

    return QJsonDocument(json).toJson(QJsonDocument::Compact);
}

QString TcpServerManager::packTestResultData(const TestResult& result)
{
    QJsonObject json;
    json["type"] = "test_result";
    json["program_number"] = result.programNumber;
    json["channel_pressure"] = result.channelPressure;
    json["pressure_unit"] = result.pressureUnit;
    json["channel_leak"] = result.channelLeak;
    json["leak_unit"] = result.leakUnit;
    json["is_passed"] = result.isPassed;
    json["is_failed"] = result.isFailed;
    json["create_time"] = result.createTime.toString("yyyy-MM-dd hh:mm:ss");
    json["timestamp"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

    return QJsonDocument(json).toJson(QJsonDocument::Compact);
}

// ==================== 连接管理 ====================

void TcpServerManager::onNewConnection()
{
    QTcpSocket* client = m_tcpServer->nextPendingConnection();
    if (!client) return;

    ClientInfo info;
    info.socket = client;
    info.address = client->peerAddress().toString();
    info.port = client->peerPort();
    info.connectTime = QDateTime::currentDateTime();
    
    m_clients.insert(client, info);
    
    qInfo() << "新客户端连接：" << info.address << ":" << info.port;
    
    connect(client, &QTcpSocket::disconnected, this, &TcpServerManager::onClientDisconnected);
    connect(client, &QTcpSocket::readyRead, this, &TcpServerManager::onReadyRead);
    
    emit sigClientConnected(info.address, info.port);

    // 推送当前状态给新客户端
    sendDeviceStatus(m_currentStatus, m_currentPressure, m_currentPressureUnit, 
                    m_currentLeakValue, m_currentLeakUnit, 
                    m_currentTestProcess, m_currentErrorMsg);
}

void TcpServerManager::onClientDisconnected()
{
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client) return;
    
    if (m_clients.contains(client)) {
        ClientInfo info = m_clients.value(client);
        qInfo() << "客户端断开连接：" << info.address << ":" << info.port;
        emit sigClientDisconnected(info.address, info.port);
        m_clients.remove(client);
    }
    client->deleteLater();
}

// ==================== 数据接收与解析 ====================

void TcpServerManager::onReadyRead()
{
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client || !m_clients.contains(client)) return;

    // 追加到接收缓冲区（处理粘包）
    m_clients[client].recvBuffer.append(client->readAll());
    processRecvBuffer(client);
}

void TcpServerManager::processRecvBuffer(QTcpSocket* client)
{
    QByteArray& buffer = m_clients[client].recvBuffer;
    
    while (true) {
        int newlinePos = buffer.indexOf('\n');
        if (newlinePos == -1) break;
        
        QByteArray line = buffer.left(newlinePos).trimmed();
        buffer.remove(0, newlinePos + 1);
        
        if (!line.isEmpty()) {
            qInfo() << "收到客户端指令：" << line;
            parseClientCmd(QString(line), client);
        }
    }
}

void TcpServerManager::parseClientCmd(const QString& data, QTcpSocket* client)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        qCritical() << "指令解析失败：" << error.errorString();
        replyToClient(client, -1, "指令格式错误");
        return;
    }

    QJsonObject json = doc.object();
    if (json["type"].toString() != "cmd") {
        return;
    }

    QString cmdCode = json["cmd_code"].toString();
    QJsonObject params = json["params"].toObject();

    emit sigRecvRemoteCmd(cmdCode, params);
    replyToClient(client, 0, "指令已接收", cmdCode);
}

void TcpServerManager::onTimerPushStatus()
{
    if (m_clients.isEmpty()) return;
    
    sendDeviceStatus(m_currentStatus, m_currentPressure, m_currentPressureUnit, 
                    m_currentLeakValue, m_currentLeakUnit, 
                    m_currentTestProcess, m_currentErrorMsg);
}
