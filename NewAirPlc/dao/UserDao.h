#ifndef USERDAO_H
#define USERDAO_H

#include <QObject>
#include <QSqlDatabase>
#include <QList>
#include "User.h"

/**
 * @brief 用户数据访问对象
 */
class UserDao : public QObject {
    Q_OBJECT
    
public:
    explicit UserDao(QObject *parent = nullptr);
    ~UserDao();
    
    /**
     * @brief 初始化用户表
     */
    bool initTable();
    
    /**
     * @brief 创建默认管理员账户
     */
    bool createDefaultAdmin();
    
    /**
     * @brief 添加用户
     */
    bool addUser(const User &user);
    
    /**
     * @brief 更新用户
     */
    bool updateUser(const User &user);
    
    /**
     * @brief 删除用户
     */
    bool deleteUser(int userId);
    
    /**
     * @brief 根据ID获取用户
     */
    User getUserById(int userId);
    
    /**
     * @brief 根据用户名获取用户
     */
    User getUserByUsername(const QString &username);
    
    /**
     * @brief 获取所有用户
     */
    QList<User> getAllUsers();
    
    /**
     * @brief 验证用户登录
     */
    User validateLogin(const QString &username, const QString &password);
    
    /**
     * @brief 更新最后登录时间
     */
    bool updateLastLoginTime(int userId);
    
    /**
     * @brief 修改密码
     */
    bool changePassword(int userId, const QString &oldPassword, const QString &newPassword);
    
    /**
     * @brief 重置密码（管理员功能）
     */
    bool resetPassword(int userId, const QString &newPassword);
    
    /**
     * @brief 检查用户名是否存在
     */
    bool usernameExists(const QString &username, int excludeUserId = -1);
    
    /**
     * @brief 密码加密
     */
    static QString encryptPassword(const QString &password);
    
private:
    QSqlDatabase m_db;
};

#endif // USERDAO_H
