// UpdateChecker 逻辑单测：版本比较 / 资产匹配（headless）
#include <QCoreApplication>
#include <cstdio>

#include "../common/UpdateChecker.h"

static int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("FAIL: %s (line %d)\n", #cond, __LINE__);            \
            g_failures++;                                                    \
        }                                                                    \
    } while (0)

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    // ---- 版本比较 ----
    CHECK(UpdateChecker::versionGreater("0.2.0", "0.1.0"));
    CHECK(UpdateChecker::versionGreater("v0.2.0", "v0.1.9"));
    CHECK(UpdateChecker::versionGreater("1.0.0", "0.9.9"));
    CHECK(UpdateChecker::versionGreater("0.1.10", "0.1.9"));
    CHECK(!UpdateChecker::versionGreater("0.1.0", "0.2.0"));
    CHECK(!UpdateChecker::versionGreater("v0.1.0", "v0.1.0"));  // 相等
    CHECK(!UpdateChecker::versionGreater("0.1.0", "0.1.0-beta"));  // 相等取前3段
    CHECK(UpdateChecker::versionGreater("0.10.0", "0.9.0"));

    // ---- 资产匹配 ----
    UpdateChecker updater(QStringLiteral("ryanuo/cpp-wzq"), QStringLiteral("gobang-"));
    const QString current = updater.currentAssetName();
    CHECK(updater.assetMatchesCurrentSystem(current));
    CHECK(!updater.assetMatchesCurrentSystem(QStringLiteral("gobang-none.zip")));
    // 平台名
    CHECK(UpdateChecker::assetPlatformName(QStringLiteral("gobang-windows-x64.zip")) == "Windows");
    CHECK(UpdateChecker::assetPlatformName(QStringLiteral("gobang-macos.zip")) == "macOS");
    CHECK(UpdateChecker::assetPlatformName(QStringLiteral("gobang-linux.zip")) == "Linux");

    // ---- mirrorUrl：Worker 重写 + 官方兜底 ----
    const QString api =
        QStringLiteral("https://api.github.com/repos/ryanuo/cpp-wzq/releases/latest");
    CHECK(UpdateChecker::mirrorUrl(0, api) ==
          QStringLiteral("https://ota.ryanuo.cc/github-api/repos/ryanuo/cpp-wzq/releases/latest"));
    const QString asset = QStringLiteral(
        "https://github.com/ryanuo/cpp-wzq/releases/download/v0.2.0/gobang-windows-x64.zip");
    CHECK(UpdateChecker::mirrorUrl(0, asset) ==
          QStringLiteral("https://ota.ryanuo.cc/release/ryanuo/cpp-wzq/"
                         "releases/download/v0.2.0/gobang-windows-x64.zip"));
    // 官方兜底（index >= kMirrorCount）原样返回
    CHECK(UpdateChecker::mirrorUrl(1, api) == api);
    CHECK(UpdateChecker::mirrorUrl(1, asset) == asset);
    // 非 GitHub 域名（如自建源）不重写
    const QString selfHosted = QStringLiteral("https://example.com/pkg.zip");
    CHECK(UpdateChecker::mirrorUrl(0, selfHosted) == selfHosted);

    std::printf("当前系统资产: %s\n", current.toUtf8().constData());
    if (g_failures == 0)
    {
        std::printf("ALL UPDATE TESTS PASSED\n");
        return 0;
    }
    std::printf("%d FAILURE(S)\n", g_failures);
    return 1;
}
