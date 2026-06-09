#ifndef AIRTIGHTNESSPARAMS_H
#define AIRTIGHTNESSPARAMS_H

#include <QString>
#include <QDateTime>

/**
 * @brief 周期时间参数结构体
 */
struct CycleTimeParams {
    double fillTime = 0.0;           // 填充时间
    double stabilizationTime = 0.0;  // 稳定时间
    double testTime = 0.0;           // 测试时间
    double dumpTime = 0.0;           // 排气时间
};

/**
 * @brief 压力参数结构体
 */
struct PressureParams {
    int unitIndex = 0;               // 压力单位索引 (0:Pa, 1:kPa, 2:MPa, 3:psi, 4:bar)
    double maximum = 0.0;            // 最大压力
    double minimum = 0.0;            // 最小压力
    double setFill = 0.0;            // 填充设置
    int fillTypeIndex = 0;           // 填充类型索引 (0:快速, 1:慢速, 2:阶梯)
    double regulatorPressure = 0.0;  // 调压阀压力
};

/**
 * @brief 泄漏参数结构体
 */
struct LeakParams {
    int leakUnitIndex = 0;           // 泄漏单位索引 (0:Pa/s, 1:kPa/min, 2:mL/min, 3:L/h)
    int leakCode1 = 0;               // 8212编码
    int leakCode2 = 0;               // 8217编码
    double testReject = 0.0;         // 测试拒绝值
    double refReject = 0.0;          // 参考拒绝值
    double offset = 0.0;             // 偏移值
    double stdAtm = 99.99;           // 标准大气压
    double stdTemp = 20.0;           // 标准温度
    double volume = 0.0;             // 体积
    int volumeUnitIndex = 0;         // 体积单位索引 (0:mL, 1:L, 2:cm³, 3:m³)
    int rejectCalcIndex = 0;         // 拒绝计算方式索引 (0:绝对, 1:相对, 2:百分比)
};

/**
 * @brief 气密仪完整参数结构体
 */
struct AirTightnessFullParams {
    int id = -1;                     // 数据库ID
    QString paramName;               // 参数组名称
    int programNumber = 1;           // 程序号 (1-3)
    CycleTimeParams cycleTime;       // 周期时间参数
    PressureParams pressure;         // 压力参数
    LeakParams leak;                 // 泄漏参数
    QDateTime createdAt;             // 创建时间
    QDateTime updatedAt;             // 更新时间
};

#endif // AIRTIGHTNESSPARAMS_H
