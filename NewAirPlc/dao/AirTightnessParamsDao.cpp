#include "AirTightnessParamsDao.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDateTime>
#include <QDebug>

// 初始化参数表
bool AirTightnessParamsDao::initTable()
{
    QString sql = "CREATE TABLE IF NOT EXISTS air_tightness_full_params ("
                  "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                  "param_name TEXT NOT NULL, "
                  "program_number INTEGER DEFAULT 1, "
                  "fill_time REAL, "
                  "stabilization_time REAL, "
                  "test_time REAL, "
                  "dump_time REAL, "
                  "pressure_unit INTEGER, "
                  "pressure_max REAL, "
                  "pressure_min REAL, "
                  "pressure_set_fill REAL, "
                  "fill_type INTEGER, "
                  "regulator_pressure REAL, "
                  "leak_unit INTEGER, "
                  "test_reject REAL, "
                  "ref_reject REAL, "
                  "offset REAL, "
                  "std_atm REAL, "
                  "std_temp REAL, "
                  "volume REAL, "
                  "volume_unit INTEGER, "
                  "reject_calc INTEGER, "
                  "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
                  "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)";

    DatabaseManager& dbManager = DatabaseManager::getInstance();
    if (!dbManager.executeQuery(sql)) {
        return false;
    }

    // 检查并添加regulator_pressure列（如果不存在）
    QString alterSql = "ALTER TABLE air_tightness_full_params ADD COLUMN regulator_pressure REAL DEFAULT 0";
    dbManager.executeQuery(alterSql);

    return true;
}

// 保存参数到数据库
bool AirTightnessParamsDao::saveParams(const AirTightnessFullParams& params)
{
    DatabaseManager& dbManager = DatabaseManager::getInstance();

    QStringList columns = {
        "param_name", "program_number", "fill_time", "stabilization_time", "test_time", "dump_time",
        "pressure_unit", "pressure_max", "pressure_min", "pressure_set_fill", "fill_type",
        "regulator_pressure", "leak_unit", "test_reject", "ref_reject", "offset", "std_atm", "std_temp",
        "volume", "volume_unit", "reject_calc", "created_at", "updated_at"
    };

    QVariantList values;
    values << params.paramName
           << params.programNumber
           << params.cycleTime.fillTime
           << params.cycleTime.stabilizationTime
           << params.cycleTime.testTime
           << params.cycleTime.dumpTime
           << params.pressure.unitIndex
           << params.pressure.maximum
           << params.pressure.minimum
           << params.pressure.setFill
           << params.pressure.fillTypeIndex
           << params.pressure.regulatorPressure
           << params.leak.leakUnitIndex
           << params.leak.testReject
           << params.leak.refReject
           << params.leak.offset
           << params.leak.stdAtm
           << params.leak.stdTemp
           << params.leak.volume
           << params.leak.volumeUnitIndex
           << params.leak.rejectCalcIndex
           << params.createdAt.toString("yyyy-MM-dd HH:mm:ss")
           << params.updatedAt.toString("yyyy-MM-dd HH:mm:ss");

    return dbManager.insert("air_tightness_full_params", columns, values);
}

// 更新参数 - 修正参数传递错误
bool AirTightnessParamsDao::updateParams(const AirTightnessFullParams& params)
{
    if (params.id == -1) return false;

    DatabaseManager& dbManager = DatabaseManager::getInstance();

    // 修正：使用QStringList定义更新字段，并确保语法正确
    QStringList columns = {
        "param_name", "program_number", "fill_time", "stabilization_time", "test_time", "dump_time",
        "pressure_unit", "pressure_max", "pressure_min", "pressure_set_fill", "fill_type",
        "regulator_pressure", "leak_unit", "test_reject", "ref_reject", "offset", "std_atm", "std_temp",
        "volume", "volume_unit", "reject_calc", "updated_at"
    };

    // 修正：使用QVariantList传递值，并确保与列数量一致
    QVariantList values;
    values << params.paramName
           << params.programNumber
           << params.cycleTime.fillTime
           << params.cycleTime.stabilizationTime
           << params.cycleTime.testTime
           << params.cycleTime.dumpTime
           << params.pressure.unitIndex
           << params.pressure.maximum
           << params.pressure.minimum
           << params.pressure.setFill
           << params.pressure.fillTypeIndex
           << params.pressure.regulatorPressure
           << params.leak.leakUnitIndex
           << params.leak.testReject
           << params.leak.refReject
           << params.leak.offset
           << params.leak.stdAtm
           << params.leak.stdTemp
           << params.leak.volume
           << params.leak.volumeUnitIndex
           << params.leak.rejectCalcIndex
           << params.updatedAt.toString("yyyy-MM-dd HH:mm:ss");

    // 修正：构造WHERE条件字符串
    QString condition = QString("id = %1").arg(params.id);

    // 修正：调用正确的update方法参数
    return dbManager.update("air_tightness_full_params", columns, values, condition);
}

