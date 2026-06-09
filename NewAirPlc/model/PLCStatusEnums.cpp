#include "PLCStatusEnums.h"

PLCStatusEnums::PLCStatusEnums(QObject *parent) : QObject(parent)
{
    // 构造函数实现
}

// 设备状态相关方法
QString PLCStatusEnums::deviceStatusToString(DeviceStatus status)
{
    switch (status) {
    case DEVICE_HEARTBEAT:
        return "设备心跳";
    case M_EMERGENCY_STOP:
        return "M急停";
    case LIGHT_CURTAIN_EMERGENCY:
        return "光栅急停";
    case NO_WORKPIECE:
        return "无工件";
    case INSTRUMENT_START_TIMEOUT:
        return "仪器启动超时";
    case MAIN_CYLINDER_NOT_IN_PLACE:
        return "主缸不在原位";
    case MAIN_CYLINDER_ADVANCE_TIMEOUT:
        return "主缸进超时";
    case MAIN_CYLINDER_RETREAT_TIMEOUT:
        return "主缸退超时";
    case BOOSTER_ADVANCE_TIMEOUT:
        return "增压进超时";
    case RETURN_CYLINDER_TIMEOUT:
        return "返程气缸超时";
    case BOOSTER_CYLINDER_NOT_IN_PLACE:
        return "增压气缸不在原位";
    default:
        return "未知状态";
    }
}

PLCStatusEnums::DeviceStatus PLCStatusEnums::deviceStatusFromCode(int code)
{
    switch (code) {
    case 1:
        return DEVICE_HEARTBEAT;
    case 2:
        return M_EMERGENCY_STOP;
    case 3:
        return LIGHT_CURTAIN_EMERGENCY;
    case 4:
        return NO_WORKPIECE;
    case 5:
        return INSTRUMENT_START_TIMEOUT;
    case 6:
        return MAIN_CYLINDER_NOT_IN_PLACE;
    case 9:
        return MAIN_CYLINDER_ADVANCE_TIMEOUT;
    case 10:
        return MAIN_CYLINDER_RETREAT_TIMEOUT;
    case 11:
        return BOOSTER_ADVANCE_TIMEOUT;
    case 12:
        return RETURN_CYLINDER_TIMEOUT;
    case 13:
        return BOOSTER_CYLINDER_NOT_IN_PLACE;
    default:
        // 如果没有匹配的枚举值，返回一个默认值或者抛出异常
        return static_cast<DeviceStatus>(-1);
    }
}

QList<PLCStatusEnums::DeviceStatus> PLCStatusEnums::getAllDeviceStatus()
{
    QList<DeviceStatus> statusList;
    statusList << DEVICE_HEARTBEAT
               << M_EMERGENCY_STOP
               << LIGHT_CURTAIN_EMERGENCY
               << NO_WORKPIECE
               << INSTRUMENT_START_TIMEOUT
               << MAIN_CYLINDER_NOT_IN_PLACE
               << MAIN_CYLINDER_ADVANCE_TIMEOUT
               << MAIN_CYLINDER_RETREAT_TIMEOUT
               << BOOSTER_ADVANCE_TIMEOUT
               << RETURN_CYLINDER_TIMEOUT
               << BOOSTER_CYLINDER_NOT_IN_PLACE;
    return statusList;
}

