#ifndef AIRTIGHTTESTRESULT_H
#define AIRTIGHTTESTRESULT_H

#include <QWidget>
#include <QModbusClient>
#include <QTimer>
#include <QDateTime>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QPointer>
#include <QMutex>
#include <QThread>
#include <QAtomicInteger>
#include "DatabaseManager.h"
#include "TestResultStruct.h"
#include "AirtightTestResultDao.h"
#include "User.h"

namespace Ui {
class AirtightTestResult;
}

class AirtightTestResult : public QWidget
{
    Q_OBJECT

signals:
    void testResultReady(const TestResult &result);
    void testResultReady2(const TestResult &result);
    void testResultReady3(const TestResult &result);

public:
    explicit AirtightTestResult(QWidget *parent = nullptr);
    ~AirtightTestResult();

    void setModbusClient(QModbusClient *client);     // 设置气密仪连接
    void setPlcModbusClient(QModbusClient *client);  // 设置PLC连接
    void updateConnectionStatus(bool isPlc, bool connected);
    void readTestResultData();
    
    // 设置当前登录用户
    void setCurrentUser(const User &user);
    
    // 公共方法：向PLC寄存器写入值（异步版本）
    void writePlcRegister(quint16 address, quint16 value, QModbusDataUnit::RegisterType type, const std::function<void(bool)>& callback);
    // 同步阻塞版本（保留用于需要阻塞调用的场景）
    bool writePlcRegisterBlocking(quint16 address, quint16 value, QModbusDataUnit::RegisterType type = QModbusDataUnit::Coils);

private slots:
    void on_exportButton_clicked();
    void on_clearQueryButton_clicked();
    void on_monitorTimer_timeout(); // 定时器超时槽函数，用于监控9088寄存器
    // 筛选条件变化时自动刷新
    void onFilterChanged();
    // 自动刷新定时器
    void onAutoRefreshTimer();
    // 分页相关槽函数
    void onFirstPageClicked();
    void onPrevPageClicked();
    void onNextPageClicked();
    void onLastPageClicked();
    void onPageSizeChanged(int index);
    
public slots:
    void onInstrumentSelectionChanged(int instrumentSelection); // 处理仪器评选号变化信号
    void setCurrentProductNumber(const QString &code); // 接收产品编号更新
    void onTestResultRegisterDataReady(const QModbusDataUnit &data); // 接收测试结果寄存器数据（8961-8972）

private:
    enum class TestResultState {
        Idle,
        Processing,
        Completed
    };

    Ui::AirtightTestResult *ui;
    QModbusClient *modbusClient;     // 气密仪连接
    QModbusClient *plcModbusClient;  // PLC连接
    quint8 slaveId;
    bool isConnected;
    bool isLogPaused; // 日志暂停状态
    QStandardItemModel *tableModel;
    QSortFilterProxyModel *proxyModel; // 用于筛选的代理模型
    QTimer *monitorTimer; // 用于定期检测9088寄存器的定时器
    QTimer *instrumentMonitorTimer; // 用于监控仪器评选后的9088寄存器
    QTimer *autoRefreshTimer; // 自动刷新定时器
    int currentProgramNumber; // 当前程序号（-1表示未知）
    QString currentProductNumber; // 当前产品编号
    QDateTime lastSavedTimestamp; // 上次保存测试结果的时间戳，用于去重
    double lastSavedPressureValue; // 上次保存的压力值，用于去重
    User m_currentUser; // 当前登录用户
    TestResultState m_testResultState; // 测试结果处理状态机，防止重复处理
    
    // 缓存的通道配置（从PLC读取）
    quint16 cachedTest1Enabled = 0;
    quint16 cachedTest2Enabled = 0;
    quint16 cachedTest3Enabled = 0;
    quint16 cachedTestChannel1 = 0;
    quint16 cachedTestChannel2 = 0;
    quint16 cachedTestChannel3 = 0;
    
    // 线程安全相关
    mutable QMutex m_stateMutex; // 状态保护互斥锁
    QAtomicInteger<bool> m_isProcessing; // 原子标记：是否正在处理
    QAtomicInteger<int> m_writeRequestCount; // 原子计数器：写入请求数
    
    // PLC写入重试配置
    static constexpr int PLC_WRITE_MAX_RETRIES = 3;
    static constexpr int PLC_WRITE_BASE_DELAY_MS = 100;
    static constexpr int PLC_WRITE_TIMEOUT_MS = 4000;
    
    void initUI();
    void setupTableModel();
    bool readDeviceData(quint16 address, quint16 &value, QModbusDataUnit::RegisterType type = QModbusDataUnit::HoldingRegisters);
    void logMessage(const QString &message, bool isError = false);
    void exportToExcel(const QString &fileName, bool exportAll = true);
    void handleWriteFailure(quint16 address, quint16 value,
                           std::shared_ptr<int> retryCount, std::shared_ptr<bool> completed,
                           const std::function<void(bool)>& callback, std::function<void()> doWrite,
                           const QString& errorStr, const QVector<int>& backoffDelays, int maxRetries);
    
    // 清空测试结果数据
    void clearTestData();
    void applyFilter(); // 应用筛选条件
    void addTestResultRow(const QDateTime &timestamp, const QString &alarmInfo,
                          double pressureValue, const QString &pressureUnit,
                          double leakValue, const QString &leakUnit,
                          int programNumber = -1,
                          const QString &productNumber = "",
                          int alarmCode = -1,
                          const QString &inspector = "");
    
    // 从数据库加载测试结果
    void loadTestResultsFromDatabase();
    // 更新记录数显示
    void updateRecordCount();
    // 分页相关
    void updatePagination();
    void displayCurrentPage();
    
    // 分页相关成员变量
    int currentPage;        // 当前页码（从1开始）
    int pageSize;           // 每页显示条数
    int totalRecords;       // 总记录数
    int totalPages;         // 总页数
    QList<AirtightTestResultData> allFilteredResults; // 筛选后的所有结果（用于分页）
};

#endif // AIRTIGHTTESTRESULT_H