// 根据ID获取参数
AirTightnessFullParams AirTightnessParamsDao::getParamsById(int id)
{
    AirTightnessFullParams params;
    DatabaseManager& dbManager = DatabaseManager::getInstance();

    QString condition = QString("id = %1").arg(id);
    QSqlQuery query = dbManager.select("air_tightness_full_params", {}, condition, "");

    if (query.next()) {
        params.id = query.value("id").toInt();
        params.paramName = query.value("param_name").toString();
        params.programNumber = query.value("program_number").toInt();

        // 周期时间参数
        params.cycleTime.fillTime = query.value("fill_time").toDouble();
        params.cycleTime.stabilizationTime = query.value("stabilization_time").toDouble();
        params.cycleTime.testTime = query.value("test_time").toDouble();
        params.cycleTime.dumpTime = query.value("dump_time").toDouble();

        // 压力参数
        params.pressure.unitIndex = query.value("pressure_unit").toInt();
        params.pressure.maximum = query.value("pressure_max").toDouble();
        params.pressure.minimum = query.value("pressure_min").toDouble();
        params.pressure.setFill = query.value("pressure_set_fill").toDouble();
        params.pressure.fillTypeIndex = query.value("fill_type").toInt();
        params.pressure.regulatorPressure = query.value("regulator_pressure").toDouble();

        // 泄漏参数
        params.leak.leakUnitIndex = query.value("leak_unit").toInt();
        params.leak.testReject = query.value("test_reject").toDouble();
        params.leak.refReject = query.value("ref_reject").toDouble();
        params.leak.offset = query.value("offset").toDouble();
        params.leak.stdAtm = query.value("std_atm").toDouble();
        params.leak.stdTemp = query.value("std_temp").toDouble();
        params.leak.volume = query.value("volume").toDouble();
        params.leak.volumeUnitIndex = query.value("volume_unit").toInt();
        params.leak.rejectCalcIndex = query.value("reject_calc").toInt();

        // 时间
        params.createdAt = QDateTime::fromString(query.value("created_at").toString(), "yyyy-MM-dd HH:mm:ss");
        params.updatedAt = QDateTime::fromString(query.value("updated_at").toString(), "yyyy-MM-dd HH:mm:ss");
    }

    return params;
}

// 根据程序号获取参数
AirTightnessFullParams AirTightnessParamsDao::getParamsByProgramNumber(int programNumber)
{
    AirTightnessFullParams params;
    DatabaseManager& dbManager = DatabaseManager::getInstance();

    QString condition = QString("program_number = %1").arg(programNumber);
    QSqlQuery query = dbManager.select("air_tightness_full_params", {}, condition, "updated_at DESC LIMIT 1");
    if (query.next()) {
        // 与getParamsById方法相同的读取逻辑
        params.id = query.value("id").toInt();
        params.paramName = query.value("param_name").toString();
        params.programNumber = query.value("program_number").toInt();

        // 周期时间参数
        params.cycleTime.fillTime = query.value("fill_time").toDouble();
        params.cycleTime.stabilizationTime = query.value("stabilization_time").toDouble();
        params.cycleTime.testTime = query.value("test_time").toDouble();
        params.cycleTime.dumpTime = query.value("dump_time").toDouble();

        // 压力参数
        params.pressure.unitIndex = query.value("pressure_unit").toInt();
        params.pressure.maximum = query.value("pressure_max").toDouble();
        params.pressure.minimum = query.value("pressure_min").toDouble();
        params.pressure.setFill = query.value("pressure_set_fill").toDouble();
        params.pressure.fillTypeIndex = query.value("fill_type").toInt();
        params.pressure.regulatorPressure = query.value("regulator_pressure").toDouble();

        // 泄漏参数
        params.leak.leakUnitIndex = query.value("leak_unit").toInt();
        params.leak.testReject = query.value("test_reject").toDouble();
        params.leak.refReject = query.value("ref_reject").toDouble();
        params.leak.offset = query.value("offset").toDouble();
        params.leak.stdAtm = query.value("std_atm").toDouble();
        params.leak.stdTemp = query.value("std_temp").toDouble();
        params.leak.volume = query.value("volume").toDouble();
        params.leak.volumeUnitIndex = query.value("volume_unit").toInt();
        params.leak.rejectCalcIndex = query.value("reject_calc").toInt();

        // 时间
        params.createdAt = QDateTime::fromString(query.value("created_at").toString(), "yyyy-MM-dd HH:mm:ss");
        params.updatedAt = QDateTime::fromString(query.value("updated_at").toString(), "yyyy-MM-dd HH:mm:ss");
    }

    return params;
}