// 设备运行值相关方法
QString PLCStatusEnums::deviceOperationToString(DeviceOperation operation)
{
    switch (operation) {
    case NOT_RUNNING:
        return "未运行";
    case MAIN_RUNNING:
        return "主运行";
    case CLEAR:
        return "清零";
    case MAIN_CYLINDER_ADVANCE:
        return "主缸进";
    case MAIN_CYLINDER_ADVANCED:
        return "主缸进到位";
    case SEAL_1_ADVANCE:
        return "封堵1进";
    case SEAL_2_ADVANCE:
        return "封堵2进";
    case SEAL_3_ADVANCE:
        return "封堵3进";
    case SEAL_COMPLETED:
        return "封堵完成";
    case BOOSTER_ADVANCE:
        return "增压进";
    case PRESS_DOWN_COMPLETED:
        return "下压完成";
    case INSPECTION_1_RUNNING:
        return "检测1运行";
    case INSPECTION_SEAL_1_ADVANCE:
        return "检测封堵1进";
    case INSPECTION_1_AIR_PREPARE:
        return "检测1气路准备";
    case INSPECTION_1_INSTRUMENT_PREPARE:
        return "检测1仪器准备";
    case INSPECTION_1_FREQUENCY_SELECT:
        return "检测1选频";
    case INSPECTION_1_INSTRUMENT_START:
        return "检测1仪器启动";
    case INSPECTION_1_COMPLETED:
        return "检测1完成";
    case INSPECTION_1_MP:
        return "检测1mp";
    case INSPECTION_2_RUNNING:
        return "检测2行";
    case INSPECTION_SEAL_2:
        return "检测封堵2";
    case INSPECTION_2_AIR_PREPARE:
        return "检测2路准备";
    case INSPECTION_2_INSTRUMENT_PREPARE:
        return "检测2仪器准备";
    case INSPECTION_2_FREQUENCY_SELECT:
        return "检测2选频";
    case INSPECTION_2_INSTRUMENT_START:
        return "检测2仪器启动";
    case INSPECTION_2_COMPLETED:
        return "检测2完成";
    case INSPECTION_2_JUMP:
        return "检测2Jump";
    case INSPECTION_3_RUNNING:
        return "检测3运行";
    case INSPECTION_SEAL_3_ADVANCE:
        return "检测封堵3进";
    case INSPECTION_3_AIR_PREPARE:
        return "检测3气路准备";
    case INSPECTION_3_INSTRUMENT_PREPARE:
        return "检测3仪器准备";
    case INSPECTION_3_FREQUENCY_SELECT:
        return "检测3选频";
    case INSPECTION_3_INSTRUMENT_START:
        return "检测3仪器启动";
    case INSPECTION_3_COMPLETED:
        return "检测3完成";
    case INSPECTION_3_JUMP:
        return "检测3Jump";
    case MAIN_RESET:
        return "主复位";
    case RESET_BUFFER:
        return "复位缓冲";
    case SEAL_3_RETREAT:
        return "封堵3退";
    case SEAL_2_RETREAT:
        return "封堵2退";
    case SEAL_1_RETREAT:
        return "封堵1退";
    case MAIN_CYLINDER_RETREAT:
        return "主缸退";
    case MAIN_CYLINDER_RETREATED:
        return "主缸退到位";
    case RETURN_CYLINDER:
        return "返程气缸";
    case BOOSTER_IN_PLACE:
        return "增压原位";
    case RESET_COMPLETED:
        return "复位完成";
    default:
        return "未知运行状态";
    }
}

PLCStatusEnums::DeviceOperation PLCStatusEnums::deviceOperationFromCode(int code)
{
    switch (code) {
    case 0:
        return NOT_RUNNING;
    case 1:
        return MAIN_RUNNING;
    case 2:
        return CLEAR;
    case 7:
        return MAIN_CYLINDER_ADVANCE;
    case 8:
        return MAIN_CYLINDER_ADVANCED;
    case 9:
        return SEAL_1_ADVANCE;
    case 10:
        return SEAL_2_ADVANCE;
    case 11:
        return SEAL_3_ADVANCE;
    case 12:
        return SEAL_COMPLETED;
    case 13:
        return BOOSTER_ADVANCE;
    case 14:
        return PRESS_DOWN_COMPLETED;
    case 20:
        return INSPECTION_1_RUNNING;
    case 21:
        return INSPECTION_SEAL_1_ADVANCE;
    case 22:
        return INSPECTION_1_AIR_PREPARE;
    case 23:
        return INSPECTION_1_INSTRUMENT_PREPARE;
    case 24:
        return INSPECTION_1_FREQUENCY_SELECT;
    case 25:
        return INSPECTION_1_INSTRUMENT_START;
    case 26:
        return INSPECTION_1_COMPLETED;
    case 27:
        return INSPECTION_1_MP;
    case 40:
        return INSPECTION_2_RUNNING;
    case 41:
        return INSPECTION_SEAL_2;
    case 42:
        return INSPECTION_2_AIR_PREPARE;
    case 43:
        return INSPECTION_2_INSTRUMENT_PREPARE;
    case 44:
        return INSPECTION_2_FREQUENCY_SELECT;
    case 45:
        return INSPECTION_2_INSTRUMENT_START;
    case 46:
        return INSPECTION_2_COMPLETED;
    case 47:
        return INSPECTION_2_JUMP;
    case 60:
        return INSPECTION_3_RUNNING;
    case 61:
        return INSPECTION_SEAL_3_ADVANCE;
    case 62:
        return INSPECTION_3_AIR_PREPARE;
    case 63:
        return INSPECTION_3_INSTRUMENT_PREPARE;
    case 64:
        return INSPECTION_3_FREQUENCY_SELECT;
    case 65:
        return INSPECTION_3_INSTRUMENT_START;
    case 66:
        return INSPECTION_3_COMPLETED;
    case 67:
        return INSPECTION_3_JUMP;
    case 100:
        return MAIN_RESET;
    case 101:
        return RESET_BUFFER;
    case 102:
        return SEAL_3_RETREAT;
    case 103:
        return SEAL_2_RETREAT;
    case 104:
        return SEAL_1_RETREAT;
    case 105:
        return MAIN_CYLINDER_RETREAT;
    case 106:
        return MAIN_CYLINDER_RETREATED;
    case 107:
        return RETURN_CYLINDER;
    case 108:
        return BOOSTER_IN_PLACE;
    case 111:
        return RESET_COMPLETED;
    default:
        // 如果没有匹配的枚举值，返回一个默认值或者抛出异常
        return static_cast<DeviceOperation>(-1);
    }
}

