#include <QApplication>
#include <QStringList>

#include "MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    // 版本号（CMake 注入，OTA 更新对比用）
    QCoreApplication::setApplicationVersion(QStringLiteral(GOMOKU_APP_VERSION));

    MainWindow w;
    w.show();

    // 支持 --online <密码>：启动后直接用该密码进入联机配对（便于自动化测试/双开）
    const QStringList args = app.arguments();
    const int idx = args.indexOf(QStringLiteral("--online"));
    if (idx >= 0 && idx + 1 < args.size()) {
        w.startOnline(args.at(idx + 1));
    }

    return app.exec();
}
