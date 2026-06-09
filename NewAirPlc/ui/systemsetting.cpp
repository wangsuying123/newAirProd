#include "systemsetting.h"
#include "ui_systemsetting.h"
#include "SystemSettingDao.h"
#include "DatabaseBackupManager.h"
#include "AutoStartManager.h"
#include "Logger.h"
#include "StyledMessageBox.h"
#include "AirtightTestResultDao.h"
#include <QFileDialog>
#include <QFont>
#include <QApplication>
#include <QCoreApplication>
#include <QStyleFactory>
#include <QDebug>
#include <QDir>
#include <QIntValidator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <QPushButton>
#include <QMessageBox>
#include <QVBoxLayout>

SystemSetting::SystemSetting(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SystemSetting)
{
    ui->setupUi(this);
    initUI();
    initConnections();
    
    // 加载已有设置
    loadSettings();
    
    // 监控数据库连接状态变化
    connect(&DatabaseManager::getInstance(), &DatabaseManager::connectionStatusChanged, this, [this](bool connected) {
        qDebug() << "数据库连接状态变化：" << (connected ? "已连接" : "未连接");
        if (connected) {
            // 数据库重新连接后，可以选择性地重新加载设置
            qDebug() << "数据库重新连接，重新加载设置...";
            loadSettings();
        }
    });
}

SystemSetting::~SystemSetting()
{
    delete ui;
}

void SystemSetting::initUI()
{
    QString defaultLogPath = QDir::currentPath() + QDir::separator() + "logs";
    ui->logPathLineEdit->setText(defaultLogPath);
    
    QDir logDir(defaultLogPath);
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }
    
    QString defaultBackupPath = QDir::currentPath() + QDir::separator() + "backups";
    ui->backupPathLineEdit->setText(defaultBackupPath);
    
    QDir backupDir(defaultBackupPath);
    if (!backupDir.exists()) {
        backupDir.mkpath(".");
    }
    
    addClearTestDataButton();
}

void SystemSetting::addClearTestDataButton()
{
    QPushButton* clearButton = new QPushButton("🗑️ 清空测试数据", this);
    clearButton->setObjectName("clearTestDataButton");
    clearButton->setMinimumHeight(40);
    clearButton->setStyleSheet(R"(
        QPushButton#clearTestDataButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, 
                stop:0 #ef4444, 
                stop:1 #dc2626);
            color: white;
            border: none;
            border-radius: 12px;
            font-weight: 700;
            font-size: 14px;
        }
        QPushButton#clearTestDataButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, 
                stop:0 #dc2626, 
                stop:1 #b91c1c);
        }
    )");
    
    connect(clearButton, &QPushButton::clicked, this, &SystemSetting::on_clearTestDataButton_clicked);
    
    QVBoxLayout* backupLayout = ui->backupSettingsGroupBox->findChild<QVBoxLayout*>("backupSettingsLayout");
    if (backupLayout) {
        backupLayout->addWidget(clearButton);
    }
}

void SystemSetting::initConnections()
{
    // 注意：使用 on_<objectName>_<signalName> 命名的槽函数会被 Qt 自动连接
    // 不需要手动 connect，否则会导致信号被连接两次
    
    // 更新备份信息显示
    updateBackupInfo();
    
    // 更新日志统计显示
    updateLogStats();
}

