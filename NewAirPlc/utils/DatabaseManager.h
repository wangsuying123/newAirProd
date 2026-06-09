#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QVariantList>
#include <QObject>

class DatabaseManager : public QObject
{
    Q_OBJECT
public:
    // 单例模式获取实例
    static DatabaseManager& getInstance();

    // 禁止拷贝构造和赋值操作
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    // 连接数据库
    bool connect(const QString& dbName = "default.db");

    // 断开数据库连接
    void disconnect();

    // 检查数据库是否已连接
    bool isConnected() const;

    // 执行SQL语句（用于创建表等）
    bool executeQuery(const QString& sql);

    // 插入数据
    bool insert(const QString& tableName, const QStringList& columns, const QVariantList& values);

    // 批量插入数据
    bool batchInsert(const QString& tableName, const QStringList& columns, const QList<QVariantList>& valuesList);

    // 更新数据
    bool update(const QString& tableName, const QStringList& columns, const QVariantList& values,
                const QString& condition = "");

    // 删除数据
    bool deleteData(const QString& tableName, const QString& condition = "");

    // 查询数据
    QSqlQuery select(const QString& tableName, const QStringList& columns = QStringList(),
                     const QString& condition = "", const QString& orderBy = "");

    // 获取最后一次错误信息
    QString getLastError() const;

signals:
    // 数据库连接状态变化信号
    void connectionStatusChanged(bool connected);

private:
    // 私有构造函数，确保单例模式
    DatabaseManager();
    ~DatabaseManager();

    QSqlDatabase m_db;       // 数据库对象
    QString m_lastError;     // 最后一次错误信息
};

#endif // DATABASEMANAGER_H
