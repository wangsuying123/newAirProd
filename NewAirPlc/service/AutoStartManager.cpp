#include "AutoStartManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QDebug>

#ifdef Q_OS_WIN
#include <QSettings>
#endif

bool AutoStartManager::setAutoStart(bool enable)
{
#ifdef Q_OS_WIN
    // Windows 系统使用注册表实现开机自启动
    QSettings settings("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 
                       QSettings::NativeFormat);
    
    QString appName = QCoreApplication::applicationName();
    if (appName.isEmpty()) {
        appName = "NewAirPlc";
    }
    
    if (enable) {
        // 获取程序完整路径（使用引号包裹，防止路径中有空格）
        QString appPath = "\"" + QDir::toNativeSeparators(getApplicationPath()) + "\"";
        
        // 写入注册表
        settings.setValue(appName, appPath);
        settings.sync();
        
        qDebug() << "已设置开机自启动:" << appPath;
        
        // 验证是否写入成功
        if (settings.value(appName).toString() == appPath) {
            return true;
        } else {
            qWarning() << "写入注册表失败";
            return false;
        }
    } else {
        // 从注册表中删除
        settings.remove(appName);
        settings.sync();
        
        qDebug() << "已取消开机自启动";
        return true;
    }
#else
    // Linux 系统可以使用 autostart 目录
    // 这里暂不实现，可以根据需要添加
    Q_UNUSED(enable);
    qWarning() << "当前系统不支持自动设置开机自启动";
    return false;
#endif
}

bool AutoStartManager::isAutoStartEnabled()
{
#ifdef Q_OS_WIN
    QSettings settings("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 
                       QSettings::NativeFormat);
    
    QString appName = QCoreApplication::applicationName();
    if (appName.isEmpty()) {
        appName = "NewAirPlc";
    }
    
    QString registryValue = settings.value(appName).toString();
    QString currentPath = "\"" + QDir::toNativeSeparators(getApplicationPath()) + "\"";
    
    // 检查注册表中的路径是否与当前程序路径一致
    bool enabled = (registryValue == currentPath);
    
    qDebug() << "开机自启动状态:" << (enabled ? "已启用" : "未启用");
    qDebug() << "注册表值:" << registryValue;
    qDebug() << "当前路径:" << currentPath;
    
    return enabled;
#else
    return false;
#endif
}

QString AutoStartManager::getApplicationPath()
{
    return QCoreApplication::applicationFilePath();
}

QString AutoStartManager::getRegistryPath()
{
    return "HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
}