void SystemSetting::loadSettings()
{
    // 检查数据库连接状态
    if (!DatabaseManager::getInstance().isConnected()) {
        qDebug() << "数据库未连接，使用默认设置！";
    }
    
    // 加载语言设置
    QString language = SystemSettingDao::getSetting("display", "language", "中文").toString();
    qDebug() << "加载语言设置：" << language;
    int languageIndex = ui->languageComboBox->findText(language);
    if (languageIndex != -1) {
        ui->languageComboBox->setCurrentIndex(languageIndex);
    }
    
    // 加载主题设置
    QString theme = SystemSettingDao::getSetting("display", "theme", "默认").toString();
    qDebug() << "加载主题设置：" << theme;
    int themeIndex = ui->themeComboBox->findText(theme);
    if (themeIndex != -1) {
        ui->themeComboBox->setCurrentIndex(themeIndex);
    }
    
    // 加载字体大小设置
    int fontSize = SystemSettingDao::getSetting("display", "fontSize", 12).toInt();
    qDebug() << "加载字体大小设置：" << fontSize;
    ui->fontSizeSpinBox->setValue(fontSize);
    
    // 加载开机自启动设置
    bool autoStartEnabled = AutoStartManager::isAutoStartEnabled();
    qDebug() << "加载开机自启动设置：" << autoStartEnabled;
    ui->autoStartCheckBox->setChecked(autoStartEnabled);
    
    // 加载日志设置
    bool logError = SystemSettingDao::getSetting("logging", "logError", true).toBool();
    qDebug() << "加载日志错误级别设置：" << logError;
    ui->logErrorCheckBox->setChecked(logError);
    
    bool logWarning = SystemSettingDao::getSetting("logging", "logWarning", true).toBool();
    qDebug() << "加载日志警告级别设置：" << logWarning;
    ui->logWarningCheckBox->setChecked(logWarning);
    
    bool logInfo = SystemSettingDao::getSetting("logging", "logInfo", true).toBool();
    qDebug() << "加载日志信息级别设置：" << logInfo;
    ui->logInfoCheckBox->setChecked(logInfo);
    
    bool logDebug = SystemSettingDao::getSetting("logging", "logDebug", false).toBool();
    qDebug() << "加载日志调试级别设置：" << logDebug;
    ui->logDebugCheckBox->setChecked(logDebug);
    
    QString logPath = SystemSettingDao::getSetting("logging", "logPath", QDir::currentPath() + QDir::separator() + "logs").toString();
    qDebug() << "加载日志路径设置：" << logPath;
    ui->logPathLineEdit->setText(logPath);
    
    // 加载日志大小和自动清理设置
    int logSize = SystemSettingDao::getSetting("logging", "logSize", 10).toInt();
    qDebug() << "加载日志大小设置：" << logSize;
    ui->logSizeSpinBox->setValue(logSize);
    
    QString logAutoClean = SystemSettingDao::getSetting("logging", "logAutoClean", "不自动清理").toString();
    qDebug() << "加载日志自动清理设置：" << logAutoClean;
    int logAutoCleanIndex = ui->logAutoCleanComboBox->findText(logAutoClean);
    if (logAutoCleanIndex != -1) {
        ui->logAutoCleanComboBox->setCurrentIndex(logAutoCleanIndex);
    }
    
    // 加载备份设置
    bool autoBackup = SystemSettingDao::getSetting("backup", "autoBackup", false).toBool();
    qDebug() << "加载自动备份设置：" << autoBackup;
    ui->autoBackupCheckBox->setChecked(autoBackup);
    
    QString backupFrequency = SystemSettingDao::getSetting("backup", "backupFrequency", "每天").toString();
    qDebug() << "加载备份频率设置：" << backupFrequency;
    int backupFrequencyIndex = ui->backupFrequencyComboBox->findText(backupFrequency);
    if (backupFrequencyIndex != -1) {
        ui->backupFrequencyComboBox->setCurrentIndex(backupFrequencyIndex);
    }
    
    QString backupPath = SystemSettingDao::getSetting("backup", "backupPath", QDir::currentPath() + QDir::separator() + "backups").toString();
    qDebug() << "加载备份路径设置：" << backupPath;
    ui->backupPathLineEdit->setText(backupPath);
    
    int backupRetention = SystemSettingDao::getSetting("backup", "backupRetention", 5).toInt();
    qDebug() << "加载备份保留数量设置：" << backupRetention;
    ui->backupRetentionSpinBox->setValue(backupRetention);
    
    // 加载通知设置
    bool enableNotifications = SystemSettingDao::getSetting("notification", "enableNotifications", true).toBool();
    qDebug() << "加载通知总开关设置：" << enableNotifications;
    ui->enableNotificationsCheckBox->setChecked(enableNotifications);
    
    bool soundNotification = SystemSettingDao::getSetting("notification", "soundNotification", true).toBool();
    qDebug() << "加载声音通知设置：" << soundNotification;
    ui->soundNotificationCheckBox->setChecked(soundNotification);
    
    bool taskCompleteNotification = SystemSettingDao::getSetting("notification", "taskCompleteNotification", true).toBool();
    qDebug() << "加载任务完成通知设置：" << taskCompleteNotification;
    ui->taskCompleteNotificationCheckBox->setChecked(taskCompleteNotification);
    
    bool errorNotification = SystemSettingDao::getSetting("notification", "errorNotification", true).toBool();
    qDebug() << "加载错误通知设置：" << errorNotification;
    ui->errorNotificationCheckBox->setChecked(errorNotification);
    
    bool connectionNotification = SystemSettingDao::getSetting("notification", "connectionNotification", true).toBool();
    qDebug() << "加载连接通知设置：" << connectionNotification;
    ui->connectionNotificationCheckBox->setChecked(connectionNotification);
    
    // 根据设置更新控件状态
    updateAutoBackupControlsState();
}

