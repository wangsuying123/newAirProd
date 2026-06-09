#ifndef PARAMSETTING_H
#define PARAMSETTING_H

#include <QWidget>
#include <QModbusDevice>
#include <QThread>
#include <QPointer>
#include <functional>
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
    void sendModbusCommand(quint16 address, quint16 value, int timeoutMs, quint16 secondValue, const std::function<void(bool)>& callback);
    
    AirTightnessFullParams collectParamsFromUI();
    void loadParamsToUI(const AirTightnessFullParams& params);
    void updateParamsList();
    void sendParamsToDevice(const std::function<void(bool)>& callback);
    void sendModbusBatchCommand(const QList<QPair<quint16, quint16>>& paramList, int timeout, const std::function<void(bool)>& callback);
    QString getVolumeUnitString(VolumeUnit unit);
    void sendPressureToRegulatorAsync(const std::function<void(bool)>& callback);
};

#endif // PARAMSETTING_H
