#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QMenu>
#include <QAction>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include "TcpServerController.h"
#include "User.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class ConnectWidget;
class ParamSetting;
class RealtimeMonitor;
class SystemSetting;
class PlcMonitor;
class DebugPage;
class AirtightTestResult;
class UserManagement;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    
    // 设置当前登录用户
    void setCurrentUser(const User &user);

private slots:
    void openConnectWindow();
    void openParamSettingWindow();
    void openRealtimeMonitorWindow();
    void openSystemSettingWindow();
    void openPlcMonitorWindow();
    void openDebugPageWindow();
    void openAirtightTestResultWindow();
    void openUserManagementWindow();
    void onLogoutClicked();

private:
    Ui::MainWindow *ui;
    ConnectWidget *connectPage;
    ParamSetting *paramSettingPage;
    RealtimeMonitor *realtimeMonitorPage;
    SystemSetting *systemSettingPage;
    PlcMonitor *plcMonitorPage;
    DebugPage *debugPage;
    AirtightTestResult *airtightTestResultPage;
    UserManagement *userManagementPage;
    
    User m_currentUser; // 当前登录用户

    TcpServerController *m_tcpServerController;

    // 菜单和动作成员变量（保留用于权限控制）
    QMenu *menuCommunication;
    QMenu *menuAirTightnessManagement;
    QMenu *menuSystem;
    QMenu *menuPlcManagement;
    QAction *actionParamSettingManagement;
    QAction *actionRealtimeMonitor;
    QAction *actionSystemSetting;
    QAction *actionPlcMonitor;
    QAction *actionDebugPage;
    QAction *actionAirtightTestResult;
    QAction *actionUserManagement;
    QAction *actionLogout;

    void initDatabase();
    void initAutoStart();
    void initUI();
    void initConnections();
    void createSidebar();           // 创建纵向侧边栏
    void setSidebarButtonActive(QPushButton *btn); // 设置当前激活按钮
    void ensureInterPageConnections();
    void updateMenuPermissions();

    // 侧边栏按钮
    QPushButton *btnConnect;
    QPushButton *btnParamSetting;
    QPushButton *btnRealtimeMonitor;
    QPushButton *btnTestResult;
    QPushButton *btnPlcMonitor;
    QPushButton *btnDebug;
    QPushButton *btnUserManagement;
    QPushButton *btnSystemSetting;
    QPushButton *btnLogout;
    QPushButton *m_activeSidebarBtn; // 当前激活的按钮
    QLabel *m_userLabel;             // 侧边栏用户信息标签
};

#endif // MAINWINDOW_H
