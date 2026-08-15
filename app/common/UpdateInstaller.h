#pragma once

#include <QString>

class QWidget;

// OTA 更新安装（三端）：下载完成后调用，负责解压替换 + 重启
// Windows/macOS 自动替换并重启（弹窗确认后由脚本接管）；Linux 提示手动解压覆盖
class UpdateInstaller
{
public:
    // 安装更新包。Windows/macOS 会弹窗确认并 close(parent) 由脚本接管重启；
    // Linux 仅提示手动操作（不关闭窗口）。
    static void install(const QString& filePath, QWidget* parent);
};
