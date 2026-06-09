#include "DatabaseBackupManager.h"
#include "DatabaseManager.h"
#include "SystemSettingDao.h"
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QSettings>
#include <QCoreApplication>
#include <QTextStream>

DatabaseBackupManager::DatabaseBackupManager()
    : m_checkTimer(nullptr)
    , m_backupIntervalDays(1)  // 默认每天备份
    , m_backupMode(Daily)       // 默认每天备份模式
    , m_backupRetention(5)     // 默认保留5个备份
    , m_autoBackupEnabled(false) // 默认禁用自动备份
{
    m_backupDirectory = QCoreApplication::applicationDirPath() + "/backups";

    m_checkTimer = new QTimer(this);
    connect(m_checkTimer, &QTimer::timeout, this, [this]() {
        if (m_autoBackupEnabled) {
            performBackup();
        }
        scheduleNextBackup();
    });
}

DatabaseBackupManager::~DatabaseBackupManager()
{
    if (m_checkTimer) {
        m_checkTimer->stop();
    }
}

DatabaseBackupManager& DatabaseBackupManager::getInstance()
{
    static DatabaseBackupManager instance;
    return instance;
}

void DatabaseBackupManager::initialize()
{
    qDebug() << "初始化数据库备份管理器...";

    // 创建备份目录
    if (!createBackupDirectory()) {
        qWarning() << "无法创建备份目录";
        return;
    }

    // 加载上次备份时间（从本地配置文件）
    loadLastBackupTime();

    // 从数据库同步最新配置（覆盖本地配置）
    syncSettingsFromDatabase();

    // 检查是否需要立即备份
    if (needsBackup()) {
        qDebug() << "检测到需要备份，开始执行备份...";
        performBackup();
    }

    // 调度下次备份
    scheduleNextBackup();

    qDebug() << "数据库备份管理器初始化完成";
    qDebug() << "上次备份时间:" << m_lastBackupTime.toString("yyyy-MM-dd hh:mm:ss");
    qDebug() << "下次备份时间:" << getNextBackupTime().toString("yyyy-MM-dd hh:mm:ss");
}

bool DatabaseBackupManager::backupNow()
{
    qDebug() << "手动触发备份...";
    return performBackup();
}

void DatabaseBackupManager::setBackupInterval(int days)
{
    if (days > 0) {
        m_backupIntervalDays = days;
        qDebug() << "备份间隔已设置为" << days << "天";

        // 保存设置到配置文件
        QSettings settings(QCoreApplication::applicationDirPath() + "/backup_config.ini", QSettings::IniFormat);
        settings.beginGroup("Backup");
        settings.setValue("backupIntervalDays", m_backupIntervalDays);
        settings.setValue("backupMode", static_cast<int>(m_backupMode));
        settings.setValue("autoBackupEnabled", m_autoBackupEnabled);
        settings.endGroup();
    }
}

void DatabaseBackupManager::setBackupMode(BackupMode mode)
{
    m_backupMode = mode;
    switch (mode) {
    case Daily:
        m_backupIntervalDays = 1;
        qDebug() << "备份模式已设置为: 每天";
        break;
    case Weekly:
        m_backupIntervalDays = 7;
        qDebug() << "备份模式已设置为: 每周";
        break;
    case Monthly:
        m_backupIntervalDays = 30;
        qDebug() << "备份模式已设置为: 每月";
        break;
    }

    // 保存设置到配置文件
    QSettings settings(QCoreApplication::applicationDirPath() + "/backup_config.ini", QSettings::IniFormat);
    settings.beginGroup("Backup");
    settings.setValue("backupMode", static_cast<int>(m_backupMode));
    settings.setValue("backupIntervalDays", m_backupIntervalDays);
    settings.setValue("autoBackupEnabled", m_autoBackupEnabled);
    settings.endGroup();

    // 重新调度下次备份
    if (m_autoBackupEnabled) {
        scheduleNextBackup();
    }
}

void DatabaseBackupManager::setBackupPath(const QString& path)
{
    if (!path.isEmpty()) {
        m_backupDirectory = path;
        qDebug() << "备份路径已设置为:" << path;
        
        createBackupDirectory();
    }
}

void DatabaseBackupManager::setBackupRetention(int count)
{
    if (count > 0) {
        m_backupRetention = count;
        qDebug() << "备份保留数量已设置为:" << count;
    }
}

