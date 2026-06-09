#ifndef CONNECTIONPARAMSDAO_H
#define CONNECTIONPARAMSDAO_H

#include "ConnectionParameters.h"
#include "DatabaseManager.h"

class ConnectionParamsDao {
public:
    // 初始化参数表
    static bool initTables() {
        // 创建PLC参数表
        QString plcTableSql = "CREATE TABLE IF NOT EXISTS plc_parameters ("
                              "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                              "protocol TEXT NOT NULL, "
                              "serial_port TEXT, "
                              "baud_rate INTEGER, "
                              "data_bits INTEGER, "
                              "parity INTEGER, "
                              "stop_bits INTEGER, "
                              "ip_address TEXT, "
                              "port INTEGER, "
                              "slave_id INTEGER DEFAULT 2, "
                              "auto_connect INTEGER DEFAULT 0, "
                              "last_connect_time TIMESTAMP, "
                              "last_connect_status TEXT, "
                              "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)";

        // 创建气密仪参数表
        QString airTableSql = "CREATE TABLE IF NOT EXISTS air_tightness_parameters ("
                              "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                              "connection_type TEXT NOT NULL, "
                              "serial_port TEXT, "
                              "baud_rate INTEGER, "
                              "data_bits INTEGER, "
                              "parity INTEGER, "
                              "stop_bits INTEGER, "
                              "ip_address TEXT, "
                              "port INTEGER, "
                              "auto_connect INTEGER DEFAULT 0, "
                              "last_connect_time TIMESTAMP, "
                              "last_connect_status TEXT, "
                              "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)";

        DatabaseManager& dbManager = DatabaseManager::getInstance();
        if (!dbManager.executeQuery(plcTableSql)) {
            return false;
        }

        if (!dbManager.executeQuery(airTableSql)) {
            return false;
        }

        // 初始化调压装置参数表
        if (!initPressureTable()) {
            return false;
        }

        // 初始化连接历史表
        if (!initConnectionHistoryTable()) {
            return false;
        }

        // 升级现有表（添加新字段）
        upgradeExistingTables();

        return true;
    }

    // 初始化调压装置参数表
    static bool initPressureTable() {
        QString pressureTableSql = "CREATE TABLE IF NOT EXISTS pressure_device_parameters ("
                                   "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                   "protocol TEXT NOT NULL, "
                                   "serial_port TEXT, "
                                   "baud_rate INTEGER, "
                                   "data_bits INTEGER, "
                                   "parity INTEGER, "
                                   "stop_bits INTEGER, "
                                   "ip_address TEXT, "
                                   "port INTEGER, "
                                   "slave_id INTEGER DEFAULT 1, "
                                   "auto_connect INTEGER DEFAULT 0, "
                                   "last_connect_time TIMESTAMP, "
                                   "last_connect_status TEXT, "
                                   "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)";

        DatabaseManager& dbManager = DatabaseManager::getInstance();
        return dbManager.executeQuery(pressureTableSql);
    }

    // 初始化连接历史表
    static bool initConnectionHistoryTable() {
        QString historyTableSql = "CREATE TABLE IF NOT EXISTS connection_history ("
                                  "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                  "device_type TEXT NOT NULL, "
                                  "connect_time TIMESTAMP NOT NULL, "
                                  "disconnect_time TIMESTAMP, "
                                  "status TEXT NOT NULL, "
                                  "error_message TEXT, "
                                  "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)";

        DatabaseManager& dbManager = DatabaseManager::getInstance();
        return dbManager.executeQuery(historyTableSql);
    }

