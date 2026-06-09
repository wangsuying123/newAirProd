#ifndef MACHINEFINGERPRINT_H
#define MACHINEFINGERPRINT_H

#include <QString>
#include <QMap>

/**
 * @brief 机器指纹生成器 - 获取计算机的唯一硬件特征
 *
 * 基于CPU ID、主板序列号、硬盘序列号、MAC地址等生成唯一指纹，
 * 即使复制程序到其他电脑也无法运行。
 */
class MachineFingerprint
{
public:
    MachineFingerprint();
    ~MachineFingerprint();

    // 获取机器指纹（综合多个硬件特征）
    QString getMachineFingerprint();

    // 获取详细的硬件信息
    QMap<QString, QString> getHardwareInfo();

    // 获取各硬件信息
    QString getCPUSerialNumber();
    QString getMotherboardSerialNumber();
    QString getHardDiskSerialNumber();
    QString getMACAddress();
    QString getWindowsProductID();
    QString getComputerName();

    // 验证机器指纹是否匹配
    bool verifyFingerprint(const QString &expectedFingerprint);

private:
    QString executeCommand(const QString &command);
    QString extractWMICValue(const QString &output);
    QString cleanString(const QString &str);
    QString generateHash(const QString &data);
};

#endif // MACHINEFINGERPRINT_H
