#ifndef USERMANAGEMENT_H
#define USERMANAGEMENT_H

#include <QWidget>
#include <QTableWidget>
#include "User.h"
#include "UserDao.h"

namespace Ui {
class UserManagement;
}

/**
 * @brief 用户管理界面
 */
class UserManagement : public QWidget {
    Q_OBJECT
    
public:
    explicit UserManagement(QWidget *parent = nullptr);
    ~UserManagement();
    
    /**
     * @brief 设置当前登录用户
     */
    void setCurrentUser(const User &user);
    
private slots:
    void onAddUserClicked();
    void onEditUserClicked();
    void onDeleteUserClicked();
    void onResetPasswordClicked();
    void onRefreshClicked();
    void onTableSelectionChanged();
    void onPageSizeChanged(int size);
    void onFirstPageClicked();
    void onPrevPageClicked();
    void onNextPageClicked();
    void onLastPageClicked();
    
private:
    void setupUi();
    void loadUsers();
    void updateButtonStates();
    void updatePaginationInfo();
    bool showUserDialog(User &user, bool isEdit);
    QString getDialogStyleSheet();
    
    Ui::UserManagement *ui;
    UserDao *m_userDao;
    User m_currentUser;
    int m_selectedUserId;
    
    // 分页相关
    QList<User> m_allUsers;
    int m_currentPage;
    int m_pageSize;
    int m_totalPages;
};

#endif // USERMANAGEMENT_H
