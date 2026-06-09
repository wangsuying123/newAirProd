#include "usermanagement.h"
#include "ui_usermanagement.h"
#include "StyledMessageBox.h"
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QLabel>
#include <QHeaderView>

UserManagement::UserManagement(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::UserManagement),
    m_userDao(new UserDao(this)),
    m_selectedUserId(-1),
    m_currentPage(1),
    m_pageSize(10),
    m_totalPages(1)
{
    ui->setupUi(this);
    setupUi();
    
    // 初始化用户表
    m_userDao->initTable();
    m_userDao->createDefaultAdmin();
    
    // 加载用户列表
    loadUsers();
    
    // 连接信号槽
    connect(ui->addButton, &QPushButton::clicked, this, &UserManagement::onAddUserClicked);
    connect(ui->editButton, &QPushButton::clicked, this, &UserManagement::onEditUserClicked);
    connect(ui->deleteButton, &QPushButton::clicked, this, &UserManagement::onDeleteUserClicked);
    connect(ui->resetPasswordButton, &QPushButton::clicked, this, &UserManagement::onResetPasswordClicked);
    connect(ui->refreshButton, &QPushButton::clicked, this, &UserManagement::onRefreshClicked);
    connect(ui->userTable, &QTableWidget::itemSelectionChanged, this, &UserManagement::onTableSelectionChanged);
    
    // 分页信号槽
    connect(ui->pageSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &UserManagement::onPageSizeChanged);
    connect(ui->firstPageButton, &QPushButton::clicked, this, &UserManagement::onFirstPageClicked);
    connect(ui->prevPageButton, &QPushButton::clicked, this, &UserManagement::onPrevPageClicked);
    connect(ui->nextPageButton, &QPushButton::clicked, this, &UserManagement::onNextPageClicked);
    connect(ui->lastPageButton, &QPushButton::clicked, this, &UserManagement::onLastPageClicked);
}

UserManagement::~UserManagement()
{
    delete ui;
}

void UserManagement::setCurrentUser(const User &user)
{
    m_currentUser = user;
    updateButtonStates();
}

void UserManagement::setupUi()
{
    // 设置表格
    ui->userTable->setColumnWidth(0, 60);   // ID
    ui->userTable->setColumnWidth(1, 120);  // 用户名
    ui->userTable->setColumnWidth(2, 120);  // 真实姓名
    ui->userTable->setColumnWidth(3, 100);  // 角色
    ui->userTable->setColumnWidth(4, 80);   // 状态
    ui->userTable->setColumnWidth(5, 150);  // 创建时间
    ui->userTable->setColumnWidth(6, 150);  // 最后登录
    ui->userTable->setColumnWidth(7, 200);  // 描述
    
    // 设置表头
    ui->userTable->horizontalHeader()->setStretchLastSection(true);
    ui->userTable->verticalHeader()->setVisible(false);
    ui->userTable->setAlternatingRowColors(true);
    
    updateButtonStates();
}

