#include "machinefingerprint.h"
#include <QProcess>
#include <QCryptographicHash>
#include <QDebug>
#include <QRegularExpression>

MachineFingerprint::MachineFingerprint() {}
MachineFingerprint::~MachineFingerprint() {}

QString MachineFingerprint::executeCommand(const QString &command)
{
    QProcess process;
    process.start("cmd.exe", QStringList() << "/c" << command);
    process.waitForFinished(5000);
    return QString::fromLocal8Bit(process.readAllStandardOutput());
}

QString MachineFingerprint::extractWMICValue(const QString &output)
{
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    if (lines.size() >= 2)
        return cleanString(lines[1]);
    return "";
}

QString MachineFingerprint::cleanString(const QString &str)
{
    QString cleaned = str.trimmed();
    cleaned.remove('\r');
    cleaned.remove('\n');
    return cleaned.simplified();
}

QString MachineFingerprint::generateHash(const QString &data)
{
    QByteArray hash = QCryptographicHash::hash(data.toUtf8(), QCryptographicHash::Sha256);
    return hash.toHex();
}

QString MachineFingerprint::getCPUSerialNumber()
{
    QString cpuId = extractWMICValue(executeCommand("wmic cpu get ProcessorId"));
    qDebug() << "[机器指纹] CPU ID:" << cpuId;
    return cpuId;
}

QString MachineFingerprint::getMotherboardSerialNumber()
{
    QString sn = extractWMICValue(executeCommand("wmic baseboard get SerialNumber"));
    qDebug() << "[机器指纹] 主板序列号:" << sn;
    return sn;
}

QString MachineFingerprint::getHardDiskSerialNumber()
{
    QString sn = extractWMICValue(executeCommand("wmic diskdrive get SerialNumber"));
    qDebug() << "[机器指纹] 硬盘序列号:" << sn;
    return sn;
}

QString MachineFingerprint::getMACAddress()
{
    QString output = executeCommand("getmac /fo csv /nh");
    QRegularExpression re("\"([0-9A-F]{2}-[0-9A-F]{2}-[0-9A-F]{2}-[0-9A-F]{2}-[0-9A-F]{2}-[0-9A-F]{2})\"");
    QRegularExpressionMatch match = re.match(output);
    QString mac = match.hasMatch() ? match.captured(1) : "";
    qDebug() << "[机器指纹] MAC地址:" << mac;
    return mac;
}

QString MachineFingerprint::getWindowsProductID()
{
    QString id = extractWMICValue(executeCommand("wmic os get SerialNumber"));
    qDebug() << "[机器指纹] Windows产品ID:" << id;
    return id;
}

QString MachineFingerprint::getComputerName()
{
    QString name = cleanString(executeCommand("hostname"));
    qDebug() << "[机器指纹] 计算机名:" << name;
    return name;
}

QMap<QString, QString> MachineFingerprint::getHardwareInfo()
{
    QMap<QString, QString> info;
    info["CPU_ID"]          = getCPUSerialNumber();
    info["Motherboard_SN"]  = getMotherboardSerialNumber();
    info["HardDisk_SN"]     = getHardDiskSerialNumber();
    info["MAC_Address"]     = getMACAddress();
    info["Windows_ID"]      = getWindowsProductID();
    info["Computer_Name"]   = getComputerName();
    return info;
}

QString MachineFingerprint::getMachineFingerprint()
{
    qDebug() << "[机器指纹] ========== 开始生成机器指纹 ==========";
    QMap<QString, QString> hw = getHardwareInfo();

    QString combined;
    combined += "CPU:" + hw["CPU_ID"] + "|";
    combined += "MB:"  + hw["Motherboard_SN"] + "|";
    combined += "HD:"  + hw["HardDisk_SN"] + "|";
    combined += "MAC:" + hw["MAC_Address"] + "|";
    combined += "WIN:" + hw["Windows_ID"];

    qDebug() << "[机器指纹] 组合特征:" << combined;
    QString fingerprint = generateHash(combined);
    qDebug() << "[机器指纹] 最终指纹:" << fingerprint;
    qDebug() << "[机器指纹] ========== 机器指纹生成完成 ==========";
    return fingerprint;
}

bool MachineFingerprint::verifyFingerprint(const QString &expectedFingerprint)
{
    QString current = getMachineFingerprint();
    bool matched = (current == expectedFingerprint);
    if (matched)
        qDebug() << "[机器指纹] ✓ 指纹验证通过";
    else
        qDebug() << "[机器指纹] ✗ 指纹验证失败 | 期望:" << expectedFingerprint << "| 实际:" << current;
    return matched;
}
