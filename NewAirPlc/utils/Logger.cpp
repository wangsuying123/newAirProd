#include "Logger.h"
#include "SystemSettingDao.h"
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QDebug>
#include <QCoreApplication>

Logger::Logger()
    : m_logPath(QCoreApplication::applicationDirPath() + "/logs")
    , m_maxFileSizeMB(10)
    , m_maxBackupFiles(10)
    , m_autoCleanDays(30)
    , m_minLogLevel(Debug)
    , m_currentFile(nullptr)
    , m_stream(nullptr)
    , m_totalLogs(0)
    , m_errorCount(0)
    , m_warningCount(0)
    , m_infoCount(0)
    , m_debugCount(0)
{
    ensureLogDirectory();
}

Logger::~Logger()
{
    if (m_stream) {
        delete m_stream;
        m_stream = nullptr;
    }
    if (m_currentFile) {
        if (m_currentFile->isOpen()) {
            m_currentFile->close();
        }
        delete m_currentFile;
        m_currentFile = nullptr;
    }
}

Logger& Logger::getInstance()
{
    static Logger instance;
    return instance;
}

// 静态日志接口
void Logger::log(Level level, const QString &message)
{
    getInstance().writeLog(level, message);
}

void Logger::log(const QString &levelStr, const QString &message)
{
    Level level = Info;
    QString lvl = levelStr.toLower();
    
    if (lvl == "debug") level = Debug;
    else if (lvl == "info" || lvl == "success" || lvl == "ok") level = Info;
    else if (lvl == "warning" || lvl == "warn") level = Warning;
    else if (lvl == "error" || lvl == "ng") level = Error;
    else if (lvl == "critical" || lvl == "fatal") level = Critical;
    
    log(level, message);
}

void Logger::debug(const QString &message)
{
    log(Debug, message);
}

void Logger::info(const QString &message)
{
    log(Info, message);
}

void Logger::warning(const QString &message)
{
    log(Warning, message);
}

void Logger::error(const QString &message)
{
    log(Error, message);
}

void Logger::critical(const QString &message)
{
    log(Critical, message);
}

// 配置接口
void Logger::setLogPath(const QString &path)
{
    if (path.isEmpty() || path == m_logPath) {
        return; // 路径相同或为空，不需要更改
    }
    
    QMutexLocker locker(&m_mutex);
    
    // 1. 先关闭并清理当前文件流
    if (m_stream) {
        m_stream->flush(); // 确保所有数据都写入
        delete m_stream;
        m_stream = nullptr;
    }
    
    // 2. 关闭并清理当前文件
    if (m_currentFile) {
        if (m_currentFile->isOpen()) {
            m_currentFile->flush(); // 确保所有数据都写入
            m_currentFile->close();
        }
        delete m_currentFile;
        m_currentFile = nullptr;
    }
    
    // 3. 更新路径
    m_logPath = path;
    
    // 4. 释放锁
    locker.unlock();
    
    // 5. 创建新的日志目录（不持有锁）
    QDir logDir(path);
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }
    
    // 注意：下次写日志时会自动创建新文件
}

void Logger::setMaxFileSize(int sizeMB)
{
    QMutexLocker locker(&m_mutex);
    m_maxFileSizeMB = sizeMB;
}

void Logger::setMaxBackupFiles(int count)
{
    QMutexLocker locker(&m_mutex);
    m_maxBackupFiles = count;
}

void Logger::setAutoCleanDays(int days)
{
    QMutexLocker locker(&m_mutex);
    m_autoCleanDays = days;
}

void Logger::setLogLevel(Level minLevel)
{
    QMutexLocker locker(&m_mutex);
    m_minLogLevel = minLevel;
}

// 日志统计
Logger::LogStats Logger::getStatistics()
{
    QMutexLocker locker(&m_mutex);
    
    LogStats stats;
    stats.totalLogs = m_totalLogs;
    stats.errorCount = m_errorCount;
    stats.warningCount = m_warningCount;
    stats.infoCount = m_infoCount;
    stats.debugCount = m_debugCount;
    
    // 获取当前日志文件大小（不持有锁）
    QString currentLogFile = getCurrentLogFilePath();
    locker.unlock();
    
    QFileInfo fileInfo(currentLogFile);
    stats.currentFileSize = fileInfo.exists() ? fileInfo.size() : 0;
    
    // 获取备份文件数量
    stats.backupFileCount = getBackupLogFiles().count();
    
    return stats;
}