void DatabaseBackupManager::setAutoBackupEnabled(bool enabled)
{
    m_autoBackupEnabled = enabled;
    qDebug() << "自动备份" << (enabled ? "已启用" : "已禁用");

    // 保存设置到配置文件
    QSettings settings(QCoreApplication::applicationDirPath() + "/backup_config.ini", QSettings::IniFormat);
    settings.beginGroup("Backup");
    settings.setValue("autoBackupEnabled", m_autoBackupEnabled);
    settings.setValue("backupIntervalDays", m_backupIntervalDays);
    settings.endGroup();

    // 调度下次备份
    if (enabled) {
        scheduleNextBackup();
    } else {
        m_checkTimer->stop();
    }
}

bool DatabaseBackupManager::isAutoBackupEnabled() const
{
    return m_autoBackupEnabled;
}

QDateTime DatabaseBackupManager::getLastBackupTime() const
{
    return m_lastBackupTime;
}

QDateTime DatabaseBackupManager::getNextBackupTime() const
{
    return calculateNextBackupTime();
}

QDateTime DatabaseBackupManager::calculateNextBackupTime() const
{
    QDateTime now = QDateTime::currentDateTime();
    QDate today = now.date();

    // 计算下一个凌晨0点
    QDate nextBackupDate;
    QTime backupTime(0, 0, 0);

    switch (m_backupMode) {
    case Daily:
        // 每天凌晨0点
        nextBackupDate = today;
        // 如果今天已经过了0点，则为明天
        if (now.time() >= backupTime) {
            nextBackupDate = nextBackupDate.addDays(1);
        }
        break;

    case Weekly:
        // 每周日凌晨0点
        // Qt 中周一到周日对应 1 到 7，周日是 7
        if (today.dayOfWeek() == 7) {
            // 今天是周日
            if (now.time() >= backupTime) {
                // 已过0点，下个周日
                nextBackupDate = today.addDays(7);
            } else {
                // 还没过0点，今天
                nextBackupDate = today;
            }
        } else {
            // 不是周日，计算距离周日的天数
            int daysUntilSunday = 7 - today.dayOfWeek();
            nextBackupDate = today.addDays(daysUntilSunday);
        }
        break;

    case Monthly:
        // 每月1日凌晨0点
        if (today.day() == 1 && now.time() >= backupTime) {
            // 今天是1号但已过0点，等下月1号
            nextBackupDate = today.addMonths(1);
        } else {
            // 还没到本月1号
            nextBackupDate = QDate(today.year(), today.month(), 1);
            if (nextBackupDate <= today) {
                nextBackupDate = nextBackupDate.addMonths(1);
            }
        }
        break;

    default:
        nextBackupDate = today.addDays(1);
        break;
    }

    return QDateTime(nextBackupDate, backupTime);
}

bool DatabaseBackupManager::needsBackup()
{
    // 如果从未备份过，需要备份
    if (!m_lastBackupTime.isValid()) {
        return true;
    }

    // 计算距离上次备份的天数
    qint64 daysSinceLastBackup = m_lastBackupTime.daysTo(QDateTime::currentDateTime());

    return daysSinceLastBackup >= m_backupIntervalDays;
}

void DatabaseBackupManager::syncSettingsFromDatabase()
{
    qDebug() << "从数据库同步备份配置...";

    // 读取自动备份状态
    bool autoBackup = SystemSettingDao::getSetting("backup", "autoBackup", false).toBool();
    if (autoBackup != m_autoBackupEnabled) {
        m_autoBackupEnabled = autoBackup;
        qDebug() << "同步自动备份状态:" << (m_autoBackupEnabled ? "已启用" : "已禁用");
    }

    // 读取备份频率
    QString frequency = SystemSettingDao::getSetting("backup", "backupFrequency", "每天").toString();
    BackupMode newMode = m_backupMode;
    if (frequency == "每周") {
        newMode = Weekly;
    } else if (frequency == "每月") {
        newMode = Monthly;
    } else {
        newMode = Daily;
    }
    if (newMode != m_backupMode) {
        m_backupMode = newMode;
        m_backupIntervalDays = static_cast<int>(m_backupMode);
        QString modeStr = (m_backupMode == Daily) ? "每天" : (m_backupMode == Weekly) ? "每周" : "每月";
        qDebug() << "同步备份频率:" << modeStr;
    }

    // 读取备份路径
    QString backupPath = SystemSettingDao::getSetting("backup", "backupPath", QCoreApplication::applicationDirPath() + "/backups").toString();
    if (backupPath != m_backupDirectory) {
        m_backupDirectory = backupPath;
        qDebug() << "同步备份路径:" << m_backupDirectory;
        createBackupDirectory();
    }

    // 读取备份保留数量
    int retention = SystemSettingDao::getSetting("backup", "backupRetention", 5).toInt();
    if (retention > 0 && retention != m_backupRetention) {
        m_backupRetention = retention;
        qDebug() << "同步备份保留数量:" << m_backupRetention;
    }

    // 将同步后的配置保存到本地配置文件
    QSettings settings(QCoreApplication::applicationDirPath() + "/backup_config.ini", QSettings::IniFormat);
    settings.beginGroup("Backup");
    settings.setValue("autoBackupEnabled", m_autoBackupEnabled);
    settings.setValue("backupMode", static_cast<int>(m_backupMode));
    settings.setValue("backupIntervalDays", m_backupIntervalDays);
    settings.setValue("lastBackupTime", m_lastBackupTime.toString(Qt::ISODate));
    settings.endGroup();

    qDebug() << "数据库备份配置同步完成";
}

