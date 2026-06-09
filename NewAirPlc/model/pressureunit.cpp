#include "pressureunit.h"

PressureUnit::PressureUnit(QObject *parent) : QObject(parent)
{

}

PressureUnit::Unit PressureUnit::fromCode(int code)
{
    switch(code) {
    case 0: return Pa;
    case 256: return Kpa;
    case 512: return Mpa;
    case 768: return Bar;
    default:
        // 可以根据需要处理无效编码，这里默认返回Pa
        return Pa;
    }
}

int PressureUnit::toCode(PressureUnit::Unit unit)
{
    return static_cast<int>(unit);
}

QString PressureUnit::toString(PressureUnit::Unit unit)
{
    switch(unit) {
    case Pa: return "Pa";
    case Kpa: return "Kpa";
    case Mpa: return "Mpa";
    case Bar: return "bar";
    default: return "Unknown";
    }
}

QList<PressureUnit::Unit> PressureUnit::getAllUnits()
{
    // 返回包含所有单位的列表，用于遍历
    return {
        Pa,
        Kpa,
        Mpa,
        Bar
    };
}

PressureUnit::Unit PressureUnit::fromString(const QString &str)
{
    QString lowerStr = str.toLower();
    if (lowerStr == "pa") return Pa;
    if (lowerStr == "kpa") return Kpa;
    if (lowerStr == "mpa") return Mpa;
    if (lowerStr == "bar") return Bar;

    // 默认返回Pa
    return Pa;
}
