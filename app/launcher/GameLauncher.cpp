#include "GameLauncher.h"

#include <QCoreApplication>
#include <QDir>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QStandardPaths>
#include <QStatusBar>
#include <QVBoxLayout>

#include "UpdateChecker.h"
#include "UpdateInstaller.h"

#include "../../games/gomoku/GomokuWindow.h"
#include "../../games/xiangqi/XiangqiWindow.h"

namespace {

// 简约图标：五子棋 = 黑白棋子；象棋 = 圆形棋子 + 汉字
QPixmap gomokuIcon(int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    const float r = size * 0.30f;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(40, 40, 42));  // 黑子
    p.drawEllipse(QPointF(size * 0.36f, size * 0.36f), r, r);
    p.setBrush(QColor(252, 252, 252));  // 白子
    p.setPen(QPen(QColor(200, 200, 205), size * 0.04f));
    p.drawEllipse(QPointF(size * 0.66f, size * 0.66f), r, r);
    return pm;
}

QPixmap xiangqiIcon(int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    const float r = size * 0.40f;
    p.setBrush(QColor(198, 152, 104));  // 棋子木色
    p.setPen(QPen(QColor(118, 82, 50), size * 0.05f));
    p.drawEllipse(QPointF(size * 0.5f, size * 0.5f), r, r);
    QFont f = p.font();
    f.setPixelSize(static_cast<int>(size * 0.46f));
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(150, 45, 30));
    p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, QStringLiteral("将"));
    return pm;
}

// 创建一张游戏卡片（图标 + 标题 + 副标题）
QPushButton* makeCard(const QPixmap& icon, const QString& name, const QString& subtitle,
                      QWidget* parent)
{
    auto* card = new QPushButton(parent);
    card->setObjectName(QStringLiteral("gameCard"));
    card->setCursor(Qt::PointingHandCursor);
    card->setMinimumSize(280, 92);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* iconLabel = new QLabel(card);
    iconLabel->setPixmap(icon);
    iconLabel->setFixedSize(56, 56);
    iconLabel->setStyleSheet(QStringLiteral("background: transparent;"));

    auto* nameLabel = new QLabel(name, card);
    QFont nf = nameLabel->font();
    nf.setPixelSize(20);
    nf.setBold(true);
    nameLabel->setFont(nf);
    nameLabel->setStyleSheet(QStringLiteral("color: #1d1d1f; background: transparent;"));

    auto* subLabel = new QLabel(subtitle, card);
    QFont sf = subLabel->font();
    sf.setPixelSize(13);
    subLabel->setFont(sf);
    subLabel->setStyleSheet(QStringLiteral("color: #8a8a8e; background: transparent;"));

    auto* textLayout = new QVBoxLayout;
    textLayout->setSpacing(2);
    textLayout->addWidget(nameLabel);
    textLayout->addWidget(subLabel);

    auto* row = new QHBoxLayout(card);
    row->setContentsMargins(20, 14, 20, 14);
    row->setSpacing(16);
    row->addWidget(iconLabel);
    row->addLayout(textLayout);
    row->addStretch();

    // 柔和投影
    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 3);
    shadow->setColor(QColor(0, 0, 0, 26));
    card->setGraphicsEffect(shadow);

    return card;
}

} // namespace

GameLauncher::GameLauncher(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("棋类对战 · 选择游戏"));
    setStyleSheet(QStringLiteral(
        "QMainWindow { background: #f1f1f3; }\n"
        "QPushButton#gameCard { background: #ffffff; border: 1px solid #e3e3e6;"
        " border-radius: 16px; }\n"
        "QPushButton#gameCard:hover { background: #fbfbfd; border-color: #cfcfd6; }\n"
        "QPushButton#gameCard:pressed { background: #f4f4f7; }"));

    auto* title = new QLabel(QStringLiteral("选择游戏"), this);
    title->setAlignment(Qt::AlignCenter);
    QFont tf = title->font();
    tf.setPixelSize(34);
    tf.setBold(true);
    title->setFont(tf);
    title->setStyleSheet(QStringLiteral("color: #1d1d1f; background: transparent;"));

    m_gomokuCard = makeCard(gomokuIcon(56), QStringLiteral("五子棋"),
                            QStringLiteral("局域网对战 · 19×19"), this);
    m_xiangqiCard = makeCard(xiangqiIcon(56), QStringLiteral("象棋"),
                             QStringLiteral("局域网对战 · 9×10"), this);
    connect(m_gomokuCard, &QPushButton::clicked, this, &GameLauncher::openGomoku);
    connect(m_xiangqiCard, &QPushButton::clicked, this, &GameLauncher::openXiangqi);

    // 菜单栏：帮助（检查更新 / 关于）
    auto* helpMenu = menuBar()->addMenu(QStringLiteral("帮助"));
    m_updateAct = helpMenu->addAction(QStringLiteral("检查更新"));
    QAction* aboutAct = helpMenu->addAction(QStringLiteral("关于"));
    connect(m_updateAct, &QAction::triggered, this, &GameLauncher::onCheckUpdateClicked);
    connect(aboutAct, &QAction::triggered, this, &GameLauncher::onAboutClicked);

    // OTA 更新（通用组件：仓库名 + 资产前缀，与游戏窗口一致）
    m_updater = new UpdateChecker(QStringLiteral("ryanuo/cpp-wzq"),
                                  QStringLiteral("gobang-"), this);
    connect(m_updater, &UpdateChecker::updateAvailable, this, &GameLauncher::onUpdateAvailable);
    connect(m_updater, &UpdateChecker::upToDate, this, &GameLauncher::onUpToDate);
    connect(m_updater, &UpdateChecker::checkFailed, this, &GameLauncher::onCheckFailed);
    connect(m_updater, &UpdateChecker::downloadProgress, this, &GameLauncher::onDownloadProgress);
    connect(m_updater, &UpdateChecker::downloadFinished, this, &GameLauncher::onDownloadFinished);
    connect(m_updater, &UpdateChecker::downloadFailed, this, &GameLauncher::onDownloadFailed);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setSpacing(16);
    layout->setContentsMargins(48, 40, 48, 48);
    layout->addSpacing(8);
    layout->addWidget(title);
    layout->addSpacing(20);
    layout->addWidget(m_gomokuCard);
    layout->addWidget(m_xiangqiCard);
    layout->addStretch();
    setCentralWidget(central);

    setFixedSize(400, 420);
}

