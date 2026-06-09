#ifndef MACHINELOCKMANAGER_H
#define MACHINELOCKMANAGER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include "machinefingerprint.h"

/**
 * @brief 机器锁定管理器 - 管理程序与机器的绑定
 *
 * 支持永久授权和限时授权，授权信息加密存储。
 * 程序只能在授权的机器上运行，复制到其他机器无法使用。
 */
class MachineLockManager : public QObject
{
    Q_OBJECT

public:
    explicit MachineLockManager(QObject *parent = nullptr);
    ~MachineLockManager();

    // 验证当前机器是否已授权（主入口）
    bool verifyMachineLicense();

    // 生成授权文件
    bool generateLicenseFile(const QString &filePath,
                             const QDateTime &startTime = QDateTime(),
                             const QDateTime &endTime   = QDateTime());

    // 读取授权文件
    bool loadLicenseFile(const QString &filePath);

    // 获取当前机器指纹
    QString getCurrentMachineFingerprint();

    // 获取已授权的机器指纹
    QString getAuthorizedFingerprint() const { return m_authorizedFingerprint; }

    // 获取授权信息文本
    QString getLicenseInfo();

    // 检查授权是否过期
    bool isLicenseExpired();

    // 获取授权剩余天数（-1 = 永久授权）
    int getRemainingDays();

private:
    bool     checkTimeIntegrity();
    void     recordCurrentTime();
    QDateTime readLastRecordedTime();

    QString   encryptLicenseData(const QString &data);
    QString   decryptLicenseData(const QString &encryptedData);
    QByteArray generateKey();
    QByteArray generateRandomIV();
    QByteArray encryptAES(const QByteArray &data, const QByteArray &key, const QByteArray &iv);
    QByteArray decryptAES(const QByteArray &encryptedData, const QByteArray &key, const QByteArray &iv);

    MachineFingerprint *m_fingerprint;
    QString   m_authorizedFingerprint;
    QDateTime m_startTime;
    QDateTime m_endTime;
    bool      m_isPermanent;
};

#endif // MACHINELOCKMANAGER_H
