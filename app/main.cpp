#include <QApplication>
#include <QCoreApplication>
#include <QIcon>

#include "launcher/GameLauncher.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    // 版本号（CMake 注入，OTA 更新对比用）
    QCoreApplication::setApplicationVersion(QStringLiteral(GOBANG_APP_VERSION));
    // 窗口/任务栏图标（三端统一，来自 app/icon/gobang-1024.png）
    app.setWindowIcon(QIcon(QStringLiteral(":/res/icon.png")));

    GameLauncher launcher;
    launcher.show();
    return app.exec();
}
