#include "mainwindow.h"
#include "loginwindow.h"
#include "ConnectionParamsDao.h"
#include "DatabaseManager.h"
#include "machinelockmanager.h"
#include "Logger.h"
#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QThread>
#include <QTimer>
#include <QCoreApplication>
#include <QDateTime>
#include <atomic>
#include <memory>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

std::atomic<bool> g_appRunning{true};
std::atomic<int> g_watchdogCounter{0};
std::atomic<size_t> g_peakMemoryUsage{0};

size_t getCurrentMemoryUsage() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        return pmc.PrivateUsage;
    }
#endif
    return 0;
}

void memoryMonitorThread() {
    while (g_appRunning.load()) {
        size_t current = getCurrentMemoryUsage();
        size_t peak = g_peakMemoryUsage.load();
        
        if (current > peak) {
            g_peakMemoryUsage.store(current);
        }
        
        QString currentStr = QString::number(current / (1024 * 1024.0), 'f', 2);
        QString peakStr = QString::number(g_peakMemoryUsage.load() / (1024 * 1024.0), 'f', 2);
        
        Logger::log("info", QString("内存监控 - 当前: %1 MB | 峰值: %2 MB").arg(currentStr).arg(peakStr));
        
        if (current > 256 * 1024 * 1024) {
            Logger::log("warning", QString("内存占用超过256MB，当前: %1 MB").arg(currentStr));
        }
        
        QThread::msleep(30000);
    }
}

void watchdogThread() {
    QThread::msleep(5000);
    
    while (g_appRunning.load()) {
        QThread::msleep(10000);
        
        if (g_watchdogCounter.load() == 0) {
            Logger::log("error", "看门狗检测到主线程无响应，程序即将重启");
            g_appRunning.store(false);
            
            QCoreApplication* app = QCoreApplication::instance();
            if (app) {
                app->exit(100);
            }
        }
        
        g_watchdogCounter.store(0);
    }
}

void setupResourceCleanup() {
    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, []() {
        Logger::log("info", "应用程序即将退出，开始释放资源...");
        
        DatabaseManager& dbManager = DatabaseManager::getInstance();
        dbManager.disconnect();
        Logger::log("info", "数据库连接已断开");
        
        g_appRunning.store(false);
        Logger::log("info", "资源释放完成");
    });
}

void setupHeartbeatTimer() {
    QTimer* heartbeatTimer = new QTimer();
    heartbeatTimer->setInterval(3000);
    QObject::connect(heartbeatTimer, &QTimer::timeout, []() {
        g_watchdogCounter.fetch_add(1);
    });
    heartbeatTimer->start();
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QApplication::setApplicationName("气密仪通讯软件");

    // ========== 机器授权验证 ==========
    // const bool debugMode = false;
    // if (!debugMode) {
    //     MachineLockManager lockManager;
    //     if (!lockManager.verifyMachineLicense()) {
    //         QMessageBox::critical(nullptr, "授权验证失败",
    //             "程序未在授权的计算机上运行！\n\n"
    //             "请联系管理员获取授权文件，\n"
    //             "并将 .machine_license 放置到程序目录。");
    //         return 1;
    //     }
    //     qDebug() << "[授权] 机器授权验证通过";
    // } else {
    //     qDebug() << "[授权] 开发模式：跳过机器授权验证";
    // }
    // ====================================

    // 初始化并连接数据库
    DatabaseManager& dbManager = DatabaseManager::getInstance();
    if (!dbManager.connect("airPlc.db")) {
        qCritical() << "数据库连接失败，应用程序可能无法正常工作！";
    } else {
        qDebug() << "数据库连接成功";
    }

    // 设置资源清理
    setupResourceCleanup();

    // 启动看门狗线程
    std::unique_ptr<std::thread> watchdog = std::make_unique<std::thread>(watchdogThread);
    watchdog->detach();
    Logger::log("info", "看门狗线程已启动");

    // 设置心跳定时器
    setupHeartbeatTimer();
    Logger::log("info", "心跳定时器已启动");

    // 启动内存监控线程
    std::unique_ptr<std::thread> memoryMonitor = std::make_unique<std::thread>(memoryMonitorThread);
    memoryMonitor->detach();
    Logger::log("info", "内存监控线程已启动");

    // 显示登录窗口
    LoginWindow loginWindow;
    if (loginWindow.exec() != QDialog::Accepted) {
        qDebug() << "用户取消登录，应用程序退出";
        dbManager.disconnect();
        return 0;
    }
    
    // 登录成功，显示主窗口
    MainWindow w;
    
    w.setCurrentUser(loginWindow.getLoggedInUser());
    
    w.showFullScreen();

    int result = a.exec();
    
    g_appRunning.store(false);
    
    return result;
}
