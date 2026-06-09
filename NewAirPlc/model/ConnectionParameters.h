#ifndef CONNECTIONPARAMETERS_H
#define CONNECTIONPARAMETERS_H

#include <QString>
#include <QSerialPort>

/**
 * @brief PLC连接参数结构体
 */
struct PLCConnectionParams {
    QString protocol;              // 协议类型："Modbus RTU" 或 "Modbus TCP"
    QString serialPort;            // 串口号（RTU模式）
    int baudRate = 9600;           // 波特率
    int dataBits = 8;              // 数据位
    QSerialPort::Parity parity = QSerialPort::NoParity;  // 校验位
    QSerialPort::StopBits stopBits = QSerialPort::OneStop;  // 停止位
    QString ipAddress;             // IP地址（TCP模式）
    int port = 502;                // 端口号（TCP模式）
    int slaveId = 2;               // 从站地址（默认为2）
    bool autoConnect = false;      // 是否自动连接
    QString lastConnectTime;       // 最后连接时间
    QString lastConnectStatus;     // 最后连接状态
};

/**
 * @brief 气密仪连接参数结构体
 */
struct AirTightnessParams {
    QString connectionType;        // 连接类型："串口" 或 "以太网"
    QString serialPort;            // 串口号（串口模式）
    int baudRate = 9600;           // 波特率
    int dataBits = 8;              // 数据位
    QSerialPort::Parity parity = QSerialPort::NoParity;  // 校验位
    QSerialPort::StopBits stopBits = QSerialPort::OneStop;  // 停止位
    QString ipAddress;             // IP地址（以太网模式）
    int port = 502;                // 端口号（以太网模式）
    bool autoConnect = false;      // 是否自动连接
    QString lastConnectTime;       // 最后连接时间
    QString lastConnectStatus;     // 最后连接状态
};

/**
 * @brief 调压装置连接参数结构体
 */
struct PressureDeviceParams {
    QString protocol;              // 协议类型："Modbus TCP" 或 "Modbus RTU"
    QString serialPort;            // 串口号（RTU模式）
    int baudRate = 9600;           // 波特率
    int dataBits = 8;              // 数据位
    QSerialPort::Parity parity = QSerialPort::NoParity;  // 校验位
    QSerialPort::StopBits stopBits = QSerialPort::OneStop;  // 停止位
    QString ipAddress;             // IP地址（TCP模式）
    int port = 502;                // 端口号（TCP模式）
    int slaveId = 1;               // 从站地址（默认为1）
    bool autoConnect = false;      // 是否自动连接
    QString lastConnectTime;       // 最后连接时间
    QString lastConnectStatus;     // 最后连接状态
};

/**
 * @brief 连接历史记录结构体
 */
struct ConnectionHistory {
    int id;                        // 记录ID
    QString deviceType;            // 设备类型："PLC", "AirTightness", "Pressure"
    QString connectTime;           // 连接时间
    QString disconnectTime;        // 断开时间
    QString status;                // 状态："success", "failed", "disconnected"
    QString errorMessage;          // 错误信息
};

#endif // CONNECTIONPARAMETERS_H
