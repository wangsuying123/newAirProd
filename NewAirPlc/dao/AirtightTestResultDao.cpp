#include "AirtightTestResultDao.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QDateTime>

// 初始化测试结果表
bool AirtightTestResultDao::initTable()
{
    QString sql = "CREATE TABLE IF NOT EXISTS air_tightness_test_results ("
                  "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                  "timestamp TIMESTAMP NOT NULL, "
                  "program_number INTEGER, "
                  "product_number TEXT, "
                  "alarm_info TEXT NOT NULL, "
                  "alarm_code INTEGER, "
                  "pressure_value REAL NOT NULL, "
                  "pressure_unit TEXT NOT NULL, "
                  "leak_value REAL NOT NULL, "
                  "leak_unit TEXT NOT NULL, "
                  "atm_pressure REAL NOT NULL, "
                  "atm_pressure_unit TEXT NOT NULL, "
                  "temperature REAL NOT NULL, "
                  "temperature_unit TEXT NOT NULL, "
                  "inspector TEXT)";

    DatabaseManager& dbManager = DatabaseManager::getInstance();
    bool ok = dbManager.executeQuery(sql);

    // 迁移：如果旧表不存在 program_number 或 product_number 列，则添加
    QSqlQuery pragmaQuery;
    if (pragmaQuery.exec("PRAGMA table_info(air_tightness_test_results)")) {
        bool hasProgramNumber = false;
        bool hasProductNumber = false;
        bool hasAlarmCode = false;
        bool hasInspector = false;
        while (pragmaQuery.next()) {
            const QString name = pragmaQuery.value("name").toString();
            if (name == "program_number") {
                hasProgramNumber = true;
            } else if (name == "product_number") {
                hasProductNumber = true;
            } else if (name == "alarm_code") {
                hasAlarmCode = true;
            } else if (name == "inspector") {
                hasInspector = true;
            }
        }
        if (!hasProgramNumber) {
            dbManager.executeQuery("ALTER TABLE air_tightness_test_results ADD COLUMN program_number INTEGER");
        }
        if (!hasProductNumber) {
            dbManager.executeQuery("ALTER TABLE air_tightness_test_results ADD COLUMN product_number TEXT");
        }
        if (!hasAlarmCode) {
            dbManager.executeQuery("ALTER TABLE air_tightness_test_results ADD COLUMN alarm_code INTEGER");
        }
        if (!hasInspector) {
            dbManager.executeQuery("ALTER TABLE air_tightness_test_results ADD COLUMN inspector TEXT");
        }
    }

    return ok;
}

// 保存测试结果到数据库
bool AirtightTestResultDao::saveResult(const AirtightTestResultData& result)
{
    DatabaseManager& dbManager = DatabaseManager::getInstance();

    QStringList columns = {
        "timestamp", "program_number", "product_number", "alarm_info", "alarm_code", "pressure_value", "pressure_unit",
        "leak_value", "leak_unit", "atm_pressure", "atm_pressure_unit",
        "temperature", "temperature_unit", "inspector"
    };

    QVariantList values;
    values << result.timestamp.toString("yyyy-MM-dd HH:mm:ss")
           << (result.programNumber >= 0 ? result.programNumber : QVariant(QVariant::Int))
           << (result.productNumber.isEmpty() ? QVariant(QVariant::String) : result.productNumber)
           << result.alarmInfo
           << (result.alarmCode >= 0 ? result.alarmCode : QVariant(QVariant::Int))
           << result.pressureValue
           << result.pressureUnit
           << result.leakValue
           << result.leakUnit
           << result.atmPressure
           << result.atmPressureUnit
           << result.temperature
           << result.temperatureUnit
           << (result.inspector.isEmpty() ? QVariant(QVariant::String) : result.inspector);

    return dbManager.insert("air_tightness_test_results", columns, values);
}

// 获取所有测试结果
QList<AirtightTestResultData> AirtightTestResultDao::getAllResults()
{
    QList<AirtightTestResultData> results;
    DatabaseManager& dbManager = DatabaseManager::getInstance();

    QSqlQuery query = dbManager.select("air_tightness_test_results", {}, "", "timestamp DESC");
    while (query.next()) {
        AirtightTestResultData result;
        result.id = query.value("id").toInt();
        result.timestamp = QDateTime::fromString(query.value("timestamp").toString(), "yyyy-MM-dd HH:mm:ss");
        result.programNumber = query.value("program_number").isNull() ? -1 : query.value("program_number").toInt();
        result.productNumber = query.value("product_number").toString();
        result.alarmInfo = query.value("alarm_info").toString();
        result.alarmCode = query.value("alarm_code").isNull() ? -1 : query.value("alarm_code").toInt();
        result.pressureValue = query.value("pressure_value").toDouble();
        result.pressureUnit = query.value("pressure_unit").toString();
        result.leakValue = query.value("leak_value").toDouble();
        result.leakUnit = query.value("leak_unit").toString();
        result.atmPressure = query.value("atm_pressure").toDouble();
        result.atmPressureUnit = query.value("atm_pressure_unit").toString();
        result.temperature = query.value("temperature").toDouble();
        result.temperatureUnit = query.value("temperature_unit").toString();
        result.inspector = query.value("inspector").toString();

        results.append(result);
    }

    return results;
}

