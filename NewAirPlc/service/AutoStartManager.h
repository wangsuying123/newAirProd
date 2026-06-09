#ifndef AUTOSTARTMANAGER_H
#define AUTOSTARTMANAGER_H

#include <QString>

class AutoStartManager
{
public:
    // 设置开机自启动
    static bool setAutoStart(bool enable);
    
    // 检查是否已设置开机自启动
    static bool isAutoStartEnabled();
    
    // 获取程序路径
    static QString getApplicationPath();
    
    // 获取注册表键路径
    static QString getRegistryPath();

private:
    AutoStartManager() = default;
};

#endif // AUTOSTARTMANAGER_H
