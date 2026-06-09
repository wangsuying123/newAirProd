#include "DatabaseManager.h"
#include <QDebug>
#include <QSqlDriver>


// 数据库操作：连接、断开、增删查改

DatabaseManager::DatabaseManager()
{
    // 初始化数据库驱动
    if (!QSqlDatabase::isDriverAvailable("QSQLITE")) {
        m_lastError = "SQLite driver not available!";
        qCritical() << m_lastError;
    }
}

DatabaseManager::~DatabaseManager()
{
    // 析构时断开连接
    disconnect();
}

DatabaseManager& DatabaseManager::getInstance()
{
    static DatabaseManager instance;
    return instance;
}

bool DatabaseManager::connect(const QString& dbName)
{
    // 如果已连接则先断开
    if (isConnected()) {
        disconnect();
    }

    // 创建数据库连接
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbName);

    // 打开数据库
    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        qCritical() << "Failed to connect to database:" << m_lastError;
        emit connectionStatusChanged(false);
        return false;
    }

    qDebug() << "Successfully connected to database:" << dbName;
    m_lastError.clear();
    emit connectionStatusChanged(true);
    return true;
}

void DatabaseManager::disconnect()
{
    if (isConnected()) {
        QString connectionName = m_db.connectionName();
        m_db.close();
        m_db = QSqlDatabase(); // 清除数据库对象
        QSqlDatabase::removeDatabase(connectionName);
        qDebug() << "Disconnected from database";
        emit connectionStatusChanged(false);
    }
}

bool DatabaseManager::isConnected() const
{
    return m_db.isOpen();
}

bool DatabaseManager::executeQuery(const QString& sql)
{
    if (!isConnected()) {
        m_lastError = "Not connected to database";
        return false;
    }

    QSqlQuery query;
    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        qCritical() << "Query execution failed:" << m_lastError << "SQL:" << sql;
        return false;
    }

    m_lastError.clear();
    return true;
}

bool DatabaseManager::insert(const QString& tableName, const QStringList& columns, const QVariantList& values)
{
    if (columns.count() != values.count()) {
        m_lastError = "Columns count does not match values count";
        return false;
    }

    // 修复arg()函数调用错误
    QString placeholders;
    for (int i = 0; i < columns.count(); ++i) {
        if (i > 0) {
            placeholders += ", ";
        }
        placeholders += "?";
    }

    QString sql = QString("INSERT INTO %1 (%2) VALUES (%3)")
                      .arg(tableName)
                      .arg(columns.join(", "))
                      .arg(placeholders);

    QSqlQuery query;
    if (!query.prepare(sql)) {
        m_lastError = query.lastError().text();
        qCritical() << "Prepare insert failed:" << m_lastError;
        return false;
    }

    for (int i = 0; i < values.count(); ++i) {
        query.addBindValue(values[i]);
    }

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        qCritical() << "Insert failed:" << m_lastError;
        return false;
    }

    m_lastError.clear();
    return true;
}

bool DatabaseManager::batchInsert(const QString& tableName, const QStringList& columns, const QList<QVariantList>& valuesList)
{
    if (columns.isEmpty() || valuesList.isEmpty()) {
        m_lastError = "Columns or values list is empty";
        return false;
    }

    // 检查所有值列表的长度是否与列数一致
    for (const auto& values : valuesList) {
        if (values.count() != columns.count()) {
            m_lastError = "Columns count does not match values count in batch insert";
            return false;
        }
    }

    // 修复arg()函数调用错误
    QString placeholders;
    for (int i = 0; i < columns.count(); ++i) {
        if (i > 0) {
            placeholders += ", ";
        }
        placeholders += "?";
    }

    QString sql = QString("INSERT INTO %1 (%2) VALUES (%3)")
                      .arg(tableName)
                      .arg(columns.join(", "))
                      .arg(placeholders);

    QSqlQuery query;
    if (!query.prepare(sql)) {
        m_lastError = query.lastError().text();
        qCritical() << "Prepare batch insert failed:" << m_lastError;
        return false;
    }

    // 开启批量操作
    if (m_db.driver()->hasFeature(QSqlDriver::BatchOperations)) {
        for (const auto& values : valuesList) {
            for (int i = 0; i < values.count(); ++i) {
                query.addBindValue(values[i]);
            }
        }

        if (!query.execBatch()) {
            m_lastError = query.lastError().text();
            qCritical() << "Batch insert failed:" << m_lastError;
            return false;
        }
    } else {
        // 如果不支持批量操作，则逐条插入
        for (const auto& values : valuesList) {
            query.clear();
            if (!query.prepare(sql)) {
                m_lastError = query.lastError().text();
                qCritical() << "Prepare insert failed:" << m_lastError;
                return false;
            }

            for (int i = 0; i < values.count(); ++i) {
                query.addBindValue(values[i]);
            }

            if (!query.exec()) {
                m_lastError = query.lastError().text();
                qCritical() << "Insert failed:" << m_lastError;
                return false;
            }
        }
    }

    m_lastError.clear();
    return true;
}

bool DatabaseManager::update(const QString& tableName, const QStringList& columns, const QVariantList& values, const QString& condition)
{
    if (columns.count() != values.count()) {
        m_lastError = "Columns count does not match values count";
        return false;
    }

    QString setClause;
    for (const QString& column : columns) {
        if (!setClause.isEmpty()) {
            setClause += ", ";
        }
        setClause += QString("%1 = ?").arg(column);
    }

    QString sql = QString("UPDATE %1 SET %2").arg(tableName).arg(setClause);
    if (!condition.isEmpty()) {
        sql += " WHERE " + condition;
    }

    QSqlQuery query;
    if (!query.prepare(sql)) {
        m_lastError = query.lastError().text();
        qCritical() << "Prepare update failed:" << m_lastError;
        return false;
    }

    for (int i = 0; i < values.count(); ++i) {
        query.addBindValue(values[i]);
    }

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        qCritical() << "Update failed:" << m_lastError;
        return false;
    }

    m_lastError.clear();
    return true;
}

bool DatabaseManager::deleteData(const QString& tableName, const QString& condition)
{
    QString sql = QString("DELETE FROM %1").arg(tableName);
    if (!condition.isEmpty()) {
        sql += " WHERE " + condition;
    }

    QSqlQuery query;
    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        qCritical() << "Delete failed:" << m_lastError;
        return false;
    }

    m_lastError.clear();
    return true;
}

QSqlQuery DatabaseManager::select(const QString& tableName, const QStringList& columns, const QString& condition, const QString& orderBy)
{
    QString columnsStr = "*";
    if (!columns.isEmpty()) {
        columnsStr = columns.join(", ");
    }

    QString sql = QString("SELECT %1 FROM %2").arg(columnsStr).arg(tableName);
    if (!condition.isEmpty()) {
        sql += " WHERE " + condition;
    }
    if (!orderBy.isEmpty()) {
        sql += " ORDER BY " + orderBy;
    }

    QSqlQuery query;
    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        qCritical() << "Select failed:" << m_lastError;
    } else {
        m_lastError.clear();
    }

    return query;
}

QString DatabaseManager::getLastError() const
{
    return m_lastError;
}