// 根据ID获取测试结果
AirtightTestResultData AirtightTestResultDao::getResultById(int id)
{
    AirtightTestResultData result;
    DatabaseManager& dbManager = DatabaseManager::getInstance();

    QString condition = QString("id = %1").arg(id);
    QSqlQuery query = dbManager.select("air_tightness_test_results", {}, condition, "");

    if (query.next()) {
        result.id = query.value("id").toInt();
        result.timestamp = QDateTime::fromString(query.value("timestamp").toString(), "yyyy-MM-dd HH:mm:ss");
        result.programNumber = query.value("program_number").isNull() ? -1 : query.value("program_number").toInt();
        result.productNumber = query.value("product_number").toString();
        result.alarmInfo = query.value("alarm_info").toString();
        result.alarmCode = query.value("alarm_code").isNull() ? -1 : query.value("alarm_code").toInt();
        result.pressureValue = query.value("pressure_value").toDouble();
        result.pressureUnit = query.value("pressure_unit").toString();
        result.leakValue = query.value("leak_value").toDouble();
        result.leakUnit = query.value("leak_unit").toString();
        result.atmPressure = query.value("atm_pressure").toDouble();
        result.atmPressureUnit = query.value("atm_pressure_unit").toString();
        result.temperature = query.value("temperature").toDouble();
        result.temperatureUnit = query.value("temperature_unit").toString();
        result.inspector = query.value("inspector").toString();
    }

    return result;
}

// 根据时间范围和条件查询测试结果
QList<AirtightTestResultData> AirtightTestResultDao::queryResults(const QDateTime& startTime, const QDateTime& endTime,
                                                             const QString& alarmCondition, const QString& resultCondition,
                                                             const QString& productNumber, const QString& inspector)
{
    QList<AirtightTestResultData> results;
    DatabaseManager& dbManager = DatabaseManager::getInstance();

    // 构建查询条件
    QString condition = QString("timestamp BETWEEN '%1' AND '%2'").arg(
        startTime.toString("yyyy-MM-dd HH:mm:ss"),
        endTime.toString("yyyy-MM-dd HH:mm:ss")
    );

    // 添加报警条件
    if (alarmCondition != "全部") {
        condition += QString(" AND alarm_info LIKE '%%1%'").arg(alarmCondition);
    }

    // 添加测试结果条件
    if (resultCondition != "全部") {
        if (resultCondition == "通过") {
            condition += " AND alarm_info = '通过'";
        } else if (resultCondition == "不通过") {
            condition += " AND alarm_info NOT LIKE '通过'";
        }
    }

    // 添加产品编号条件
    if (!productNumber.isEmpty()) {
        condition += QString(" AND product_number LIKE '%%1%'").arg(productNumber);
    }

    // 添加质检人条件
    if (!inspector.isEmpty() && inspector != "全部") {
        condition += QString(" AND inspector = '%1'").arg(inspector);
    }

    QSqlQuery query = dbManager.select("air_tightness_test_results", {}, condition, "timestamp DESC");
    while (query.next()) {
        AirtightTestResultData result;
        result.id = query.value("id").toInt();
        result.timestamp = QDateTime::fromString(query.value("timestamp").toString(), "yyyy-MM-dd HH:mm:ss");
        result.programNumber = query.value("program_number").isNull() ? -1 : query.value("program_number").toInt();
        result.productNumber = query.value("product_number").toString();
        result.alarmInfo = query.value("alarm_info").toString();
        result.alarmCode = query.value("alarm_code").isNull() ? -1 : query.value("alarm_code").toInt();
        result.pressureValue = query.value("pressure_value").toDouble();
        result.pressureUnit = query.value("pressure_unit").toString();
        result.leakValue = query.value("leak_value").toDouble();
        result.leakUnit = query.value("leak_unit").toString();
        result.atmPressure = query.value("atm_pressure").toDouble();
        result.atmPressureUnit = query.value("atm_pressure_unit").toString();
        result.temperature = query.value("temperature").toDouble();
        result.temperatureUnit = query.value("temperature_unit").toString();
        result.inspector = query.value("inspector").toString();

        results.append(result);
    }

    return results;
}

// 删除测试结果
bool AirtightTestResultDao::deleteResult(int id)
{
    DatabaseManager& dbManager = DatabaseManager::getInstance();
    return dbManager.deleteData("air_tightness_test_results", QString("id = %1").arg(id));
}

// 清空测试结果表
bool AirtightTestResultDao::clearResults()
{
    DatabaseManager& dbManager = DatabaseManager::getInstance();
    return dbManager.executeQuery("DELETE FROM air_tightness_test_results");
}

// 获取所有不重复的质检人列表
QStringList AirtightTestResultDao::getDistinctInspectors()
{
    QStringList inspectors;
    QSqlQuery query;
    if (query.exec("SELECT DISTINCT inspector FROM air_tightness_test_results WHERE inspector IS NOT NULL AND inspector != '' ORDER BY inspector ASC")) {
        while (query.next()) {
            inspectors << query.value(0).toString();
        }
    }
    return inspectors;
}