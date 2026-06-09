#ifndef FILLTYPE_H
#define FILLTYPE_H

#include <QObject>
#include <QString>
#include <QList>  // 引入QList用于返回所有类型

class FillType : public QObject
{
    Q_OBJECT
public:
    // 定义填充类型枚举
    enum Type {
        Standard = 0,      // 标准
        Auto = 256,        // 自动
        Instruction = 512, // 指令
        Ramp = 768,        // 斜坡
        RampControl = 1024 // 斜坡控制
    };
    Q_ENUM(Type)  // 使枚举可用于Qt元对象系统

    // 构造函数
    explicit FillType(QObject *parent = nullptr);

    // 根据编码获取对应的填充类型枚举
    static Type fromCode(int code);

    // 获取枚举对应的编码值
    static int toCode(Type type);

    // 获取枚举对应的中文名称
    static QString toChineseString(Type type);

    // 获取枚举对应的英文名称
    static QString toEnglishString(Type type);

    // 返回所有填充类型的列表（核心遍历功能）
    static QList<Type> getAllTypes();

};

#endif // FILLTYPE_H