void UserManagement::loadUsers()
{
    ui->userTable->setRowCount(0);
    
    // 获取所有用户
    m_allUsers = m_userDao->getAllUsers();
    
    // 计算总页数
    m_totalPages = (m_allUsers.size() + m_pageSize - 1) / m_pageSize;
    if (m_totalPages < 1) m_totalPages = 1;
    
    // 确保当前页在有效范围内
    if (m_currentPage > m_totalPages) {
        m_currentPage = m_totalPages;
    }
    if (m_currentPage < 1) {
        m_currentPage = 1;
    }
    
    // 计算当前页的起始和结束索引
    int startIndex = (m_currentPage - 1) * m_pageSize;
    int endIndex = qMin(startIndex + m_pageSize, m_allUsers.size());
    
    // 显示当前页的用户
    for (int i = startIndex; i < endIndex; ++i) {
        const User &user = m_allUsers[i];
        int row = ui->userTable->rowCount();
        ui->userTable->insertRow(row);
        
        // ID
        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(user.id));
        idItem->setTextAlignment(Qt::AlignCenter);
        ui->userTable->setItem(row, 0, idItem);
        
        // 用户名
        QTableWidgetItem *usernameItem = new QTableWidgetItem(user.username);
        usernameItem->setTextAlignment(Qt::AlignCenter);
        ui->userTable->setItem(row, 1, usernameItem);
        
        // 真实姓名
        QTableWidgetItem *realNameItem = new QTableWidgetItem(user.realName);
        realNameItem->setTextAlignment(Qt::AlignCenter);
        ui->userTable->setItem(row, 2, realNameItem);
        
        // 角色
        QTableWidgetItem *roleItem = new QTableWidgetItem(user.getRoleName());
        roleItem->setTextAlignment(Qt::AlignCenter);
        ui->userTable->setItem(row, 3, roleItem);
        
        // 状态
        QTableWidgetItem *statusItem = new QTableWidgetItem(user.isActive ? "✓ 激活" : "✗ 禁用");
        statusItem->setTextAlignment(Qt::AlignCenter);
        if (user.isActive) {
            statusItem->setForeground(QColor("#10b981"));
        } else {
            statusItem->setForeground(QColor("#ef4444"));
        }
        ui->userTable->setItem(row, 4, statusItem);
        
        // 创建时间
        QTableWidgetItem *createdItem = new QTableWidgetItem(
            user.createdAt.toString("yyyy-MM-dd HH:mm:ss")
        );
        createdItem->setTextAlignment(Qt::AlignCenter);
        ui->userTable->setItem(row, 5, createdItem);
        
        // 最后登录
        QString lastLogin = user.lastLoginAt.isValid() ? 
            user.lastLoginAt.toString("yyyy-MM-dd HH:mm:ss") : "从未登录";
        QTableWidgetItem *lastLoginItem = new QTableWidgetItem(lastLogin);
        lastLoginItem->setTextAlignment(Qt::AlignCenter);
        ui->userTable->setItem(row, 6, lastLoginItem);
        
        // 描述
        QTableWidgetItem *descItem = new QTableWidgetItem(user.description);
        ui->userTable->setItem(row, 7, descItem);
    }
    
    // 更新分页信息
    updatePaginationInfo();
}

void UserManagement::updateButtonStates()
{
    bool hasSelection = m_selectedUserId > 0;
    // 如果没有设置当前用户，默认给予管理员权限（用于开发和测试）
    bool isAdmin = (m_currentUser.id <= 0) ? true : m_currentUser.isAdmin();
    
    ui->addButton->setEnabled(isAdmin);
    ui->editButton->setEnabled(isAdmin && hasSelection);
    ui->deleteButton->setEnabled(isAdmin && hasSelection && m_selectedUserId != m_currentUser.id);
    ui->resetPasswordButton->setEnabled(isAdmin && hasSelection);
}

void UserManagement::onAddUserClicked()
{
    User user;
    user.createdAt = QDateTime::currentDateTime();
    user.isActive = true;
    user.role = UserRole::Operator; // 默认为操作员
    
    if (showUserDialog(user, false)) {
        // 加密密码
        user.password = UserDao::encryptPassword(user.password);
        
        if (m_userDao->addUser(user)) {
            StyledMessageBox::information(this, "成功", "用户添加成功！");
            loadUsers();
        } else {
            StyledMessageBox::critical(this, "错误", "用户添加失败！");
        }
    }
}

void UserManagement::onEditUserClicked()
{
    if (m_selectedUserId <= 0) {
        return;
    }
    
    User user = m_userDao->getUserById(m_selectedUserId);
    if (user.id <= 0) {
        StyledMessageBox::warning(this, "警告", "未找到选中的用户！");
        return;
    }
    
    if (showUserDialog(user, true)) {
        if (m_userDao->updateUser(user)) {
            StyledMessageBox::information(this, "成功", "用户更新成功！");
            loadUsers();
        } else {
            StyledMessageBox::critical(this, "错误", "用户更新失败！");
        }
    }
}

