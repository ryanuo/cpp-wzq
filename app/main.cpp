#include <QApplication>
#include <QCoreApplication>

#include "launcher/GameLauncher.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    // 版本号（CMake 注入，OTA 更新对比用）
    QCoreApplication::setApplicationVersion(QStringLiteral(GOBANG_APP_VERSION));

    GameLauncher launcher;
    launcher.show();
    return app.exec();
}
