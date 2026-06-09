#ifndef SYSTEMSETTINGDAO_H
#define SYSTEMSETTINGDAO_H

#include <QString>
#include <QVariant>
#include "DatabaseManager.h"

class SystemSettingDao
{
public:
    // 创建系统设置表
    static bool createTable();

    // 保存系统设置
    static bool saveSetting(const QString& section, const QString& key, const QVariant& value);

    // 批量保存系统设置
    static bool batchSaveSettings(const QHash<QString, QVariant>& settingsMap);

    // 读取系统设置
    static QVariant getSetting(const QString& section, const QString& key, const QVariant& defaultValue = QVariant());

    // 检查设置是否存在
    static bool settingExists(const QString& section, const QString& key);

    // 删除系统设置
    static bool deleteSetting(const QString& section, const QString& key);

    // 清空所有设置
    static bool clearAllSettings();

private:
    // 构建完整的键名（section.key）
    static QString buildFullKey(const QString& section, const QString& key);
};

#endif // SYSTEMSETTINGDAO_H