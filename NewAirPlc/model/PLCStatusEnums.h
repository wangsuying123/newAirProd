#ifndef PLCSTATUSENUMS_H
#define PLCSTATUSENUMS_H

#include <QObject>
#include <QList>
#include <QString>

/**
 * @brief PLC状态枚举类，包含设备状态、运行状态和检验结果的枚举定义
 */
class PLCStatusEnums : public QObject
{
    Q_OBJECT
public:
    // 设备状态枚举（寄存器D400的值）
    enum DeviceStatus {
        DEVICE_HEARTBEAT = 1,     // 设备心跳
        M_EMERGENCY_STOP = 2,     // M急停
        LIGHT_CURTAIN_EMERGENCY = 3, // 光栅急停
        NO_WORKPIECE = 4,         // 无工件
        INSTRUMENT_START_TIMEOUT = 5, // 仪器启动超时
        MAIN_CYLINDER_NOT_IN_PLACE = 6, // 主缸不在原位
        MAIN_CYLINDER_ADVANCE_TIMEOUT = 9, // 主缸进超时
        MAIN_CYLINDER_RETREAT_TIMEOUT = 10, // 主缸退超时
        BOOSTER_ADVANCE_TIMEOUT = 11, // 增压进超时
        RETURN_CYLINDER_TIMEOUT = 12, // 返程气缸超时
        BOOSTER_CYLINDER_NOT_IN_PLACE = 13 // 增压气缸不在原位
    };
    Q_ENUM(DeviceStatus)

    // 设备运行值枚举（寄存器D402的值）
    enum DeviceOperation {
        NOT_RUNNING = 0,          // 未运行
        MAIN_RUNNING = 1,         // 主运行
        CLEAR = 2,                // 清零
        MAIN_CYLINDER_ADVANCE = 7, // 主缸进
        MAIN_CYLINDER_ADVANCED = 8, // 主缸进到位
        SEAL_1_ADVANCE = 9,       // 封堵1进
        SEAL_2_ADVANCE = 10,      // 封堵2进
        SEAL_3_ADVANCE = 11,      // 封堵3进
        SEAL_COMPLETED = 12,      // 封堵完成
        BOOSTER_ADVANCE = 13,     // 增压进
        PRESS_DOWN_COMPLETED = 14, // 下压完成
        INSPECTION_1_RUNNING = 20, // 检测1运行
        INSPECTION_SEAL_1_ADVANCE = 21, // 检测封堵1进
        INSPECTION_1_AIR_PREPARE = 22, // 检测1气路准备
        INSPECTION_1_INSTRUMENT_PREPARE = 23, // 检测1仪器准备
        INSPECTION_1_FREQUENCY_SELECT = 24, // 检测1选频
        INSPECTION_1_INSTRUMENT_START = 25, // 检测1仪器启动
        INSPECTION_1_COMPLETED = 26, // 检测1完成
        INSPECTION_1_MP = 27,     // 检测1mp
        INSPECTION_2_RUNNING = 40, // 检测2行
        INSPECTION_SEAL_2 = 41,   // 检测封堵2
        INSPECTION_2_AIR_PREPARE = 42, // 检测2路准备
        INSPECTION_2_INSTRUMENT_PREPARE = 43, // 检测2仪器准备
        INSPECTION_2_FREQUENCY_SELECT = 44, // 检测2选频
        INSPECTION_2_INSTRUMENT_START = 45, // 检测2仪器启动
        INSPECTION_2_COMPLETED = 46, // 检测2完成
        INSPECTION_2_JUMP = 47,   // 检测2Jump
        INSPECTION_3_RUNNING = 60, // 检测3运行
        INSPECTION_SEAL_3_ADVANCE = 61, // 检测封堵3进
        INSPECTION_3_AIR_PREPARE = 62, // 检测3气路准备
        INSPECTION_3_INSTRUMENT_PREPARE = 63, // 检测3仪器准备
        INSPECTION_3_FREQUENCY_SELECT = 64, // 检测3选频
        INSPECTION_3_INSTRUMENT_START = 65, // 检测3仪器启动
        INSPECTION_3_COMPLETED = 66, // 检测3完成
        INSPECTION_3_JUMP = 67,   // 检测3Jump
        MAIN_RESET = 100,         // 主复位
        RESET_BUFFER = 101,       // 复位缓冲
        SEAL_3_RETREAT = 102,     // 封堵3退
        SEAL_2_RETREAT = 103,     // 封堵2退
        SEAL_1_RETREAT = 104,     // 封堵1退
        MAIN_CYLINDER_RETREAT = 105, // 主缸退
        MAIN_CYLINDER_RETREATED = 106, // 主缸退到位
        RETURN_CYLINDER = 107,    // 返程气缸
        BOOSTER_IN_PLACE = 108,   // 增压原位
        RESET_COMPLETED = 111     // 复位完成
    };
    Q_ENUM(DeviceOperation)

    // 检验结果值枚举（寄存器D404的值）
    enum InspectionResult {
        NO_RESULT = 0,            // 无结果
        MAIN_RUNNING_RESULT = 1,  // 主运行
        WORKPIECE_OK = 2,         // 工件OK
        WORKPIECE_RE = 3,         // 工件RE
        WORKPIECE_NG = 4          // 工件NG
    };
    Q_ENUM(InspectionResult)

    // 构造函数
    explicit PLCStatusEnums(QObject *parent = nullptr);

    // 获取设备状态对应的中文名称
    static QString deviceStatusToString(DeviceStatus status);
    // 根据编码获取设备状态枚举
    static DeviceStatus deviceStatusFromCode(int code);
    // 获取所有设备状态的列表
    static QList<DeviceStatus> getAllDeviceStatus();

    // 获取设备运行值对应的中文名称
    static QString deviceOperationToString(DeviceOperation operation);
    // 根据编码获取设备运行值枚举
    static DeviceOperation deviceOperationFromCode(int code);
    // 获取所有设备运行值的列表
    static QList<DeviceOperation> getAllDeviceOperations();

    // 获取检验结果对应的中文名称
    static QString inspectionResultToString(InspectionResult result);
    // 根据编码获取检验结果枚举
    static InspectionResult inspectionResultFromCode(int code);
    // 获取所有检验结果的列表
    static QList<InspectionResult> getAllInspectionResults();
};

#endif // PLCSTATUSENUMS_H