void UserManagement::onDeleteUserClicked()
{
    if (m_selectedUserId <= 0) {
        return;
    }
    
    User user = m_userDao->getUserById(m_selectedUserId);
    if (user.id <= 0) {
        StyledMessageBox::warning(this, "警告", "未找到选中的用户！");
        return;
    }
    
    if (user.id == m_currentUser.id) {
        StyledMessageBox::warning(this, "警告", "不能删除当前登录的用户！");
        return;
    }
    
    auto reply = StyledMessageBox::question(this, "确认删除", 
        QString("确定要删除用户 \"%1\" 吗？\n此操作不可恢复！").arg(user.username));
    
    if (reply == QMessageBox::Yes) {
        if (m_userDao->deleteUser(m_selectedUserId)) {
            StyledMessageBox::information(this, "成功", "用户删除成功！");
            m_selectedUserId = -1;
            loadUsers();
            updateButtonStates();
        } else {
            StyledMessageBox::critical(this, "错误", "用户删除失败！");
        }
    }
}

void UserManagement::onResetPasswordClicked()
{
    if (m_selectedUserId <= 0) {
        return;
    }
    
    User user = m_userDao->getUserById(m_selectedUserId);
    if (user.id <= 0) {
        StyledMessageBox::warning(this, "警告", "未找到选中的用户！");
        return;
    }
    
    // 创建密码输入对话框
    QDialog dialog(this);
    dialog.setWindowTitle("🔑 重置密码");
    dialog.setMinimumWidth(500);
    dialog.setStyleSheet(getDialogStyleSheet());
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->setSpacing(20);
    layout->setContentsMargins(30, 30, 30, 20);
    
    QLabel *infoLabel = new QLabel(QString("为用户 <b>\"%1\"</b> 设置新密码：").arg(user.username));
    infoLabel->setStyleSheet("font-size: 15px; color: #1e40af; background: transparent;");
    layout->addWidget(infoLabel);
    
    QLineEdit *passwordEdit = new QLineEdit();
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setPlaceholderText("请输入新密码");
    passwordEdit->setMinimumHeight(45);
    layout->addWidget(passwordEdit);
    
    QLineEdit *confirmEdit = new QLineEdit();
    confirmEdit->setEchoMode(QLineEdit::Password);
    confirmEdit->setPlaceholderText("请再次输入新密码");
    confirmEdit->setMinimumHeight(45);
    layout->addWidget(confirmEdit);
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttonBox->button(QDialogButtonBox::Ok)->setText("确定");
    buttonBox->button(QDialogButtonBox::Cancel)->setText("取消");
    layout->addWidget(buttonBox);
    
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    if (dialog.exec() == QDialog::Accepted) {
        QString newPassword = passwordEdit->text();
        QString confirmPassword = confirmEdit->text();
        
        if (newPassword.isEmpty()) {
            StyledMessageBox::warning(this, "警告", "密码不能为空！");
            return;
        }
        
        if (newPassword != confirmPassword) {
            StyledMessageBox::warning(this, "警告", "两次输入的密码不一致！");
            return;
        }
        
        if (m_userDao->resetPassword(m_selectedUserId, newPassword)) {
            StyledMessageBox::information(this, "成功", "密码重置成功！");
        } else {
            StyledMessageBox::critical(this, "错误", "密码重置失败！");
        }
    }
}

void UserManagement::onRefreshClicked()
{
    m_currentPage = 1;  // 刷新时回到第一页
    loadUsers();
    StyledMessageBox::information(this, "提示", "用户列表已刷新！");
}

void UserManagement::onTableSelectionChanged()
{
    QList<QTableWidgetItem*> selectedItems = ui->userTable->selectedItems();
    if (!selectedItems.isEmpty()) {
        int row = selectedItems.first()->row();
        QTableWidgetItem *idItem = ui->userTable->item(row, 0);
        if (idItem) {
            m_selectedUserId = idItem->text().toInt();
        }
    } else {
        m_selectedUserId = -1;
    }
    
    updateButtonStates();
}

