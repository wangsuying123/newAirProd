#ifndef USER_H
#define USER_H

#include <QString>
#include <QDateTime>

/**
 * @brief 用户角色枚举
 */
enum class UserRole {
    Admin = 0,      // 管理员 - 拥有所有权限
    Operator = 1    // 操作员 - 可以操作设备和查看数据
};

/**
 * @brief 用户类
 */
class User {
public:
    User() : id(-1), role(UserRole::Operator), isActive(true) {}
    
    int id;                     // 用户ID
    QString username;           // 用户名
    QString password;           // 密码（加密存储）
    QString realName;           // 真实姓名
    UserRole role;              // 用户角色
    bool isActive;              // 是否激活
    QDateTime createdAt;        // 创建时间
    QDateTime lastLoginAt;      // 最后登录时间
    QString description;        // 描述
    
    /**
     * @brief 获取角色名称
     */
    QString getRoleName() const {
        switch (role) {
            case UserRole::Admin: return "管理员";
            case UserRole::Operator: return "操作员";
            default: return "未知";
        }
    }
    
    /**
     * @brief 检查是否有管理员权限
     */
    bool isAdmin() const {
        return role == UserRole::Admin;
    }
    
    /**
     * @brief 检查是否有操作权限
     */
    bool canOperate() const {
        return role == UserRole::Admin || role == UserRole::Operator;
    }
};

#endif // USER_H
