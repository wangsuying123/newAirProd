#ifndef CONNECT_H
#define CONNECT_H

#include <QWidget>
#include <QTimer>
#include <QDateTime>
#include <QLabel>
#include <QModbusClient>
#include <QModbusRtuSerialClient>
#include <QModbusTcpClient>
#include <QModbusDevice>

namespace Ui {
class ConnectWidget;
}

class ConnectWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ConnectWidget(QWidget *parent = nullptr);
    ~ConnectWidget();

    // 获取Modbus客户端对象
    QModbusClient* getModbusClient(bool isPlc = false);
    
    // 获取PLC从站地址
    int getPlcSlaveId() const;
    
    // 获取气密仪从站地址
    int getAirtightSlaveId() const;
    
    // 获取调压装置Modbus客户端
    QModbusClient* getPressureModbusClient() const;
    
    // 获取调压装置从站地址
    int getPressureSlaveId() const;

signals:
    // 发送连接状态变化信号
    void connectionStatusChanged(bool isPlc, bool connected);
    // 发送Modbus客户端对象更新信号
    void modbusClientUpdated(bool isPlc, QModbusClient* client);
    // 当调压装置Modbus客户端对象变更（如协议切换重建）时发射
    void pressureClientUpdated(QModbusClient* client);
    // 当调压装置连接状态变化时发射
    void pressureConnectionChanged(bool connected);

private slots:
    // PLC相关槽函数
    void onPlcProtocolChanged(int index);
    void on_plcConnectBtn_clicked();  // PLC连接按钮槽函数
    void on_plcDisconnectBtn_clicked(); // PLC断开按钮槽函数
    void onPlcStateChanged(QModbusDevice::State state);

    // 气密仪相关槽函数
    void onAirConnectionTypeChanged(int index);
    void on_airConnectBtn_clicked();  // 气密仪连接按钮槽函数
    void on_airDisconnectBtn_clicked(); // 气密仪断开按钮槽函数
    void onAirStateChanged(QModbusDevice::State state);
    
    // 调压装置相关槽函数
    void on_protocolComboBox_currentIndexChanged(int index);
    void on_connectButton_clicked();
    void on_disconnectButton_clicked();
    void onPressureStateChanged(QModbusDevice::State state);
    void onPressureErrorOccurred(QModbusDevice::Error error);

    // 串口刷新槽函数
    void on_refreshSerialPortBtn_clicked();

    // 自动连接相关槽函数
    void performAutoConnect();

    // 恢复按钮状态槽函数
    void restoreButtonsState();

private:
    Ui::ConnectWidget *ui;

    // 成员变量按初始化顺序声明
    QModbusClient *airModbusClient; // 气密仪的Modbus客户端
    QModbusClient *plcModbusClient; // PLC的Modbus客户端
    QModbusClient *pressureModbusClient; // 调压装置的Modbus客户端

    bool plcConnected;
    bool airConnected;
    bool pressureConnected;
    bool pressureWasConnected;
    bool pressureConnectPromptShown;

    QTimer *plcTestTimer;
    QTimer *airTestTimer;
    QTimer *autoConnectTimer;

    int plcSlaveId; // PLC从站地址，默认为2
    int airtightSlaveId; // 气密仪从站地址，默认为1
    int pressureSlaveId; // 调压装置从站地址，默认为1
    
    // 自动连接相关变量
    bool autoConnectEnabled;
    int autoConnectRetryCount;
    
    // 自动重连相关变量
    QTimer *plcReconnectTimer;
    int plcReconnectAttempts;
    bool plcReconnectEnabled;
    const int maxReconnectAttempts = 5; // 最大重连次数
    const int reconnectInterval = 3000; // 重连间隔（毫秒）
    
    // 连接状态处理标志（防止并发问题）
    bool isProcessingPlcDisconnect; // 是否正在处理PLC断开
    
    // 气密仪自动重连相关变量
    QTimer *airReconnectTimer;
    int airReconnectAttempts;
    bool airReconnectEnabled;
    
    // 调压装置自动重连相关变量
    QTimer *pressureReconnectTimer;
    int pressureReconnectAttempts;
    bool pressureReconnectEnabled;

    // 自动检测并更新串口列表
    void updateSerialPorts();

    // 辅助函数
    void addLog(const QString &message);
    void showMessage(QLabel *label, const QString &text, bool isSuccess = true);
    void updateConnectionStatus(bool isPlc, bool connected);
    QString getCurrentTimeString();

    // 保存参数
    void loadPLCParameters();
    void loadAirTightnessParameters();
    void loadPressureParameters();
    void savePLCParameters();
    void saveAirTightnessParameters();
    void savePressureParameters();
    
    // 调压装置相关辅助函数
    void updatePressureProtocolUI();
    void updatePressureDeviceStatus(bool connected);
    void connectToPressureDevice();
    void disconnectFromPressureDevice();
    void setupPressureModbusClientParameters();
    void onPressureConnected();
    void onPressureDisconnected();
    
    // 自动连接相关辅助函数
    bool shouldAutoConnect(const QString &deviceType);
    void autoConnectPLC();
    void autoConnectAirTightness();
    void autoConnectPressure();
    
    // PLC自动重连与心跳检测相关函数
    void startPlcReconnect();
    void stopPlcReconnect();
    void attemptPlcReconnect();
    
    // 气密仪自动重连相关函数
    void startAirReconnect();
    void stopAirReconnect();
    void attemptAirReconnect();
    
    // 调压装置自动重连相关函数
    void startPressureReconnect();
    void stopPressureReconnect();
    void attemptPressureReconnect();

};

#endif // CONNECT_H
