#ifndef TESTRESULTSTRUCT_H
#define TESTRESULTSTRUCT_H

#include <QDateTime>
#include <QString>

/**
 * @brief 测试结果结构体
 * 
 * 包含单个通道的测试数据，包括创建时间、程序号、通道压力、泄漏值以及通过/不通过状态
 */
struct TestResult {
    QDateTime createTime;    // 创建时间
    int programNumber;       // 程序号
    double channelPressure;  // 通道压力
    QString pressureUnit;    // 压力单位
    double channelLeak;      // 通道泄漏值
    QString leakUnit;        // 泄漏单位
    bool isPassed;           // 通过状态
    bool isFailed;           // 不通过状态
    
    // 构造函数
    TestResult() : 
        createTime(QDateTime::currentDateTime()),
        programNumber(0),
        channelPressure(0.0),
        pressureUnit(""),
        channelLeak(0.0),
        leakUnit(""),
        isPassed(false),
        isFailed(false) {}
};

#endif // TESTRESULTSTRUCT_H