// 清理日志
void Logger::cleanOldLogs()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_autoCleanDays <= 0) {
        return;
    }
    
    QDir logDir(m_logPath);
    if (!logDir.exists()) {
        return;
    }
    
    QDateTime cutoffTime = QDateTime::currentDateTime().addDays(-m_autoCleanDays);
    QFileInfoList files = logDir.entryInfoList(QStringList() << "*.log", QDir::Files);
    
    int removedCount = 0;
    for (const QFileInfo &fileInfo : files) {
        if (fileInfo.lastModified() < cutoffTime) {
            if (QFile::remove(fileInfo.filePath())) {
                removedCount++;
                // 不使用 qDebug，避免递归
            }
        }
    }
    
    // 不使用 qDebug，避免递归
}

void Logger::cleanAllLogs()
{
    QMutexLocker locker(&m_mutex);
    
    // 关闭当前文件
    if (m_stream) {
        delete m_stream;
        m_stream = nullptr;
    }
    if (m_currentFile) {
        if (m_currentFile->isOpen()) {
            m_currentFile->close();
        }
        delete m_currentFile;
        m_currentFile = nullptr;
    }
    
    // 删除所有日志文件
    QDir logDir(m_logPath);
    if (logDir.exists()) {
        QFileInfoList files = logDir.entryInfoList(QStringList() << "*.log", QDir::Files);
        for (const QFileInfo &fileInfo : files) {
            QFile::remove(fileInfo.filePath());
        }
        // 不使用 qDebug，避免递归
    }
    
    // 重置统计
    m_totalLogs = 0;
    m_errorCount = 0;
    m_warningCount = 0;
    m_infoCount = 0;
    m_debugCount = 0;
}

// 内部方法
void Logger::writeLog(Level level, const QString &message)
{
    QMutexLocker locker(&m_mutex);
    
    // 检查日志级别
    if (!shouldLog(level)) {
        return;
    }
    
    // 确保日志目录存在
    ensureLogDirectory();
    
    // 获取当前日志文件路径
    QString logFilePath = getCurrentLogFilePath();
    
    // 检查是否需要轮转
    QFileInfo fileInfo(logFilePath);
    if (fileInfo.exists() && fileInfo.size() > static_cast<qint64>(m_maxFileSizeMB) * 1024 * 1024) {
        rotateLogFile();
    }
    
    // 打开或创建日志文件
    if (!m_currentFile || m_currentFile->fileName() != logFilePath) {
        if (m_stream) {
            delete m_stream;
            m_stream = nullptr;
        }
        if (m_currentFile) {
            if (m_currentFile->isOpen()) {
                m_currentFile->close();
            }
            delete m_currentFile;
        }
        
        m_currentFile = new QFile(logFilePath);
        if (!m_currentFile->open(QIODevice::Append | QIODevice::Text)) {
            // 不使用 qWarning，避免递归死锁
            return;
        }
        
        m_stream = new QTextStream(m_currentFile);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        m_stream->setEncoding(QStringConverter::Utf8);
#else
        m_stream->setCodec("UTF-8");
#endif
    }
    
    // 写入日志
    QString timestamp = QDateTime::currentDateTime().toString("[yyyy-MM-dd HH:mm:ss.zzz]");
    QString levelStr = QString("[%1]").arg(levelToString(level).toUpper());
    *m_stream << timestamp << " " << levelStr << " " << message << "\n";
    m_stream->flush();
    
    // 不输出到控制台，避免递归死锁
    // qDebug().noquote() << timestamp << levelStr << message;
    
    // 更新统计
    m_totalLogs++;
    switch (level) {
        case Debug: m_debugCount++; break;
        case Info: m_infoCount++; break;
        case Warning: m_warningCount++; break;
        case Error: m_errorCount++; break;
        case Critical: m_errorCount++; break;
    }
    
    // 不在这里清理旧日志，避免在持有锁时再次获取锁导致死锁
    // 旧日志清理应该由用户手动触发或定时任务处理
}