bool UserManagement::showUserDialog(User &user, bool isEdit)
{
    QDialog dialog(this);
    dialog.setWindowTitle(isEdit ? "✏️ 编辑用户" : "➕ 添加用户");
    dialog.setMinimumWidth(550);
    dialog.setStyleSheet(getDialogStyleSheet());
    
    QFormLayout *formLayout = new QFormLayout();
    formLayout->setSpacing(15);
    formLayout->setContentsMargins(20, 20, 20, 10);
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    
    // 用户名
    QLineEdit *usernameEdit = new QLineEdit(user.username);
    usernameEdit->setPlaceholderText("请输入用户名（必填）");
    usernameEdit->setMinimumHeight(40);
    formLayout->addRow("用户名:", usernameEdit);
    
    // 密码（仅添加时显示）
    QLineEdit *passwordEdit = nullptr;
    QLineEdit *confirmPasswordEdit = nullptr;
    if (!isEdit) {
        passwordEdit = new QLineEdit();
        passwordEdit->setEchoMode(QLineEdit::Password);
        passwordEdit->setPlaceholderText("请输入密码（必填）");
        passwordEdit->setMinimumHeight(40);
        formLayout->addRow("密码:", passwordEdit);
        
        confirmPasswordEdit = new QLineEdit();
        confirmPasswordEdit->setEchoMode(QLineEdit::Password);
        confirmPasswordEdit->setPlaceholderText("请再次输入密码");
        confirmPasswordEdit->setMinimumHeight(40);
        formLayout->addRow("确认密码:", confirmPasswordEdit);
    }
    
    // 真实姓名
    QLineEdit *realNameEdit = new QLineEdit(user.realName);
    realNameEdit->setPlaceholderText("请输入真实姓名");
    realNameEdit->setMinimumHeight(40);
    formLayout->addRow("真实姓名:", realNameEdit);
    
    // 角色
    QComboBox *roleCombo = new QComboBox();
    roleCombo->addItem("管理员", static_cast<int>(UserRole::Admin));
    roleCombo->addItem("操作员", static_cast<int>(UserRole::Operator));
    roleCombo->setCurrentIndex(static_cast<int>(user.role));
    roleCombo->setMinimumHeight(40);
    formLayout->addRow("角色:", roleCombo);
    
    // 状态
    QCheckBox *activeCheck = new QCheckBox("激活此用户");
    activeCheck->setChecked(user.isActive);
    formLayout->addRow("状态:", activeCheck);
    
    // 描述
    QTextEdit *descEdit = new QTextEdit(user.description);
    descEdit->setPlaceholderText("请输入描述信息（可选）");
    descEdit->setMaximumHeight(100);
    formLayout->addRow("描述:", descEdit);
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttonBox->button(QDialogButtonBox::Ok)->setText("确定");
    buttonBox->button(QDialogButtonBox::Cancel)->setText("取消");
    
    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(0, 0, 0, 20);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(buttonBox);
    
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    if (dialog.exec() == QDialog::Accepted) {
        // 验证输入
        QString username = usernameEdit->text().trimmed();
        if (username.isEmpty()) {
            StyledMessageBox::warning(this, "警告", "用户名不能为空！");
            return false;
        }
        
        // 检查用户名是否已存在
        if (m_userDao->usernameExists(username, user.id)) {
            StyledMessageBox::warning(this, "警告", "用户名已存在！");
            return false;
        }
        
        if (!isEdit) {
            QString password = passwordEdit->text();
            QString confirmPassword = confirmPasswordEdit->text();
            
            if (password.isEmpty()) {
                StyledMessageBox::warning(this, "警告", "密码不能为空！");
                return false;
            }
            
            if (password != confirmPassword) {
                StyledMessageBox::warning(this, "警告", "两次输入的密码不一致！");
                return false;
            }
            
            user.password = password;
        }
        
        user.username = username;
        user.realName = realNameEdit->text().trimmed();
        user.role = static_cast<UserRole>(roleCombo->currentData().toInt());
        user.isActive = activeCheck->isChecked();
        user.description = descEdit->toPlainText().trimmed();
        
        return true;
    }
    
    return false;
}

void UserManagement::updatePaginationInfo()
{
    // 更新总计标签
    ui->totalLabel->setText(QString("总计: %1 条").arg(m_allUsers.size()));
    
    // 更新页码信息
    ui->pageInfoLabel->setText(QString("第 %1 / %2 页").arg(m_currentPage).arg(m_totalPages));
    
    // 更新按钮状态
    ui->firstPageButton->setEnabled(m_currentPage > 1);
    ui->prevPageButton->setEnabled(m_currentPage > 1);
    ui->nextPageButton->setEnabled(m_currentPage < m_totalPages);
    ui->lastPageButton->setEnabled(m_currentPage < m_totalPages);
}