void SystemSetting::saveSettings()
{
    // 首先检查数据库连接状态
    if (!DatabaseManager::getInstance().isConnected()) {
        qDebug() << "数据库未连接，尝试重新连接...";
        if (!DatabaseManager::getInstance().connect("airPlc.db")) {
            qDebug() << "数据库重新连接失败：" << DatabaseManager::getInstance().getLastError();
            StyledMessageBox::critical(this, tr("错误"), tr("数据库未连接，无法保存设置！"));
            return;
        }
        
        // 确保表已创建
        if (!SystemSettingDao::createTable()) {
            qDebug() << "创建系统设置表失败";
            StyledMessageBox::critical(this, tr("错误"), tr("创建系统设置表失败，无法保存设置！"));
            return;
        }
    }
    
    // 创建设置映射，用于批量保存
    QHash<QString, QVariant> settingsMap;
    
    // 添加显示设置
    settingsMap.insert("display.language", ui->languageComboBox->currentText());
    settingsMap.insert("display.theme", ui->themeComboBox->currentText());
    settingsMap.insert("display.fontSize", ui->fontSizeSpinBox->value());
    
    // 添加日志设置
    settingsMap.insert("logging.logError", ui->logErrorCheckBox->isChecked());
    settingsMap.insert("logging.logWarning", ui->logWarningCheckBox->isChecked());
    settingsMap.insert("logging.logInfo", ui->logInfoCheckBox->isChecked());
    settingsMap.insert("logging.logDebug", ui->logDebugCheckBox->isChecked());
    settingsMap.insert("logging.logPath", ui->logPathLineEdit->text());
    settingsMap.insert("logging.logSize", ui->logSizeSpinBox->value());
    settingsMap.insert("logging.logAutoClean", ui->logAutoCleanComboBox->currentText());
    
    // 添加备份设置
    settingsMap.insert("backup.autoBackup", ui->autoBackupCheckBox->isChecked());
    settingsMap.insert("backup.backupFrequency", ui->backupFrequencyComboBox->currentText());
    settingsMap.insert("backup.backupPath", ui->backupPathLineEdit->text());
    settingsMap.insert("backup.backupRetention", ui->backupRetentionSpinBox->value());
    
    // 添加通知设置
    settingsMap.insert("notification.enableNotifications", ui->enableNotificationsCheckBox->isChecked());
    settingsMap.insert("notification.soundNotification", ui->soundNotificationCheckBox->isChecked());
    settingsMap.insert("notification.taskCompleteNotification", ui->taskCompleteNotificationCheckBox->isChecked());
    settingsMap.insert("notification.errorNotification", ui->errorNotificationCheckBox->isChecked());
    settingsMap.insert("notification.connectionNotification", ui->connectionNotificationCheckBox->isChecked());
    
    // 调试信息：打印要保存的设置
    qDebug() << "准备保存的设置数量：" << settingsMap.size();
    for (auto it = settingsMap.constBegin(); it != settingsMap.constEnd(); ++it) {
        qDebug() << "设置 - 键：" << it.key() << " 值：" << it.value() << " 类型：" << it.value().typeName();
    }
    
    // 批量保存设置到数据库
    bool success = SystemSettingDao::batchSaveSettings(settingsMap);
    
    if (success) {
        qDebug() << "设置保存成功！";

        DatabaseBackupManager& backupManager = DatabaseBackupManager::getInstance();

        backupManager.setAutoBackupEnabled(ui->autoBackupCheckBox->isChecked());

        QString frequency = ui->backupFrequencyComboBox->currentText();
        if (frequency == "每周") {
            backupManager.setBackupMode(DatabaseBackupManager::Weekly);
        } else if (frequency == "每月") {
            backupManager.setBackupMode(DatabaseBackupManager::Monthly);
        } else {
            backupManager.setBackupMode(DatabaseBackupManager::Daily);
        }

        backupManager.setBackupRetention(ui->backupRetentionSpinBox->value());

        backupManager.setBackupPath(ui->backupPathLineEdit->text());

        StyledMessageBox::success(this, tr("成功"), tr("设置已保存到数据库！"));
    } else {
        qDebug() << "设置保存失败！错误信息：" << DatabaseManager::getInstance().getLastError();
        StyledMessageBox::critical(this, tr("错误"), tr("设置保存失败！"));
    }
}