QList<PLCStatusEnums::DeviceOperation> PLCStatusEnums::getAllDeviceOperations()
{
    QList<DeviceOperation> operationList;
    operationList << NOT_RUNNING
                  << MAIN_RUNNING
                  << CLEAR
                  << MAIN_CYLINDER_ADVANCE
                  << MAIN_CYLINDER_ADVANCED
                  << SEAL_1_ADVANCE
                  << SEAL_2_ADVANCE
                  << SEAL_3_ADVANCE
                  << SEAL_COMPLETED
                  << BOOSTER_ADVANCE
                  << PRESS_DOWN_COMPLETED
                  << INSPECTION_1_RUNNING
                  << INSPECTION_SEAL_1_ADVANCE
                  << INSPECTION_1_AIR_PREPARE
                  << INSPECTION_1_INSTRUMENT_PREPARE
                  << INSPECTION_1_FREQUENCY_SELECT
                  << INSPECTION_1_INSTRUMENT_START
                  << INSPECTION_1_COMPLETED
                  << INSPECTION_1_MP
                  << INSPECTION_2_RUNNING
                  << INSPECTION_SEAL_2
                  << INSPECTION_2_AIR_PREPARE
                  << INSPECTION_2_INSTRUMENT_PREPARE
                  << INSPECTION_2_FREQUENCY_SELECT
                  << INSPECTION_2_INSTRUMENT_START
                  << INSPECTION_2_COMPLETED
                  << INSPECTION_2_JUMP
                  << INSPECTION_3_RUNNING
                  << INSPECTION_SEAL_3_ADVANCE
                  << INSPECTION_3_AIR_PREPARE
                  << INSPECTION_3_INSTRUMENT_PREPARE
                  << INSPECTION_3_FREQUENCY_SELECT
                  << INSPECTION_3_INSTRUMENT_START
                  << INSPECTION_3_COMPLETED
                  << INSPECTION_3_JUMP
                  << MAIN_RESET
                  << RESET_BUFFER
                  << SEAL_3_RETREAT
                  << SEAL_2_RETREAT
                  << SEAL_1_RETREAT
                  << MAIN_CYLINDER_RETREAT
                  << MAIN_CYLINDER_RETREATED
                  << RETURN_CYLINDER
                  << BOOSTER_IN_PLACE
                  << RESET_COMPLETED;
    return operationList;
}

// 检验结果相关方法
QString PLCStatusEnums::inspectionResultToString(InspectionResult result)
{
    switch (result) {
    case NO_RESULT:
        return "无结果";
    case MAIN_RUNNING_RESULT:
        return "主运行";
    case WORKPIECE_OK:
        return "工件OK";
    case WORKPIECE_RE:
        return "工件RE";
    case WORKPIECE_NG:
        return "工件NG";
    default:
        return "未知结果";
    }
}

PLCStatusEnums::InspectionResult PLCStatusEnums::inspectionResultFromCode(int code)
{
    switch (code) {
    case 0:
        return NO_RESULT;
    case 1:
        return MAIN_RUNNING_RESULT;
    case 2:
        return WORKPIECE_OK;
    case 3:
        return WORKPIECE_RE;
    case 4:
        return WORKPIECE_NG;
    default:
        // 如果没有匹配的枚举值，返回一个默认值或者抛出异常
        return static_cast<InspectionResult>(-1);
    }
}

QList<PLCStatusEnums::InspectionResult> PLCStatusEnums::getAllInspectionResults()
{
    QList<InspectionResult> resultList;
    resultList << NO_RESULT
               << MAIN_RUNNING_RESULT
               << WORKPIECE_OK
               << WORKPIECE_RE
               << WORKPIECE_NG;
    return resultList;
}