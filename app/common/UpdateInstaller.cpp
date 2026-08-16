#include "UpdateInstaller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTextStream>

void UpdateInstaller::install(const QString& filePath, QWidget* parent)
{
#if defined(Q_OS_WIN)
    // Windows：解压 + 延迟覆盖脚本（主进程退出后复制新文件，再重启）
    // tasklist 轮询等进程真正退出(替代固定 ping, 防 exe 锁定复制失败);
    // xcopy /r 覆盖只读; 全程写 %TEMP%\gobang_update.log 便于排查; bat 放 %TEMP% 防自删
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString updDir = appDir + QStringLiteral("/updates");
    const QString extractDir = updDir + QStringLiteral("/extracted");
    // 清理上次残留：解压中断/失败留下的文件会让 Expand-Archive 反复失败（互相踩）
    QDir(extractDir).removeRecursively();
    QDir().mkpath(extractDir);

    QProcess::execute(QStringLiteral("powershell"),
                      {QStringLiteral("-NoProfile"), QStringLiteral("-Command"),
                       QStringLiteral("Expand-Archive -Force -LiteralPath '%1' -DestinationPath '%2'")
                           .arg(filePath, extractDir)});

    const QString batPath = QDir::temp().filePath(QStringLiteral("gobang_update.bat"));
    QFile batFile(batPath);
    if (batFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream ts(&batFile);
        // bat 由 cmd 按系统编码(中文系统=GBK)解析，Qt6 默认 UTF-8 会导致中文弹窗乱码
        ts.setEncoding(QStringConverter::System);
        ts << "@echo off\r\n"
           << "set LOG=%TEMP%\\\\gobang_update.log\r\n"
           << "set \"ZIP=" << filePath << "\"\r\n"
           << "echo %date% %time% update.bat start >> \"%LOG%\"\r\n"
           // 等待主进程完全退出（最多 30 秒; tasklist 找不到进程即就绪）
           << "set /a n=0\r\n"
           << ":waitproc\r\n"
           << "tasklist /fi \"imagename eq gobang.exe\" | find /i \"gobang.exe\" >nul\r\n"
           << "if errorlevel 1 goto copystart\r\n"
           << "set /a n+=1\r\n"
           << "if %n% geq 30 goto copystart\r\n"
           << "ping -n 2 127.0.0.1 >nul\r\n"
           << "goto waitproc\r\n"
           << ":copystart\r\n"
           << "echo %date% %time% wait done retry=%n% >> \"%LOG%\"\r\n"
           // 解压失败检测：extracted 无 exe 则跳过复制（保留旧版）+ 弹窗提示原因
           << "if not exist \"" << extractDir << "\\gobang.exe\" (\r\n"
           << "  echo %date% %time% ERROR: extracted gobang.exe missing, unzip failed >> \"%LOG%\"\r\n"
           << "  start \"\" powershell -NoProfile -Command \"Add-Type -AssemblyName PresentationFramework; [System.Windows.MessageBox]::Show('更新包解压失败，请重新检查更新。','Gobang 更新')\"\r\n"
           << "  goto cleanup\r\n"
           << ")\r\n"
           << "xcopy /y /r /e /q \"" << extractDir << "\\*\" \"" << appDir << "\\\" >> \"%LOG%\" 2>&1\r\n"
           << "echo %date% %time% xcopy exit=%errorlevel% >> \"%LOG%\"\r\n"
           // 覆盖失败（权限/文件占用）：弹窗提示，不再启动旧 exe（避免"更新了但版本没变"）
           << "if errorlevel 1 (\r\n"
           << "  start \"\" powershell -NoProfile -Command \"Add-Type -AssemblyName PresentationFramework; [System.Windows.MessageBox]::Show('更新文件复制失败，请检查目录权限后重试。','Gobang 更新')\"\r\n"
           << "  goto cleanup\r\n"
           << ")\r\n"
           << "start \"\" \"" << appDir << "\\gobang.exe\"\r\n"
           << "echo %date% %time% relaunch issued >> \"%LOG%\"\r\n"
           << ":cleanup\r\n"
           << "rmdir /s /q \"" << updDir << "\" >> \"%LOG%\" 2>&1\r\n"
           << "del /q \"%ZIP%\" >> \"%LOG%\" 2>&1\r\n";
        batFile.close();
    }
    // 弹窗确认后再启动更新脚本：用户在弹窗停留时 bat 不会提前跑（避免等超时）
    QMessageBox::information(parent, QStringLiteral("更新"),
                             QStringLiteral("更新包已就绪。\n\n点击「确定」后程序将退出并自动重启，完成更新。"));
    QProcess::startDetached(batPath);
    parent->close();
#elif defined(Q_OS_MACOS)
    // macOS：解压 + 延迟替换 .app（主进程退出后脚本替换整个 bundle 并重启）
    // 脚本放用户可写目录（AppDataLocation），bundle 位置由脚本运行时决定
    const QString bundlePath = QCoreApplication::applicationDirPath();  // .../gobang.app/Contents/MacOS
    const QString bundleParent = QDir(bundlePath).filePath(QStringLiteral("../.."));
    const QString updDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                           + QStringLiteral("/updates");
    QDir().mkpath(updDir);

    const QString scriptPath = updDir + QStringLiteral("/apply_update.sh");
    QFile scriptFile(scriptPath);
    if (scriptFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream ts(&scriptFile);
        ts << "#!/bin/sh\n"
           << "sleep 2\n"
           << "UPD=\"" << updDir << "\"\n"
           << "BUNDLE_PARENT=\"" << bundleParent << "\"\n"
           << "ZIP=\"" << filePath << "\"\n"
           << "unzip -o -q \"$ZIP\" -d \"$UPD/extracted\" || exit 1\n"
           << "rm -rf \"$BUNDLE_PARENT/gobang.app\"\n"
           << "cp -R \"$UPD/extracted/gobang.app\" \"$BUNDLE_PARENT/gobang.app\"\n"
           << "rm -f \"$ZIP\"\n"
           << "rm -rf \"$UPD\"\n"
           << "open \"$BUNDLE_PARENT/gobang.app\"\n";
        scriptFile.close();
    }
    // 弹窗确认后再启动更新脚本（同上，避免脚本提前跑）
    QMessageBox::information(parent, QStringLiteral("更新"),
                             QStringLiteral("更新包已就绪。\n\n点击「确定」后程序将退出并自动重启，完成更新。"));
    QProcess::startDetached(QStringLiteral("/bin/sh"), {scriptPath});
    parent->close();
#else
    // Linux：安装方式多样（解压目录/发行包），保持手动引导
    QMessageBox::information(
        parent, QStringLiteral("更新"),
        QStringLiteral("更新包已下载到：\n%1\n\n请退出程序后，解压并覆盖原安装目录中的文件。")
            .arg(filePath));
#endif
}