    // 升级现有表（添加新字段）
    static void upgradeExistingTables() {
        DatabaseManager& dbManager = DatabaseManager::getInstance();
        
        // 为 plc_parameters 表添加新字段（如果不存在）
        dbManager.executeQuery("ALTER TABLE plc_parameters ADD COLUMN auto_connect INTEGER DEFAULT 0");
        dbManager.executeQuery("ALTER TABLE plc_parameters ADD COLUMN last_connect_time TIMESTAMP");
        dbManager.executeQuery("ALTER TABLE plc_parameters ADD COLUMN last_connect_status TEXT");
        dbManager.executeQuery("ALTER TABLE plc_parameters ADD COLUMN slave_id INTEGER DEFAULT 2");
        
        // 为 air_tightness_parameters 表添加新字段（如果不存在）
        dbManager.executeQuery("ALTER TABLE air_tightness_parameters ADD COLUMN auto_connect INTEGER DEFAULT 0");
        dbManager.executeQuery("ALTER TABLE air_tightness_parameters ADD COLUMN last_connect_time TIMESTAMP");
        dbManager.executeQuery("ALTER TABLE air_tightness_parameters ADD COLUMN last_connect_status TEXT");
    }

    // 保存PLC参数到数据库
    static bool savePLCParams(const PLCConnectionParams& params) {
        DatabaseManager& dbManager = DatabaseManager::getInstance();

        // 先删除现有记录
        if (!dbManager.deleteData("plc_parameters", "")) {
            return false;
        }

        // 插入新记录
        QStringList columns = {
            "protocol", "serial_port", "baud_rate", "data_bits",
            "parity", "stop_bits", "ip_address", "port", "slave_id",
            "auto_connect", "last_connect_time", "last_connect_status"
        };

        QVariantList values;
        values << params.protocol
               << params.serialPort
               << params.baudRate
               << params.dataBits
               << static_cast<int>(params.parity)
               << static_cast<int>(params.stopBits)
               << params.ipAddress
               << params.port
               << params.slaveId
               << (params.autoConnect ? 1 : 0)
               << params.lastConnectTime
               << params.lastConnectStatus;

        return dbManager.insert("plc_parameters", columns, values);
    }

    // 从数据库加载PLC参数
    static PLCConnectionParams loadPLCParams() {
        PLCConnectionParams params;
        DatabaseManager& dbManager = DatabaseManager::getInstance();

        QSqlQuery query = dbManager.select("plc_parameters", {}, "", "id DESC LIMIT 1");
        if (query.next()) {
            params.protocol = query.value("protocol").toString();
            params.serialPort = query.value("serial_port").toString();
            params.baudRate = query.value("baud_rate").toInt();
            params.dataBits = query.value("data_bits").toInt();
            params.parity = static_cast<QSerialPort::Parity>(query.value("parity").toInt());
            params.stopBits = static_cast<QSerialPort::StopBits>(query.value("stop_bits").toInt());
            params.ipAddress = query.value("ip_address").toString();
            params.port = query.value("port").toInt();
            params.slaveId = query.value("slave_id").toInt();
            params.autoConnect = query.value("auto_connect").toInt() == 1;
            params.lastConnectTime = query.value("last_connect_time").toString();
            params.lastConnectStatus = query.value("last_connect_status").toString();
        }

        return params;
    }

    // 保存气密仪参数到数据库
    static bool saveAirTightnessParams(const AirTightnessParams& params) {
        DatabaseManager& dbManager = DatabaseManager::getInstance();

        // 先删除现有记录
        if (!dbManager.deleteData("air_tightness_parameters", "")) {
            return false;
        }

        // 插入新记录
        QStringList columns = {
            "connection_type", "serial_port", "baud_rate", "data_bits",
            "parity", "stop_bits", "ip_address", "port",
            "auto_connect", "last_connect_time", "last_connect_status"
        };

        QVariantList values;
        values << params.connectionType
               << params.serialPort
               << params.baudRate
               << params.dataBits
               << static_cast<int>(params.parity)
               << static_cast<int>(params.stopBits)
               << params.ipAddress
               << params.port
               << (params.autoConnect ? 1 : 0)
               << params.lastConnectTime
               << params.lastConnectStatus;

        return dbManager.insert("air_tightness_parameters", columns, values);
    }

