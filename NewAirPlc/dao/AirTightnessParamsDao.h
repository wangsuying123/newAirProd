#ifndef AIRTIGHTNESSPARAMSDAO_H
#define AIRTIGHTNESSPARAMSDAO_H

#include "AirTightnessParams.h"
#include "DatabaseManager.h"
#include <QList>

class AirTightnessParamsDao {
public:
    // 初始化参数表
    static bool initTable();

    // 保存参数到数据库
    static bool saveParams(const AirTightnessFullParams& params);

    // 更新参数
    static bool updateParams(const AirTightnessFullParams& params);

    // 根据ID获取参数
    static AirTightnessFullParams getParamsById(int id);

    // 获取所有参数组
    static QList<AirTightnessFullParams> getAllParams();

    // 删除参数
    static bool deleteParams(int id);

    // 获取最后一次保存的参数
    static AirTightnessFullParams getLastSavedParams();
    
    // 根据程序号获取参数
    static AirTightnessFullParams getParamsByProgramNumber(int programNumber);
};

#endif // AIRTIGHTNESSPARAMSDAO_H
