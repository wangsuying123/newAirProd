#include "machinelockmanager.h"
#include <QFile>
#include <QTextStream>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QDebug>
#include <QCoreApplication>
#include <QProcess>

// ---- 程序唯一标识（与 ProductAirTightness 区分） ----
static const QString PROGRAM_ID       = "NEW_AIR_PLC_MACHINE_V1";
static const QString BASE_KEY         = "MACHINE_LOCK_ENCRYPTION_KEY_2026";
static const QString LICENSE_FILE_NAME = ".machine_license";
static const QString TIME_RECORD_FILE  = ".time_record";

MachineLockManager::MachineLockManager(QObject *parent)
    : QObject(parent),
      m_fingerprint(new MachineFingerprint()),
      m_isPermanent(true)
{}

MachineLockManager::~MachineLockManager()
{
    delete m_fingerprint;
}

// ─────────────────────────────────────────────────────────────
//  公共接口
// ─────────────────────────────────────────────────────────────

QString MachineLockManager::getCurrentMachineFingerprint()
{
    return m_fingerprint->getMachineFingerprint();
}

bool MachineLockManager::verifyMachineLicense()
{
    qDebug() << "[机器锁定] ========== 开始验证机器授权 ==========";

    if (!checkTimeIntegrity()) {
        qDebug() << "[机器锁定] ✗ 检测到系统时间被篡改";
        return false;
    }

    QString licenseFilePath = QCoreApplication::applicationDirPath() + "/" + LICENSE_FILE_NAME;
    qDebug() << "[机器锁定] 授权文件路径:" << licenseFilePath;

    if (!loadLicenseFile(licenseFilePath)) {
        qDebug() << "[机器锁定] ✗ 授权文件读取失败";
        return false;
    }

    QString current = m_fingerprint->getMachineFingerprint();
    if (current != m_authorizedFingerprint) {
        qDebug() << "[机器锁定] ✗ 机器指纹不匹配";
        qDebug() << "[机器锁定] 期望:" << m_authorizedFingerprint;
        qDebug() << "[机器锁定] 实际:" << current;
        return false;
    }
    qDebug() << "[机器锁定] ✓ 机器指纹验证通过";

    if (!m_isPermanent) {
        QDateTime now = QDateTime::currentDateTime();
        if (m_startTime.isValid() && now < m_startTime) {
            qDebug() << "[机器锁定] ✗ 授权尚未生效";
            return false;
        }
        if (m_endTime.isValid() && now > m_endTime) {
            qDebug() << "[机器锁定] ✗ 授权已过期";
            return false;
        }
        qDebug() << "[机器锁定] ✓ 时间授权验证通过";
    } else {
        qDebug() << "[机器锁定] ✓ 永久授权";
    }

    recordCurrentTime();
    qDebug() << "[机器锁定] ========== 机器授权验证通过！ ==========";
    return true;
}

bool MachineLockManager::generateLicenseFile(const QString &filePath,
                                              const QDateTime &startTime,
                                              const QDateTime &endTime)
{
    qDebug() << "[机器锁定] 开始生成授权文件:" << filePath;

    QString fingerprint  = m_fingerprint->getMachineFingerprint();
    QString startTimeStr = startTime.isValid() ? startTime.toString("yyyyMMddHHmmss") : "";
    QString endTimeStr   = endTime.isValid()   ? endTime.toString("yyyyMMddHHmmss")   : "";

    // 格式: PROGRAM_ID|fingerprint|startTime|endTime
    QString licenseData = PROGRAM_ID + "|" + fingerprint + "|" + startTimeStr + "|" + endTimeStr;
    qDebug() << "[机器锁定] 授权数据:" << licenseData;

    QString encryptedData = encryptLicenseData(licenseData);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "[错误] 无法创建授权文件:" << file.errorString();
        return false;
    }
    QTextStream(&file) << encryptedData;
    file.close();

    // 设置为隐藏文件
    QProcess p;
    p.start("cmd.exe", QStringList() << "/c" << "attrib" << "+h" << filePath);
    p.waitForFinished();

    qDebug() << "[机器锁定] 授权文件生成成功";
    return true;
}