    // 从数据库加载气密仪参数
    static AirTightnessParams loadAirTightnessParams() {
        AirTightnessParams params;
        DatabaseManager& dbManager = DatabaseManager::getInstance();

        QSqlQuery query = dbManager.select("air_tightness_parameters", {}, "", "id DESC LIMIT 1");
        if (query.next()) {
            params.connectionType = query.value("connection_type").toString();
            params.serialPort = query.value("serial_port").toString();
            params.baudRate = query.value("baud_rate").toInt();
            params.dataBits = query.value("data_bits").toInt();
            params.parity = static_cast<QSerialPort::Parity>(query.value("parity").toInt());
            params.stopBits = static_cast<QSerialPort::StopBits>(query.value("stop_bits").toInt());
            params.ipAddress = query.value("ip_address").toString();
            params.port = query.value("port").toInt();
            params.autoConnect = query.value("auto_connect").toInt() == 1;
            params.lastConnectTime = query.value("last_connect_time").toString();
            params.lastConnectStatus = query.value("last_connect_status").toString();
        }

        return params;
    }

    // 保存调压装置参数到数据库
    static bool savePressureParams(const PressureDeviceParams& params) {
        DatabaseManager& dbManager = DatabaseManager::getInstance();

        // 先删除现有记录
        if (!dbManager.deleteData("pressure_device_parameters", "")) {
            return false;
        }

        // 插入新记录
        QStringList columns = {
            "protocol", "serial_port", "baud_rate", "data_bits",
            "parity", "stop_bits", "ip_address", "port", "slave_id",
            "auto_connect", "last_connect_time", "last_connect_status"
        };

        QVariantList values;
        values << params.protocol
               << params.serialPort
               << params.baudRate
               << params.dataBits
               << static_cast<int>(params.parity)
               << static_cast<int>(params.stopBits)
               << params.ipAddress
               << params.port
               << params.slaveId
               << (params.autoConnect ? 1 : 0)
               << params.lastConnectTime
               << params.lastConnectStatus;

        return dbManager.insert("pressure_device_parameters", columns, values);
    }

    // 从数据库加载调压装置参数
    static PressureDeviceParams loadPressureParams() {
        PressureDeviceParams params;
        DatabaseManager& dbManager = DatabaseManager::getInstance();

        QSqlQuery query = dbManager.select("pressure_device_parameters", {}, "", "id DESC LIMIT 1");
        if (query.next()) {
            params.protocol = query.value("protocol").toString();
            params.serialPort = query.value("serial_port").toString();
            params.baudRate = query.value("baud_rate").toInt();
            params.dataBits = query.value("data_bits").toInt();
            params.parity = static_cast<QSerialPort::Parity>(query.value("parity").toInt());
            params.stopBits = static_cast<QSerialPort::StopBits>(query.value("stop_bits").toInt());
            params.ipAddress = query.value("ip_address").toString();
            params.port = query.value("port").toInt();
            params.slaveId = query.value("slave_id").toInt();
            params.autoConnect = query.value("auto_connect").toInt() == 1;
            params.lastConnectTime = query.value("last_connect_time").toString();
            params.lastConnectStatus = query.value("last_connect_status").toString();
        }

        return params;
    }

    // 设置设备的自动连接标志
    static bool setAutoConnect(const QString& deviceType, bool enabled) {
        DatabaseManager& dbManager = DatabaseManager::getInstance();
        QString tableName;
        
        if (deviceType == "PLC") {
            tableName = "plc_parameters";
        } else if (deviceType == "AirTightness") {
            tableName = "air_tightness_parameters";
        } else if (deviceType == "Pressure") {
            tableName = "pressure_device_parameters";
        } else {
            return false;
        }

        QString sql = QString("UPDATE %1 SET auto_connect = %2").arg(tableName).arg(enabled ? 1 : 0);
        return dbManager.executeQuery(sql);
    }