void SystemSetting::applySettings()
{
    // 应用主题
    setApplicationStyle(ui->themeComboBox->currentText());
    
    // 应用字体大小
    setApplicationFontSize(ui->fontSizeSpinBox->value());
    
    // 应用语言
    setApplicationLanguage(ui->languageComboBox->currentText());
    
    // 显示应用成功消息
    StyledMessageBox::success(this, tr("成功"), tr("设置已应用！"));
}

void SystemSetting::resetSettings()
{
    // 重置显示设置
    ui->languageComboBox->setCurrentText("中文");
    ui->themeComboBox->setCurrentText("默认");
    ui->fontSizeSpinBox->setValue(12);
    
    // 重置日志设置
    ui->logErrorCheckBox->setChecked(true);
    ui->logWarningCheckBox->setChecked(true);
    ui->logInfoCheckBox->setChecked(true);
    ui->logDebugCheckBox->setChecked(false);
    ui->logPathLineEdit->setText(QDir::currentPath() + QDir::separator() + "logs");
    ui->logSizeSpinBox->setValue(10);
    ui->logAutoCleanComboBox->setCurrentText("不自动清理");
    
    // 重置备份设置
    ui->autoBackupCheckBox->setChecked(false);
    ui->backupFrequencyComboBox->setCurrentText("每天");
    ui->backupPathLineEdit->setText(QDir::currentPath() + QDir::separator() + "backups");
    ui->backupRetentionSpinBox->setValue(5);
    
    // 重置通知设置
    ui->enableNotificationsCheckBox->setChecked(true);
    ui->soundNotificationCheckBox->setChecked(true);
    ui->taskCompleteNotificationCheckBox->setChecked(true);
    ui->errorNotificationCheckBox->setChecked(true);
    ui->connectionNotificationCheckBox->setChecked(true);
    
    // 更新控件状态
    updateAutoBackupControlsState();
    
    StyledMessageBox::success(this, tr("提示"), tr("设置已重置为默认值！"));
}

// 重复定义移除，保留下方带写日志的版本

void SystemSetting::on_saveButton_clicked()
{
    saveSettings();
    writeLog("info", tr("系统设置已保存"));
}

void SystemSetting::on_applyButton_clicked()
{
    applySettings();
    writeLog("info", tr("系统设置已应用"));
}

void SystemSetting::on_resetButton_clicked()
{
    resetSettings();
    writeLog("warning", tr("系统设置已重置为默认值"));
}

