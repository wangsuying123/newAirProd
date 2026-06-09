// barcode_scanner.h
#ifndef BARCODE_SCANNER_H
#define BARCODE_SCANNER_H

#include <QObject>
#include <QEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QElapsedTimer>
#include <QDebug>

class BarcodeScanner : public QObject
{
    Q_OBJECT
public:
    explicit BarcodeScanner(QObject *parent = nullptr) : QObject(parent) {
        // 扫码结束判断定时器（间隔150ms，超过则视为输入结束）
        m_scanTimer = new QTimer(this);
        m_scanTimer->setSingleShot(true); // 单次触发
        m_scanTimer->setInterval(150);
        connect(m_scanTimer, &QTimer::timeout, this, &BarcodeScanner::onScanCompleted);

        // 记录输入时间，用于区分扫码和人工输入
        m_elapsedTimer = new QElapsedTimer();
    }

signals:
    void barcodeReceived(const QString &barcode); // 扫码完成信号

private slots:
    // 处理扫码结束逻辑
    void onScanCompleted() {
        QString barcode = m_buffer.trimmed(); // 去除首尾空格/换行
        qDebug() << "BarcodeScanner completed, buffer:" << barcode;
        if (!barcode.isEmpty()) {
            emit barcodeReceived(barcode);
        }
        m_buffer.clear(); // 清空缓冲，准备下次扫码
    }

private:
    QTimer *m_scanTimer;         // 扫码结束判断定时器
    QElapsedTimer *m_elapsedTimer; // 输入间隔计时器
    QString m_buffer;            // 扫码缓冲区

    // 重写事件过滤，捕获键盘输入（全局，不依赖输入框聚焦）
    bool eventFilter(QObject *watched, QEvent *event) override {
        Q_UNUSED(watched);
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

            // 扫码通常以回车（Enter）结束，优先通过回车判断
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                qDebug() << "BarcodeScanner: Enter pressed";
                onScanCompleted();
                return false; // 不拦截回车，允许原目标继续处理
            }

            // 非回车按键：记录输入并启动定时器
            const QString text = keyEvent->text();
            qDebug() << "BarcodeScanner key:" << keyEvent->key() << "text:" << text;
            if (!text.isEmpty()) {
                if (!m_elapsedTimer->isValid()) {
                    m_elapsedTimer->start(); // 首次输入，启动计时器
                } else {
                    // 若输入间隔>300ms，视为人工输入（扫码间隔通常<100ms），重置缓冲
                    if (m_elapsedTimer->elapsed() > 300) {
                        qDebug() << "BarcodeScanner: gap >300ms, reset buffer";
                        m_buffer.clear();
                    }
                    m_elapsedTimer->restart(); // 重置计时器
                }

                // 将按键内容追加到缓冲区
                m_buffer.append(text);
                m_scanTimer->start(); // 重启定时器（等待下一个字符）
            }
            return false; // 不拦截按键，允许原目标继续处理
        }
        return QObject::eventFilter(watched, event);
    }
};

#endif // BARCODE_SCANNER_H
