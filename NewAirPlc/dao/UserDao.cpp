#include "UserDao.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QCryptographicHash>
#include <QDebug>

UserDao::UserDao(QObject *parent) : QObject(parent) {
    m_db = QSqlDatabase::database();
}

UserDao::~UserDao() {
}

bool UserDao::initTable() {
    QSqlQuery query(m_db);
    
    QString createTableSql = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL UNIQUE,
            password TEXT NOT NULL,
            real_name TEXT,
            role INTEGER NOT NULL DEFAULT 2,
            is_active INTEGER NOT NULL DEFAULT 1,
            created_at TEXT NOT NULL,
            last_login_at TEXT,
            description TEXT
        )
    )";
    
    if (!query.exec(createTableSql)) {
        qDebug() << "创建用户表失败:" << query.lastError().text();
        return false;
    }
    
    qDebug() << "用户表初始化成功";
    return true;
}

bool UserDao::createDefaultAdmin() {
    // 检查是否已存在管理员
    QSqlQuery query(m_db);
    query.prepare("SELECT COUNT(*) FROM users WHERE role = 0");
    
    if (query.exec() && query.next()) {
        int adminCount = query.value(0).toInt();
        if (adminCount > 0) {
            qDebug() << "管理员账户已存在";
            return true;
        }
    }
    
    // 创建默认管理员
    User admin;
    admin.username = "admin";
    admin.password = encryptPassword("admin123");
    admin.realName = "系统管理员";
    admin.role = UserRole::Admin;
    admin.isActive = true;
    admin.createdAt = QDateTime::currentDateTime();
    admin.description = "默认管理员账户";
    
    return addUser(admin);
}

bool UserDao::addUser(const User &user) {
    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT INTO users (username, password, real_name, role, is_active, created_at, description)
        VALUES (:username, :password, :real_name, :role, :is_active, :created_at, :description)
    )");
    
    query.bindValue(":username", user.username);
    query.bindValue(":password", user.password);
    query.bindValue(":real_name", user.realName);
    query.bindValue(":role", static_cast<int>(user.role));
    query.bindValue(":is_active", user.isActive ? 1 : 0);
    query.bindValue(":created_at", user.createdAt.toString(Qt::ISODate));
    query.bindValue(":description", user.description);
    
    if (!query.exec()) {
        qDebug() << "添加用户失败:" << query.lastError().text();
        return false;
    }
    
    qDebug() << "用户添加成功:" << user.username;
    return true;
}

bool UserDao::updateUser(const User &user) {
    QSqlQuery query(m_db);
    query.prepare(R"(
        UPDATE users 
        SET username = :username, real_name = :real_name, role = :role, 
            is_active = :is_active, description = :description
        WHERE id = :id
    )");
    
    query.bindValue(":id", user.id);
    query.bindValue(":username", user.username);
    query.bindValue(":real_name", user.realName);
    query.bindValue(":role", static_cast<int>(user.role));
    query.bindValue(":is_active", user.isActive ? 1 : 0);
    query.bindValue(":description", user.description);
    
    if (!query.exec()) {
        qDebug() << "更新用户失败:" << query.lastError().text();
        return false;
    }
    
    qDebug() << "用户更新成功:" << user.username;
    return true;
}

bool UserDao::deleteUser(int userId) {
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM users WHERE id = :id");
    query.bindValue(":id", userId);
    
    if (!query.exec()) {
        qDebug() << "删除用户失败:" << query.lastError().text();
        return false;
    }
    
    qDebug() << "用户删除成功, ID:" << userId;
    return true;
}

User UserDao::getUserById(int userId) {
    User user;
    QSqlQuery query(m_db);
    query.prepare("SELECT * FROM users WHERE id = :id");
    query.bindValue(":id", userId);
    
    if (query.exec() && query.next()) {
        user.id = query.value("id").toInt();
        user.username = query.value("username").toString();
        user.password = query.value("password").toString();
        user.realName = query.value("real_name").toString();
        user.role = static_cast<UserRole>(query.value("role").toInt());
        user.isActive = query.value("is_active").toInt() == 1;
        user.createdAt = QDateTime::fromString(query.value("created_at").toString(), Qt::ISODate);
        user.lastLoginAt = QDateTime::fromString(query.value("last_login_at").toString(), Qt::ISODate);
        user.description = query.value("description").toString();
    }
    
    return user;
}