void DatabaseBackupManager::scheduleNextBackup()
{
    if (!m_autoBackupEnabled) {
        return;
    }

    QDateTime nextBackup = calculateNextBackupTime();
    qint64 msUntilBackup = QDateTime::currentDateTime().msecsTo(nextBackup);

    if (msUntilBackup <= 0) {
        msUntilBackup = 1000; // 至少等1秒避免死循环
    }

    m_checkTimer->stop();
    m_checkTimer->setSingleShot(true);
    m_checkTimer->start(msUntilBackup);

    qDebug() << "已调度下次备份时间:" << nextBackup.toString("yyyy-MM-dd hh:mm:ss")
             << ", 距离现在:" << msUntilBackup / 1000 << "秒";
}

bool DatabaseBackupManager::performBackup()
{
    emit backupProgress("开始备份数据库...");
    qDebug() << "开始执行数据库备份...";
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString backupFileName = QString("air_tightness_test_results_backup_%1.xls").arg(timestamp);
    QString backupFilePath = m_backupDirectory + "/" + backupFileName;
    
    emit backupProgress("正在备份测试结果数据...");
    if (!backupTable("air_tightness_test_results", backupFilePath)) {
        QString errorMsg = "备份失败：无法导出数据";
        qCritical() << errorMsg;
        emit backupCompleted(false, errorMsg);
        return false;
    }
    
    saveBackupRecord();
    
    QString successMsg = QString("备份成功！文件保存至：%1").arg(backupFilePath);
    qDebug() << successMsg;
    emit backupProgress("备份完成！");
    emit backupCompleted(true, successMsg);
    
    return true;
}