void SystemSetting::on_browseLogPathButton_clicked()
{
    QString currentPath = ui->logPathLineEdit->text();
    
    // 确保当前路径存在，否则使用应用程序目录
    if (currentPath.isEmpty() || !QDir(currentPath).exists()) {
        currentPath = QCoreApplication::applicationDirPath();
    }
    
    QString directory = QFileDialog::getExistingDirectory(
        this,
        tr("选择日志保存路径"),
        currentPath,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    // 只有在用户选择了路径（不是取消）时才更新
    if (!directory.isEmpty()) {
        ui->logPathLineEdit->setText(directory);
        
        // 更新 Logger 的日志路径
        // 注意：不要在这里调用 Logger::info，因为路径正在切换中
        Logger::getInstance().setLogPath(directory);
    }
}

void SystemSetting::on_browseBackupPathButton_clicked()
{
    QString currentPath = ui->backupPathLineEdit->text();
    
    // 确保当前路径存在，否则使用应用程序目录
    if (currentPath.isEmpty() || !QDir(currentPath).exists()) {
        currentPath = QCoreApplication::applicationDirPath();
    }
    
    QString directory = QFileDialog::getExistingDirectory(
        this,
        tr("选择备份保存路径"),
        currentPath,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    // 只有在用户选择了路径（不是取消）时才更新
    if (!directory.isEmpty()) {
        ui->backupPathLineEdit->setText(directory);
        
        // 更新DatabaseBackupManager的备份路径
        DatabaseBackupManager::getInstance().setBackupPath(directory);
    }
}

void SystemSetting::on_autoBackupCheckBox_toggled(bool checked)
{
    Q_UNUSED(checked);
    updateAutoBackupControlsState();
}

void SystemSetting::updateAutoBackupControlsState()
{
    bool autoBackup = ui->autoBackupCheckBox->isChecked();
    ui->backupFrequencyLabel->setEnabled(autoBackup);
    ui->backupFrequencyComboBox->setEnabled(autoBackup);
    ui->backupPathLabel->setEnabled(autoBackup);
    ui->backupPathLineEdit->setEnabled(autoBackup);
    ui->browseBackupPathButton->setEnabled(autoBackup);
    ui->backupRetentionLabel->setEnabled(autoBackup);
    ui->backupRetentionSpinBox->setEnabled(autoBackup);
    ui->backupRetentionUnitLabel->setEnabled(autoBackup);
}

void SystemSetting::setApplicationStyle(const QString &themeName)
{
    if (themeName == "暗色") {
        qApp->setStyle(QStyleFactory::create("Fusion"));
        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
        darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
        darkPalette.setColor(QPalette::ToolTipText, Qt::white);
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::HighlightedText, Qt::black);
        qApp->setPalette(darkPalette);
    } else if (themeName == "亮色") {
        qApp->setStyle(QStyleFactory::create("Fusion"));
        qApp->setPalette(qApp->style()->standardPalette());
    } else {
        // 默认主题
        qApp->setStyle(QStyleFactory::create("Windows"));
        qApp->setPalette(qApp->style()->standardPalette());
    }
}

void SystemSetting::setApplicationFontSize(int fontSize)
{
    QFont font = qApp->font();
    font.setPointSize(fontSize);
    qApp->setFont(font);
}

void SystemSetting::setApplicationLanguage(const QString &language)
{
    // 注意：实际的语言切换需要使用Qt的翻译机制
    // 这里只是一个示例，实际应用中需要加载相应的翻译文件
    qDebug() << "设置语言: " << language;
    
    // 对于简单的演示，我们只显示一个提示
    if (language != "中文" && language != "English") {
        StyledMessageBox::warning(this, tr("提示"), tr("所选语言包未找到，将使用默认语言。"));
    }
}

// 每天保存一个日志文件
void SystemSetting::writeLog(const QString &level, const QString &message)
{
    // 级别过滤
    bool logError = SystemSettingDao::getSetting("logging", "logError", true).toBool();
    bool logWarning = SystemSettingDao::getSetting("logging", "logWarning", true).toBool();
    bool logInfo = SystemSettingDao::getSetting("logging", "logInfo", true).toBool();
    bool logDebug = SystemSettingDao::getSetting("logging", "logDebug", false).toBool();

    QString lvl = level.toLower();
    if ((lvl == "error" && !logError) ||
        (lvl == "warning" && !logWarning) ||
        (lvl == "info" && !logInfo) ||
        (lvl == "debug" && !logDebug)) {
        return;
    }

    // 目录与文件名（每日一个文件）
    QString logDirPath = SystemSettingDao::getSetting("logging", "logPath", QDir::currentPath() + QDir::separator() + "logs").toString();
    QDir logDir(logDirPath);
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }

    QString dateStr = QDate::currentDate().toString("yyyy-MM-dd");
    QString logFilePath = logDir.filePath(QString("NewAirPlc_%1.log").arg(dateStr));

    // 文件大小限制与滚动（同一天内超出则滚动一次）
    int maxSizeMB = SystemSettingDao::getSetting("logging", "logSize", 10).toInt();
    QFileInfo fi(logFilePath);
    if (fi.exists() && fi.size() > static_cast<qint64>(maxSizeMB) * 1024 * 1024) {
        QString rotatedName = logDir.filePath(QString("NewAirPlc_%1_%2.log")
            .arg(dateStr)
            .arg(QDateTime::currentDateTime().toString("HHmmss")));
        QFile::rename(logFilePath, rotatedName);
    }

    // 追加写入
    QFile f(logFilePath);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        QString timeStr = QDateTime::currentDateTime().toString("[yyyy-MM-dd HH:mm:ss] ");
        ts << timeStr << "[" << lvl << "] " << message << "\n";
        f.close();
    }

    // 自动清理：保留最近N天的日志文件
    QString autoClean = SystemSettingDao::getSetting("logging", "logAutoClean", "不自动清理").toString();
    int retainDays = 0;
    if (autoClean.contains("7天")) retainDays = 7;
    else if (autoClean.contains("30天")) retainDays = 30;
    else if (autoClean.contains("90天")) retainDays = 90;
    if (retainDays > 0) {
        QDateTime cutoff = QDateTime::currentDateTime().addDays(-retainDays);
        QFileInfoList files = logDir.entryInfoList(QStringList() << "NewAirPlc_*.log", QDir::Files);
        for (const QFileInfo &fi2 : files) {
            if (fi2.lastModified() < cutoff) {
                QFile::remove(fi2.filePath());
            }
        }
    }
}