void UserManagement::onPageSizeChanged(int size)
{
    m_pageSize = size;
    m_currentPage = 1;  // 改变每页大小时回到第一页
    loadUsers();
}

void UserManagement::onFirstPageClicked()
{
    if (m_currentPage > 1) {
        m_currentPage = 1;
        loadUsers();
    }
}

void UserManagement::onPrevPageClicked()
{
    if (m_currentPage > 1) {
        m_currentPage--;
        loadUsers();
    }
}

void UserManagement::onNextPageClicked()
{
    if (m_currentPage < m_totalPages) {
        m_currentPage++;
        loadUsers();
    }
}

void UserManagement::onLastPageClicked()
{
    if (m_currentPage < m_totalPages) {
        m_currentPage = m_totalPages;
        loadUsers();
    }
}

QString UserManagement::getDialogStyleSheet()
{
    return R"(
        QDialog {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, 
                stop:0 #f0f9ff, 
                stop:0.5 #e0f2fe,
                stop:1 #dbeafe);
        }
        
        QLabel {
            color: #1e40af;
            font-size: 14px;
            font-weight: 600;
            background: transparent;
        }
        
        QLineEdit, QTextEdit {
            background-color: white;
            border: 2px solid #93c5fd;
            border-radius: 8px;
            padding: 8px 12px;
            color: #1e40af;
            font-size: 14px;
            selection-background-color: #3b82f6;
            selection-color: white;
        }
        
        QLineEdit:focus, QTextEdit:focus {
            border: 2px solid #3b82f6;
            background-color: #f0f9ff;
        }
        
        QComboBox {
            background-color: white;
            border: 2px solid #93c5fd;
            border-radius: 8px;
            padding: 8px 12px;
            color: #1e40af;
            font-size: 14px;
            font-weight: 600;
            min-height: 36px;
        }
        
        QComboBox:hover {
            border: 2px solid #60a5fa;
            background-color: #f0f9ff;
        }
        
        QComboBox::drop-down {
            border: none;
            width: 30px;
        }
        
        QComboBox::down-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 6px solid #3b82f6;
            margin-right: 8px;
        }
        
        QComboBox QAbstractItemView {
            background-color: white;
            border: 2px solid #93c5fd;
            border-radius: 8px;
            selection-background-color: #3b82f6;
            selection-color: white;
            padding: 4px;
        }
        
        QCheckBox {
            color: #1e40af;
            font-size: 14px;
            font-weight: 600;
            spacing: 8px;
            background: transparent;
        }
        
        QCheckBox::indicator {
            width: 20px;
            height: 20px;
            border: 2px solid #93c5fd;
            border-radius: 6px;
            background-color: white;
        }
        
        QCheckBox::indicator:hover {
            border: 2px solid #60a5fa;
            background-color: #f0f9ff;
        }
        
        QCheckBox::indicator:checked {
            background-color: #3b82f6;
            border: 2px solid #2563eb;
            image: none;
        }
        
        QCheckBox::indicator:checked:after {
            content: "✓";
            color: white;
            font-weight: bold;
        }
        
        QDialogButtonBox QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #dbeafe,
                stop:1 #bfdbfe);
            color: #1e40af;
            border: 2px solid #93c5fd;
            border-radius: 8px;
            padding: 10px 24px;
            font-weight: 700;
            font-size: 14px;
            min-width: 100px;
            min-height: 40px;
        }
        
        QDialogButtonBox QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #bfdbfe,
                stop:1 #93c5fd);
            border: 2px solid #60a5fa;
        }
        
        QDialogButtonBox QPushButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #60a5fa,
                stop:1 #3b82f6);
            border: 2px solid #2563eb;
            color: white;
        }
        
        QDialogButtonBox QPushButton[text="OK"],
        QDialogButtonBox QPushButton[text="确定"] {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #34d399,
                stop:1 #10b981);
            border: 2px solid #059669;
            color: white;
        }
        
        QDialogButtonBox QPushButton[text="OK"]:hover,
        QDialogButtonBox QPushButton[text="确定"]:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #10b981,
                stop:1 #059669);
            border: 2px solid #047857;
        }
    )";
}
