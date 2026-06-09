#include "machinelockauthorizer.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QDateTime>
#include <QApplication>
#include <QClipboard>
#include <QScreen>
#include <QScrollArea>
#include <QCoreApplication>
#include <QStandardPaths>

MachineLockAuthorizer::MachineLockAuthorizer(QWidget *parent)
    : QMainWindow(parent),
      m_lockManager(new MachineLockManager(this))
{
    setupUI();
    centerWindow();
}

MachineLockAuthorizer::~MachineLockAuthorizer() {}

void MachineLockAuthorizer::centerWindow()
{
    QScreen *screen = QApplication::primaryScreen();
    if (!screen) return;
    QRect sg = screen->availableGeometry();
    move((sg.width() - width()) / 2 + sg.x(),
         (sg.height() - height()) / 2 + sg.y());
}

void MachineLockAuthorizer::setupUI()
{
    setWindowTitle("NewAirPlc 机器锁定授权工具");
    setWindowFlags(Qt::Window);
    setMinimumSize(800, 600);
    resize(850, 700);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setCentralWidget(scrollArea);

    QWidget *content = new QWidget();
    scrollArea->setWidget(content);

    QVBoxLayout *mainLayout = new QVBoxLayout(content);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(25, 25, 25, 25);

    QLabel *title = new QLabel("NewAirPlc 机器锁定授权工具", this);
    title->setStyleSheet("font-size: 20px; font-weight: bold; color: #333; padding: 10px;");
    title->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(title);

    QLabel *subtitle = new QLabel("程序将绑定到特定计算机，复制到其他电脑无法使用", this);
    subtitle->setStyleSheet("font-size: 13px; color: #666; padding-bottom: 10px;");
    subtitle->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(subtitle);

    // 步骤1
    QGroupBox *step1 = new QGroupBox("步骤1：获取目标计算机的机器指纹", this);
    step1->setStyleSheet("QGroupBox { font-size: 14px; font-weight: bold; padding-top: 15px; }");
    QVBoxLayout *lay1 = new QVBoxLayout(step1);
    lay1->setSpacing(12);

    QLabel *hint1 = new QLabel("在需要授权的计算机上运行此工具，点击下方按钮获取该机器的硬件指纹。\n"
                               "机器指纹基于CPU、主板、硬盘等硬件特征生成，每台电脑都是唯一的。", this);
    hint1->setStyleSheet("color: #666; padding: 8px; font-size: 12px;");
    hint1->setWordWrap(true);
    lay1->addWidget(hint1);

    m_getFingerprintButton = new QPushButton("获取当前机器指纹", this);
    m_getFingerprintButton->setFixedHeight(45);
    m_getFingerprintButton->setStyleSheet(
        "QPushButton { background-color:#2196F3; color:white; font-size:14px; font-weight:bold; border:none; border-radius:5px; }"
        "QPushButton:hover { background-color:#1976D2; }");
    lay1->addWidget(m_getFingerprintButton);

    m_fingerprintTextEdit = new QTextEdit(this);
    m_fingerprintTextEdit->setReadOnly(true);
    m_fingerprintTextEdit->setFixedHeight(110);
    m_fingerprintTextEdit->setStyleSheet("font-size: 12px; padding: 8px;");
    m_fingerprintTextEdit->setPlaceholderText("机器指纹将显示在这里...");
    lay1->addWidget(m_fingerprintTextEdit);
    mainLayout->addWidget(step1);

    // 步骤2
    QGroupBox *step2 = new QGroupBox("步骤2：生成授权文件", this);
    step2->setStyleSheet("QGroupBox { font-size: 14px; font-weight: bold; padding-top: 15px; }");
    QVBoxLayout *lay2 = new QVBoxLayout(step2);
    lay2->setSpacing(12);

    QLabel *hint2 = new QLabel("获取机器指纹后，生成授权文件并将其复制到目标计算机的程序目录。", this);
    hint2->setStyleSheet("color: #666; padding: 8px; font-size: 12px;");
    hint2->setWordWrap(true);
    lay2->addWidget(hint2);

    QHBoxLayout *pathLayout = new QHBoxLayout();
    QLabel *pathLabel = new QLabel("授权文件保存位置:", this);
    pathLabel->setStyleSheet("font-size: 12px;");
    pathLayout->addWidget(pathLabel);
    m_outputPathEdit = new QLineEdit(this);
    m_outputPathEdit->setPlaceholderText("选择保存位置...");
    m_outputPathEdit->setMinimumHeight(32);
    m_outputPathEdit->setStyleSheet("font-size: 12px; padding: 5px;");
    pathLayout->addWidget(m_outputPathEdit);
    m_browseButton = new QPushButton("浏览...", this);
    m_browseButton->setFixedSize(90, 32);
    m_browseButton->setStyleSheet("font-size: 12px;");
    pathLayout->addWidget(m_browseButton);
    lay2->addLayout(pathLayout);

    m_timeGroupBox = new QGroupBox("授权时间设置", this);
    m_timeGroupBox->setStyleSheet("QGroupBox { font-size: 12px; padding-top: 12px; }");
    QVBoxLayout *timeLayout = new QVBoxLayout(m_timeGroupBox);
    timeLayout->setSpacing(10);

    m_permanentCheckBox = new QCheckBox("永久授权", this);
    m_permanentCheckBox->setChecked(true);
    m_permanentCheckBox->setStyleSheet("font-size: 12px;");
    timeLayout->addWidget(m_permanentCheckBox);

    QGridLayout *dateLayout = new QGridLayout();
    dateLayout->setSpacing(10);
    QLabel *startLabel = new QLabel("开始时间:", this);
    startLabel->setStyleSheet("font-size: 12px;");
    dateLayout->addWidget(startLabel, 0, 0);
    m_startDateTimeEdit = new QDateTimeEdit(QDateTime::currentDateTime(), this);
    m_startDateTimeEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    m_startDateTimeEdit->setMinimumHeight(32);
    m_startDateTimeEdit->setEnabled(false);
    dateLayout->addWidget(m_startDateTimeEdit, 0, 1);
    QLabel *endLabel = new QLabel("结束时间:", this);
    endLabel->setStyleSheet("font-size: 12px;");
    dateLayout->addWidget(endLabel, 1, 0);
    m_endDateTimeEdit = new QDateTimeEdit(QDateTime::currentDateTime().addYears(1), this);
    m_endDateTimeEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    m_endDateTimeEdit->setMinimumHeight(32);
    m_endDateTimeEdit->setEnabled(false);
    dateLayout->addWidget(m_endDateTimeEdit, 1, 1);
    timeLayout->addLayout(dateLayout);
    lay2->addWidget(m_timeGroupBox);

    m_generateLicenseButton = new QPushButton("生成授权文件", this);
    m_generateLicenseButton->setFixedHeight(45);
    m_generateLicenseButton->setStyleSheet(
        "QPushButton { background-color:#4CAF50; color:white; font-size:14px; font-weight:bold; border:none; border-radius:5px; }"
        "QPushButton:hover { background-color:#45a049; }");
    lay2->addWidget(m_generateLicenseButton);
    mainLayout->addWidget(step2);

    // 步骤3
    QGroupBox *step3 = new QGroupBox("步骤3：验证授权（可选）", this);
    step3->setStyleSheet("QGroupBox { font-size: 14px; font-weight: bold; padding-top: 15px; }");
    QVBoxLayout *lay3 = new QVBoxLayout(step3);
    lay3->setSpacing(12);
    QLabel *hint3 = new QLabel("在目标计算机上运行此工具，验证授权文件是否有效。", this);
    hint3->setStyleSheet("color: #666; padding: 8px; font-size: 12px;");
    hint3->setWordWrap(true);
    lay3->addWidget(hint3);
    m_verifyLicenseButton = new QPushButton("验证当前机器授权", this);
    m_verifyLicenseButton->setFixedHeight(45);
    m_verifyLicenseButton->setStyleSheet(
        "QPushButton { background-color:#FF9800; color:white; font-size:14px; font-weight:bold; border:none; border-radius:5px; }"
        "QPushButton:hover { background-color:#F57C00; }");
    lay3->addWidget(m_verifyLicenseButton);
    mainLayout->addWidget(step3);

    // 日志
    QGroupBox *logGroup = new QGroupBox("操作日志", this);
    logGroup->setStyleSheet("QGroupBox { font-size: 14px; font-weight: bold; padding-top: 15px; }");
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
    m_logTextEdit = new QTextEdit(this);
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setMinimumHeight(150);
    m_logTextEdit->setStyleSheet("font-size: 12px; padding: 5px;");
    logLayout->addWidget(m_logTextEdit);
    mainLayout->addWidget(logGroup);

    connect(m_getFingerprintButton,  &QPushButton::clicked, this, &MachineLockAuthorizer::onGetFingerprintClicked);
    connect(m_generateLicenseButton, &QPushButton::clicked, this, &MachineLockAuthorizer::onGenerateLicenseClicked);
    connect(m_verifyLicenseButton,   &QPushButton::clicked, this, &MachineLockAuthorizer::onVerifyLicenseClicked);
    connect(m_browseButton, &QPushButton::clicked, [this]() {
        QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/.machine_license";
        QString filePath = QFileDialog::getSaveFileName(this, "选择保存位置", defaultPath,
                                                        "授权文件 (.machine_license);;所有文件 (*.*)");
        if (!filePath.isEmpty())
            m_outputPathEdit->setText(filePath);
    });
    connect(m_permanentCheckBox, &QCheckBox::toggled, [this](bool checked) {
        m_startDateTimeEdit->setEnabled(!checked);
        m_endDateTimeEdit->setEnabled(!checked);
    });

    appendLog("授权工具已启动 - NewAirPlc 专用版", "INFO");
}

