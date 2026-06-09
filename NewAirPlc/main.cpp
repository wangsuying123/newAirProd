#include "mainwindow.h"
#include "loginwindow.h"
#include "ConnectionParamsDao.h"
#include "DatabaseManager.h"
#include "machinelockmanager.h"
#include <QApplication>
#include <QMessageBox>
#include <QDebug>

int main(int argc, char *argv[])
{

    QApplication a(argc, argv);

    // 确保中文显示正常
    QApplication::setApplicationName("气密仪通讯软件");

    // ========== 机器授权验证 ==========
    // 开发阶段可将 debugMode 设为 true 跳过验证，发布前必须改为 false
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

    // 显示登录窗口
    LoginWindow loginWindow;
    if (loginWindow.exec() != QDialog::Accepted) {
        // 用户取消登录，退出应用程序
        qDebug() << "用户取消登录，应用程序退出";
        dbManager.disconnect();
        return 0;
    }
    
    // 登录成功，显示主窗口
    MainWindow w;
    
    // 将登录用户信息传递给主窗口
    w.setCurrentUser(loginWindow.getLoggedInUser());
    
    w.showFullScreen();

    // 应用程序退出时断开数据库连接
    int result = a.exec();
    dbManager.disconnect();
    
    return result;
}
