#include "UpdateChecker.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>

namespace {
// OTA 优先走 GitHub 加速镜像（国内直连慢），失败兜底 GitHub 官方直连
// 镜像可用性波动大（第三方服务），按实测可用性排序；全部失败后兜底官方直连（慢但可用）
// 条目格式：
//   "https://..."        普通镜像：前缀拼接（需同时支持 API 与 Release 下载）
//   "cnb:"               CNB 国内镜像：路径重写（github.com/{o}/{r}/releases/download/{tag}/{f}
//                        -> cnb.cool/{o}/{r}/-/releases/latest/download/{f}），
//                        前提：cnb.cool 上存在同名 {owner}/{repo} 仓库且同步了 Releases
const char* kMirrorPrefixes[] = {
    "cnb:",  // CNB 镜像（国内快，需先在 cnb.cool 建立同名仓库并同步 Releases；仅对 Release 下载生效，API 检查原样直连 GitHub）
    "https://ghfast.top/",  // 实测可用镜像（前缀拼接，API 与下载均代理）
    // "https://gh-proxy.com/",   // 其他候选：填入实测可用的镜像前缀（需同时支持 API 与 Release 下载）
};
constexpr int kMirrorCount = sizeof(kMirrorPrefixes) / sizeof(kMirrorPrefixes[0]);
constexpr int kTotalSources = kMirrorCount + 1;  // 镜像 + 官方直连兜底
constexpr int kCheckTimeoutMs = 10000;   // 检查请求超时
constexpr int kDownloadTimeoutMs = 30000; // 下载请求超时
} // namespace

QString UpdateChecker::mirrorUrl(int index, const QString& rawUrl)
{
    if (index >= kMirrorCount)
    {
        return rawUrl;  // 官方直连兜底
    }
    const QByteArray item = QByteArray::fromRawData(kMirrorPrefixes[index],
                                                    int(qstrlen(kMirrorPrefixes[index])));
    // CNB 镜像：路径重写（仅对 GitHub Release 资产 URL 生效，其他原样返回）
    if (item == "cnb:")
    {
        const QStringList segs = rawUrl.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        // https: / github.com / owner / repo / releases / download / tag / file
        if (segs.size() >= 8 && segs[1] == QStringLiteral("github.com")
            && segs[4] == QStringLiteral("releases") && segs[5] == QStringLiteral("download"))
        {
            // CNB 下载 URL: /-/releases/download/{tag}/{file}（实测 latest 前缀 404）
            return QStringLiteral("https://cnb.cool/%1/%2/-/releases/download/%3/%4")
                .arg(segs[2], segs[3], segs[6], segs[7]);
        }
        return rawUrl;
    }
    // 普通镜像：前缀拼接
    return QString::fromLatin1(kMirrorPrefixes[index]) + rawUrl;
}

UpdateChecker::UpdateChecker(const QString& repoPath, const QString& assetPrefix,
                             QObject* parent)
    : QObject(parent)
    , m_repoPath(repoPath)
    , m_assetPrefix(assetPrefix)
{
    m_nam = new QNetworkAccessManager(this);
}

QString UpdateChecker::apiUrl() const
{
    return QStringLiteral("https://api.github.com/repos/%1/releases/latest")
        .arg(m_repoPath);
}

QString UpdateChecker::releasePage() const
{
    return QStringLiteral("https://github.com/%1/releases/latest").arg(m_repoPath);
}

bool UpdateChecker::versionGreater(const QString& a, const QString& b)
{
    const auto parts = [](const QString& v) {
        QString s = v;
        if (s.startsWith(QLatin1Char('v')))
        {
            s = s.mid(1);
        }
        // 只取前 3 段数字（主.次.修订），忽略尾部（如 -beta）
        const QStringList segs = s.split(QLatin1Char('.'));
        QVector<int> nums(3, 0);
        for (int i = 0; i < 3 && i < segs.size(); i++)
        {
            bool ok = false;
            const int n = segs[i].toInt(&ok);
            nums[i] = ok ? n : 0;
        }
        return nums;
    };
    const QVector<int> pa = parts(a);
    const QVector<int> pb = parts(b);
    for (int i = 0; i < 3; i++)
    {
        if (pa[i] != pb[i])
        {
            return pa[i] > pb[i];
        }
    }
    return false; // 相等
}

QString UpdateChecker::currentAssetName() const
{
#if defined(Q_OS_WIN)
    return m_assetPrefix + QStringLiteral("windows-x64.zip");
#elif defined(Q_OS_MACOS)
    return m_assetPrefix + QStringLiteral("macos.zip");
#else
    return m_assetPrefix + QStringLiteral("linux.zip");
#endif
}

bool UpdateChecker::assetMatchesCurrentSystem(const QString& assetName) const
{
    return assetName == currentAssetName();
}

QString UpdateChecker::assetPlatformName(const QString& assetName)
{
    if (assetName.contains(QLatin1String("windows")))
    {
        return QStringLiteral("Windows");
    }
    if (assetName.contains(QLatin1String("macos")))
    {
        return QStringLiteral("macOS");
    }
    if (assetName.contains(QLatin1String("linux")))
    {
        return QStringLiteral("Linux");
    }
    return assetName;
}

