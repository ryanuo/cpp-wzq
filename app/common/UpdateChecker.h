#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

// OTA 更新检查与下载：查询 GitHub Releases 最新版，按系统匹配资产并下载
// 通用组件：仓库名与资产前缀由构造函数传入，新应用接入只需改一处调用
//   UpdateChecker updater("ryanuo/cpp-wzq", "gobang-", this);
// 网络路径：检查/下载优先走 GitHub 加速镜像（kMirrorPrefixes 可配置列表），失败兜底官方直连
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    // repoPath: GitHub "owner/repo"（如 "ryanuo/cpp-wzq"）
    // assetPrefix: Release 资产文件名前缀（如 "gobang-" → gobang-windows-x64.zip）
    explicit UpdateChecker(const QString& repoPath, const QString& assetPrefix,
                           QObject* parent = nullptr);

    // 查询最新 release（async，结果走信号）
    void checkForUpdate();
    // 下载指定资产到 destDir（async，进度/完成走信号）
    // expectedSize: API 返回的资产字节数（>0 时下载完成后校验，防止坏文件进安装流程）
    void download(const QString& url, const QString& destDir, qint64 expectedSize = -1);

    // 语义化版本比较：a > b（"0.2.0" > "0.1.9"，可带 v 前缀）
    static bool versionGreater(const QString& a, const QString& b);
    // 当前系统对应的资产名（{prefix}windows-x64.zip / -macos.zip / -linux.zip）
    QString currentAssetName() const;
    // 资产名是否匹配当前系统
    bool assetMatchesCurrentSystem(const QString& assetName) const;
    // 资产名 -> 系统名（用于提示）
    static QString assetPlatformName(const QString& assetName);
    // 镜像 URL 重写（index < kMirrorCount 走镜像前缀，否则官方直连）：
    //   前缀拼接：kMirrorPrefixes[index] + 原始 URL（镜像需同时支持 API 与 Release 下载）
    static QString mirrorUrl(int index, const QString& rawUrl);

signals:
    void updateAvailable(const QString& version, const QString& assetName,
                         const QString& downloadUrl, qint64 size);
    void upToDate(const QString& version);          // 已是最新
    void checkFailed(const QString& reason);
    void downloadProgress(qint64 received, qint64 total);
    void downloadFinished(const QString& filePath);
    void downloadFailed(const QString& reason);

private slots:
    void onReleaseReplyFinished();  // Qt6 finished 信号无参，用 sender() 取 reply
    void onDownloadReadyRead();
    void onDownloadFinished();

private:
    // 最新 release 的 API / 页面 URL（由 repoPath 拼出）
    QString apiUrl() const;
    QString releasePage() const;
    // 按镜像序号尝试下载（失败依次切换，最后官方直连兜底）
    void tryDownloadWithMirror(int index);

    QNetworkAccessManager* m_nam = nullptr;
    QString m_repoPath;     // "owner/repo"
    QString m_assetPrefix;  // 资产文件名前缀，如 "gobang-"
    QString m_downloadTarget;   // 当前下载的临时文件路径
    QString m_downloadDestDir;  // 下载目标目录
    QString m_downloadRawUrl;   // 原始下载 URL（未加镜像前缀）
    qint64 m_downloadExpectedSize = -1;  // 期望文件大小（API 返回，下载后校验）
    int m_downloadTried = 0;    // 下载已尝试的镜像数
};
