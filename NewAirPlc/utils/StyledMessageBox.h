#ifndef STYLEDMESSAGEBOX_H
#define STYLEDMESSAGEBOX_H

#include <QMessageBox>
#include <QString>
#include <QWidget>

class StyledMessageBox
{
public:
    // 信息提示框
    static void information(QWidget *parent, const QString &title, const QString &text);
    
    // 警告提示框
    static void warning(QWidget *parent, const QString &title, const QString &text);
    
    // 错误提示框
    static void critical(QWidget *parent, const QString &title, const QString &text);
    
    // 成功提示框
    static void success(QWidget *parent, const QString &title, const QString &text);
    
    // 询问对话框（是/否）
    static QMessageBox::StandardButton question(QWidget *parent, const QString &title, const QString &text);
    
    // 询问对话框（是/否/取消）
    static QMessageBox::StandardButton questionWithCancel(QWidget *parent, const QString &title, const QString &text);

private:
    // 应用样式到消息框
    static void applyStyle(QMessageBox *msgBox, const QString &iconColor, const QString &icon, const QString &typeText);
    
    // 获取通用样式表
    static QString getBaseStyleSheet();
};

#endif // STYLEDMESSAGEBOX_H