void UpdateChecker::checkForUpdate()
{
    // 检查更新直接走官方 API（JSON 小，无需镜像）；下载才走镜像加速（大文件）
    QNetworkRequest req{QUrl(apiUrl())};
    req.setRawHeader("User-Agent", "gobang-updater");
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setTransferTimeout(kCheckTimeoutMs);
    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, &UpdateChecker::onReleaseReplyFinished);
}

void UpdateChecker::onReleaseReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
    {
        return;
    }
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError)
    {
        // 检查失败：直接提示手动下载（检查不走镜像，无需切换源）
        emit checkFailed(QStringLiteral("无法连接更新服务器。\n"
                                        "可手动打开 Releases 页下载：\n")
                         + releasePage());
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    const QJsonObject root = doc.object();
    const QString tagName = root.value(QStringLiteral("tag_name")).toString();
    if (tagName.isEmpty())
    {
        emit checkFailed(QStringLiteral("更新数据解析失败"));
        return;
    }

    // 匹配当前系统资产
    QString assetName;
    QString assetUrl;
    qint64 assetSize = -1;
    const QJsonArray assets = root.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue& v : assets)
    {
        const QJsonObject a = v.toObject();
        const QString name = a.value(QStringLiteral("name")).toString();
        if (assetMatchesCurrentSystem(name))
        {
            assetName = name;
            assetUrl = a.value(QStringLiteral("browser_download_url")).toString();
            assetSize = a.value(QStringLiteral("size")).toVariant().toLongLong();
            break;
        }
    }

    const QString current = QCoreApplication::applicationVersion();
    if (versionGreater(tagName, current))
    {
        if (assetName.isEmpty())
        {
            emit checkFailed(QStringLiteral("发现新版本 v") + tagName
                             + QStringLiteral("，但未找到当前系统的安装包"));
        }
        else
        {
            emit updateAvailable(tagName, assetName, assetUrl, assetSize);
        }
    }
    else
    {
        emit upToDate(current);
    }
}

void UpdateChecker::download(const QString& url, const QString& destDir,
                             qint64 expectedSize)
{
    m_downloadDestDir = destDir;
    m_downloadRawUrl = url;
    m_downloadExpectedSize = expectedSize;
    const QString fileName = url.mid(url.lastIndexOf(QLatin1Char('/')) + 1);
    m_downloadTarget = destDir + QLatin1Char('/') + fileName;

    // 清理下载目录的旧文件：上次失败/中断可能残留坏 zip（大小不符），
    // 不删会导致本次下载与旧文件冲突、或误用旧文件
    QFile::remove(m_downloadTarget);

    // 从第一个镜像开始下载；失败依次切换，最后官方兜底
    m_downloadTried = 0;
    tryDownloadWithMirror(0);
}

void UpdateChecker::tryDownloadWithMirror(int index)
{
    if (index >= kTotalSources)
    {
        emit downloadFailed(QStringLiteral("下载失败（已尝试加速镜像与官方源）。\n"
                                           "可手动打开 Releases 页下载：\n")
                            + releasePage());
        return;
    }
    m_downloadTried = index;
    QNetworkRequest req{QUrl(mirrorUrl(index, m_downloadRawUrl))};
    req.setRawHeader("User-Agent", "gobang-updater");
    req.setTransferTimeout(kDownloadTimeoutMs);
    // GitHub releases 资产 302 跳转，Qt 默认 NoLessSafeRedirectPolicy 自动跟随
    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) { emit downloadProgress(received, total); });
    connect(reply, &QNetworkReply::readyRead, this, &UpdateChecker::onDownloadReadyRead);
    connect(reply, &QNetworkReply::finished, this, &UpdateChecker::onDownloadFinished);
}

void UpdateChecker::onDownloadReadyRead()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
    {
        return;
    }
    QFile file(m_downloadTarget);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        file.write(reply->readAll());
        file.close();
    }
}

void UpdateChecker::onDownloadFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
    {
        return;
    }
    reply->deleteLater();
    // 处理最后剩余数据（readyRead 后可能还有）
    if (reply->error() == QNetworkReply::NoError)
    {
        QFile file(m_downloadTarget);
        if (file.open(QIODevice::WriteOnly | QIODevice::Append))
        {
            file.write(reply->readAll());
            file.close();
        }
        // 大小校验：与 API 返回的资产 size 对比，防止坏文件进安装流程
        if (m_downloadExpectedSize > 0)
        {
            const qint64 actual = QFileInfo(m_downloadTarget).size();
            if (actual != m_downloadExpectedSize)
            {
                // 删掉坏文件，换下一个源重试（Worker 可能返回坏文件）；全部失败则提示
                QFile::remove(m_downloadTarget);
                tryDownloadWithMirror(m_downloadTried + 1);
                return;
            }
        }
        emit downloadFinished(m_downloadTarget);
    }
    else
    {
        QFile::remove(m_downloadTarget);
        // 当前源失败（含超时）：切换到下一个镜像重试
        tryDownloadWithMirror(m_downloadTried + 1);
    }
}
