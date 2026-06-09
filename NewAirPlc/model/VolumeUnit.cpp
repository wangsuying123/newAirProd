#include "VolumeUnit.h"

QString volumeUnitToString(VolumeUnit unit)
{
    switch (unit)
    {
    case VolumeUnit::CubicCentimeter: return "cm³";
    case VolumeUnit::CubicMillimeter: return "mm³";
    case VolumeUnit::Milliliter: return "ml";
    case VolumeUnit::Liter: return "l";
    case VolumeUnit::CubicInch: return "inch³";
    case VolumeUnit::CubicFeet: return "feet³";
    default: return "";
    }
}

VolumeUnit volumeUnitFromCode(int code)
{
    switch (code)
    {
    case 0: return VolumeUnit::CubicCentimeter;
    case 256: return VolumeUnit::CubicMillimeter;
    case 512: return VolumeUnit::Milliliter;
    case 768: return VolumeUnit::Liter;
    case 1024: return VolumeUnit::CubicInch;
    case 1280: return VolumeUnit::CubicFeet;
    default: return VolumeUnit::CubicCentimeter;
    }
}

int volumeUnitToCode(VolumeUnit unit)
{
    return static_cast<int>(unit);
}

// 实现获取所有体积单位的函数
QList<VolumeUnit> getAllVolumeUnits()
{
    // 按枚举定义的顺序返回所有体积单位
    return {
        VolumeUnit::CubicCentimeter,
        VolumeUnit::CubicMillimeter,
        VolumeUnit::Milliliter,
        VolumeUnit::Liter,
        VolumeUnit::CubicInch,
        VolumeUnit::CubicFeet
    };
}
