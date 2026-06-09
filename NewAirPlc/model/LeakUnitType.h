#ifndef LEAKUNITTYPE_H
#define LEAKUNITTYPE_H

#include <QObject>
#include <QMetaType>
#include <QList>

// 泄漏单位类型枚举类
class LeakUnitType : public QObject
{
    Q_OBJECT
    Q_ENUMS(Unit)
    Q_ENUMS(CodeType)

public:
    // 定义编码类型枚举，区分两个不同的编码列
    enum CodeType {
        CodeType1,  // 对应编码列1
        CodeType2   // 对应编码列2
    };

    // 泄漏单位枚举
    enum Unit {
        Pa = 0,          // 帕斯卡
        PaPerSecond,     // Pa/s
        PaPerHour,       // Pa/HR
        PaPerSecondHR,   // Pa/s HR
        Reserve1,        // Reserve
        Reserve2,        // Reserve
        CcPerMinute,     // cc/Min
        CcPerSecond,     // cc/s
        CcPerHour,       // cc/h
        Mm3PerSecond,    // mm³/s
        Cc3PerSecond,    // cc3/s
        Cc3PerMinute,    // cc3/Min
        Cc3PerHour,      // cc3/h
        MlPerSecond,     // ml/s
        MlPerMinute,     // ml/min
        MlPerHour,       // ml/h
        In3PerSecond,    // in³/s
        In3PerMinute,    // in³/min
        In3PerHour,      // in³/h
        Reserve3,        // Reserve
        Sccm,            // sccm
        Points,          // points
        UnitCount        // 用于计数的标记，不作为实际单位
    };
    Q_ENUM(Unit)

    // 存储单位及其两种编码的结构体
    struct UnitData {
        LeakUnitType::Unit unit;  // 单位枚举
        int code1;                // CodeType1对应的编码
        int code2;                // CodeType2对应的编码
    };

    // 构造函数
    explicit LeakUnitType(QObject *parent = nullptr);

    // 获取单位对应的编码
    static int getCode(Unit unit, CodeType codeType = CodeType1);

    // 获取单位的字符串表示
    static QString getUnitString(Unit unit);

    // 根据编码获取对应的单位
    static Unit getUnitFromCode(int code, CodeType codeType = CodeType1);

    // 获取所有泄漏单位的列表
    static QList<Unit> getAllUnits();

    // 获取所有泄漏单位的完整数据（包含两种编码）
    static QList<UnitData> getAllUnitDatas();
};

// 声明元类型，以便在QVariant中使用
Q_DECLARE_METATYPE(LeakUnitType::Unit)
Q_DECLARE_METATYPE(LeakUnitType::CodeType)
Q_DECLARE_METATYPE(LeakUnitType::UnitData)

#endif // LEAKUNITTYPE_H
