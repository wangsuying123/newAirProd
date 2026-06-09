#ifndef SYSTEMSETTING_H
#define SYSTEMSETTING_H

#include <QWidget>

class QModbusClient;
class QPushButton;

namespace Ui {
class SystemSetting;
}

class SystemSetting : public QWidget
{
    Q_OBJECT

public:
    explicit SystemSetting(QWidget *parent = nullptr);
    ~SystemSetting();

    void loadSettings();
    void saveSettings();
    void applySettings();
    void resetSettings();

private slots:
    void on_browseLogPathButton_clicked();
    void on_browseBackupPathButton_clicked();
    void on_saveButton_clicked();
    void on_applyButton_clicked();
    void on_resetButton_clicked();
    void on_autoBackupCheckBox_toggled(bool checked);
    void on_backupNowButton_clicked();
    void on_autoStartCheckBox_toggled(bool checked);
    void on_viewLogButton_clicked();
    void on_openLogFolderButton_clicked();
    void on_cleanOldLogsButton_clicked();
    void on_clearAllLogsButton_clicked();
    void on_clearTestDataButton_clicked();

private:
    Ui::SystemSetting *ui;
    
    void initUI();
    void initConnections();
    void setApplicationStyle(const QString &themeName);
    void setApplicationFontSize(int fontSize);
    void setApplicationLanguage(const QString &language);
    void updateAutoBackupControlsState();
    void updateBackupInfo();
    void updateLogStats();
    void writeLog(const QString &level, const QString &message);
    void addClearTestDataButton();
};

#endif // SYSTEMSETTING_H