void GameLauncher::openGomoku()
{
    auto* w = new GomokuWindow;
    w->setAttribute(Qt::WA_DeleteOnClose);
    connect(w, &QObject::destroyed, this, &GameLauncher::onGameClosed);
    hide();
    w->show();
}

void GameLauncher::openXiangqi()
{
    auto* w = new XiangqiWindow;
    w->setAttribute(Qt::WA_DeleteOnClose);
    connect(w, &QObject::destroyed, this, &GameLauncher::onGameClosed);
    hide();
    w->show();
}

void GameLauncher::onGameClosed()
{
    show();
}

// ---- 帮助菜单 ----

void GameLauncher::onCheckUpdateClicked()
{
    m_updateAct->setEnabled(false);
    statusBar()->showMessage(QStringLiteral("正在检查更新…"));
    m_updater->checkForUpdate();
}

void GameLauncher::onAboutClicked()
{
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("关于"));
    box.setTextFormat(Qt::RichText);
    box.setText(QStringLiteral(
                    "<h3>棋类对战（五子棋 / 象棋）</h3>"
                    "<p>局域网双人对战小游戏：五子棋 19×19、象棋 9×10（吃将即胜）。"
                    "支持密码配对联机、悔棋、认输、再来一局、OTA 自动更新。</p>"
                    "<p>版本：v%1</p>"
                    "<p>源码：<a href=\"https://github.com/ryanuo/cpp-wzq\">"
                    "https://github.com/ryanuo/cpp-wzq</a></p>")
                    .arg(QCoreApplication::applicationVersion()));
    box.addButton(QStringLiteral("关闭"), QMessageBox::AcceptRole);
    // 链接可点击（默认浏览器打开）
    if (auto* label = box.findChild<QLabel*>(QStringLiteral("qt_msgbox_label")))
    {
        label->setOpenExternalLinks(true);
    }
    box.exec();
}

// ---- OTA 更新 ----

void GameLauncher::onUpdateAvailable(const QString& version, const QString& assetName,
                                     const QString& url)
{
    m_updateAct->setEnabled(true);
    const auto reply = QMessageBox::question(
        this, QStringLiteral("发现新版本"),
        QStringLiteral("发现新版本 v%1（当前 v%2）\n\n%3\n\n是否下载并更新？")
            .arg(version, QCoreApplication::applicationVersion(), assetName),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes);
    if (reply != QMessageBox::Yes)
    {
        statusBar()->showMessage(QStringLiteral("已取消更新"));
        return;
    }
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QDir().mkpath(dir);
    m_updater->download(url, dir);
    statusBar()->showMessage(QStringLiteral("正在下载更新…"));
}

void GameLauncher::onUpToDate(const QString& version)
{
    m_updateAct->setEnabled(true);
    QMessageBox::information(this, QStringLiteral("检查更新"),
                             QStringLiteral("已是最新版本 v%1").arg(version));
    statusBar()->showMessage(QStringLiteral("已是最新版本"));
}

void GameLauncher::onCheckFailed(const QString& reason)
{
    m_updateAct->setEnabled(true);
    QMessageBox::warning(this, QStringLiteral("检查更新失败"), reason);
    statusBar()->showMessage(QStringLiteral("检查更新失败"));
}

void GameLauncher::onDownloadProgress(qint64 received, qint64 total)
{
    if (total > 0)
    {
        statusBar()->showMessage(
            QStringLiteral("正在下载更新… %1%").arg(static_cast<int>(received * 100 / total)));
    }
}

void GameLauncher::onDownloadFinished(const QString& filePath)
{
    m_updateAct->setEnabled(true);
    statusBar()->showMessage(QStringLiteral("更新包已下载：") + filePath);
    // 三端安装逻辑与游戏窗口共用（Windows/macOS 弹窗确认后 close 本窗口由脚本重启）
    UpdateInstaller::install(filePath, this);
}

void GameLauncher::onDownloadFailed(const QString& reason)
{
    m_updateAct->setEnabled(true);
    QMessageBox::warning(this, QStringLiteral("下载失败"),
                         QStringLiteral("更新包下载失败：%1").arg(reason));
    statusBar()->showMessage(QStringLiteral("下载失败"));
}