// 立即备份按钮点击事件
void SystemSetting::on_backupNowButton_clicked()
{
    qDebug() << "用户点击立即备份按钮";
    
    ui->backupNowButton->setEnabled(false);
    ui->backupNowButton->setText("正在备份...");
    
    DatabaseBackupManager& backupManager = DatabaseBackupManager::getInstance();
    
    backupManager.setBackupPath(ui->backupPathLineEdit->text());
    backupManager.setBackupRetention(ui->backupRetentionSpinBox->value());
    
    backupManager.backupNow();
    
    ui->backupNowButton->setEnabled(true);
    ui->backupNowButton->setText("💾 立即备份数据库");
    
    updateBackupInfo();
}

void SystemSetting::on_clearTestDataButton_clicked()
{
    qDebug() << "用户点击清空测试数据按钮";
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("确认清空数据");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setTextFormat(Qt::PlainText);
    msgBox.setText("警告：此操作将永久删除测试结果表中的所有数据！\n\n建议先进行数据库备份。");
    msgBox.setStyleSheet(R"(
        QMessageBox {
            font-size: 12px;
        }
        QMessageBox QLabel {
            font-size: 12px;
        }
        QMessageBox QPushButton {
            font-size: 12px;
            padding: 6px 12px;
            min-width: 80px;
        }
    )");
    
    QPushButton* btnBackupAndClear = msgBox.addButton("先备份再清空", QMessageBox::ActionRole);
    QPushButton* btnClearDirectly = msgBox.addButton("直接清空", QMessageBox::DestructiveRole);
    Q_UNUSED(btnClearDirectly);
    QPushButton* btnCancel = msgBox.addButton("取消", QMessageBox::RejectRole);
    
    msgBox.exec();
    
    QPushButton* clickedButton = qobject_cast<QPushButton*>(msgBox.clickedButton());
    
    if (clickedButton == btnCancel) {
        return;
    }
    
    if (clickedButton == btnBackupAndClear) {
        qDebug() << "用户选择先备份再清空";
        
        DatabaseBackupManager& backupManager = DatabaseBackupManager::getInstance();
        backupManager.setBackupPath(ui->backupPathLineEdit->text());
        
        bool backupSuccess = backupManager.backupNow();
        if (!backupSuccess) {
            StyledMessageBox::warning(this, "备份失败", "备份操作失败，请检查后重试");
            return;
        }
        
        StyledMessageBox::success(this, "备份成功", "数据备份成功！即将清空数据...");
    }
    
    bool success = AirtightTestResultDao::clearResults();
    
    if (success) {
        StyledMessageBox::success(this, "操作成功", "测试数据已全部清空！");
        qDebug() << "测试数据清空成功";
    } else {
        StyledMessageBox::critical(this, "操作失败", "清空测试数据失败！");
        qWarning() << "清空测试数据失败";
    }
}

// 更新备份信息显示
void SystemSetting::updateBackupInfo()
{
    DatabaseBackupManager& backupManager = DatabaseBackupManager::getInstance();
    QDateTime lastBackupTime = backupManager.getLastBackupTime();
    
    if (lastBackupTime.isValid()) {
        QString timeStr = lastBackupTime.toString("yyyy-MM-dd hh:mm:ss");
        ui->lastBackupTimeLabel->setText("上次备份时间：" + timeStr);
        
        // 计算下次备份时间
        QDateTime nextBackupTime = backupManager.getNextBackupTime();
        QString nextTimeStr = nextBackupTime.toString("yyyy-MM-dd hh:mm:ss");
        ui->lastBackupTimeLabel->setToolTip("下次自动备份时间：" + nextTimeStr);
    } else {
        ui->lastBackupTimeLabel->setText("上次备份时间：从未备份");
        ui->lastBackupTimeLabel->setToolTip("尚未进行过备份");
    }
}