bool Logger::shouldLog(Level level) const
{
    // 只检查日志级别，不访问数据库避免性能问题
    // 数据库配置应该在程序启动时读取并设置到 m_minLogLevel
    return level >= m_minLogLevel;
}

QString Logger::levelToString(Level level) const
{
    switch (level) {
        case Debug: return "DEBUG";
        case Info: return "INFO";
        case Warning: return "WARN";
        case Error: return "ERROR";
        case Critical: return "CRITICAL";
    }
    return "INFO";
}

QString Logger::levelToColoredString(Level level) const
{
    // 用于控制台彩色输出（如果支持）
    switch (level) {
        case Debug: return "\033[36mDEBUG\033[0m";    // 青色
        case Info: return "\033[32mINFO\033[0m";      // 绿色
        case Warning: return "\033[33mWARN\033[0m";   // 黄色
        case Error: return "\033[31mERROR\033[0m";    // 红色
        case Critical: return "\033[35mCRITICAL\033[0m"; // 紫色
    }
    return "INFO";
}

void Logger::rotateLogFile()
{
    // 关闭当前文件
    if (m_stream) {
        delete m_stream;
        m_stream = nullptr;
    }
    if (m_currentFile) {
        if (m_currentFile->isOpen()) {
            m_currentFile->close();
        }
        delete m_currentFile;
        m_currentFile = nullptr;
    }
    
    // 生成备份文件名
    QString currentLogFile = getCurrentLogFilePath();
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString backupFileName = QString("NewAirPlc_%1_%2.log")
        .arg(QDate::currentDate().toString("yyyy-MM-dd"))
        .arg(timestamp);
    QString backupFilePath = m_logPath + "/" + backupFileName;
    
    // 重命名当前文件
    if (QFile::exists(currentLogFile)) {
        QFile::rename(currentLogFile, backupFilePath);
        // 不使用 qDebug，避免递归死锁
    }
    
    // 清理超过数量限制的备份文件
    QStringList backupFiles = getBackupLogFiles();
    if (backupFiles.count() > m_maxBackupFiles) {
        // 按时间排序，删除最旧的文件
        QDir logDir(m_logPath);
        QFileInfoList fileInfoList;
        for (const QString &fileName : backupFiles) {
            fileInfoList.append(QFileInfo(logDir.filePath(fileName)));
        }
        
        // 按修改时间排序
        std::sort(fileInfoList.begin(), fileInfoList.end(), 
                  [](const QFileInfo &a, const QFileInfo &b) {
            return a.lastModified() < b.lastModified();
        });
        
        // 删除最旧的文件
        int toRemove = fileInfoList.count() - m_maxBackupFiles;
        for (int i = 0; i < toRemove; ++i) {
            QFile::remove(fileInfoList[i].filePath());
            // 不使用 qDebug，避免递归死锁
        }
    }
}

void Logger::ensureLogDirectory()
{
    // 注意：此函数在持有锁的情况下被调用，不要使用 qDebug/qWarning 避免递归死锁
    QDir logDir(m_logPath);
    if (!logDir.exists()) {
        // 使用完整路径创建目录，确保日志目录能正确创建
        logDir.mkpath(m_logPath);
    }
}

QString Logger::getCurrentLogFilePath() const
{
    QString dateStr = QDate::currentDate().toString("yyyy-MM-dd");
    return QString("%1/NewAirPlc_%2.log").arg(m_logPath).arg(dateStr);
}

QStringList Logger::getBackupLogFiles() const
{
    QDir logDir(m_logPath);
    if (!logDir.exists()) {
        return QStringList();
    }
    
    // 获取所有备份文件（包含时间戳的文件）
    QStringList filters;
    filters << "NewAirPlc_*_*.log";
    return logDir.entryList(filters, QDir::Files, QDir::Time);
}
