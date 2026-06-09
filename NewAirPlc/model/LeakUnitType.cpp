#include "LeakUnitType.h"

LeakUnitType::LeakUnitType(QObject *parent) : QObject(parent)
{

}

int LeakUnitType::getCode(Unit unit, CodeType codeType)
{
    // 根据单位和编码类型返回对应的编码值
    switch (unit) {
    case Pa:
        return (codeType == CodeType1) ? 0 : 0;
    case PaPerSecond:
        return (codeType == CodeType1) ? 256 : 0;
    case PaPerHour:
        return (codeType == CodeType1) ? 512 : 0;
    case PaPerSecondHR:
        return (codeType == CodeType1) ? 768 : 0;
    case Reserve1:
        return (codeType == CodeType1) ? 1024 : 7680;
    case Reserve2:
        return (codeType == CodeType1) ? 1280 : 7680;
    case CcPerMinute:
        return (codeType == CodeType1) ? 1536 : 47115;
    case CcPerSecond:
        return (codeType == CodeType1) ? 1792 : 47115;
    case CcPerHour:
        return (codeType == CodeType1) ? 2048 : 47115;
    case Mm3PerSecond:
        return (codeType == CodeType1) ? 2304 : 47115;
    case Cc3PerSecond:
        return (codeType == CodeType1) ? 2560 : 47115;
    case Cc3PerMinute:
        return (codeType == CodeType1) ? 2816 : 47115;
    case Cc3PerHour:
        return (codeType == CodeType1) ? 3072 : 47115;
    case MlPerSecond:
        return (codeType == CodeType1) ? 3328 : 47115;
    case MlPerMinute:
        return (codeType == CodeType1) ? 3584 : 47115;
    case MlPerHour:
        return (codeType == CodeType1) ? 3840 : 47115;
    case In3PerSecond:
        return (codeType == CodeType1) ? 4096 : 47115;
    case In3PerMinute:
        return (codeType == CodeType1) ? 4352 : 47115;
    case In3PerHour:
        return (codeType == CodeType1) ? 4608 : 47115;
    case Reserve3:
        return (codeType == CodeType1) ? 4864 : 47115;
    case Sccm:
        return (codeType == CodeType1) ? 6144 : 12405;
    case Points:
        return (codeType == CodeType1) ? 6400 : 768;
    default:
        return -1; // 无效单位
    }
}

QString LeakUnitType::getUnitString(Unit unit)
{
    // 返回单位的字符串表示
    switch (unit) {
    case Pa:
        return "Pa";
    case PaPerSecond:
        return "Pa/s";
    case PaPerHour:
        return "Pa/HR";
    case PaPerSecondHR:
        return "Pa/s HR";
    case Reserve1:
        return "Reserve";
    case Reserve2:
        return "Reserve";
    case CcPerMinute:
        return "cc/Min";
    case CcPerSecond:
        return "cc/s";
    case CcPerHour:
        return "cc/h";
    case Mm3PerSecond:
        return "mm³/s";
    case Cc3PerSecond:
        return "cc3/s";
    case Cc3PerMinute:
        return "cc3/Min";
    case Cc3PerHour:
        return "cc3/h";
    case MlPerSecond:
        return "ml/s";
    case MlPerMinute:
        return "ml/min";
    case MlPerHour:
        return "ml/h";
    case In3PerSecond:
        return "in³/s";
    case In3PerMinute:
        return "in³/min";
    case In3PerHour:
        return "in³/h";
    case Reserve3:
        return "Reserve";
    case Sccm:
        return "sccm";
    case Points:
        return "points";
    default:
        return "Unknown";
    }
}

LeakUnitType::Unit LeakUnitType::getUnitFromCode(int code, CodeType codeType)
{
    // 遍历所有单位，查找匹配的编码
    for (Unit unit : getAllUnits()) {
        if (getCode(unit, codeType) == code) {
            return unit;
        }
    }
    return static_cast<Unit>(-1); // 无效编码返回无效单位
}

// 返回所有泄漏单位的列表
QList<LeakUnitType::Unit> LeakUnitType::getAllUnits()
{
    QList<Unit> units;
    // 从第一个枚举值遍历到Points
    for (int i = 0; i < UnitCount; ++i) {
        units.append(static_cast<Unit>(i));
    }
    return units;
}
