#include "SystemSettingDao.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

bool SystemSettingDao::createTable()
{
    DatabaseManager& dbManager = DatabaseManager::getInstance();
    
    if (!dbManager.isConnected()) {
        qCritical() << "数据库未连接";
        return false;
    }

    // 创建系统设置表SQL语句
    QString sql = "CREATE TABLE IF NOT EXISTS system_settings ("
                  "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                  "section TEXT NOT NULL, "
                  "key TEXT NOT NULL, "
                  "value_type TEXT NOT NULL, "
                  "value TEXT NOT NULL, "
                  "UNIQUE(section, key) ON CONFLICT REPLACE)";

    bool result = dbManager.executeQuery(sql);
    if (!result) {
        qCritical() << "创建系统设置表失败:" << dbManager.getLastError();
    }
    return result;
}

bool SystemSettingDao::saveSetting(const QString& section, const QString& key, const QVariant& value)
{
    DatabaseManager& dbManager = DatabaseManager::getInstance();
    
    if (!dbManager.isConnected()) {
        qCritical() << "数据库未连接";
        return false;
    }

    // 准备插入的列和值
    QStringList columns = {"section", "key", "value_type", "value"};
    
    QVariantList values;
    values << section
           << key
           << value.typeName()
           << value.toString();

    bool result = dbManager.insert("system_settings", columns, values);
    if (!result) {
        qCritical() << "保存系统设置失败:" << dbManager.getLastError();
    }
    return result;
}

bool SystemSettingDao::batchSaveSettings(const QHash<QString, QVariant>& settingsMap)
{
    DatabaseManager& dbManager = DatabaseManager::getInstance();
    
    if (!dbManager.isConnected()) {
        qCritical() << "数据库未连接";
        return false;
    }

    if (settingsMap.isEmpty()) {
        qCritical() << "批量保存的设置为空";
        return false;
    }

    // 准备插入的列
    QStringList columns = {"section", "key", "value_type", "value"};

    // 准备批量插入的值列表
    QList<QVariantList> valuesList;
    
    for (auto it = settingsMap.constBegin(); it != settingsMap.constEnd(); ++it) {
        // 解析键名，格式为 section.key
        QString fullKey = it.key();
        int dotPos = fullKey.indexOf(".");
        
        if (dotPos == -1) {
            qCritical() << "无效的键名格式，应为 section.key: " << fullKey;
            continue;
        }
        
        QString section = fullKey.left(dotPos);
        QString key = fullKey.mid(dotPos + 1);
        
        QVariantList values;
        values << section
               << key
               << it.value().typeName()
               << it.value().toString();
        
        valuesList.append(values);
    }

    bool result = dbManager.batchInsert("system_settings", columns, valuesList);
    if (!result) {
        qCritical() << "批量保存系统设置失败:" << dbManager.getLastError();
    }
    return result;
}

QVariant SystemSettingDao::getSetting(const QString& section, const QString& key, const QVariant& defaultValue)
{
    DatabaseManager& dbManager = DatabaseManager::getInstance();
    
    if (!dbManager.isConnected()) {
        qCritical() << "数据库未连接";
        return defaultValue;
    }

    QString condition = QString("section = '%1' AND key = '%2'")
            .arg(section)
            .arg(key);
    
    QSqlQuery query = dbManager.select("system_settings", QStringList(), condition);

    if (query.lastError().isValid()) {
        qCritical() << "查询系统设置失败:" << query.lastError().text();
        return defaultValue;
    }

    if (query.next()) {
        QString valueType = query.value("value_type").toString();
        QString valueStr = query.value("value").toString();
        
        // 根据类型转换值
        if (valueType == "bool") {
            // 支持"true"/"false"和数字格式的布尔值
            return QVariant(valueStr.compare("true", Qt::CaseInsensitive) == 0 || 
                           valueStr.compare("yes", Qt::CaseInsensitive) == 0 || 
                           valueStr.toInt() != 0);
        } else if (valueType == "int") {
            return QVariant(valueStr.toInt());
        } else if (valueType == "double") {
            return QVariant(valueStr.toDouble());
        } else {
            return QVariant(valueStr);
        }
    }
    
    return defaultValue;
}

bool SystemSettingDao::settingExists(const QString& section, const QString& key)
{
    DatabaseManager& dbManager = DatabaseManager::getInstance();
    
    if (!dbManager.isConnected()) {
        qCritical() << "数据库未连接";
        return false;
    }

    QString condition = QString("section = '%1' AND key = '%2'")
            .arg(section)
            .arg(key);
    
    QSqlQuery query = dbManager.select("system_settings", QStringList(), condition);

    if (query.lastError().isValid()) {
        qCritical() << "检查系统设置是否存在失败:" << query.lastError().text();
        return false;
    }
    
    return query.next();
}

bool SystemSettingDao::deleteSetting(const QString& section, const QString& key)
{
    DatabaseManager& dbManager = DatabaseManager::getInstance();
    
    if (!dbManager.isConnected()) {
        qCritical() << "数据库未连接";
        return false;
    }

    QString condition = QString("section = '%1' AND key = '%2'")
            .arg(section)
            .arg(key);
    
    bool result = dbManager.deleteData("system_settings", condition);
    if (!result) {
        qCritical() << "删除系统设置失败:" << dbManager.getLastError();
    }
    return result;
}

bool SystemSettingDao::clearAllSettings()
{
    DatabaseManager& dbManager = DatabaseManager::getInstance();
    
    if (!dbManager.isConnected()) {
        qCritical() << "数据库未连接";
        return false;
    }
    
    bool result = dbManager.deleteData("system_settings");
    if (!result) {
        qCritical() << "清空所有系统设置失败:" << dbManager.getLastError();
    }
    return result;
}

QString SystemSettingDao::buildFullKey(const QString& section, const QString& key)
{
    return section + "." + key;
}