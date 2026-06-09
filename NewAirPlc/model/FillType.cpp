#include "FillType.h"

FillType::FillType(QObject *parent) : QObject(parent)
{

}

FillType::Type FillType::fromCode(int code)
{
    switch(code) {
    case 0: return Standard;
    case 256: return Auto;
    case 512: return Instruction;
    case 768: return Ramp;
    case 1024: return RampControl;
    default:
        // 处理无效编码，默认返回Standard
        return Standard;
    }
}

int FillType::toCode(FillType::Type type)
{
    return static_cast<int>(type);
}

QString FillType::toChineseString(FillType::Type type)
{
    switch(type) {
    case Standard: return "标准";
    case Auto: return "自动";
    case Instruction: return "指令";
    case Ramp: return "斜坡";
    case RampControl: return "斜坡控制";
    default: return "未知";
    }
}

QString FillType::toEnglishString(FillType::Type type)
{
    switch(type) {
    case Standard: return "Standard";
    case Auto: return "Auto";
    case Instruction: return "Instruction";
    case Ramp: return "Ramp";
    case RampControl: return "Ramp Control";
    default: return "Unknown";
    }
}

QList<FillType::Type> FillType::getAllTypes()
{
    // 按枚举定义顺序返回所有类型（顺序可根据需求调整）
    return {
        Standard,
        Auto,
        Instruction,
        Ramp,
        RampControl
    };
}
