#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include "User.h"
#include "UserDao.h"

namespace Ui {
class LoginWindow;
}

/**
 * @brief 登录窗口
 */
class LoginWindow : public QDialog {
    Q_OBJECT
    
public:
    explicit LoginWindow(QWidget *parent = nullptr);
    ~LoginWindow();
    
    /**
     * @brief 获取登录的用户信息
     */
    User getLoggedInUser() const { return m_loggedInUser; }
    
    /**
     * @brief 检查是否登录成功
     */
    bool isLoginSuccessful() const { return m_loginSuccessful; }
    
private slots:
    void onLoginClicked();
    void onCancelClicked();
    void onUsernameChanged(const QString &text);
    void onPasswordChanged(const QString &text);
    
private:
    void setupUi();
    void loadRememberedCredentials();
    void saveCredentials();
    void clearCredentials();
    bool validateLogin(const QString &username, const QString &password);
    
    Ui::LoginWindow *ui;
    UserDao *m_userDao;
    User m_loggedInUser;
    bool m_loginSuccessful;
};

#endif // LOGINWINDOW_H