bool MachineLockManager::loadLicenseFile(const QString &filePath)
{
    qDebug() << "[机器锁定] 读取授权文件:" << filePath;

    QFile file(filePath);
    if (!file.exists()) { qDebug() << "[错误] 授权文件不存在"; return false; }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "[错误] 无法打开授权文件:" << file.errorString();
        return false;
    }
    QString encryptedData = file.readAll().trimmed();
    file.close();

    QString licenseData = decryptLicenseData(encryptedData);
    if (licenseData.isEmpty()) { qDebug() << "[错误] 授权数据解密失败"; return false; }

    qDebug() << "[机器锁定] 解密后的授权数据:" << licenseData;

    QStringList parts = licenseData.split("|");
    if (parts.size() < 2) { qDebug() << "[错误] 授权数据格式错误"; return false; }

    if (parts[0] != PROGRAM_ID) { qDebug() << "[错误] 程序ID不匹配"; return false; }

    m_authorizedFingerprint = parts[1];

    if (parts.size() >= 4) {
        if (!parts[2].isEmpty())
            m_startTime = QDateTime::fromString(parts[2], "yyyyMMddHHmmss");
        if (!parts[3].isEmpty()) {
            m_endTime    = QDateTime::fromString(parts[3], "yyyyMMddHHmmss");
            m_isPermanent = false;
        } else {
            m_isPermanent = true;
        }
    }

    qDebug() << "[机器锁定] 授权文件读取成功";
    return true;
}

bool MachineLockManager::isLicenseExpired()
{
    if (m_isPermanent || !m_endTime.isValid()) return false;
    return QDateTime::currentDateTime() > m_endTime;
}

int MachineLockManager::getRemainingDays()
{
    if (m_isPermanent || !m_endTime.isValid()) return -1;
    qint64 secs = QDateTime::currentDateTime().secsTo(m_endTime);
    return static_cast<int>(secs / (24 * 3600));
}

QString MachineLockManager::getLicenseInfo()
{
    QString info;
    info += "程序ID: " + PROGRAM_ID + "\n";
    info += "机器指纹: " + m_authorizedFingerprint.left(32) + "...\n";
    if (m_isPermanent) {
        info += "授权类型: 永久授权\n";
    } else {
        info += "授权类型: 限时授权\n";
        if (m_startTime.isValid())
            info += "开始时间: " + m_startTime.toString("yyyy-MM-dd HH:mm:ss") + "\n";
        if (m_endTime.isValid()) {
            info += "结束时间: " + m_endTime.toString("yyyy-MM-dd HH:mm:ss") + "\n";
            info += "剩余天数: " + QString::number(getRemainingDays()) + " 天\n";
        }
    }
    return info;
}

// ─────────────────────────────────────────────────────────────
//  时间完整性检测
// ─────────────────────────────────────────────────────────────

bool MachineLockManager::checkTimeIntegrity()
{
    QDateTime last = readLastRecordedTime();
    if (!last.isValid()) {
        qDebug() << "[机器锁定] 首次运行，无时间记录";
        return true;
    }

    qint64 diff = last.secsTo(QDateTime::currentDateTime());
    if (diff < -10) {
        qDebug() << "[机器锁定] 检测到时间回调！上次:" << last.toString("yyyy-MM-dd HH:mm:ss")
                 << "| 当前:" << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")
                 << "| 差值:" << diff << "秒";
        return false;
    }
    qDebug() << "[机器锁定] ✓ 时间完整性验证通过";
    return true;
}

void MachineLockManager::recordCurrentTime()
{
    QString path = QCoreApplication::applicationDirPath() + "/" + TIME_RECORD_FILE;
    QString encryptedTime = encryptLicenseData(QDateTime::currentDateTime().toString("yyyyMMddHHmmss"));

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream(&file) << encryptedTime;
        file.close();
        QProcess p;
        p.start("cmd.exe", QStringList() << "/c" << "attrib" << "+h" << path);
        p.waitForFinished();
        qDebug() << "[机器锁定] 时间记录已更新";
    }
}

