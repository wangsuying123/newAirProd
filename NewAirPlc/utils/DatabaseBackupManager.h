#ifndef DATABASEBACKUPMANAGER_H
#define DATABASEBACKUPMANAGER_H

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QString>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif

class DatabaseBackupManager : public QObject
{
    Q_OBJECT
public:
    // 备份模式枚举
    enum BackupMode {
        Daily = 1,      // 每天
        Weekly = 7,     // 每周
        Monthly = 30    // 每月
    };

    // 单例模式获取实例
    static DatabaseBackupManager& getInstance();

    // 禁止拷贝构造和赋值操作
    DatabaseBackupManager(const DatabaseBackupManager&) = delete;
    DatabaseBackupManager& operator=(const DatabaseBackupManager&) = delete;

    // 初始化备份管理器（在程序启动时调用）
    void initialize();

    // 手动触发备份
    bool backupNow();

    // 设置备份间隔（天数）
    void setBackupInterval(int days);

    // 设置备份模式（每天/每周/每月）
    void setBackupMode(BackupMode mode);
    BackupMode getBackupMode() const { return m_backupMode; }

    // 设置备份路径
    void setBackupPath(const QString& path);

    // 设置备份保留数量
    void setBackupRetention(int count);

    // 设置自动备份启用状态
    void setAutoBackupEnabled(bool enabled);

    // 获取自动备份启用状态
    bool isAutoBackupEnabled() const;

    // 获取上次备份时间
    QDateTime getLastBackupTime() const;

    // 获取下次备份时间
    QDateTime getNextBackupTime() const;

signals:
    // 备份完成信号
    void backupCompleted(bool success, const QString& message);

    // 备份进度信号
    void backupProgress(const QString& status);

private:
    // 私有构造函数，确保单例模式
    DatabaseBackupManager();
    ~DatabaseBackupManager();

    // 计算下次备份时间点（固定凌晨0点）
    QDateTime calculateNextBackupTime() const;

    // 检查是否需要备份
    bool needsBackup();

    // 执行备份操作
    bool performBackup();

    // 备份指定表
    bool backupTable(const QString& tableName, const QString& backupFilePath);

    // 清空表数据（保留方法但不再使用）
    bool clearTableData(const QString& tableName);

    // 清理过期备份文件
    void cleanOldBackups();

    // 保存备份记录
    void saveBackupRecord();

    // 加载上次备份时间
    void loadLastBackupTime();

    // 创建备份目录
    bool createBackupDirectory();

    // 启动定时器到下一个备份时间点
    void scheduleNextBackup();

    // 从数据库同步配置
    void syncSettingsFromDatabase();

    QTimer* m_checkTimer;           // 定时检查定时器
    QDateTime m_lastBackupTime;     // 上次备份时间
    int m_backupIntervalDays;       // 备份间隔（天数）
    BackupMode m_backupMode;         // 备份模式（每天/每周/每月）
    QString m_backupDirectory;      // 备份目录路径
    int m_backupRetention;          // 备份保留数量
    bool m_autoBackupEnabled;       // 自动备份启用状态
};

#endif // DATABASEBACKUPMANAGER_H
