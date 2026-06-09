#ifndef AIRTIGHTTESTRESULTDAO_H
#define AIRTIGHTTESTRESULTDAO_H

#include <QString>
#include <QDateTime>
#include <QList>
#include "DatabaseManager.h"

struct AirtightTestResultData {
    int id;                  // 主键ID
    QDateTime timestamp;     // 测试时间戳
    int programNumber;       // 程序号 (1-3，未知为-1)
    QString productNumber;   // 产品编号
    QString alarmInfo;       // 报警信息
    int alarmCode;           // 报警代码（寄存器8962的原始值）
    double pressureValue;    // 压力值
    QString pressureUnit;    // 压力单位
    double leakValue;        // 泄露值
    QString leakUnit;        // 泄露单位
    double atmPressure;      // 标准大气压
    QString atmPressureUnit; // 大气压单位
    double temperature;      // 温度值
    QString temperatureUnit; // 温度单位
    QString inspector;       // 质检人

    // 构造函数
    AirtightTestResultData() : id(-1), programNumber(-1), productNumber(""), alarmCode(-1), pressureValue(0), leakValue(0), atmPressure(0), temperature(0), inspector("") {}
};

class AirtightTestResultDao {
public:
    // 初始化测试结果表
    static bool initTable();

    // 保存测试结果到数据库
    static bool saveResult(const AirtightTestResultData& result);

    // 获取所有测试结果
    static QList<AirtightTestResultData> getAllResults();

    // 根据ID获取测试结果
    static AirtightTestResultData getResultById(int id);

    // 根据时间范围和条件查询测试结果
    static QList<AirtightTestResultData> queryResults(const QDateTime& startTime, const QDateTime& endTime,
                                                     const QString& alarmCondition = "全部",
                                                     const QString& resultCondition = "全部",
                                                     const QString& productNumber = "",
                                                     const QString& inspector = "");

    // 删除测试结果
    static bool deleteResult(int id);

    // 清空测试结果表
    static bool clearResults();

    // 获取所有不重复的质检人列表
    static QStringList getDistinctInspectors();
};

#endif // AIRTIGHTTESTRESULTDAO_H