void MachineLockAuthorizer::onGetFingerprintClicked()
{
    appendLog("正在获取当前机器指纹...", "INFO");
    QString fp = m_lockManager->getCurrentMachineFingerprint();
    if (fp.isEmpty()) {
        appendLog("获取机器指纹失败！", "ERROR");
        QMessageBox::critical(this, "错误", "无法获取机器指纹！");
        return;
    }
    m_fingerprintTextEdit->clear();
    m_fingerprintTextEdit->append("机器指纹：");
    m_fingerprintTextEdit->append(fp);
    m_fingerprintTextEdit->append("（已自动复制到剪贴板）");
    QApplication::clipboard()->setText(fp);
    appendLog("指纹获取成功: " + fp, "SUCCESS");
    appendLog("已复制到剪贴板", "SUCCESS");
}

void MachineLockAuthorizer::onGenerateLicenseClicked()
{
    QString outputPath = m_outputPathEdit->text().trimmed();
    if (outputPath.isEmpty()) {
        QMessageBox::warning(this, "警告", "请选择授权文件保存位置！");
        return;
    }
    QString confirmMsg = "确定要生成授权文件吗？\n\n保存位置: " + outputPath +
                         "\n授权类型: " + (m_permanentCheckBox->isChecked() ? "永久授权" : "限时授权");
    if (QMessageBox::question(this, "确认", confirmMsg,
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    QDateTime startTime, endTime;
    if (!m_permanentCheckBox->isChecked()) {
        startTime = m_startDateTimeEdit->dateTime();
        endTime   = m_endDateTimeEdit->dateTime();
        if (endTime <= startTime) {
            QMessageBox::warning(this, "警告", "结束时间必须晚于开始时间！");
            return;
        }
    }

    if (m_lockManager->generateLicenseFile(outputPath, startTime, endTime)) {
        appendLog("授权文件生成成功: " + outputPath, "SUCCESS");
        QMessageBox::information(this, "成功",
            "授权文件生成成功！\n\n文件位置: " + outputPath +
            "\n\n【重要】将此文件复制到目标电脑的程序目录，\n文件名必须为 .machine_license");
    } else {
        appendLog("授权文件生成失败！", "ERROR");
        QMessageBox::critical(this, "失败", "授权文件生成失败！");
    }
}

void MachineLockAuthorizer::onVerifyLicenseClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择授权文件",
                           QCoreApplication::applicationDirPath(),
                           "授权文件 (*.machine_license);;所有文件 (*.*)");
    if (filePath.isEmpty()) return;

    if (!m_lockManager->loadLicenseFile(filePath)) {
        appendLog("授权文件读取失败！", "ERROR");
        QMessageBox::critical(this, "验证失败", "授权文件读取失败！\n文件可能已损坏或格式不正确。");
        return;
    }
    if (m_lockManager->getCurrentMachineFingerprint() != m_lockManager->getAuthorizedFingerprint()) {
        appendLog("机器指纹不匹配！", "ERROR");
        QMessageBox::critical(this, "验证失败", "机器指纹不匹配！\n此授权文件不适用于当前计算机。");
        return;
    }
    if (m_lockManager->isLicenseExpired()) {
        appendLog("授权已过期！", "ERROR");
        QMessageBox::critical(this, "验证失败", "授权已过期！");
        return;
    }
    QString info = m_lockManager->getLicenseInfo();
    appendLog("授权验证通过！\n" + info, "SUCCESS");
    QMessageBox::information(this, "验证成功", "授权验证通过！\n\n" + info);
}

void MachineLockAuthorizer::appendLog(const QString &message, const QString &type)
{
    QString color = "#1976d2";
    if      (type == "ERROR")   color = "#d32f2f";
    else if (type == "WARNING") color = "#f57c00";
    else if (type == "SUCCESS") color = "#388e3c";

    m_logTextEdit->append(
        QString("<span style='color:#999;'>[%1]</span> <span style='color:%2;'>%3</span>")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
        .arg(color).arg(message));
}
