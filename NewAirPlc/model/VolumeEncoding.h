#ifndef VOLUME_ENCODING_H
#define VOLUME_ENCODING_H

#include <QtGlobal>
#include <cmath>

namespace VolumeEncoding {

// 使用分段函数进行容积值编码
// 根据容积V的不同范围，采用不同的编码方式：
// 
// 1. 当 V < 1000 时：
//    - 8215 = 0
//    - 8216 = E（按原始对数公式计算）
// 
// 2. 当 1000 ≤ V < 1024 时：
//    - X = 31232 + 64 × (V - 1000)
//    - 8215 = X mod 256
//    - 8216 = ⌊X/256⌋ × 256 + 68
// 
// 3. 当 1024 ≤ V ≤ 2047 时：
//    - 8215 = [32(V - 1024)] mod 256
//    - 8216 = ⌊32(V - 1024)/256⌋ × 256 + 32836
// 
// 4. 当 2048 ≤ V ≤ 4095 时：
//    - 8215 = [16(V - 2048)] mod 256
//    - 8216 = ⌊16(V - 2048)/256⌋ × 256 + 69
// 
// 5. 当 4096 ≤ V ≤ 8191 时：
//    - 8215 = [8(V - 4096)] mod 256
//    - 8216 = ⌊8(V - 4096)/256⌋ × 256 + 32837
// 
// 6. 当 8192 ≤ V ≤ 16383 时：
//    - 8215 = [4(V - 8192)] mod 256
//    - 8216 = ⌊4(V - 8192)/256⌋ × 256 + 70
// 
// 7. 当 16384 ≤ V ≤ 32767 时：
//    - 8215 = [2(V - 16384)] mod 256
//    - 8216 = ⌊2(V - 16384)/256⌋ × 256 + 32838
// 
// 8. 当 V ≥ 32768 时：
//    - 8215 = (V - 32768) mod 256
//    - 8216 = ⌊(V - 32768)/256⌋ × 256 + 71

// 计算对数公式编码值
inline quint16 calculateLogEncoding(int volume) {
    if (volume <= 0) {
        return 0;
    }
    
    double log2V = std::log2(static_cast<double>(volume));
    int k = static_cast<int>(std::floor(log2V));
    
    quint16 term1 = static_cast<quint16>(volume) << (15 - k);
    int floorVal = (k + 1) / 2;
    quint16 term2 = static_cast<quint16>(63 + floorVal);
    int modResult = k % 2;
    quint16 term3 = static_cast<quint16>(32768 * modResult);
    
    return term1 + term2 - term3;
}

// 尝试将"容积值"编码为"对应编码"。成功返回 true，并通过 outCode 输出编码；失败返回 false。
inline bool tryEncode(int volume, quint16 &outCode) {
    if (volume < 0) {
        return false;
    }
    
    if (volume == 0) {
        outCode = 0;
        return true;
    }
    
    if (volume < 1000) {
        // V < 1000: 使用对数公式
        outCode = calculateLogEncoding(volume);
    } else if (volume < 1024) {
        // 1000 ≤ V < 1024
        quint32 X = 31232U + 64U * static_cast<quint32>(volume - 1000);
        // outCode 对应 8216: ⌊X/256⌋ × 256 + 68
        outCode = static_cast<quint16>((X / 256U) * 256U + 68U);
    } else if (volume <= 2047) {
        // 1024 ≤ V ≤ 2047
        quint32 X = 32U * static_cast<quint32>(volume - 1024);
        outCode = static_cast<quint16>((X / 256U) * 256U + 32836U);
    } else if (volume <= 4095) {
        // 2048 ≤ V ≤ 4095
        quint32 X = 16U * static_cast<quint32>(volume - 2048);
        outCode = static_cast<quint16>((X / 256U) * 256U + 69U);
    } else if (volume <= 8191) {
        // 4096 ≤ V ≤ 8191
        quint32 X = 8U * static_cast<quint32>(volume - 4096);
        outCode = static_cast<quint16>((X / 256U) * 256U + 32837U);
    } else if (volume <= 16383) {
        // 8192 ≤ V ≤ 16383
        quint32 X = 4U * static_cast<quint32>(volume - 8192);
        outCode = static_cast<quint16>((X / 256U) * 256U + 70U);
    } else if (volume <= 32767) {
        // 16384 ≤ V ≤ 32767
        quint32 X = 2U * static_cast<quint32>(volume - 16384);
        outCode = static_cast<quint16>((X / 256U) * 256U + 32838U);
    } else {
        // V ≥ 32768
        quint32 X = static_cast<quint32>(volume - 32768);
        outCode = static_cast<quint16>((X / 256U) * 256U + 71U);
    }
    
    return true;
}

// 计算寄存器8215的值
inline quint16 encode8215(int volume) {
    if (volume < 0) {
        return 0;
    }
    
    if (volume < 1000) {
        // V < 1000: 固定为0
        return 0;
    } else if (volume < 1024) {
        // 1000 ≤ V < 1024: X mod 256
        quint32 X = 31232U + 64U * static_cast<quint32>(volume - 1000);
        return static_cast<quint16>(X % 256U);
    } else if (volume <= 2047) {
        // 1024 ≤ V ≤ 2047
        quint32 X = 32U * static_cast<quint32>(volume - 1024);
        return static_cast<quint16>(X % 256U);
    } else if (volume <= 4095) {
        // 2048 ≤ V ≤ 4095
        quint32 X = 16U * static_cast<quint32>(volume - 2048);
        return static_cast<quint16>(X % 256U);
    } else if (volume <= 8191) {
        // 4096 ≤ V ≤ 8191
        quint32 X = 8U * static_cast<quint32>(volume - 4096);
        return static_cast<quint16>(X % 256U);
    } else if (volume <= 16383) {
        // 8192 ≤ V ≤ 16383
        quint32 X = 4U * static_cast<quint32>(volume - 8192);
        return static_cast<quint16>(X % 256U);
    } else if (volume <= 32767) {
        // 16384 ≤ V ≤ 32767
        quint32 X = 2U * static_cast<quint32>(volume - 16384);
        return static_cast<quint16>(X % 256U);
    } else {
        // V ≥ 32768
        quint32 X = static_cast<quint32>(volume - 32768);
        return static_cast<quint16>(X % 256U);
    }
}

} // namespace VolumeEncoding

#endif // VOLUME_ENCODING_H