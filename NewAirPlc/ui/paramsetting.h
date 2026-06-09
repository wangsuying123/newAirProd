#ifndef PARAMSETTING_H
#define PARAMSETTING_H

#include <QWidget>
#include <QModbusDevice>  // 包含QModbusDevice头文件
#include <QThread>        // 用于线程检查
#include <QPointer>
#include "AirTightnessParams.h"
#include "VolumeUnit.h"

class QModbusClient;
class QScrollBar;

namespace Ui {
class ParamSetting;
}

class ParamSetting : public QWidget
{
    Q_OBJECT

public:
    explicit ParamSetting(QWidget *parent = nullptr);
    ~ParamSetting();

    void setModbusClient(QModbusClient *client);
    void setPressureModbusClient(QModbusClient *client);
    void updateConnectionStatus(bool isPlc, bool connected);

private slots:
    void on_sendToDeviceButton_clicked();
    void on_saveParametersButton_clicked();
    void on_loadParametersButton_clicked();
    void onModbusError(QModbusDevice::Error error);

public slots:
    // 程序号变化时的处理函数
    void onProgramNumberChanged(const QString &programNumber);
    // 新增：调压装置连接状态变化槽
    void updatePressureConnectionStatus(bool connected);

signals:
    // 参数发送成功后发送程序号信号
    void programNumberSent(int programNumber);

private:
    Ui::ParamSetting *ui;
    QModbusClient *modbusClient;
    QPointer<QModbusClient> pressureModbusClient; // 专门用于调压装置的Modbus客户端
    quint8 slaveId;
    int currentParamId = -1;
    QTimer *connectionDelayTimer;

    void initUi();
    bool sendModbusCommand(quint16 address, quint16 value, int timeoutMs = 1000, quint16 secondValue = 0); // 添加第二个寄存器值参数，默认0
    
    AirTightnessFullParams collectParamsFromUI();
    void loadParamsToUI(const AirTightnessFullParams& params);
    void updateParamsList();
    bool sendParamsToDevice();
    bool sendModbusBatchCommand(const QList<QPair<quint16, quint16>>& paramList, int timeout);
    QString getVolumeUnitString(VolumeUnit unit);
};

#endif // PARAMSETTING_H
