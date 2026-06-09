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
#include <exception>
#include <stdexcept>
#include <thread>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

std::atomic<bool> g_appRunning{true};
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
        try {
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
        } catch (const std::exception& e) {
            Logger::log("error", QString("内存监控线程异常: %1").arg(e.what()));
        } catch (...) {
            Logger::log("error", "内存监控线程发生未知异常");
        }
        
        // 使用标准库的sleep_for替代Qt的msleep，避免在非Qt线程中使用Qt API
        std::this_thread::sleep_for(std::chrono::seconds(30));
    }
}

void setupResourceCleanup() {
    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, []() {
        Logger::log("info", "应用程序即将退出，开始释放资源...");
        
        g_appRunning.store(false);
        
        try {
            DatabaseManager& dbManager = DatabaseManager::getInstance();
            dbManager.disconnect();
            Logger::log("info", "数据库连接已断开");
        } catch (const std::exception& e) {
            Logger::log("error", QString("数据库断开异常: %1").arg(e.what()));
        } catch (...) {
            Logger::log("error", "数据库断开时发生未知异常");
        }
        
        Logger::log("info", "资源释放完成");
    });
}

// 全局异常处理函数
void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString level;
    switch (type) {
    case QtDebugMsg:
        level = "debug";
        break;
    case QtInfoMsg:
        level = "info";
        break;
    case QtWarningMsg:
        level = "warning";
        break;
    case QtCriticalMsg:
        level = "critical";
        break;
    case QtFatalMsg:
        level = "critical";
        break;
    default:
        level = "info";
    }
    
    QString logMsg = QString("[%1] %2").arg(level.toUpper()).arg(msg);
    Logger::log(level, logMsg);
    
    // 如果是致命错误，记录更多信息
    if (type == QtFatalMsg) {
        Logger::log("critical", QString("致命错误位置: %1:%2").arg(context.file).arg(context.line));
        // 确保日志被写入后再退出
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main(int argc, char *argv[])
{
    // 安装自定义消息处理器，捕获Qt的警告、错误等消息
    qInstallMessageHandler(customMessageHandler);
    
    int exitCode = 0;
    
    // 声明线程指针，以便在异常处理中也能清理
    std::unique_ptr<std::thread> memoryMonitor = nullptr;
    
    try {
        QApplication a(argc, argv);

        QApplication::setApplicationName("气密仪通讯软件");

        // 初始化日志系统
        Logger::log("info", "========== 应用程序启动 ==========");
        Logger::log("info", QString("启动时间: %1").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")));

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
            Logger::log("error", "数据库连接失败，应用程序可能无法正常工作！");
            qCritical() << "数据库连接失败，应用程序可能无法正常工作！";
        } else {
            Logger::log("info", "数据库连接成功");
            qDebug() << "数据库连接成功";
        }

        // 设置资源清理
        setupResourceCleanup();

        // 启动内存监控线程（不分离，使用join等待结束）
        memoryMonitor = std::make_unique<std::thread>(memoryMonitorThread);
        Logger::log("info", "内存监控线程已启动");

        // 显示登录窗口
        LoginWindow loginWindow;
        if (loginWindow.exec() != QDialog::Accepted) {
            Logger::log("info", "用户取消登录，应用程序退出");
            qDebug() << "用户取消登录，应用程序退出";
            g_appRunning.store(false);
            if (memoryMonitor && memoryMonitor->joinable()) {
                memoryMonitor->join();
            }
            dbManager.disconnect();
            return 0;
        }
        
        // 登录成功，显示主窗口
        MainWindow w;
        
        w.setCurrentUser(loginWindow.getLoggedInUser());
        
        w.showFullScreen();

        exitCode = a.exec();
        
        // 设置标志让线程退出
        g_appRunning.store(false);
        
        // 等待内存监控线程结束
        if (memoryMonitor && memoryMonitor->joinable()) {
            memoryMonitor->join();
        }
        
        Logger::log("info", QString("========== 应用程序正常退出，退出码: %1 ==========").arg(exitCode));
        
        return exitCode;
        
    } catch (const std::exception& e) {
        // 确保线程正确停止
        g_appRunning.store(false);
        if (memoryMonitor && memoryMonitor->joinable()) {
            memoryMonitor->join();
        }
        
        QString errorMsg = QString("未处理的标准异常: %1").arg(e.what());
        Logger::log("critical", errorMsg);
        qCritical() << errorMsg;
        QMessageBox::critical(nullptr, "程序异常", errorMsg);
        return -1;
    } catch (...) {
        // 确保线程正确停止
        g_appRunning.store(false);
        if (memoryMonitor && memoryMonitor->joinable()) {
            memoryMonitor->join();
        }
        
        QString errorMsg = "未处理的未知异常";
        Logger::log("critical", errorMsg);
        qCritical() << errorMsg;
        QMessageBox::critical(nullptr, "程序异常", errorMsg);
        return -1;
    }
}