    // 获取设备的自动连接标志
    static bool getAutoConnect(const QString& deviceType) {
        DatabaseManager& dbManager = DatabaseManager::getInstance();
        QString tableName;
        
        if (deviceType == "PLC") {
            tableName = "plc_parameters";
        } else if (deviceType == "AirTightness") {
            tableName = "air_tightness_parameters";
        } else if (deviceType == "Pressure") {
            tableName = "pressure_device_parameters";
        } else {
            return false;
        }

        QSqlQuery query = dbManager.select(tableName, {"auto_connect"}, "", "id DESC LIMIT 1");
        if (query.next()) {
            return query.value("auto_connect").toInt() == 1;
        }
        return false;
    }

    // 更新设备的连接状态和时间戳
    static bool updateConnectionStatus(const QString& deviceType, bool connected, const QString& errorMessage = "") {
        DatabaseManager& dbManager = DatabaseManager::getInstance();
        QString tableName;
        
        if (deviceType == "PLC") {
            tableName = "plc_parameters";
        } else if (deviceType == "AirTightness") {
            tableName = "air_tightness_parameters";
        } else if (deviceType == "Pressure") {
            tableName = "pressure_device_parameters";
        } else {
            return false;
        }

        QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        QString status = connected ? "已连接" : (errorMessage.isEmpty() ? "未连接" : errorMessage);
        QString sql = QString("UPDATE %1 SET last_connect_time = '%2', last_connect_status = '%3'")
                          .arg(tableName)
                          .arg(currentTime)
                          .arg(status);
        
        return dbManager.executeQuery(sql);
    }

    // 保存连接历史记录
    static bool saveConnectionHistory(const QString& deviceType, const QString& status, const QString& errorMessage = "") {
        DatabaseManager& dbManager = DatabaseManager::getInstance();
        
        QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        
        QStringList columns = {"device_type", "connect_time", "status", "error_message"};
        QVariantList values;
        values << deviceType << currentTime << status << errorMessage;
        
        // 保存新记录
        if (!dbManager.insert("connection_history", columns, values)) {
            return false;
        }
        
        // 限制记录数量为10条
        QString condition = QString("device_type = '%1'").arg(deviceType);
        QSqlQuery countQuery = dbManager.select("connection_history", {"COUNT(*) as count"}, condition);
        if (countQuery.next() && countQuery.value("count").toInt() > 10) {
            // 删除最旧的记录
            QString deleteSql = QString("DELETE FROM connection_history WHERE id IN "
                                       "(SELECT id FROM connection_history WHERE device_type = '%1' "
                                       "ORDER BY created_at ASC LIMIT %2)")
                                   .arg(deviceType)
                                   .arg(countQuery.value("count").toInt() - 10);
            dbManager.executeQuery(deleteSql);
        }
        
        return true;
    }

    // 获取设备的连接历史记录
    static QList<ConnectionHistory> getConnectionHistory(const QString& deviceType, int limit = 10) {
        QList<ConnectionHistory> historyList;
        DatabaseManager& dbManager = DatabaseManager::getInstance();
        
        QString condition = QString("device_type = '%1'").arg(deviceType);
        QString orderBy = QString("created_at DESC LIMIT %1").arg(limit);
        
        QSqlQuery query = dbManager.select("connection_history", {}, condition, orderBy);
        while (query.next()) {
            ConnectionHistory history;
            history.id = query.value("id").toInt();
            history.deviceType = query.value("device_type").toString();
            history.connectTime = query.value("connect_time").toString();
            history.disconnectTime = query.value("disconnect_time").toString();
            history.status = query.value("status").toString();
            history.errorMessage = query.value("error_message").toString();
            historyList.append(history);
        }
        
        return historyList;
    }
};

#endif // CONNECTIONPARAMSDAO_H
