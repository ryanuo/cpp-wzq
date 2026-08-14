#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

// OTA 更新检查与下载：查询 GitHub Releases 最新版，按系统匹配资产并下载
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(QObject* parent = nullptr);

    // 查询最新 release（async，结果走信号）
    void checkForUpdate();
    // 下载指定资产到 destDir（async，进度/完成走信号）
    void download(const QString& url, const QString& destDir);

    // 语义化版本比较：a > b（"0.2.0" > "0.1.9"，可带 v 前缀）
    static bool versionGreater(const QString& a, const QString& b);
    // 当前系统对应的资产名（gomoku-ai-windows-x64.zip / -macos.zip / -linux.zip）
    static QString currentAssetName();
    // 资产名是否匹配当前系统
    static bool assetMatchesCurrentSystem(const QString& assetName);
    // 资产名 -> 系统名（用于提示）
    static QString assetPlatformName(const QString& assetName);

signals:
    void updateAvailable(const QString& version, const QString& assetName, const QString& downloadUrl);
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
    QNetworkAccessManager* m_nam = nullptr;
    QString m_downloadTarget;   // 当前下载的临时文件路径
    QString m_downloadDestDir;  // 下载目标目录
};
