QT       += core gui serialbus serialport sql network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets serialbus


CONFIG += c++17
QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += ui dao model service utils machinelock

SOURCES += \
    dao/AirTightnessParamsDao.cpp \
    dao/AirtightTestResultDao.cpp \
    dao/SystemSettingDao.cpp \
    dao/UserDao.cpp \
    model/FillType.cpp \
    model/LeakUnitType.cpp \
    model/PLCStatusEnums.cpp \
    model/VolumeUnit.cpp \
    model/pressureunit.cpp \
    service/AutoStartManager.cpp \
    service/TcpServerController.cpp \
    service/TcpServerManager.cpp \
    ui/airtighttestresult.cpp \
    ui/connect.cpp \
    ui/debugpage.cpp \
    ui/loginwindow.cpp \
    ui/mainwindow.cpp \
    ui/paramsetting.cpp \
    ui/plcmonitor.cpp \
    ui/realtimemonitor.cpp \
    ui/systemsetting.cpp \
    ui/usermanagement.cpp \
    utils/DatabaseBackupManager.cpp \
    utils/DatabaseManager.cpp \
    utils/Logger.cpp \
    utils/StyledMessageBox.cpp \
    machinelock/machinefingerprint.cpp \
    machinelock/machinelockmanager.cpp \
    main.cpp

HEADERS += \
    dao/AirTightnessParamsDao.h \
    dao/AirtightTestResultDao.h \
    dao/ConnectionParamsDao.h \
    dao/SystemSettingDao.h \
    dao/UserDao.h \
    model/AirTightnessParams.h \
    model/ConnectionParameters.h \
    model/FillType.h \
    model/LeakUnitType.h \
    model/PLCStatusEnums.h \
    model/TestResultStruct.h \
    model/User.h \
    model/VolumeEncoding.h \
    model/VolumeUnit.h \
    model/pressureunit.h \
    service/AutoStartManager.h \
    service/TcpServerController.h \
    service/TcpServerManager.h \
    service/barcode_scanner.h \
    ui/airtighttestresult.h \
    ui/connect.h \
    ui/debugpage.h \
    ui/loginwindow.h \
    ui/mainwindow.h \
    ui/paramsetting.h \
    ui/plcmonitor.h \
    ui/realtimemonitor.h \
    ui/systemsetting.h \
    ui/usermanagement.h \
    utils/DatabaseBackupManager.h \
    utils/DatabaseManager.h \
    utils/Logger.h \
    utils/StyledMessageBox.h \
    machinelock/machinefingerprint.h \
    machinelock/machinelockmanager.h

FORMS += \
    ui/airtighttestresult.ui \
    ui/connect.ui \
    ui/debugpage.ui \
    ui/loginwindow.ui \
    ui/mainwindow.ui \
    ui/paramsetting.ui \
    ui/plcmonitor.ui \
    ui/realtimemonitor.ui \
    ui/systemsetting.ui \
    ui/usermanagement.ui

# Default rules for deployment.
qnx: target.path = /tmp/${TARGET}/bin
else: unix:!android: target.path = /opt/${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# 设置程序图标
win32: RC_ICONS = app_icon.ico

RESOURCES += resources.qrc
