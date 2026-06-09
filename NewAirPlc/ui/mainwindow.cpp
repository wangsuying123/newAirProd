#include "mainwindow.h"
#pragma execution_character_set("utf-8")
#include "ui_mainwindow.h"
#include "ConnectionParameters.h"
#include "ConnectionParamsDao.h"
#include "SystemSettingDao.h"
#include "AirTightnessParamsDao.h"
#include "AirtightTestResultDao.h"
#include "DatabaseBackupManager.h"
#include "AutoStartManager.h"
#include "StyledMessageBox.h"
#include "UserDao.h"
#include <QMenu>
#include <QAction>
#include <QWidget>
#include <QPixmap>
#include <QHBoxLayout>
#include <Qt>
#include "connect.h"
#include "paramsetting.h"
#include "realtimemonitor.h"
#include "systemsetting.h"
#include "plcmonitor.h"
#include "debugpage.h"
#include "airtighttestresult.h"
#include "usermanagement.h"
#include "loginwindow.h"
#include "User.h"
#include <QModbusClient>
#include <QTimer>
#include <QApplication>
#include <QModbusDataUnit>
#include <QModbusDevice>
#include <QDebug>
#include <QTimer>
#include <QStackedWidget>
#include <QMessageBox>
#include <QApplication>
#include <QSettings>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , connectPage(nullptr)
    , paramSettingPage(nullptr)
    , realtimeMonitorPage(nullptr)
    , systemSettingPage(nullptr)
    , menuCommunication(nullptr)
    , menuAirTightnessManagement(nullptr)
    , menuSystem(nullptr)
    , actionParamSettingManagement(nullptr)
    , actionRealtimeMonitor(nullptr)
    , actionSystemSetting(nullptr)
    , actionPlcMonitor(nullptr)
    , plcMonitorPage(nullptr)
    , debugPage(nullptr)
    , actionAirtightTestResult(nullptr)
    , airtightTestResultPage(nullptr)
    , userManagementPage(nullptr)
    , m_tcpServerController(new TcpServerController(this))
{
    ui->setupUi(this);
    setWindowTitle(tr("智芯气密仪上位机"));
    // 完全通过代码创建侧边栏导航，不依赖UI菜单
    createSidebar();
    m_tcpServerController->initTcpServer();
    initDatabase();
    initAutoStart();  // 初始化开机自启动
    initUI();
    initConnections();
    // 延迟同步连接状态到实时监控页面（确保所有初始化完成后）
    QTimer::singleShot(100, this, [this]() {
        if (realtimeMonitorPage && connectPage) {
            // 同步气密仪连接状
            QModbusClient* instrumentClient = connectPage->getModbusClient(false);
            if (instrumentClient) {
                bool airtightConnected = (instrumentClient->state() == QModbusDevice::ConnectedState);
                realtimeMonitorPage->updateConnectionStatus(false, airtightConnected);
                qDebug() << "延迟同步气密仪连接状" << airtightConnected;
            }
            // 同步PLC连接状
            QModbusClient* plcClient = connectPage->getModbusClient(true);
            if (plcClient) {
                bool plcConnected = (plcClient->state() == QModbusDevice::ConnectedState);
                realtimeMonitorPage->updateConnectionStatus(true, plcConnected);
                qDebug() << "延迟同步PLC连接状" << plcConnected;
            }
            // 同步调压装置连接状
            QModbusClient* pressureClient = connectPage->getPressureModbusClient();
            if (pressureClient) {
                bool pressureConnected = (pressureClient->state() == QModbusDevice::ConnectedState);
                realtimeMonitorPage->updatePressureConnectionStatus(pressureConnected);
                qDebug() << "延迟同步调压装置连接状" << pressureConnected;
            } else {
                realtimeMonitorPage->updatePressureConnectionStatus(false);
            }
        }
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

// 创建纵向侧边栏导

void MainWindow::createSidebar()
{
    // 隐藏原有菜单栏
    menuBar()->setMaximumHeight(0);
    menuBar()->setVisible(false);

    // 直接使用 UI 文件里已有的 sidebarWidget
    QWidget *sidebar = findChild<QWidget*>("sidebarWidget");
    if (!sidebar) {
        qDebug() << "createSidebar: sidebarWidget not found";
        return;
    }
    QVBoxLayout *sideLayout = qobject_cast<QVBoxLayout*>(sidebar->layout());
    if (!sideLayout) {
        sideLayout = new QVBoxLayout(sidebar);
        sideLayout->setContentsMargins(8, 16, 8, 16);
        sideLayout->setSpacing(4);
    }

    // 清空已有内容（UI文件里只有标题label和spacer）
    QLayoutItem *item;
    while ((item = sideLayout->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    // 标题行：图标 + 文字
    QWidget *titleWidget = new QWidget(sidebar);
    titleWidget->setStyleSheet("background: transparent; border-bottom: 1px solid rgba(255,255,255,0.2); margin-bottom: 8px;");
    QHBoxLayout *titleLayout = new QHBoxLayout(titleWidget);
    titleLayout->setContentsMargins(4, 8, 4, 8);
    titleLayout->setSpacing(8);

    QLabel *iconLabel = new QLabel(titleWidget);
    QPixmap iconPixmap(":/app_icon.ico");
    if (!iconPixmap.isNull()) {
        iconLabel->setPixmap(iconPixmap.scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    iconLabel->setStyleSheet("background: transparent;");
    iconLabel->setAlignment(Qt::AlignCenter);

    QLabel *titleLabel = new QLabel("智芯气密仪", titleWidget);
    titleLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    titleLabel->setStyleSheet("color: #ffffff; font-size: 15px; font-weight: bold; background: transparent;");

    titleLayout->addStretch();
    titleLayout->addWidget(iconLabel);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    sideLayout->addWidget(titleWidget);

    // 按钮样式
    auto makeBtn = [sidebar](const QString &text) -> QPushButton* {
        QPushButton *btn = new QPushButton(text, sidebar);
        btn->setStyleSheet(
            "QPushButton {"
            "  background: transparent;"
            "  color: rgba(255,255,255,0.85);"
            "  border: none;"
            "  border-radius: 8px;"
            "  padding: 12px 8px;"
            "  font-size: 13px;"
            "  font-weight: 500;"
            "  text-align: left;"
            "}"
            "QPushButton:hover {"
            "  background: rgba(255,255,255,0.15);"
            "  color: #ffffff;"
            "}"
            "QPushButton[active=true] {"
            "  background: rgba(255,255,255,0.25);"
            "  color: #ffffff;"
            "  font-weight: 700;"
            "  border-left: 3px solid #93c5fd;"
            "}"
        );
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    };

    btnConnect         = makeBtn("🔌  气密仪通信");
    btnParamSetting    = makeBtn("⚙️  参数设置");
    btnRealtimeMonitor = makeBtn("📊  实时监控");
    btnTestResult      = makeBtn("📋  测试结果");
    btnPlcMonitor      = makeBtn("🖥️  PLC监控");
    btnDebug           = makeBtn("🔍  调试");
    btnUserManagement  = makeBtn("👤  用户管理");
    btnSystemSetting   = makeBtn("⚙️  系统设置");

    sideLayout->addWidget(btnConnect);
    sideLayout->addWidget(btnParamSetting);
    sideLayout->addWidget(btnRealtimeMonitor);
    sideLayout->addWidget(btnTestResult);
    sideLayout->addWidget(btnPlcMonitor);
    sideLayout->addWidget(btnDebug);
    sideLayout->addStretch();

    QString userName = m_currentUser.realName.isEmpty() ? m_currentUser.username : m_currentUser.realName;
    m_userLabel = new QLabel(QString("👤 %1").arg(userName), sidebar);
    m_userLabel->setStyleSheet("color: rgba(255,255,255,0.7); font-size: 12px; padding: 4px 8px; background: transparent;");
    m_userLabel->setAlignment(Qt::AlignCenter);
    sideLayout->addWidget(m_userLabel);
    sideLayout->addWidget(btnUserManagement);
    sideLayout->addWidget(btnSystemSetting);

    btnLogout = makeBtn("🚪  退出登录");
    btnLogout->setStyleSheet(btnLogout->styleSheet() +
        "QPushButton { color: #fca5a5; }"
        "QPushButton:hover { background: rgba(239,68,68,0.3); color: #fecaca; }");
    sideLayout->addWidget(btnLogout);

    // 退出全屏按钮
    QPushButton *btnExitFullscreen = new QPushButton("⛶  退出全屏", sidebar);
    btnExitFullscreen->setStyleSheet(
        "QPushButton {"
        "  background: transparent;"
        "  color: rgba(255,255,255,0.6);"
        "  border: none;"
        "  border-radius: 8px;"
        "  padding: 8px 8px;"
        "  font-size: 12px;"
        "  text-align: left;"
        "}"
        "QPushButton:hover {"
        "  background: rgba(255,255,255,0.1);"
        "  color: rgba(255,255,255,0.9);"
        "}"
    );
    btnExitFullscreen->setCursor(Qt::PointingHandCursor);
    connect(btnExitFullscreen, &QPushButton::clicked, this, [this, btnExitFullscreen]() {
        if (isFullScreen()) {
            showNormal();
            btnExitFullscreen->setText("⛶  全屏显示");
        } else {
            showFullScreen();
            btnExitFullscreen->setText("⛶  退出全屏");
        }
    });
    sideLayout->addWidget(btnExitFullscreen);

    m_activeSidebarBtn = nullptr;

    // 兼容性置空
    menuCommunication = nullptr;
    menuAirTightnessManagement = nullptr;
    menuSystem = nullptr;
    menuPlcManagement = nullptr;
    actionParamSettingManagement = nullptr;
    actionRealtimeMonitor = nullptr;
    actionSystemSetting = nullptr;
    actionPlcMonitor = nullptr;
    actionDebugPage = nullptr;
    actionAirtightTestResult = nullptr;
    actionUserManagement = nullptr;
    actionLogout = nullptr;
}

void MainWindow::setSidebarButtonActive(QPushButton *btn)
{
    if (m_activeSidebarBtn) {
        m_activeSidebarBtn->setProperty("active", false);
        m_activeSidebarBtn->style()->unpolish(m_activeSidebarBtn);
        m_activeSidebarBtn->style()->polish(m_activeSidebarBtn);
    }
    m_activeSidebarBtn = btn;
    if (btn) {
        btn->setProperty("active", true);
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
    }
}

void MainWindow::initDatabase()
{
    // 数据库连接已在main.cpp中完成，这里只负责初始化表结
    if (DatabaseManager::getInstance().isConnected()) {
        if (ConnectionParamsDao::initTables() && AirTightnessParamsDao::initTable()
            && SystemSettingDao::createTable() && AirtightTestResultDao::initTable()) {
            // 初始化用户表
            UserDao userDao;
            if (userDao.initTable()) {
                qDebug() << "用户表初始化成功";
                // 创建默认管理员账
                if (userDao.createDefaultAdmin()) {
                }
            } else {
                qDebug() << "用户表初始化失败";
            }
            // 初始化数据库备份管理
            qDebug() << "初始化数据库备份管理..";
            DatabaseBackupManager& backupManager = DatabaseBackupManager::getInstance();
            // 连接备份完成信号
            connect(&backupManager, &DatabaseBackupManager::backupCompleted, this,
                    [this](bool success, const QString& message) {
                qDebug() << "备份完成信号:" << (success ? "成功" : "失败") << "-" << message;
                if (success) {
                    StyledMessageBox::success(this, "数据库备份", message);
                } else {
                    StyledMessageBox::warning(this, "数据库备份", message);
                }
            });
            // 连接备份进度信号
            connect(&backupManager, &DatabaseBackupManager::backupProgress, this,
                    [](const QString& status) {
                qDebug() << "备份进度:" << status;
            });
            // 初始化备份管理器（会自动检查是否需要备份）
            backupManager.initialize();
        } else {
        }
    } else {
    }
}

void MainWindow::initAutoStart()
{
    qDebug() << "初始化开机自启动设置...";
    // 使用 QSettings 检查是否是首次运行
    QSettings settings(QCoreApplication::applicationDirPath() + "/app_config.ini", QSettings::IniFormat);
    bool isFirstRun = settings.value("General/FirstRun", true).toBool();
    if (isFirstRun) {
        qDebug() << "检测到首次运行，设置默认开机自启动";
        // 设置开机自启动
        bool success = AutoStartManager::setAutoStart(true);
        if (success) {
            qDebug() << "默认开机自启动设置成功";
        } else {
            qWarning() << "默认开机自启动设置失败";
        }
        // 标记为非首次运行
        settings.setValue("General/FirstRun", false);
        settings.sync();
    } else {
        // 非首次运行，检查当前自启动状
        bool autoStartEnabled = AutoStartManager::isAutoStartEnabled();
        qDebug() << "当前开机自启动状态:" << (autoStartEnabled ? "已启用" : "未启用");
    }
}

void MainWindow::initUI()
{
    qDebug() << "开始初始化UI";
    QStackedWidget* stackedWidget = findChild<QStackedWidget*>("pageStack");
    if (!stackedWidget) {
        qDebug() << "错误：找不到pageStack";
        return;
    }
    if (!connectPage) {
        qDebug() << "创建连接页面";
        connectPage = new ConnectWidget(this);
        stackedWidget->addWidget(connectPage);
    }
    if (!paramSettingPage) {
        qDebug() << "创建参数设置页面";
        paramSettingPage = new ParamSetting(this);
        stackedWidget->addWidget(paramSettingPage);
    }
    if (!realtimeMonitorPage) {
        qDebug() << "创建实时监控页面";
        realtimeMonitorPage = new RealtimeMonitor(this);
        stackedWidget->addWidget(realtimeMonitorPage);
    }
    if (!systemSettingPage) {
        qDebug() << "创建系统设置页面";
        systemSettingPage = new SystemSetting(this);
        stackedWidget->addWidget(systemSettingPage);
    }
    // 连接ParamSetting的programNumberSent信号与RealtimeMonitor的handleProgramNumberReceived槽函
    if (paramSettingPage && realtimeMonitorPage) {
        connect(paramSettingPage, &ParamSetting::programNumberSent,
                realtimeMonitorPage, &RealtimeMonitor::handleProgramNumberReceived);
        qDebug() << "已建立ParamSetting到RealtimeMonitor的程序号信号连接";
    }
    // 初始化型号选择页面已移
    if (!plcMonitorPage) {
        qDebug() << "创建PLC监控页面";
        plcMonitorPage = new PlcMonitor(this);
        stackedWidget->addWidget(plcMonitorPage);
    }
    // 创建调试页面（修复未初始化导致无法打开
    if (!debugPage) {
        qDebug() << "创建调试页面";
        debugPage = new DebugPage(this);
        stackedWidget->addWidget(debugPage);
    }
    // 创建气密测试结果页面（修复未初始化导致无法打开
    if (!airtightTestResultPage) {
        qDebug() << "创建气密测试结果页面";
        airtightTestResultPage = new AirtightTestResult(this);
        stackedWidget->addWidget(airtightTestResultPage);
    }
    // 创建用户管理页面
    if (!userManagementPage) {
        qDebug() << "创建用户管理页面";
        userManagementPage = new UserManagement(this);
        stackedWidget->addWidget(userManagementPage);
    }
    // 气密测试结果页面已在前面创建，此处不再重复创
    // 恢复：连接PlcMonitor的instrumentSelectionChanged信号与AirtightTestResult的onInstrumentSelectionChanged槽函
    qDebug() << "准备连接信号: plcMonitorPage =" << (void*)plcMonitorPage << ", airtightTestResultPage =" << (void*)airtightTestResultPage;
    if (plcMonitorPage && airtightTestResultPage) {
        bool connectionResult = connect(plcMonitorPage, &PlcMonitor::instrumentSelectionChanged,
                airtightTestResultPage, &AirtightTestResult::onInstrumentSelectionChanged,
                Qt::UniqueConnection);
        qDebug() << "信号连接结果:" << connectionResult;
        qDebug() << "已建立PlcMonitor到AirtightTestResult的仪器评选号信号连接";
    } else {
        qDebug() << "警告: 无法建立PlcMonitor到AirtightTestResult的信号连接，页面可能未正确初始化";
    }
    // 连接PlcMonitor的instrumentSelectionChanged信号与RealtimeMonitor的handleProgramNumberReceived槽函
    if (plcMonitorPage && realtimeMonitorPage) {
        connect(plcMonitorPage, &PlcMonitor::instrumentSelectionChanged,
                realtimeMonitorPage, &RealtimeMonitor::handleProgramNumberReceived,
                Qt::UniqueConnection);
        qDebug() << "已建立PlcMonitor到RealtimeMonitor的仪器评选号信号连接";
        // 新增：连接RealtimeMonitor的通道结果更新信号到PlcMonitor，用于刷新PLC监控页面的通道结果
        bool channelConnectOk = connect(realtimeMonitorPage, &RealtimeMonitor::updateChannelResult,
                                        plcMonitorPage, &PlcMonitor::clearChannelResults,
                                        Qt::UniqueConnection);
        qDebug() << "已建立RealtimeMonitor到PlcMonitor的通道结果更新信号连接:" << channelConnectOk;
    } else {
        qDebug() << "警告: 无法建立PlcMonitor到RealtimeMonitor的信号连接，页面可能未正确初始化";
    }
    // 新增：连接AirtightTestResult测试结果信号到RealtimeMonitor槽函
    if (airtightTestResultPage && realtimeMonitorPage) {
        bool c1 = connect(airtightTestResultPage, &AirtightTestResult::testResultReady,
                          realtimeMonitorPage, &RealtimeMonitor::handleTestResultReady);
        bool c2 = connect(airtightTestResultPage, &AirtightTestResult::testResultReady2,
                          realtimeMonitorPage, &RealtimeMonitor::handleTestResultReady2);
        bool c3 = connect(airtightTestResultPage, &AirtightTestResult::testResultReady3,
                          realtimeMonitorPage, &RealtimeMonitor::handleTestResultReady3);
        qDebug() << "已建立AirtightTestResult到RealtimeMonitor的测试结果信号连" << c1 << c2 << c3;
    } else {
        qDebug() << "警告: 无法建立AirtightTestResult到RealtimeMonitor的信号连接，页面可能未正确初始化";
    }
    // 新增：连接RealtimeMonitor的产品编号变化信号到AirtightTestResult的setter
    if (realtimeMonitorPage && airtightTestResultPage) {
        bool ok = connect(realtimeMonitorPage, &RealtimeMonitor::productNumberChanged,
                          airtightTestResultPage, &AirtightTestResult::setCurrentProductNumber);
        qDebug() << "已建立RealtimeMonitor到AirtightTestResult的产品编号信号连" << ok;
    }
    if (airtightTestResultPage && realtimeMonitorPage) {
        bool c1 = connect(airtightTestResultPage, &AirtightTestResult::testResultReady,
                          realtimeMonitorPage, &RealtimeMonitor::handleTestResultReady);
        bool c2 = connect(airtightTestResultPage, &AirtightTestResult::testResultReady2,
                          realtimeMonitorPage, &RealtimeMonitor::handleTestResultReady2);
        bool c3 = connect(airtightTestResultPage, &AirtightTestResult::testResultReady3,
                          realtimeMonitorPage, &RealtimeMonitor::handleTestResultReady3);
        qDebug() << "已建立AirtightTestResult到RealtimeMonitor信号连接:" << c1 << c2 << c3;
    }
    // 重新建立PlcMonitor到AirtightTestResult的信号连接，确保连接不会丢失
    qDebug() << "========= 确保页面间信号连=========";
    ensureInterPageConnections();
    // 设置默认显示实时监控页面
    if (stackedWidget && realtimeMonitorPage) {
        stackedWidget->setCurrentWidget(realtimeMonitorPage);
        setWindowTitle(tr("实时监控"));
        qDebug() << "已设置默认页面为实时监控";
    }
}

void MainWindow::initConnections()
{
    // 侧边栏按钮连接
    connect(btnConnect, &QPushButton::clicked, this, [this]() {
        setSidebarButtonActive(btnConnect);
        openConnectWindow();
    });
    connect(btnParamSetting, &QPushButton::clicked, this, [this]() {
        setSidebarButtonActive(btnParamSetting);
        openParamSettingWindow();
    });
    connect(btnRealtimeMonitor, &QPushButton::clicked, this, [this]() {
        setSidebarButtonActive(btnRealtimeMonitor);
        openRealtimeMonitorWindow();
    });
    connect(btnTestResult, &QPushButton::clicked, this, [this]() {
        setSidebarButtonActive(btnTestResult);
        openAirtightTestResultWindow();
    });
    connect(btnPlcMonitor, &QPushButton::clicked, this, [this]() {
        setSidebarButtonActive(btnPlcMonitor);
        openPlcMonitorWindow();
    });
    connect(btnDebug, &QPushButton::clicked, this, [this]() {
        setSidebarButtonActive(btnDebug);
        openDebugPageWindow();
    });
    connect(btnUserManagement, &QPushButton::clicked, this, [this]() {
        setSidebarButtonActive(btnUserManagement);
        openUserManagementWindow();
    });
    connect(btnSystemSetting, &QPushButton::clicked, this, [this]() {
        setSidebarButtonActive(btnSystemSetting);
        openSystemSettingWindow();
    });
    connect(btnLogout, &QPushButton::clicked, this, &MainWindow::onLogoutClicked);
    // 统一确保跨页面信号连接（使用UniqueConnection防重复）
    ensureInterPageConnections();
    // 初始化时立即同步调压装置客户端到 PlcMonitor，不等用户打开页面
    // 否则 startAirtightInstrument 触发m_isPressureConnected 永远false
    if (connectPage && plcMonitorPage) {
        QModbusClient* pressureClient = connectPage->getPressureModbusClient();
        plcMonitorPage->setPressureModbusClient(pressureClient);
        QObject::connect(connectPage, &ConnectWidget::pressureClientUpdated,
                         plcMonitorPage, &PlcMonitor::setPressureModbusClient,
                         Qt::UniqueConnection);
        QObject::connect(connectPage, &ConnectWidget::pressureConnectionChanged,
                         plcMonitorPage, &PlcMonitor::updatePressureConnectionStatus,
                         Qt::UniqueConnection);
    }
}

void MainWindow::openRealtimeMonitorWindow()
{
    QStackedWidget* stackedWidget = findChild<QStackedWidget*>("pageStack");
    if (stackedWidget && realtimeMonitorPage) {
        stackedWidget->setCurrentWidget(realtimeMonitorPage);
        realtimeMonitorPage->show();
        setWindowTitle(tr("实时监控"));
        // 同步连接页面的modbus客户端到RealtimeMonitor页面
        if (connectPage) {
            // 获取气密仪modbus客户端（isPlc=false
            QModbusClient* instrumentClient = connectPage->getModbusClient(false);
            if (instrumentClient) {
                realtimeMonitorPage->setModbusClient(instrumentClient);
                // 同步气密仪从站地址到实时监控页面，避免使用默认1导致读寄存器失败
                realtimeMonitorPage->setAirtightSlaveId(connectPage->getAirtightSlaveId());
                // 确认客户端处于ConnectedState，并同步到实时监控页的连接状
                bool airtightConnected = (instrumentClient->state() == QModbusDevice::ConnectedState);
                realtimeMonitorPage->updateConnectionStatus(false, airtightConnected);
            } else {
                StyledMessageBox::warning(this, "连接提示", "气密仪尚未连接，部分功能可能无法使用)");
            }
            // 获取PLC modbus客户端（isPlc=true
            QModbusClient* plcClient = connectPage->getModbusClient(true);
            if (plcClient) {
                realtimeMonitorPage->setPlcModbusClient(plcClient);
                qDebug() << "已将PLC modbus客户端传递给RealtimeMonitor页面";
            }
            // 同步调压装置连接状态到实时监控页面
            QModbusClient* pressureClient = connectPage->getPressureModbusClient();
            if (pressureClient) {
                bool pressureConnected = (pressureClient->state() == QModbusDevice::ConnectedState);
                realtimeMonitorPage->updatePressureConnectionStatus(pressureConnected);
                qDebug() << "已同步调压装置连接状态到RealtimeMonitor页面";
            } else {
                realtimeMonitorPage->updatePressureConnectionStatus(false);
            }
            // 监听调压装置连接状态变
            QObject::connect(connectPage, &ConnectWidget::pressureConnectionChanged,
                             realtimeMonitorPage, &RealtimeMonitor::updatePressureConnectionStatus,
                             Qt::UniqueConnection);
        } else {
            StyledMessageBox::warning(this, "错误", "连接页面未初始化，无法获取设备连)");
        }
        // 统一确保跨页面信号连
        ensureInterPageConnections();
    }
}

void MainWindow::openSystemSettingWindow()
{
    // 检查权
    if (!m_currentUser.isAdmin()) {
        StyledMessageBox::warning(this, "权限不足", "只有管理员才能访问系统设置页面！");
        return;
    }
    QStackedWidget* stackedWidget = findChild<QStackedWidget*>("pageStack");
    if (stackedWidget && systemSettingPage) {
        stackedWidget->setCurrentWidget(systemSettingPage);
        systemSettingPage->show();
        setWindowTitle(tr("系统设置"));
    }
}

void MainWindow::openPlcMonitorWindow()
{
    QStackedWidget* stackedWidget = findChild<QStackedWidget*>("pageStack");
    if (stackedWidget && plcMonitorPage) {
        stackedWidget->setCurrentWidget(plcMonitorPage);
        plcMonitorPage->show();
        setWindowTitle(tr("PLC监控"));
        // 同步连接页面的modbus客户端到PlcMonitor页面
        if (connectPage) {
            // 气密仪客户端（isPlc=false
            QModbusClient* instrumentClient = connectPage->getModbusClient(false);
            if (instrumentClient) {
                plcMonitorPage->setAirtightModbusClient(instrumentClient);
                qDebug() << "已将气密仪modbus客户端传递给PLC监控页面";
            } else {
                qDebug() << "警告：气密仪未连接，PLC监控页面气密仪功能不可用";
            }
            // PLC 客户端（isPlc=true
            QModbusClient* plcClient = connectPage->getModbusClient(true);
            if (plcClient) {
                plcMonitorPage->setModbusClient(plcClient);
                qDebug() << "已将PLC modbus客户端传递给PLC监控页面";
            } else {
                qDebug() << "警告：PLC未连接，PLC监控页面将显示未连接";
            }
            // 监听连接页的客户端变更，保持页面同步
            QObject::connect(connectPage, &ConnectWidget::modbusClientUpdated, this,
                             [this](bool isPlc, QModbusClient* client){
                                 if (plcMonitorPage) {
                                     if (isPlc) {
                                         plcMonitorPage->setModbusClient(client);
                                     } else {
                                         plcMonitorPage->setAirtightModbusClient(client);
                                     }
                                 }
                             });
        } else {
        }
        // 同步调压装置客户端到PLC监控页面（来源于ConnectWidget
        if (connectPage) {
            QModbusClient* pressureClient = connectPage->getPressureModbusClient();
            plcMonitorPage->setPressureModbusClient(pressureClient);
            // 立即同步当前的调压装置连接状
            if (pressureClient) {
                bool isPressureConnected = (pressureClient->state() == QModbusDevice::ConnectedState);
                plcMonitorPage->updatePressureConnectionStatus(isPressureConnected);
                qDebug() << "已同步调压装置当前连接状态到PLC监控页面:" << (isPressureConnected ? "已连接" : "未连接");
            }
            QObject::connect(connectPage, &ConnectWidget::pressureClientUpdated,
                             plcMonitorPage, &PlcMonitor::setPressureModbusClient,
                             Qt::UniqueConnection);
            // 连接调压装置连接状态变化信
            QObject::connect(connectPage, &ConnectWidget::pressureConnectionChanged,
                             plcMonitorPage, &PlcMonitor::updatePressureConnectionStatus,
                             Qt::UniqueConnection);
        } else {
            qDebug() << "提示：ConnectWidget未初始化，PLC监控页面暂无法获取调压装置客户端";
        }
        // 设置PLC监控页面指针到TCP服务器控制器
        m_tcpServerController->setPlcMonitorPage(plcMonitorPage);
    }
}

void MainWindow::openDebugPageWindow()
{
    // 检查权
    if (!m_currentUser.isAdmin()) {
        StyledMessageBox::warning(this, "权限不足", "只有管理员才能访问调试页面！");
        return;
    }
    QStackedWidget* stackedWidget = findChild<QStackedWidget*>("pageStack");
    if (stackedWidget && debugPage) {
        stackedWidget->setCurrentWidget(debugPage);
        debugPage->show();
        setWindowTitle(tr("调试页面"));
        // 同步连接页面的modbus客户端到调试页面
        if (connectPage) {
            QModbusClient* instrumentClient = connectPage->getModbusClient(false);
            if (instrumentClient) {
                debugPage->setAirtightModbusClient(instrumentClient);
                qDebug() << "已将气密仪modbus客户端传递给调试页面";
            }
            QModbusClient* plcClient = connectPage->getModbusClient(true);
            if (plcClient) {
                debugPage->setPlcModbusClient(plcClient);
                qDebug() << "已将PLC modbus客户端传递给调试页面";
            }
            // 监听连接页的客户端变更，确保状态同步（PLC与气密仪
            QObject::connect(connectPage, &ConnectWidget::modbusClientUpdated, this,
                             [this](bool isPlc, QModbusClient* client){
                                 if (debugPage) {
                                     if (isPlc) {
                                         debugPage->setPlcModbusClient(client);
                                     } else {
                                         debugPage->setAirtightModbusClient(client);
                                     }
                                 }
                             });
        }
        // 同步调压装置客户端到调试页面（来源于ConnectWidget
        if (connectPage) {
            QModbusClient* pressureClient = connectPage->getPressureModbusClient();
            debugPage->setPressureModbusClient(pressureClient);
            QObject::connect(connectPage, &ConnectWidget::pressureClientUpdated,
                             debugPage, &DebugPage::setPressureModbusClient,
                             Qt::UniqueConnection);
            // 连接调压装置连接状态变化信号（如果调试页面需要显示状态）
            // 注意：debugpage 目前没有 updatePressureConnectionStatus 方法
            // 如果需要显示状态，需要在 debugpage 中添加该方法
        } else {
        }
    }
}

void MainWindow::openAirtightTestResultWindow()
{
    QStackedWidget* stackedWidget = findChild<QStackedWidget*>("pageStack");
    if (stackedWidget && airtightTestResultPage) {
        stackedWidget->setCurrentWidget(airtightTestResultPage);
        airtightTestResultPage->show();
        setWindowTitle(tr("气密测试结果"));
        // 同步连接页面的modbus客户端到AirtightTestResult页面
        if (connectPage) {
            // 获取气密仪modbus客户端（isPlc=false
            QModbusClient* instrumentClient = connectPage->getModbusClient(false);
            if (instrumentClient) {
                airtightTestResultPage->setModbusClient(instrumentClient);
                qDebug() << "已将气密仪modbus客户端传递给气密测试结果页面";
            } else {
                StyledMessageBox::warning(this, "连接提示", "气密仪尚未连接，部分功能可能无法使用)");
            }
            // 获取PLC modbus客户端（isPlc=true
            QModbusClient* plcClient = connectPage->getModbusClient(true);
            if (plcClient) {
                airtightTestResultPage->setPlcModbusClient(plcClient);
                qDebug() << "已将PLC modbus客户端传递给气密测试结果页面";
            }
        } else {
            StyledMessageBox::warning(this, "错误", "连接页面未初始化，无法获取设备连)");
        }
        // 统一确保跨页面信号连
        ensureInterPageConnections();
    } else {
        qDebug() << "无法打开气密测试结果页面: 堆叠窗口或页面不存在";
        StyledMessageBox::warning(this, "错误", "无法打开气密测试结果页面: 组件未初始化");
    }
}

// 集中管理跨页面信号连接，防止重复与遗

void MainWindow::ensureInterPageConnections()
{
    // PlcMonitor -> AirtightTestResult
    if (plcMonitorPage && airtightTestResultPage) {
        QObject::connect(plcMonitorPage, &PlcMonitor::instrumentSelectionChanged,
                         airtightTestResultPage, &AirtightTestResult::onInstrumentSelectionChanged,
                         Qt::UniqueConnection);
    }
    // 将connectPage指针传递给TcpServerController
    if (connectPage && m_tcpServerController) {
        m_tcpServerController->setConnectPage(connectPage);
    }
    // PlcMonitor -> TcpServerController: 传递仪器评选号/程序
    if (plcMonitorPage && m_tcpServerController) {
        QObject::connect(plcMonitorPage, &PlcMonitor::instrumentSelectionChanged,
                         m_tcpServerController, &TcpServerController::onInstrumentSelectionChanged,
                         Qt::UniqueConnection);
        qDebug() << "已建立PlcMonitor到TcpServerController的程序号信号连接";
    } else {
        qDebug() << "警告: 无法建立PlcMonitor到TcpServerController的信号连接，页面可能未正确初始化";
    }
    // PlcMonitor -> RealtimeMonitor
    if (plcMonitorPage && realtimeMonitorPage) {
        QObject::connect(plcMonitorPage, &PlcMonitor::instrumentSelectionChanged,
                         realtimeMonitorPage, &RealtimeMonitor::handleProgramNumberReceived,
                         Qt::UniqueConnection);
        QObject::connect(realtimeMonitorPage, &RealtimeMonitor::updateChannelResult,
                         plcMonitorPage, &PlcMonitor::clearChannelResults,
                         Qt::UniqueConnection);
    }
    // AirtightTestResult -> RealtimeMonitor
    if (airtightTestResultPage && realtimeMonitorPage) {
        QObject::connect(airtightTestResultPage, &AirtightTestResult::testResultReady,
                         realtimeMonitorPage, &RealtimeMonitor::handleTestResultReady,
                         Qt::UniqueConnection);
        QObject::connect(airtightTestResultPage, &AirtightTestResult::testResultReady2,
                         realtimeMonitorPage, &RealtimeMonitor::handleTestResultReady2,
                         Qt::UniqueConnection);
        QObject::connect(airtightTestResultPage, &AirtightTestResult::testResultReady3,
                         realtimeMonitorPage, &RealtimeMonitor::handleTestResultReady3,
                         Qt::UniqueConnection);
    }
    // RealtimeMonitor -> AirtightTestResult: 传递测试结果寄存器数据
    if (realtimeMonitorPage && airtightTestResultPage) {
        QObject::connect(realtimeMonitorPage, &RealtimeMonitor::testResultRegisterDataReady,
                         airtightTestResultPage, &AirtightTestResult::onTestResultRegisterDataReady,
                         Qt::UniqueConnection);
    }
    // AirtightTestResult -> TcpServerController: 传递测试结果信号，以便通过TCP发送给客户
    if (airtightTestResultPage && m_tcpServerController) {
        bool t1 = QObject::connect(airtightTestResultPage, &AirtightTestResult::testResultReady,
                                 m_tcpServerController, &TcpServerController::onTestResultReady,
                                 Qt::UniqueConnection);
        bool t2 = QObject::connect(airtightTestResultPage, &AirtightTestResult::testResultReady2,
                                 m_tcpServerController, &TcpServerController::onTestResultReady2,
                                 Qt::UniqueConnection);
        bool t3 = QObject::connect(airtightTestResultPage, &AirtightTestResult::testResultReady3,
                                 m_tcpServerController, &TcpServerController::onTestResultReady3,
                                 Qt::UniqueConnection);
        qDebug() << "已建立AirtightTestResult到TcpServerController的测试结果信号连" << t1 << t2 << t3;
    } else {
        qDebug() << "警告: 无法建立AirtightTestResult到TcpServerController的信号连接，页面或控制器可能未正确初始化";
    }
    // AirtightTestResult -> PlcMonitor
    if (airtightTestResultPage && plcMonitorPage) {
        QObject::connect(airtightTestResultPage, &AirtightTestResult::testResultReady,
                         plcMonitorPage, &PlcMonitor::handleTestResultReady,
                         Qt::UniqueConnection);
        QObject::connect(airtightTestResultPage, &AirtightTestResult::testResultReady2,
                         plcMonitorPage, &PlcMonitor::handleTestResultReady2,
                         Qt::UniqueConnection);
        QObject::connect(airtightTestResultPage, &AirtightTestResult::testResultReady3,
                         plcMonitorPage, &PlcMonitor::handleTestResultReady3,
                         Qt::UniqueConnection);
    }
    // RealtimeMonitor -> AirtightTestResult
    if (realtimeMonitorPage && airtightTestResultPage) {
        QObject::connect(realtimeMonitorPage, &RealtimeMonitor::productNumberChanged,
                         airtightTestResultPage, &AirtightTestResult::setCurrentProductNumber,
                         Qt::UniqueConnection);
    }
    // RealtimeMonitor -> DebugPage: 传递寄存器100-103变更
    if (realtimeMonitorPage && debugPage) {
        QObject::connect(realtimeMonitorPage, &RealtimeMonitor::registerValuesChanged,
                         debugPage, &DebugPage::onRegisterValuesChanged,
                         Qt::UniqueConnection);
    }
    // RealtimeMonitor -> TcpServerController: 传递实时压力和泄漏数据
    if (realtimeMonitorPage && m_tcpServerController) {
        QObject::connect(realtimeMonitorPage, &RealtimeMonitor::realtimeDataUpdated,
                         m_tcpServerController, &TcpServerController::onRealtimeDataUpdated,
                         Qt::UniqueConnection);
    } else {
        qDebug() << "警告: 无法建立RealtimeMonitor到TcpServerController的信号连接，页面或控制器可能未正确初始化";
    }
    // ConnectWidget -> RealtimeMonitor/PlcMonitor/DebugPage: 同步客户端与连接状态，确保页面联动
    if (connectPage) {
        if (realtimeMonitorPage) {
            // 先断开旧的连接，避免重
            QObject::disconnect(connectPage, &ConnectWidget::modbusClientUpdated, realtimeMonitorPage, nullptr);
            QObject::connect(connectPage, &ConnectWidget::modbusClientUpdated, this,
                             [this](bool isPlc, QModbusClient* client){
                                 if (!realtimeMonitorPage) return;
                                 if (isPlc) {
                                     // PLC客户端更
                                     realtimeMonitorPage->setPlcModbusClient(client);
                                     bool connected = client && client->state() == QModbusDevice::ConnectedState;
                                     realtimeMonitorPage->updateConnectionStatus(true, connected);
                                     qDebug() << "ConnectWidget通知RealtimeMonitor: PLC连接状" << connected;
                                 } else {
                                     // 气密仪客户端更新
                                     realtimeMonitorPage->setModbusClient(client);
                                     realtimeMonitorPage->setAirtightSlaveId(connectPage->getAirtightSlaveId());
                                     bool connected = client && client->state() == QModbusDevice::ConnectedState;
                                     realtimeMonitorPage->updateConnectionStatus(false, connected);
                                     qDebug() << "ConnectWidget通知RealtimeMonitor: 气密仪连接状" << connected;
                                 }
                             });
            // 监听调压装置连接状态变
            QObject::disconnect(connectPage, &ConnectWidget::pressureConnectionChanged, realtimeMonitorPage, nullptr);
            QObject::connect(connectPage, &ConnectWidget::pressureConnectionChanged,
                             realtimeMonitorPage, &RealtimeMonitor::updatePressureConnectionStatus,
                             Qt::UniqueConnection);
            // 初始同步一次当前客户端状
            QModbusClient* curAir = connectPage->getModbusClient(false);
            realtimeMonitorPage->setModbusClient(curAir);
            realtimeMonitorPage->setAirtightSlaveId(connectPage->getAirtightSlaveId());
            bool airConnected = curAir && curAir->state() == QModbusDevice::ConnectedState;
            realtimeMonitorPage->updateConnectionStatus(false, airConnected);
            QModbusClient* curPlc = connectPage->getModbusClient(true);
            realtimeMonitorPage->setPlcModbusClient(curPlc);
            bool plcConnected = curPlc && curPlc->state() == QModbusDevice::ConnectedState;
            realtimeMonitorPage->updateConnectionStatus(true, plcConnected);
            QModbusClient* curPressure = connectPage->getPressureModbusClient();
            bool pressureConnected = curPressure && curPressure->state() == QModbusDevice::ConnectedState;
            realtimeMonitorPage->updatePressureConnectionStatus(pressureConnected);
            qDebug() << "初始同步RealtimeMonitor连接状 气密" << airConnected << " PLC=" << plcConnected << " 调压=" << pressureConnected;
        }
        if (plcMonitorPage) {
            // 先断开旧的连接，避免重
            QObject::disconnect(connectPage, &ConnectWidget::modbusClientUpdated, plcMonitorPage, nullptr);
            // 使用默认连接类型，不使用 UniqueConnection（lambda 不支持唯一连接
            QObject::connect(connectPage, &ConnectWidget::modbusClientUpdated, plcMonitorPage,
                             [this](bool isPlc, QModbusClient* client){
                                 if (!plcMonitorPage) return;
                                 if (isPlc) {
                                     plcMonitorPage->setModbusClient(client);
                                 } else {
                                     plcMonitorPage->setAirtightModbusClient(client);
                                 }
                             });
            // 初始同步一次当前客户端
            QModbusClient* curPlc = connectPage->getModbusClient(true);
            QModbusClient* curAir = connectPage->getModbusClient(false);
            plcMonitorPage->setModbusClient(curPlc);
            plcMonitorPage->setAirtightModbusClient(curAir);
            // 同步从站地址（Unit ID
            plcMonitorPage->setDeviceAddress(connectPage->getPlcSlaveId());
            plcMonitorPage->setAirtightDeviceAddress(connectPage->getAirtightSlaveId());
        }
        if (debugPage) {
            // 先断开旧的连接，避免重
            QObject::disconnect(connectPage, &ConnectWidget::modbusClientUpdated, debugPage, nullptr);
            // 使用默认连接类型，不使用 UniqueConnection（lambda 不支持唯一连接
            QObject::connect(connectPage, &ConnectWidget::modbusClientUpdated, debugPage,
                             [this](bool isPlc, QModbusClient* client){
                                 if (!debugPage) return;
                                 if (isPlc) {
                                     debugPage->setPlcModbusClient(client);
                                 } else {
                                     debugPage->setAirtightModbusClient(client);
                                 }
                             });
            // 初始同步一次当前客户端
            QModbusClient* curPlc = connectPage->getModbusClient(true);
            QModbusClient* curAir = connectPage->getModbusClient(false);
            debugPage->setPlcModbusClient(curPlc);
            debugPage->setAirtightModbusClient(curAir);
            // 同步从站地址（Unit ID
            debugPage->setPlcDeviceAddress(connectPage->getPlcSlaveId());
            debugPage->setAirtightDeviceAddress(connectPage->getAirtightSlaveId());
        }
        // 同步ConnectWidget到AirtightTestResult：客户端与连接状
        if (airtightTestResultPage) {
            // 先断开旧的连接，避免重
            QObject::disconnect(connectPage, &ConnectWidget::modbusClientUpdated, airtightTestResultPage, nullptr);
            QObject::connect(connectPage, &ConnectWidget::modbusClientUpdated, this,
                             [this](bool isPlc, QModbusClient* client){
                                 if (!airtightTestResultPage) return;
                                 bool connected = client && client->state() == QModbusDevice::ConnectedState;
                                 if (isPlc) {
                                     airtightTestResultPage->setPlcModbusClient(client);
                                     airtightTestResultPage->updateConnectionStatus(true, connected);
                                 } else {
                                     airtightTestResultPage->setModbusClient(client);
                                     airtightTestResultPage->updateConnectionStatus(false, connected);
                                 }
                             });
            // 初始同步一次当前客户端
            QModbusClient* curAir = connectPage->getModbusClient(false);
            QModbusClient* curPlc = connectPage->getModbusClient(true);
            airtightTestResultPage->setModbusClient(curAir);
            airtightTestResultPage->setPlcModbusClient(curPlc);
            bool airConnected = curAir && curAir->state() == QModbusDevice::ConnectedState;
            airtightTestResultPage->updateConnectionStatus(false, airConnected);
            bool plcConnected = curPlc && curPlc->state() == QModbusDevice::ConnectedState;
            airtightTestResultPage->updateConnectionStatus(true, plcConnected);
        }
    }
}

void MainWindow::openConnectWindow()
{
    QStackedWidget* stackedWidget = findChild<QStackedWidget*>("pageStack");
    if (stackedWidget && connectPage) {
        stackedWidget->setCurrentWidget(connectPage);
        setWindowTitle(tr("通讯连接"));
    }
}

void MainWindow::openParamSettingWindow()
{
    if (!m_currentUser.isAdmin()) {
        StyledMessageBox::warning(this, "权限不足", "只有管理员才能访问参数设置页面！");
        return;
    }
    QStackedWidget* stackedWidget = findChild<QStackedWidget*>("pageStack");
    if (stackedWidget && paramSettingPage) {
        stackedWidget->setCurrentWidget(paramSettingPage);
        paramSettingPage->show();
        setWindowTitle(tr("参数设置管理"));
        // 同步连接页面的气密仪modbus客户端到参数设置页面
        if (connectPage) {
            QModbusClient* instrumentClient = connectPage->getModbusClient(false); // isPlc=false => 气密
            paramSettingPage->setModbusClient(instrumentClient);
            // 监听连接页的客户端变更，保持参数设置页面与连接状态一
            QObject::connect(connectPage, &ConnectWidget::modbusClientUpdated, this,
                             [this](bool isPlc, QModbusClient* client){
                                 if (!isPlc && paramSettingPage) {
                                     paramSettingPage->setModbusClient(client);
                                 }
                             }, Qt::UniqueConnection);
        } else {
        }
        // 同步调压装置客户端到参数设置页面（来源于ConnectWidget
        if (connectPage) {
            QModbusClient* pressureClient = connectPage->getPressureModbusClient();
            paramSettingPage->setPressureModbusClient(pressureClient);
            // 立即同步当前的调压装置连接状
            if (pressureClient) {
                bool isPressureConnected = (pressureClient->state() == QModbusDevice::ConnectedState);
                paramSettingPage->updatePressureConnectionStatus(isPressureConnected);
                qDebug() << "已同步调压装置当前连接状态到参数设置页面:" << (isPressureConnected ? "已连接" : "未连接");
            }
            QObject::connect(connectPage, &ConnectWidget::pressureClientUpdated,
                             paramSettingPage, &ParamSetting::setPressureModbusClient,
                             Qt::UniqueConnection);
            // 额外：直接响应ConnectWidget的连接状态变化，更新ParamSetting右上角状
            QObject::connect(connectPage, &ConnectWidget::pressureConnectionChanged,
                             paramSettingPage, &ParamSetting::updatePressureConnectionStatus,
                             Qt::UniqueConnection);
        } else {
            // 未打开过ConnectWidget时，暂不设置（保持未连接显示
        }
    }
}

void MainWindow::openUserManagementWindow()
{
    // 检查权
    if (!m_currentUser.isAdmin()) {
        StyledMessageBox::warning(this, "权限不足", "只有管理员才能访问用户管理页面！");
        return;
    }
    QStackedWidget* stackedWidget = findChild<QStackedWidget*>("pageStack");
    if (stackedWidget && userManagementPage) {
        stackedWidget->setCurrentWidget(userManagementPage);
        userManagementPage->show();
        setWindowTitle(tr("用户管理"));
        qDebug() << "已切换到用户管理页面";
    } else {
        qDebug() << "无法打开用户管理页面: 堆叠窗口或页面不存在";
        StyledMessageBox::warning(this, "错误", "无法打开用户管理页面: 组件未初始化");
    }
}

void MainWindow::setCurrentUser(const User &user)
{
    m_currentUser = user;
    // 更新侧边栏用户信息标签
    if (m_userLabel) {
        QString userName = user.realName.isEmpty() ? user.username : user.realName;
        m_userLabel->setText(QString("👤 %1 (%2)").arg(userName, user.getRoleName()));
    }
    // 根据用户角色控制菜单项的可见性和可用
    updateMenuPermissions();
    // 将用户信息传递给用户管理页面
    if (userManagementPage) {
        userManagementPage->setCurrentUser(user);
    }
    // 将用户信息传递给测试结果页面
    if (airtightTestResultPage) {
        airtightTestResultPage->setCurrentUser(user);
    }
    // 将用户信息传递给实时监控页面
    if (realtimeMonitorPage) {
        realtimeMonitorPage->setCurrentUser(user);
    }
}

void MainWindow::onLogoutClicked()
{
    // 确认退出登
    auto reply = StyledMessageBox::question(this, "退出登",
        "确定要退出当前账户吗？\n\n退出后需要重新登录才能继续使用系统)");
    if (reply == QMessageBox::Yes) {
        // 隐藏主窗口（不关闭）
        hide();
        // 显示登录窗口
        LoginWindow loginWindow(this);
        if (loginWindow.exec() == QDialog::Accepted) {
            // 登录成功，更新当前用户信
            setCurrentUser(loginWindow.getLoggedInUser());
            // 重新显示主窗
            show();
            // 切换到实时监控页面（默认页面
            QStackedWidget* stackedWidget = findChild<QStackedWidget*>("pageStack");
            if (stackedWidget && realtimeMonitorPage) {
                stackedWidget->setCurrentWidget(realtimeMonitorPage);
                setWindowTitle(tr("实时监控"));
            }
        } else {
            // 用户取消登录，退出应用程
            qApp->quit();
        }
    }
}

void MainWindow::updateMenuPermissions()
{
    bool isAdmin = m_currentUser.isAdmin();
    // 管理员才能看到参数设置、调试、用户管理、系统设置
    if (btnParamSetting)    btnParamSetting->setVisible(isAdmin);
    if (btnDebug)           btnDebug->setVisible(isAdmin);
    if (btnUserManagement)  btnUserManagement->setVisible(isAdmin);
    if (btnSystemSetting)   btnSystemSetting->setVisible(isAdmin);
}