User UserDao::getUserByUsername(const QString &username) {
    User user;
    QSqlQuery query(m_db);
    query.prepare("SELECT * FROM users WHERE username = :username");
    query.bindValue(":username", username);
    
    if (query.exec() && query.next()) {
        user.id = query.value("id").toInt();
        user.username = query.value("username").toString();
        user.password = query.value("password").toString();
        user.realName = query.value("real_name").toString();
        user.role = static_cast<UserRole>(query.value("role").toInt());
        user.isActive = query.value("is_active").toInt() == 1;
        user.createdAt = QDateTime::fromString(query.value("created_at").toString(), Qt::ISODate);
        user.lastLoginAt = QDateTime::fromString(query.value("last_login_at").toString(), Qt::ISODate);
        user.description = query.value("description").toString();
    }
    
    return user;
}

QList<User> UserDao::getAllUsers() {
    QList<User> users;
    QSqlQuery query(m_db);
    
    if (query.exec("SELECT * FROM users ORDER BY id")) {
        while (query.next()) {
            User user;
            user.id = query.value("id").toInt();
            user.username = query.value("username").toString();
            user.password = query.value("password").toString();
            user.realName = query.value("real_name").toString();
            user.role = static_cast<UserRole>(query.value("role").toInt());
            user.isActive = query.value("is_active").toInt() == 1;
            user.createdAt = QDateTime::fromString(query.value("created_at").toString(), Qt::ISODate);
            user.lastLoginAt = QDateTime::fromString(query.value("last_login_at").toString(), Qt::ISODate);
            user.description = query.value("description").toString();
            users.append(user);
        }
    }
    
    return users;
}

User UserDao::validateLogin(const QString &username, const QString &password) {
    User user = getUserByUsername(username);
    
    if (user.id > 0 && user.isActive) {
        QString encryptedPassword = encryptPassword(password);
        if (user.password == encryptedPassword) {
            return user;
        }
    }
    
    return User(); // 返回空用户表示登录失败
}

bool UserDao::updateLastLoginTime(int userId) {
    QSqlQuery query(m_db);
    query.prepare("UPDATE users SET last_login_at = :last_login_at WHERE id = :id");
    query.bindValue(":id", userId);
    query.bindValue(":last_login_at", QDateTime::currentDateTime().toString(Qt::ISODate));
    
    return query.exec();
}

bool UserDao::changePassword(int userId, const QString &oldPassword, const QString &newPassword) {
    User user = getUserById(userId);
    
    if (user.id <= 0) {
        return false;
    }
    
    QString encryptedOldPassword = encryptPassword(oldPassword);
    if (user.password != encryptedOldPassword) {
        qDebug() << "旧密码不正确";
        return false;
    }
    
    QSqlQuery query(m_db);
    query.prepare("UPDATE users SET password = :password WHERE id = :id");
    query.bindValue(":id", userId);
    query.bindValue(":password", encryptPassword(newPassword));
    
    if (!query.exec()) {
        qDebug() << "修改密码失败:" << query.lastError().text();
        return false;
    }
    
    qDebug() << "密码修改成功";
    return true;
}

bool UserDao::resetPassword(int userId, const QString &newPassword) {
    QSqlQuery query(m_db);
    query.prepare("UPDATE users SET password = :password WHERE id = :id");
    query.bindValue(":id", userId);
    query.bindValue(":password", encryptPassword(newPassword));
    
    if (!query.exec()) {
        qDebug() << "重置密码失败:" << query.lastError().text();
        return false;
    }
    
    qDebug() << "密码重置成功";
    return true;
}

bool UserDao::usernameExists(const QString &username, int excludeUserId) {
    QSqlQuery query(m_db);
    
    if (excludeUserId > 0) {
        query.prepare("SELECT COUNT(*) FROM users WHERE username = :username AND id != :id");
        query.bindValue(":username", username);
        query.bindValue(":id", excludeUserId);
    } else {
        query.prepare("SELECT COUNT(*) FROM users WHERE username = :username");
        query.bindValue(":username", username);
    }
    
    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }
    
    return false;
}

QString UserDao::encryptPassword(const QString &password) {
    // 使用SHA256加密密码
    QByteArray hash = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);
    return QString(hash.toHex());
}
