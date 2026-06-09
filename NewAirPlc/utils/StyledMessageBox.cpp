#include "StyledMessageBox.h"
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGraphicsDropShadowEffect>

void StyledMessageBox::information(QWidget *parent, const QString &title, const QString &text)
{
    QMessageBox msgBox(parent);
    msgBox.setWindowTitle(title);
    msgBox.setText(text);
    msgBox.setIcon(QMessageBox::Information);
    applyStyle(&msgBox, "#3b82f6", "ℹ️", "信息");
    msgBox.exec();
}

void StyledMessageBox::warning(QWidget *parent, const QString &title, const QString &text)
{
    QMessageBox msgBox(parent);
    msgBox.setWindowTitle(title);
    msgBox.setText(text);
    msgBox.setIcon(QMessageBox::Warning);
    applyStyle(&msgBox, "#f59e0b", "⚠️", "警告");
    msgBox.exec();
}

void StyledMessageBox::critical(QWidget *parent, const QString &title, const QString &text)
{
    QMessageBox msgBox(parent);
    msgBox.setWindowTitle(title);
    msgBox.setText(text);
    msgBox.setIcon(QMessageBox::Critical);
    applyStyle(&msgBox, "#ef4444", "❌", "错误");
    msgBox.exec();
}

void StyledMessageBox::success(QWidget *parent, const QString &title, const QString &text)
{
    QMessageBox msgBox(parent);
    msgBox.setWindowTitle(title);
    msgBox.setText(text);
    msgBox.setIcon(QMessageBox::Information);
    applyStyle(&msgBox, "#10b981", "✅", "成功");
    msgBox.exec();
}

QMessageBox::StandardButton StyledMessageBox::question(QWidget *parent, const QString &title, const QString &text)
{
    QMessageBox msgBox(parent);
    msgBox.setWindowTitle(title);
    msgBox.setText(text);
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    
    // 设置按钮文本为中文
    msgBox.button(QMessageBox::Yes)->setText("是");
    msgBox.button(QMessageBox::No)->setText("否");
    
    applyStyle(&msgBox, "#8b5cf6", "❓", "确认");
    return (QMessageBox::StandardButton)msgBox.exec();
}

QMessageBox::StandardButton StyledMessageBox::questionWithCancel(QWidget *parent, const QString &title, const QString &text)
{
    QMessageBox msgBox(parent);
    msgBox.setWindowTitle(title);
    msgBox.setText(text);
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);
    
    // 设置按钮文本为中文
    msgBox.button(QMessageBox::Yes)->setText("是");
    msgBox.button(QMessageBox::No)->setText("否");
    msgBox.button(QMessageBox::Cancel)->setText("取消");
    
    applyStyle(&msgBox, "#8b5cf6", "❓", "确认");
    return (QMessageBox::StandardButton)msgBox.exec();
}

void StyledMessageBox::applyStyle(QMessageBox *msgBox, const QString &iconColor, const QString &icon, const QString &typeText)
{
    // 设置消息框的样式
    msgBox->setStyleSheet(getBaseStyleSheet());
    
    // 设置图标文本（使用 Emoji）
    msgBox->setIconPixmap(QPixmap());  // 清除默认图标
    
    // 创建更美观的文本布局
    QString styledText = QString(
        "<div style='padding: 20px;'>"
        "  <div style='display: flex; align-items: center; margin-bottom: 20px;'>"
        "    <div style='font-size: 48px; margin-right: 20px; line-height: 1;'>%1</div>"
        "    <div>"
        "      <div style='font-size: 16px; font-weight: 600; color: %2; margin-bottom: 8px;'>%3</div>"
        "      <div style='font-size: 14px; color: #374151; line-height: 1.6;'>%4</div>"
        "    </div>"
        "  </div>"
        "</div>"
    ).arg(icon, iconColor, typeText, msgBox->text());
    
    msgBox->setText(styledText);
    msgBox->setTextFormat(Qt::RichText);
    
    // 设置最小宽度
    msgBox->setMinimumWidth(450);
    
    // 美化按钮
    QList<QPushButton*> buttons = msgBox->findChildren<QPushButton*>();
    for (QPushButton *button : buttons) {
        if (msgBox->buttonRole(button) == QMessageBox::AcceptRole || 
            msgBox->buttonRole(button) == QMessageBox::YesRole) {
            // 确定/是 按钮 - 蓝色主题
            button->setStyleSheet(
                "QPushButton {"
                "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                "        stop:0 #3b82f6, stop:1 #2563eb);"
                "    color: white;"
                "    border: none;"
                "    border-radius: 8px;"
                "    padding: 12px 32px;"
                "    font-size: 15px;"
                "    font-weight: 600;"
                "    min-width: 100px;"
                "    min-height: 40px;"
                "}"
                "QPushButton:hover {"
                "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                "        stop:0 #60a5fa, stop:1 #3b82f6);"
                "    transform: translateY(-1px);"
                "}"
                "QPushButton:pressed {"
                "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                "        stop:0 #2563eb, stop:1 #1e40af);"
                "    transform: translateY(0px);"
                "}"
            );
            
            // 添加阴影效果
            QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect();
            shadow->setBlurRadius(15);
            shadow->setColor(QColor(59, 130, 246, 80));
            shadow->setOffset(0, 4);
            button->setGraphicsEffect(shadow);
            
        } else if (msgBox->buttonRole(button) == QMessageBox::RejectRole || 
                   msgBox->buttonRole(button) == QMessageBox::NoRole) {
            // 取消/否 按钮 - 灰色主题
            button->setStyleSheet(
                "QPushButton {"
                "    background: white;"
                "    color: #6b7280;"
                "    border: 2px solid #e5e7eb;"
                "    border-radius: 8px;"
                "    padding: 12px 32px;"
                "    font-size: 15px;"
                "    font-weight: 600;"
                "    min-width: 100px;"
                "    min-height: 40px;"
                "}"
                "QPushButton:hover {"
                "    background: #f9fafb;"
                "    border-color: #d1d5db;"
                "    color: #374151;"
                "}"
                "QPushButton:pressed {"
                "    background: #f3f4f6;"
                "    border-color: #9ca3af;"
                "}"
            );
        }
    }
}

QString StyledMessageBox::getBaseStyleSheet()
{
    return QString(
        "QMessageBox {"
        "    background-color: #ffffff;"
        "    border: none;"
        "    border-radius: 16px;"
        "}"
        "QMessageBox QLabel {"
        "    color: #1f2937;"
        "    font-size: 14px;"
        "    background: transparent;"
        "    padding: 0px;"
        "    min-width: 400px;"
        "}"
        "QMessageBox QPushButton {"
        "    margin: 8px 6px;"
        "}"
        "QMessageBox QDialogButtonBox {"
        "    padding: 0px 20px 20px 20px;"
        "}"
    );
}