QDateTime MachineLockManager::readLastRecordedTime()
{
    QString path = QCoreApplication::applicationDirPath() + "/" + TIME_RECORD_FILE;
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QDateTime();

    QString encryptedTime = file.readAll().trimmed();
    file.close();

    QString timeStr = decryptLicenseData(encryptedTime);
    if (timeStr.isEmpty()) return QDateTime();

    return QDateTime::fromString(timeStr, "yyyyMMddHHmmss");
}

// ─────────────────────────────────────────────────────────────
//  加密 / 解密
// ─────────────────────────────────────────────────────────────

QByteArray MachineLockManager::generateKey()
{
    return QCryptographicHash::hash((BASE_KEY + PROGRAM_ID).toUtf8(), QCryptographicHash::Sha256);
}

QByteArray MachineLockManager::generateRandomIV()
{
    QByteArray iv(16, 0);
    for (int i = 0; i < 16; i++)
        iv[i] = static_cast<char>(QRandomGenerator::global()->generate() % 256);
    return iv;
}

QByteArray MachineLockManager::encryptAES(const QByteArray &data,
                                           const QByteArray &key,
                                           const QByteArray &iv)
{
    if (key.size() != 32 || iv.size() != 16) return {};

    QByteArray padded = data;
    int pad = 16 - (padded.size() % 16);
    padded.append(QByteArray(pad, static_cast<char>(pad)));

    QByteArray result;
    for (int i = 0; i < padded.size(); i += 16) {
        QByteArray block = padded.mid(i, 16);
        QByteArray bk = QCryptographicHash::hash(
            (QString::fromUtf8(key) + QString::fromUtf8(iv) + QString::number(i)).toUtf8(),
            QCryptographicHash::Sha256);
        for (int j = 0; j < block.size(); j++)
            result.append(static_cast<char>(block[j] ^ bk[j % bk.size()]));
    }
    return result;
}

QByteArray MachineLockManager::decryptAES(const QByteArray &encryptedData,
                                           const QByteArray &key,
                                           const QByteArray &iv)
{
    if (key.size() != 32 || iv.size() != 16) return {};

    QByteArray result;
    for (int i = 0; i < encryptedData.size(); i += 16) {
        QByteArray block = encryptedData.mid(i, 16);
        QByteArray bk = QCryptographicHash::hash(
            (QString::fromUtf8(key) + QString::fromUtf8(iv) + QString::number(i)).toUtf8(),
            QCryptographicHash::Sha256);
        for (int j = 0; j < block.size(); j++)
            result.append(static_cast<char>(block[j] ^ bk[j % bk.size()]));
    }

    if (!result.isEmpty()) {
        int pad = static_cast<unsigned char>(result.back());
        if (pad > 0 && pad <= 16) result.chop(pad);
    }
    return result;
}

QString MachineLockManager::encryptLicenseData(const QString &data)
{
    QByteArray key = generateKey();
    QByteArray iv  = generateRandomIV();
    QByteArray enc = encryptAES(data.toUtf8(), key, iv);

    QByteArray combined;
    quint32 len = static_cast<quint32>(enc.size());
    combined.append(reinterpret_cast<const char*>(&len), sizeof(len));
    combined.append(enc);
    combined.append(iv);
    return combined.toBase64();
}

QString MachineLockManager::decryptLicenseData(const QString &encryptedData)
{
    QByteArray combined = QByteArray::fromBase64(encryptedData.toUtf8());
    if (combined.size() < 20) { qDebug() << "[错误] 授权数据太短"; return ""; }

    quint32 encLen = *reinterpret_cast<const quint32*>(combined.constData());
    if (static_cast<int>(4 + encLen + 16) != combined.size()) {
        qDebug() << "[错误] 授权数据格式错误";
        return "";
    }

    QByteArray enc = combined.mid(4, encLen);
    QByteArray iv  = combined.mid(4 + encLen, 16);
    return QString::fromUtf8(decryptAES(enc, generateKey(), iv));
}
