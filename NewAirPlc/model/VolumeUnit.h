#ifndef VOLUMEUNIT_H
#define VOLUMEUNIT_H

#include <QMetaType>
#include <QString>
#include <QList>

/**
 * @brief 体积单位枚举类，包含数据表六中的所有单位及对应编码
 */
enum class VolumeUnit
{
    CubicCentimeter = 0,    // cm³，立方厘米
    CubicMillimeter = 256,  // mm³，立方毫米
    Milliliter = 512,       // ml，毫升
    Liter = 768,            // l，升
    CubicInch = 1024,       // inch³，立方英寸
    CubicFeet = 1280        // feet³，立方英尺
};

// 声明元类型，便于在Qt信号槽中使用
Q_DECLARE_METATYPE(VolumeUnit)

/**
 * @brief 获取体积单位的字符串表示
 * @param unit 体积单位枚举值
 * @return 单位的字符串表示（如"cm³"）
 */
QString volumeUnitToString(VolumeUnit unit);

/**
 * @brief 根据编码获取体积单位枚举值
 * @param code 单位编码
 * @return 对应的体积单位枚举值，默认返回立方厘米
 */
VolumeUnit volumeUnitFromCode(int code);

/**
 * @brief 获取体积单位对应的编码
 * @param unit 体积单位枚举值
 * @return 对应的编码值
 */
int volumeUnitToCode(VolumeUnit unit);

/**
 * @brief 获取所有体积单位的列表
 * @return 包含所有体积单位的QList
 */
QList<VolumeUnit> getAllVolumeUnits();

#endif // VOLUMEUNIT_H