// 获取所有参数组
QList<AirTightnessFullParams> AirTightnessParamsDao::getAllParams()
{
    QList<AirTightnessFullParams> paramsList;
    DatabaseManager& dbManager = DatabaseManager::getInstance();

    // 修正：调用select方法时参数数量正确
    QSqlQuery query = dbManager.select("air_tightness_full_params", {}, "", "created_at DESC");
    while (query.next()) {
        AirTightnessFullParams params;
        // 读取参数值，与getParamsById方法相同
        params.id = query.value("id").toInt();
        params.paramName = query.value("param_name").toString();
        params.programNumber = query.value("program_number").toInt();

        // 周期时间参数
        params.cycleTime.fillTime = query.value("fill_time").toDouble();
        params.cycleTime.stabilizationTime = query.value("stabilization_time").toDouble();
        params.cycleTime.testTime = query.value("test_time").toDouble();
        params.cycleTime.dumpTime = query.value("dump_time").toDouble();

        // 压力参数
        params.pressure.unitIndex = query.value("pressure_unit").toInt();
        params.pressure.maximum = query.value("pressure_max").toDouble();
        params.pressure.minimum = query.value("pressure_min").toDouble();
        params.pressure.setFill = query.value("pressure_set_fill").toDouble();
        params.pressure.fillTypeIndex = query.value("fill_type").toInt();
        params.pressure.regulatorPressure = query.value("regulator_pressure").toDouble();

        // 泄漏参数
        params.leak.leakUnitIndex = query.value("leak_unit").toInt();
        params.leak.testReject = query.value("test_reject").toDouble();
        params.leak.refReject = query.value("ref_reject").toDouble();
        params.leak.offset = query.value("offset").toDouble();
        params.leak.stdAtm = query.value("std_atm").toDouble();
        params.leak.stdTemp = query.value("std_temp").toDouble();
        params.leak.volume = query.value("volume").toDouble();
        params.leak.volumeUnitIndex = query.value("volume_unit").toInt();
        params.leak.rejectCalcIndex = query.value("reject_calc").toInt();

        // 时间
        params.createdAt = QDateTime::fromString(query.value("created_at").toString(), "yyyy-MM-dd HH:mm:ss");
        params.updatedAt = QDateTime::fromString(query.value("updated_at").toString(), "yyyy-MM-dd HH:mm:ss");

        paramsList.append(params);
    }

    return paramsList;
}

// 删除参数 - 修正参数传递错误
bool AirTightnessParamsDao::deleteParams(int id)
{
    DatabaseManager& dbManager = DatabaseManager::getInstance();
    // 修正：使用正确的参数类型和数量
    return dbManager.deleteData("air_tightness_full_params", QString("id = %1").arg(id));
}

// 获取最后一次保存的参数
AirTightnessFullParams AirTightnessParamsDao::getLastSavedParams()
{
    AirTightnessFullParams params;
    DatabaseManager& dbManager = DatabaseManager::getInstance();

    // 修正：调用select方法时参数正确
    QSqlQuery query = dbManager.select("air_tightness_full_params", {}, "", "updated_at DESC LIMIT 1");
    if (query.next()) {
        // 与getParamsById方法相同的读取逻辑
        params.id = query.value("id").toInt();
        params.paramName = query.value("param_name").toString();
        params.programNumber = query.value("program_number").toInt();

        // 周期时间参数
        params.cycleTime.fillTime = query.value("fill_time").toDouble();
        params.cycleTime.stabilizationTime = query.value("stabilization_time").toDouble();
        params.cycleTime.testTime = query.value("test_time").toDouble();
        params.cycleTime.dumpTime = query.value("dump_time").toDouble();

        // 压力参数
        params.pressure.unitIndex = query.value("pressure_unit").toInt();
        params.pressure.maximum = query.value("pressure_max").toDouble();
        params.pressure.minimum = query.value("pressure_min").toDouble();
        params.pressure.setFill = query.value("pressure_set_fill").toDouble();
        params.pressure.fillTypeIndex = query.value("fill_type").toInt();
        params.pressure.regulatorPressure = query.value("regulator_pressure").toDouble();

        // 泄漏参数
        params.leak.leakUnitIndex = query.value("leak_unit").toInt();
        params.leak.testReject = query.value("test_reject").toDouble();
        params.leak.refReject = query.value("ref_reject").toDouble();
        params.leak.offset = query.value("offset").toDouble();
        params.leak.stdAtm = query.value("std_atm").toDouble();
        params.leak.stdTemp = query.value("std_temp").toDouble();
        params.leak.volume = query.value("volume").toDouble();
        params.leak.volumeUnitIndex = query.value("volume_unit").toInt();
        params.leak.rejectCalcIndex = query.value("reject_calc").toInt();

        // 时间
        params.createdAt = QDateTime::fromString(query.value("created_at").toString(), "yyyy-MM-dd HH:mm:ss");
        params.updatedAt = QDateTime::fromString(query.value("updated_at").toString(), "yyyy-MM-dd HH:mm:ss");
    }

    return params;
}