bool DatabaseBackupManager::backupTable(const QString& tableName, const QString& backupFilePath)
{
    DatabaseManager& dbManager = DatabaseManager::getInstance();
    
    QSqlQuery query = dbManager.select(tableName, {}, "", "id ASC");
    
    if (query.lastError().isValid()) {
        qCritical() << "查询表数据失败:" << query.lastError().text();
        return false;
    }
    
    QFile file(backupFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical() << "无法创建备份文件:" << backupFilePath;
        return false;
    }
    
    QTextStream out(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#else
    out.setCodec("UTF-8");
#endif
    
    out << "<html xmlns:o=\"urn:schemas-microsoft-com:office:office\" xmlns:x=\"urn:schemas-microsoft-com:office:excel\" xmlns=\"http://www.w3.org/TR/REC-html40\">\n";
    out << "<head>\n";
    out << "<!--[if gte mso 9]><xml><x:ExcelWorkbook><x:ExcelWorksheets><x:ExcelWorksheet><x:Name>测试结果备份</x:Name><x:WorksheetOptions><x:DisplayGridlines/></x:WorksheetOptions></x:ExcelWorksheet></x:ExcelWorksheets></x:ExcelWorkbook></xml><![endif]-->\n";
    out << "<meta charset=\"utf-8\"/>\n";
    out << "</head>\n";
    out << "<body>\n";
    out << "<table border=\"1\">\n";
    
    QStringList chineseHeaders = {"序号", "创建时间", "程序号", "产品编号", "报警信息", "报警代码", 
                                  "压力值", "压力单位", "泄露值", "泄露单位", "标准大气压", 
                                  "大气压单位", "温度", "温度单位", "质检人"};
    
    out << "<tr>";
    for (const QString& header : chineseHeaders) {
        out << "<th style=\"background-color:#4472C4;color:white;font-weight:bold;\">" << header.toHtmlEscaped() << "</th>";
    }
    out << "</tr>\n";
    
    int rowCount = 0;
    while (query.next()) {
        out << "<tr>";
        for (int i = 0; i < 15; ++i) {
            QString value = query.value(i).toString();
            bool isNumeric = (i == 0 || i == 2 || i == 5 || i == 6 || i == 8 || i == 10 || i == 12);
            if (isNumeric) {
                out << "<td style=\"mso-number-format:\\#\\,\\#\\#0\\.000;\">" << value.toHtmlEscaped() << "</td>";
            } else {
                out << "<td>" << value.toHtmlEscaped() << "</td>";
            }
        }
        out << "</tr>\n";
        rowCount++;
    }
    
    out << "</table>\n";
    out << "</body>\n";
    out << "</html>\n";
    
    file.close();
    
    qDebug() << "成功备份" << rowCount << "条记录到" << backupFilePath;
    return true;
}

bool DatabaseBackupManager::clearTableData(const QString& tableName)
{
    DatabaseManager& dbManager = DatabaseManager::getInstance();
    
    if (!dbManager.deleteData(tableName, "")) {
        qCritical() << "清空表数据失败:" << dbManager.getLastError();
        return false;
    }
    
    QString resetSql = QString("DELETE FROM sqlite_sequence WHERE name='%1'").arg(tableName);
    if (!dbManager.executeQuery(resetSql)) {
        qWarning() << "重置自增ID失败，但不影响主要功能";
    }
    
    qDebug() << "成功清空表" << tableName << "的数据";
    return true;
}

void DatabaseBackupManager::cleanOldBackups()
{
    if (m_backupRetention <= 0) {
        return;
    }
    
    QDir backupDir(m_backupDirectory);
    if (!backupDir.exists()) {
        return;
    }
    
    QStringList filters;
    filters << "air_tightness_test_results_backup_*.xls";
    filters << "air_tightness_test_results_backup_*.csv";
    
    QFileInfoList files = backupDir.entryInfoList(
        filters,
        QDir::Files,
        QDir::Time | QDir::Reversed
    );
    
    if (files.size() <= m_backupRetention) {
        return;
    }
    
    int deletedCount = 0;
    
    for (int i = m_backupRetention; i < files.size(); ++i) {
        const QFileInfo& fileInfo = files[i];
        if (QFile::remove(fileInfo.filePath())) {
            qDebug() << "删除过期备份文件:" << fileInfo.fileName();
            deletedCount++;
        }
    }
    
    if (deletedCount > 0) {
        qDebug() << "已清理" << deletedCount << "个过期备份文件";
    }
}

void DatabaseBackupManager::saveBackupRecord()
{
    // 使用QSettings保存备份记录
    QSettings settings(QCoreApplication::applicationDirPath() + "/backup_config.ini", QSettings::IniFormat);
    settings.beginGroup("Backup");
    settings.setValue("lastBackupTime", QDateTime::currentDateTime().toString(Qt::ISODate));
    settings.setValue("backupIntervalDays", m_backupIntervalDays);
    settings.setValue("backupMode", static_cast<int>(m_backupMode));
    settings.setValue("autoBackupEnabled", m_autoBackupEnabled);
    settings.endGroup();

    m_lastBackupTime = QDateTime::currentDateTime();

    qDebug() << "备份记录已保存";
}

void DatabaseBackupManager::loadLastBackupTime()
{
    // 从QSettings加载上次备份时间
    QSettings settings(QCoreApplication::applicationDirPath() + "/backup_config.ini", QSettings::IniFormat);
    settings.beginGroup("Backup");

    QString lastBackupStr = settings.value("lastBackupTime", "").toString();
    if (!lastBackupStr.isEmpty()) {
        m_lastBackupTime = QDateTime::fromString(lastBackupStr, Qt::ISODate);
    }

    int intervalDays = settings.value("backupIntervalDays", 15).toInt();
    if (intervalDays > 0) {
        m_backupIntervalDays = intervalDays;
    }

    // 加载备份模式
    if (settings.contains("backupMode")) {
        m_backupMode = static_cast<BackupMode>(settings.value("backupMode", 1).toInt());
    }

    // 加载自动备份启用状态
    if (settings.contains("autoBackupEnabled")) {
        m_autoBackupEnabled = settings.value("autoBackupEnabled", false).toBool();
    }

    settings.endGroup();

    QString modeStr;
    switch (m_backupMode) {
    case Daily: modeStr = "每天"; break;
    case Weekly: modeStr = "每周"; break;
    case Monthly: modeStr = "每月"; break;
    default: modeStr = "每天"; break;
    }

    qDebug() << "加载备份配置 - 上次备份时间:" << m_lastBackupTime.toString("yyyy-MM-dd hh:mm:ss")
             << ", 备份模式:" << modeStr
             << ", 备份间隔:" << m_backupIntervalDays << "天"
             << ", 自动备份:" << (m_autoBackupEnabled ? "已启用" : "已禁用");
}

bool DatabaseBackupManager::createBackupDirectory()
{
    QDir dir;
    if (!dir.exists(m_backupDirectory)) {
        if (!dir.mkpath(m_backupDirectory)) {
            qCritical() << "无法创建备份目录:" << m_backupDirectory;
            return false;
        }
        qDebug() << "创建备份目录:" << m_backupDirectory;
    }
    return true;
}
