#include "loginwindow.h"
#include "ui_loginwindow.h"
#include "StyledMessageBox.h"
#include <QSettings>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QKeyEvent>

LoginWindow::LoginWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoginWindow),
    m_userDao(new UserDao(this)),
    m_loginSuccessful(false)
{
    ui->setupUi(this);
    setupUi();
    
    // 初始化用户表
    m_userDao->initTable();
    m_userDao->createDefaultAdmin();
    
    // 加载记住的凭据
    loadRememberedCredentials();
    
    // 连接信号槽
    connect(ui->loginButton, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
    connect(ui->cancelButton, &QPushButton::clicked, this, &LoginWindow::onCancelClicked);
    connect(ui->usernameEdit, &QLineEdit::textChanged, this, &LoginWindow::onUsernameChanged);
    connect(ui->passwordEdit, &QLineEdit::textChanged, this, &LoginWindow::onPasswordChanged);
    
    // 回车键登录
    connect(ui->usernameEdit, &QLineEdit::returnPressed, this, [this]() {
        ui->passwordEdit->setFocus();
    });
    connect(ui->passwordEdit, &QLineEdit::returnPressed, this, &LoginWindow::onLoginClicked);
}

LoginWindow::~LoginWindow()
{
    delete ui;
}

void LoginWindow::setupUi()
{
    // 设置窗口标志（无边框、置顶）
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    
    // 设置固定大小
    setFixedSize(480, 600);
    
    // 居中显示
    if (parentWidget()) {
        move(parentWidget()->geometry().center() - rect().center());
    }
    
    // 初始状态下登录按钮禁用
    ui->loginButton->setEnabled(false);
    
    // 设置焦点到用户名输入框
    ui->usernameEdit->setFocus();
}

void LoginWindow::loadRememberedCredentials()
{
    QSettings settings(QCoreApplication::applicationDirPath() + "/login_config.ini", QSettings::IniFormat);
    
    bool rememberPassword = settings.value("Login/RememberPassword", false).toBool();
    QString username = settings.value("Login/Username", "").toString();
    QString password = settings.value("Login/Password", "").toString();
    
    if (rememberPassword && !username.isEmpty()) {
        ui->usernameEdit->setText(username);
        ui->passwordEdit->setText(password);
        ui->rememberCheckBox->setChecked(true);
        ui->loginButton->setEnabled(true);
    }
}

void LoginWindow::saveCredentials()
{
    QSettings settings(QCoreApplication::applicationDirPath() + "/login_config.ini", QSettings::IniFormat);
    
    if (ui->rememberCheckBox->isChecked()) {
        settings.setValue("Login/RememberPassword", true);
        settings.setValue("Login/Username", ui->usernameEdit->text());
        settings.setValue("Login/Password", ui->passwordEdit->text());
    } else {
        clearCredentials();
    }
    
    settings.sync();
}

void LoginWindow::clearCredentials()
{
    QSettings settings(QCoreApplication::applicationDirPath() + "/login_config.ini", QSettings::IniFormat);
    settings.remove("Login/RememberPassword");
    settings.remove("Login/Username");
    settings.remove("Login/Password");
    settings.sync();
}

bool LoginWindow::validateLogin(const QString &username, const QString &password)
{
    if (username.isEmpty()) {
        StyledMessageBox::warning(this, "登录失败", "请输入用户名！");
        ui->usernameEdit->setFocus();
        return false;
    }
    
    if (password.isEmpty()) {
        StyledMessageBox::warning(this, "登录失败", "请输入密码！");
        ui->passwordEdit->setFocus();
        return false;
    }
    
    // 验证用户登录
    User user = m_userDao->validateLogin(username, password);
    
    if (user.id <= 0) {
        StyledMessageBox::critical(this, "登录失败", "用户名或密码错误！");
        ui->passwordEdit->clear();
        ui->passwordEdit->setFocus();
        return false;
    }
    
    if (!user.isActive) {
        StyledMessageBox::warning(this, "登录失败", "该账户已被禁用，请联系管理员！");
        return false;
    }
    
    // 更新最后登录时间
    m_userDao->updateLastLoginTime(user.id);
    
    // 保存登录用户信息
    m_loggedInUser = user;
    m_loginSuccessful = true;
    
    return true;
}

void LoginWindow::onLoginClicked()
{
    QString username = ui->usernameEdit->text().trimmed();
    QString password = ui->passwordEdit->text();
    
    if (validateLogin(username, password)) {
        // 保存凭据（如果勾选了记住密码）
        saveCredentials();
        
        // 直接接受对话框，不显示欢迎消息
        accept();
    }
}

void LoginWindow::onCancelClicked()
{
    // 拒绝对话框
    reject();
}

void LoginWindow::onUsernameChanged(const QString &text)
{
    // 当用户名和密码都不为空时启用登录按钮
    ui->loginButton->setEnabled(!text.trimmed().isEmpty() && !ui->passwordEdit->text().isEmpty());
}

void LoginWindow::onPasswordChanged(const QString &text)
{
    // 当用户名和密码都不为空时启用登录按钮
    ui->loginButton->setEnabled(!ui->usernameEdit->text().trimmed().isEmpty() && !text.isEmpty());
}
