#ifndef PRESSUREUNIT_H
#define PRESSUREUNIT_H

#include <QObject>
#include <QList>
#include <QString>

class PressureUnit : public QObject
{
    Q_OBJECT
public:
    // 定义压力单位枚举
    enum Unit {
        Pa = 0,      // 帕斯卡
        Kpa = 256,   // 千帕
        Mpa = 512,   // 兆帕
        Bar = 768    // 巴
    };
    Q_ENUM(Unit)  // 使枚举可用于Qt元对象系统

    // 构造函数
    explicit PressureUnit(QObject *parent = nullptr);

    // 根据编码获取对应的单位枚举
    static Unit fromCode(int code);

    // 获取枚举对应的编码值
    static int toCode(Unit unit);

    // 获取枚举对应的字符串表示
    static QString toString(Unit unit);

    // 获取所有单位的列表，用于遍历
    static QList<Unit> getAllUnits();

    // 根据单位字符串获取枚举值
    static Unit fromString(const QString& str);
};

#endif // PRESSUREUNIT_H