// 开机自启动复选框状态变化
void SystemSetting::on_autoStartCheckBox_toggled(bool checked)
{
    qDebug() << "开机自启动设置变化:" << (checked ? "启用" : "禁用");
    
    // 设置开机自启动
    bool success = AutoStartManager::setAutoStart(checked);
    
    if (success) {
        QString message = checked ? "已启用开机自动启动" : "已取消开机自动启动";
        qDebug() << message;
        
        // 可选：显示提示信息
        // QMessageBox::information(this, "设置成功", message);
    } else {
        QString errorMessage = checked ? "启用开机自动启动失败" : "取消开机自动启动失败";
        qWarning() << errorMessage;
        
        // 恢复复选框状态
        ui->autoStartCheckBox->blockSignals(true);
        ui->autoStartCheckBox->setChecked(!checked);
        ui->autoStartCheckBox->blockSignals(false);
        
        StyledMessageBox::warning(this, "设置失败", 
            errorMessage + "\n请检查是否有足够的权限修改系统设置。");
    }
}


// 查看日志按钮点击
void SystemSetting::on_viewLogButton_clicked()
{
    QString logPath = Logger::getInstance().getLogPath();
    QString currentLogFile = logPath + "/NewAirPlc_" + QDate::currentDate().toString("yyyy-MM-dd") + ".log";
    
    if (!QFile::exists(currentLogFile)) {
        StyledMessageBox::information(this, "查看日志", "当前没有日志文件");
        return;
    }
    
    // 使用系统默认程序打开日志文件
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(currentLogFile))) {
        StyledMessageBox::warning(this, "打开失败", "无法打开日志文件");
    }
}

// 打开日志文件夹按钮点击
void SystemSetting::on_openLogFolderButton_clicked()
{
    QString logPath = Logger::getInstance().getLogPath();
    
    // 确保目录存在
    QDir logDir(logPath);
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }
    
    // 打开文件夹
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(logPath))) {
        StyledMessageBox::warning(this, "打开失败", "无法打开日志文件夹");
    }
}

// 清理旧日志按钮点击
void SystemSetting::on_cleanOldLogsButton_clicked()
{
    int autoCleanDays = Logger::getInstance().getAutoCleanDays();
    
    if (autoCleanDays <= 0) {
        StyledMessageBox::information(this, "清理日志", 
            "当前未设置自动清理天数，无法执行清理操作。\n请在日志设置中配置自动清理选项。");
        return;
    }
    
    QMessageBox::StandardButton reply = StyledMessageBox::question(this, "确认清理", 
        QString("确定要清理 %1 天前的旧日志吗？").arg(autoCleanDays));
    
    if (reply == QMessageBox::Yes) {
        Logger::getInstance().cleanOldLogs();
        updateLogStats();
        StyledMessageBox::success(this, "清理完成", "旧日志已清理完成");
    }
}

// 清空所有日志按钮点击
void SystemSetting::on_clearAllLogsButton_clicked()
{
    QMessageBox::StandardButton reply = StyledMessageBox::question(this, "确认清空", 
        "确定要清空所有日志文件吗？\n此操作不可恢复！");
    
    if (reply == QMessageBox::Yes) {
        Logger::getInstance().cleanAllLogs();
        updateLogStats();
        StyledMessageBox::success(this, "清空完成", "所有日志已清空");
    }
}

// 更新日志统计显示
void SystemSetting::updateLogStats()
{
    Logger::LogStats stats = Logger::getInstance().getStatistics();
    
    QString statsText = QString("📈 日志统计：总计 %1 条 | 错误 %2 | 警告 %3 | 信息 %4 | 调试 %5 | 当前文件 %6 KB | 备份文件 %7 个")
        .arg(stats.totalLogs)
        .arg(stats.errorCount)
        .arg(stats.warningCount)
        .arg(stats.infoCount)
        .arg(stats.debugCount)
        .arg(stats.currentFileSize / 1024)
        .arg(stats.backupFileCount);
    
    ui->logStatsLabel->setText(statsText);
}
