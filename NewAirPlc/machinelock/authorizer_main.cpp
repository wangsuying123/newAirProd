#include <QApplication>
#include "machinelockauthorizer.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("NewAirPlc 机器锁定授权工具");
    a.setApplicationVersion("1.0");
    a.setStyle("Fusion");

    MachineLockAuthorizer window;
    window.setWindowState(Qt::WindowNoState);
    window.show();

    return a.exec();
}
