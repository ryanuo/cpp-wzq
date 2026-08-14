#include "UpdateChecker.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>

namespace {
constexpr auto kApiUrl =
    "https://api.github.com/repos/ryanuo/cpp-wzq/releases/latest";
constexpr auto kReleasePage =
    "https://github.com/ryanuo/cpp-wzq/releases/latest";

// GitHub 加速镜像（国内直连慢，全部走镜像；按实测可达性排序）
// 镜像可用性会变化，按需增删；全部失败时提示手动下载
const char* kMirrorPrefixes[] = {
    "https://gh-proxy.com/",        // 0: 首选（实测可用）
    "https://mirror.ghproxy.com/",  // 1
    "https://ghfast.top/",          // 2
};
constexpr int kMirrorCount = 3;
constexpr int kCheckTimeoutMs = 10000;  // 检查请求超时
constexpr int kDownloadTimeoutMs = 30000; // 下载请求超时
} // namespace

QString UpdateChecker::mirrorUrl(int index, const QString& rawUrl)
{
    return QString::fromLatin1(kMirrorPrefixes[index]) + rawUrl;
}

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
{
    m_nam = new QNetworkAccessManager(this);
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

QString UpdateChecker::currentAssetName()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("gobang-windows-x64.zip");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("gobang-macos.zip");
#else
    return QStringLiteral("gobang-linux.zip");
#endif
}

bool UpdateChecker::assetMatchesCurrentSystem(const QString& assetName)
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
    m_checkTried = 0;
    tryCheckNextMirror(0);
}

bool UpdateChecker::tryCheckNextMirror(int index)
{
    if (index >= kMirrorCount)
    {
        // 所有镜像均失败：提示手动下载
        emit checkFailed(QStringLiteral("无法连接更新服务器（已尝试官方与加速镜像）。\n"
                                        "可手动打开 Releases 页下载：\n")
                         + QString::fromLatin1(kReleasePage));
        return false;
    }
    m_checkTried = index;
    QNetworkRequest req{QUrl(mirrorUrl(index, QString::fromLatin1(kApiUrl)))};
    req.setRawHeader("User-Agent", "gobang-updater");
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setTransferTimeout(kCheckTimeoutMs);  // 慢网络下快速切换到下一个源
    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, &UpdateChecker::onReleaseReplyFinished);
    return true;
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
        // 当前源失败（含超时）：切换到下一个镜像重试
        tryCheckNextMirror(m_checkTried + 1);
        return;
    }
    m_mirrorIndex = m_checkTried;  // 记录成功镜像，下载沿用

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
    const QJsonArray assets = root.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue& v : assets)
    {
        const QJsonObject a = v.toObject();
        const QString name = a.value(QStringLiteral("name")).toString();
        if (assetMatchesCurrentSystem(name))
        {
            assetName = name;
            assetUrl = a.value(QStringLiteral("browser_download_url")).toString();
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
            emit updateAvailable(tagName, assetName, assetUrl);
        }
    }
    else
    {
        emit upToDate(current);
    }
}

void UpdateChecker::download(const QString& url, const QString& destDir)
{
    m_downloadDestDir = destDir;
    m_downloadRawUrl = url;
    const QString fileName = url.mid(url.lastIndexOf(QLatin1Char('/')) + 1);
    m_downloadTarget = destDir + QLatin1Char('/') + fileName;

    // 从检查更新成功的镜像开始下载；失败再依次切换
    m_downloadTried = m_mirrorIndex;
    tryDownloadWithMirror(m_mirrorIndex);
}

void UpdateChecker::tryDownloadWithMirror(int index)
{
    if (index >= kMirrorCount)
    {
        emit downloadFailed(QStringLiteral("下载失败（已尝试官方与加速镜像）。\n"
                                           "可手动打开 Releases 页下载：\n")
                            + QString::fromLatin1(kReleasePage));
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
        emit downloadFinished(m_downloadTarget);
    }
    else
    {
        QFile::remove(m_downloadTarget);
        // 当前源失败（含超时）：切换到下一个镜像重试
        tryDownloadWithMirror(m_downloadTried + 1);
    }
}
