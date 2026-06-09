#ifndef DEBUGPAGE_H
#define DEBUGPAGE_H

#include <QWidget>
#include <QTimer>
#include <QDateTime>
#include <QString>
#include <QModbusClient>
#include <QPushButton>
#include <QDebug>
#include <QMap>
#include "AirTightnessParams.h"

QT_BEGIN_NAMESPACE
namespace Ui { class DebugPage; }
QT_END_NAMESPACE

class DebugPage : public QWidget
{
    Q_OBJECT

public:
    DebugPage(QWidget *parent = nullptr);
    ~DebugPage();
    
    // 设置PLC Modbus客户端
    void setPlcModbusClient(QModbusClient *client);
    
    // 设置气密仪 Modbus客户端
    void setAirtightModbusClient(QModbusClient *client);
    
    // 设置PLC设备地址
    void setPlcDeviceAddress(int address);
    
    // 设置气密仪设备地址
    void setAirtightDeviceAddress(int address);

    // 设置调压装置 Modbus客户端
    void setPressureModbusClient(QModbusClient *client);

    // 设置调压装置设备地址
    void setPressureDeviceAddress(int address);
    
    // 将参数发送到气密仪（寄存器8192-8229），逻辑参考PlcMonitor::sendParamsToDevice
    bool sendParamsToDevice(const AirTightnessFullParams& params);

    // 将压力值发送到调压装置（寄存器80），逻辑参考PlcMonitor::sendPressureToRegulator
    bool sendPressureToRegulator(const AirTightnessFullParams& params);
    
    // 处理连接状态变化的槽函数
public slots:
    void onConnectionStatusChanged(bool isPlc, bool connected);
    // 槽函数：接收RealtimeMonitor发送的寄存器值
    void onRegisterValuesChanged(quint16 reg100, quint16 reg101, quint16 reg102, quint16 reg103);
    // 新增槽：接收RealtimeMonitor的警告按钮状态，更新报警按钮样式
    void onWarningButtonStateChanged(int state);

private slots:
    // 托盘相关按钮槽函数
    void on_trayInBtn_clicked();
    void on_trayOutBtn_clicked();

    // 主缸相关按钮槽函数
    void on_mainCylinderInBtn_clicked();
    void on_mainCylinderOutBtn_clicked();

    // 封堵相关按钮槽函数
    void on_block1Btn_clicked();
    void on_block2Btn_clicked();
    void on_block3Btn_clicked();
    
    // 手动按钮槽函数
    void on_manualBtn_clicked();

    // 仪器相关按钮槽函数
    void on_instrumentStartBtn_clicked();
    void on_instrumentResetBtn_clicked();

    // 气阀相关按钮槽函数
    void on_inflateValve1Btn_clicked();
    void on_exhaustValve1Btn_clicked();
    void on_inflateValve2Btn_clicked();
    void on_exhaustValve2Btn_clicked();
    void on_inflateValve3Btn_clicked();
    void on_exhaustValve3Btn_clicked();

    // 功能按钮槽函数
    void on_okBtn_clicked();
    void on_tdBtn_clicked();
    void on_rdBtn_clicked();
    void on_alBtn_clicked();
    void on_runBtn_clicked();

    // 右列“关”按钮独立槽函数（显式关闭动作）
    void on_trayInOffBtn_clicked();
    void on_trayOutOffBtn_clicked();
    void on_mainCylinderInOffBtn_clicked();
    void on_mainCylinderOutOffBtn_clicked();
    void on_block1OffBtn_clicked();
    void on_block2OffBtn_clicked();
    void on_block3OffBtn_clicked();
    void on_inflateValve1OffBtn_clicked();
    void on_exhaustValve1OffBtn_clicked();
    void on_inflateValve2OffBtn_clicked();
    void on_exhaustValve2OffBtn_clicked();
    void on_inflateValve3OffBtn_clicked();
    void on_exhaustValve3OffBtn_clicked();
    void on_manualOffBtn_clicked();
    void on_sendRegulatorPressureBtn_clicked();
    void on_totalSealOnBtn_clicked();
    void on_totalSealOffBtn_clicked();
    // 时间更新槽函数
    void updateTime();
    // 同步PLC寄存器值到按钮状态
    void syncPlcButtonStates();

    // 设置单个按钮样式
    void setButtonStyle(QPushButton *button, bool active);
    // 更新按钮样式和闪烁效果
    void updateButtonStyles();

private:
    Ui::DebugPage *ui;
    QTimer *timeTimer;
    QModbusClient *m_plcModbusClient;  // PLC Modbus客户端指针
    QModbusClient *m_airtightModbusClient;  // 气密仪 Modbus客户端指针
    QModbusClient *m_pressureModbusClient;  // 调压装置 Modbus客户端指针
    bool m_isPlcConnected;  // PLC连接状态
    bool m_isAirtightConnected;  // 气密仪连接状态
    bool m_isPressureConnected;  // 调压装置连接状态
    int m_plcDeviceAddress;  // PLC设备地址
    int m_airtightDeviceAddress;  // 气密仪设备地址
    int m_pressureDeviceAddress;  // 调压装置设备地址
    
    // 防按钮事件重复触发标志
    bool m_isProcessingButtonClick;
    // 新增：报警状态标记（true=有警告，闪烁红色；false=无警告，绿色）
    bool m_isWarningActive;

    // 初始化界面和定时器
    void initUI();
    
    // 初始化所有开关按钮状态
    void initializeSwitchButtons();

    // 显示操作反馈
    void showFeedback(const QString &operation);

    // 记录操作历史
    void recordOperation(const QString &operation);
    // 开关事件记录（含时间与持续时长）
    void recordSwitchEvent(const QString &name, bool isOn);
    
    // 向PLC寄存器写入数据
    bool writePlcRegister(quint16 address, quint16 value, QModbusDataUnit::RegisterType type = QModbusDataUnit::HoldingRegisters);
    
    // 向气密仪寄存器写入数据
    bool writeAirtightRegister(quint16 address, quint16 value, QModbusDataUnit::RegisterType type = QModbusDataUnit::HoldingRegisters);
    
    
    // 寄存器值存储
    quint16 m_reg100; // 仪器实时OK
    quint16 m_reg101; // 仪器实时RUN
    quint16 m_reg102; // 仪器实时TD
    quint16 m_reg103; // 仪器实时RD
    
    // 用于闪烁效果的定时器
    QTimer *m_flashTimer;
    // 定时同步计时器
    QTimer *m_plcSyncTimer;
    
    // 用于闪烁效果的计数器
    static int m_flashCounter;

    // 各开关的最近开启时间，用于统计持续时长
    QMap<QString, QDateTime> m_switchOnStartTimes;
};
#endif // DEBUGPAGE_H
