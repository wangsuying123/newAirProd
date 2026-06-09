#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QMutex>
#include <QFile>
#include <QTextStream>
#include <QDateTime>

class Logger {
public:
    enum Level { 
        Debug = 0,    // 调试信息
        Info = 1,     // 一般信息
        Warning = 2,  // 警告
        Error = 3,    // 错误
        Critical = 4  // 严重错误
    };

    // 单例模式
    static Logger& getInstance();
    
    // 禁止拷贝
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // 日志记录接口
    static void log(Level level, const QString &message);
    static void log(const QString &levelStr, const QString &message);
    
    // 便捷接口
    static void debug(const QString &message);
    static void info(const QString &message);
    static void warning(const QString &message);
    static void error(const QString &message);
    static void critical(const QString &message);
    
    // 配置接口
    void setLogPath(const QString &path);
    void setMaxFileSize(int sizeMB);
    void setMaxBackupFiles(int count);
    void setAutoCleanDays(int days);
    void setLogLevel(Level minLevel);
    
    // 获取配置
    QString getLogPath() const { return m_logPath; }
    int getMaxFileSize() const { return m_maxFileSizeMB; }
    int getMaxBackupFiles() const { return m_maxBackupFiles; }
    int getAutoCleanDays() const { return m_autoCleanDays; }
    Level getLogLevel() const { return m_minLogLevel; }
    
    // 日志统计
    struct LogStats {
        qint64 totalLogs;
        qint64 errorCount;
        qint64 warningCount;
        qint64 infoCount;
        qint64 debugCount;
        qint64 currentFileSize;
        int backupFileCount;
    };
    LogStats getStatistics();
    
    // 清理日志
    void cleanOldLogs();
    void cleanAllLogs();

private:
    Logger();
    ~Logger();
    
    // 内部方法
    void writeLog(Level level, const QString &message);
    bool shouldLog(Level level) const;
    QString levelToString(Level level) const;
    QString levelToColoredString(Level level) const;
    void rotateLogFile();
    void ensureLogDirectory();
    QString getCurrentLogFilePath() const;
    QStringList getBackupLogFiles() const;
    
    // 成员变量
    QMutex m_mutex;
    QString m_logPath;
    int m_maxFileSizeMB;
    int m_maxBackupFiles;
    int m_autoCleanDays;
    Level m_minLogLevel;
    QFile* m_currentFile;
    QTextStream* m_stream;
    
    // 统计信息
    qint64 m_totalLogs;
    qint64 m_errorCount;
    qint64 m_warningCount;
    qint64 m_infoCount;
    qint64 m_debugCount;
};

// 便捷宏定义
#define LOG_DEBUG(msg) Logger::debug(msg)
#define LOG_INFO(msg) Logger::info(msg)
#define LOG_WARNING(msg) Logger::warning(msg)
#define LOG_ERROR(msg) Logger::error(msg)
#define LOG_CRITICAL(msg) Logger::critical(msg)

#endif // LOGGER_H
