#include "GameWindow.h"

#include <QAction>
#include <QAudioOutput>
#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMediaPlayer>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QSoundEffect>
#include <QStandardPaths>
#include <QStatusBar>
#include <QVBoxLayout>

#include "NetworkManager.h"
#include "UpdateChecker.h"

GameWindow::GameWindow(const QString& title, QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(title);

    // 菜单栏操作（macOS 集成到屏幕顶部系统菜单栏；按类型分菜单）
    m_connectAct = new QAction(QStringLiteral("联机对战"), this);
    m_newGameAct = new QAction(QStringLiteral("新游戏"), this);
    m_undoAct = new QAction(QStringLiteral("悔棋"), this);
    m_surrenderAct = new QAction(QStringLiteral("认输"), this);
    m_disconnectAct = new QAction(QStringLiteral("断开"), this);
    m_newGameAct->setEnabled(false);
    m_undoAct->setEnabled(false);
    m_surrenderAct->setEnabled(false);
    m_disconnectAct->setEnabled(false);
    connect(m_connectAct, &QAction::triggered, this, &GameWindow::onConnectClicked);
    connect(m_newGameAct, &QAction::triggered, this, &GameWindow::onNewGameClicked);
    connect(m_undoAct, &QAction::triggered, this, &GameWindow::onUndoClicked);
    connect(m_surrenderAct, &QAction::triggered, this, &GameWindow::onSurrenderClicked);
    connect(m_disconnectAct, &QAction::triggered, this, &GameWindow::onDisconnectClicked);

    QMenu* gameMenu = menuBar()->addMenu(QStringLiteral("游戏"));
    gameMenu->addAction(m_connectAct);
    gameMenu->addAction(m_newGameAct);
    gameMenu->addAction(m_undoAct);
    gameMenu->addAction(m_surrenderAct);
    gameMenu->addSeparator();
    gameMenu->addAction(m_disconnectAct);

    // 「皮肤」菜单（由子类 fillSkinMenu 填充）
    m_skinMenu = menuBar()->addMenu(QStringLiteral("皮肤"));
    m_skinAct = m_skinMenu->menuAction();

    // 「帮助」菜单：检查更新（OTA）
    m_updateAct = new QAction(QStringLiteral("检查更新"), this);
    connect(m_updateAct, &QAction::triggered, this, &GameWindow::onCheckUpdateClicked);
    QMenu* helpMenu = menuBar()->addMenu(QStringLiteral("帮助"));
    helpMenu->addAction(m_updateAct);

    // central 布局（子类把棋盘 addWidget 进来）
    auto* central = new QWidget(this);
    m_centralLayout = new QVBoxLayout(central);
    m_centralLayout->setContentsMargins(8, 8, 8, 8);
    setCentralWidget(central);

    statusBar()->showMessage(QStringLiteral("未连接 · 点击「联机对战」输入密码配对"));

    // 回合提示：状态栏右下角永久区（左侧为状态消息，一左一右）
    m_turnLabel = new QLabel(this);
    m_turnLabel->setVisible(false);
    statusBar()->addPermanentWidget(m_turnLabel);
    updateTurnHint();

    // 音效：WAV 用 QSoundEffect，MP3 用 QMediaPlayer
    m_startSound = new QSoundEffect(this);
    m_startSound->setSource(QUrl(QStringLiteral("qrc:/res/start.wav")));
    m_downSound = new QSoundEffect(this);
    m_downSound->setSource(QUrl(QStringLiteral("qrc:/res/down.wav")));

    m_bgPlayer = new QMediaPlayer(this);
    m_bgAudio = new QAudioOutput(this);
    m_bgAudio->setVolume(0.5f);
    m_bgPlayer->setAudioOutput(m_bgAudio);
    m_bgPlayer->setSource(QUrl(QStringLiteral("qrc:/res/bg.mp3")));
    m_bgPlayer->setLoops(QMediaPlayer::Infinite);

    m_sfxPlayer = new QMediaPlayer(this);
    m_sfxAudio = new QAudioOutput(this);
    m_sfxAudio->setVolume(0.8f);
    m_sfxPlayer->setAudioOutput(m_sfxAudio);

    // 联机
    m_network = new NetworkManager(this);
    connect(m_network, &NetworkManager::statusChanged, this, &GameWindow::onStatusChanged);
    connect(m_network, &NetworkManager::connected, this, &GameWindow::onConnected);
    connect(m_network, &NetworkManager::searchFailed, this, &GameWindow::onSearchFailed);
    connect(m_network, &NetworkManager::moveReceived, this, &GameWindow::onMoveReceived);
    connect(m_network, &NetworkManager::moveFromToReceived, this, &GameWindow::onMoveFromToReceived);
    connect(m_network, &NetworkManager::restartRequested, this, &GameWindow::onRestartRequested);
    connect(m_network, &NetworkManager::undoRequested, this, &GameWindow::onUndoRequested);
    connect(m_network, &NetworkManager::undoAccepted, this, &GameWindow::onUndoAccepted);
    connect(m_network, &NetworkManager::undoRejected, this, &GameWindow::onUndoRejected);
    connect(m_network, &NetworkManager::surrendered, this, &GameWindow::onSurrendered);
    connect(m_network, &NetworkManager::disconnected, this, &GameWindow::onDisconnected);

    // OTA 更新（通用组件：仓库名 + 资产前缀，新应用接入只改这一行）
    m_updater = new UpdateChecker(QStringLiteral("ryanuo/cpp-wzq"),
                                  QStringLiteral("gobang-"), this);
    connect(m_updater, &UpdateChecker::updateAvailable, this, &GameWindow::onUpdateAvailable);
    connect(m_updater, &UpdateChecker::upToDate, this, &GameWindow::onUpToDate);
    connect(m_updater, &UpdateChecker::checkFailed, this, &GameWindow::onCheckFailed);
    connect(m_updater, &UpdateChecker::downloadProgress, this, &GameWindow::onDownloadProgress);
    connect(m_updater, &UpdateChecker::downloadFinished, this, &GameWindow::onDownloadFinished);
    connect(m_updater, &UpdateChecker::downloadFailed, this, &GameWindow::onDownloadFailed);

    // 窗口尺寸紧凑：棋盘(624) + 边距 + 状态栏
    resize(640, 675);
}

GameWindow::~GameWindow() = default;

void GameWindow::startOnline(const QString& password)
{
    const QString trimmed = password.trimmed();
    if (trimmed.isEmpty())
    {
        setStatus(QStringLiteral("密码不能为空"));
        return;
    }
    m_connectAct->setEnabled(false);
    m_network->start(trimmed);
}

void GameWindow::closeEvent(QCloseEvent* event)
{
    m_network->stop();
    event->accept();
}

void GameWindow::onConnectClicked()
{
    bool ok = false;
    const QString password = QInputDialog::getText(
        this,
        QStringLiteral("联机对战"),
        QStringLiteral("输入房间密码\n与局域网内输入相同密码的玩家自动配对"),
        QLineEdit::Password,
        QString(),
        &ok);
    if (ok)
    {
        startOnline(password);
    }
}

void GameWindow::onNewGameClicked()
{
    // 对局进行中（已落子且未分胜负）：点「新游戏」视为认输
    if (m_connected && moveCount() > 0 && !m_gameOver)
    {
        const auto reply = QMessageBox::question(
            this, QStringLiteral("新游戏"),
            QStringLiteral("对局已开始，点击「新游戏」将视为认输，确定吗？"),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (reply != QMessageBox::Yes)
        {
            return;
        }
        m_localDisconnect = true;
        m_network->sendSurrender();
        m_network->stop();
        setStatus(QStringLiteral("你认输了"));
        return;
    }

    // 对局已结束（结果窗「再来一局」/平局窗）：表达重开意愿，双方都点才开新局
    if (m_gameOver)
    {
        if (m_restartPending)
        {
            setStatus(QStringLiteral("已发送重开请求，等待对方也点「再来一局」…"));
            return;
        }
        if (m_connected)
        {
            m_restartPending = true;
            m_network->sendRestartRequest();
            if (m_peerRestartRequested)
            {
                // 对方已点 → 双方达成，直接新局
                m_peerRestartRequested = false;
                m_restartPending = false;
                resetBoard();
                updateTurnHint();
                setStatus(QStringLiteral("双方都请求重开 · 新的一局"));
            }
            else
            {
                setStatus(QStringLiteral("已发送重开请求，等待对方也点「再来一局」…"));
            }
        }
        return;
    }

    // 未开局：本地重开（双方棋盘本就为空，无需通知对方）
    resetBoard();
}

void GameWindow::onDisconnectClicked()
{
    if (!m_connected)
    {
        return;
    }
    // 断开 = 认输，弹确认防止误点
    const auto reply = QMessageBox::question(
        this, QStringLiteral("断开连接"),
        QStringLiteral("断开连接将视为认输，确定吗？"),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (reply == QMessageBox::Yes)
    {
        m_localDisconnect = true;
        m_network->stop();
    }
}

void GameWindow::onSurrenderClicked()
{
    if (!m_connected || m_gameOver)
    {
        return;
    }
    const auto reply = QMessageBox::question(
        this, QStringLiteral("认输"),
        QStringLiteral("确定认输吗？"),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (reply != QMessageBox::Yes)
    {
        return;
    }
    m_localDisconnect = true;
    m_network->sendSurrender();
    m_network->stop();  // 认输后断开
    setStatus(QStringLiteral("你认输了"));
}

void GameWindow::onCellClicked(int row, int col)
{
    if (!m_connected)
    {
        setStatus(QStringLiteral("未连接 · 请先点击「联机对战」输入密码配对"));
        return;
    }
    if (m_gameOver)
    {
        return;
    }
    if (!myTurn())
    {
        setStatus(waitText());
        return;
    }
    if (!canPlace(row, col))
    {
        return; // 已有棋子/非法落点
    }

    placePiece(row, col, m_myKind);
    m_downSound->play();
    m_network->sendMove(row, col);
    updateTurnHint();
    checkGameEnd(m_myKind);
}

void GameWindow::onMoveReceived(int row, int col)
{
    if (!m_connected || m_gameOver)
    {
        return;
    }
    if (!canPlace(row, col))
    {
        return; // 已占/非法
    }

    placePiece(row, col, opponentKind());
    m_downSound->play();
    updateTurnHint();
    checkGameEnd(opponentKind());
}

void GameWindow::onMoveFromToReceived(int fr, int fc, int tr, int tc)
{
    Q_UNUSED(fr);
    Q_UNUSED(fc);
    Q_UNUSED(tr);
    Q_UNUSED(tc);
    // 五子棋不使用起止格走子；象棋子类覆盖
}

void GameWindow::onConnected(bool isHost)
{
    m_connected = true;
    m_gameOver = false;
    m_myKind = isHost ? CHESS_BLACK : CHESS_WHITE;
    m_undoPending = false;
    m_restartPending = false;
    m_localDisconnect = false;
    m_resultShown = false;

    setConnectedUi(true);

    resetBoard();
    m_bgPlayer->play();
    m_startSound->play();

    // 状态栏显示执子 + 对手 IP
    const QString peerIp = m_network->peerAddress().toString();
    setStatus(isHost ? QStringLiteral("已连接 · 你执黑（先手） · 对手 IP: ") + peerIp
                     : QStringLiteral("已连接 · 你执白（后手） · 对手 IP: ") + peerIp);
    setWindowTitle(windowTitle() + QStringLiteral(" - 对手 ") + peerIp);
    updateTurnHint();
}

void GameWindow::onRestartRequested()
{
    if (!m_connected || !m_gameOver)
    {
        return; // 对局中收到视为误发，忽略
    }
    closeResultDialog();  // 对方已点重开，胜负结果框（若开着）一并关闭

    m_peerRestartRequested = true;
    if (m_restartPending)
    {
        // 双方都点了 → 自动新局；再发一次 REQ 作回执（对端收到后同样达成，
        // 对端若已 reset 则被 m_gameOver 拦截，不会循环）
        m_peerRestartRequested = false;
        m_restartPending = false;
        m_network->sendRestartRequest();
        resetBoard();
        updateTurnHint();
        setStatus(QStringLiteral("双方都请求重开 · 新的一局"));
    }
    else
    {
        setStatus(QStringLiteral("对方请求再来一局，你也点「再来一局」即可重开"));
    }
}

void GameWindow::onDisconnected()
{
    closeResultDialog();   // 断开：结果框（若开着）一并关闭，防止手动关闭误触发断开逻辑
    // 对局中且非本地主动断开、且未弹过结果 → 对方认输（QUIT 或掉线）
    const bool inGame = moveCount() > 0;
    const bool opponentLeft = inGame && !m_localDisconnect && !m_resultShown;
    m_connected = false;
    m_gameOver = false;
    m_undoPending = false;
    m_restartPending = false;
    m_localDisconnect = false;
    m_resultShown = false;

    setConnectedUi(false);
    m_bgPlayer->stop();
    resetBoard();
    setWindowTitle(windowTitle());

    if (opponentLeft)
    {
        playSfx(QStringLiteral("qrc:/res/win.mp3"));
        showResultDialog(QStringLiteral("对局结束"),
                         QStringLiteral("对方已断开连接，视为认输，你获胜！"));
    }
    setStatus(opponentLeft ? QStringLiteral("对方已断开（视为认输），你获胜") : QStringLiteral("连接已断开"));
    updateTurnHint();
}

void GameWindow::onSearchFailed(const QString& reason)
{
    // 配对失败/中断：恢复「联机对战」按钮，允许重新尝试
    setConnectedUi(false);
    setStatus(reason);
}

void GameWindow::onUndoClicked()
{
    if (!m_connected || m_gameOver)
    {
        return;
    }
    if (m_undoPending)
    {
        setStatus(QStringLiteral("已发送悔棋请求，等待对方回复…"));
        return;
    }
    if (moveCount() == 0)
    {
        setStatus(QStringLiteral("还没有落子，无法悔棋"));
        return;
    }
    m_undoPending = true;
    m_network->sendUndo();
    setStatus(QStringLiteral("已发送悔棋请求，等待对方回复…"));
}

void GameWindow::onUndoRequested()
{
    if (!m_connected || m_gameOver)
    {
        return;
    }
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("悔棋"));
    box.setText(QStringLiteral("对方请求悔棋（撤回最后一手），是否同意？"));
    QPushButton* acceptBtn = box.addButton(QStringLiteral("同意"), QMessageBox::AcceptRole);
    box.addButton(QStringLiteral("拒绝"), QMessageBox::RejectRole);
    box.exec();

    const bool accept = (box.clickedButton() == acceptBtn);
    m_network->sendUndoReply(accept);
    if (accept)
    {
        doUndo();
        setStatus(QStringLiteral("已同意悔棋"));
    }
    else
    {
        setStatus(QStringLiteral("已拒绝悔棋"));
    }
}

void GameWindow::onUndoAccepted()
{
    m_undoPending = false;
    doUndo();
    setStatus(QStringLiteral("对方同意悔棋"));
}

void GameWindow::onUndoRejected()
{
    m_undoPending = false;
    setStatus(QStringLiteral("对方拒绝了悔棋"));
}

void GameWindow::doUndo()
{
    if (undoLastMove())
    {
        updateTurnHint();
    }
}

void GameWindow::resetBoard()
{
    m_gameOver = false;
    m_resultShown = false;
    m_restartPending = false;
    m_peerRestartRequested = false;
    resetBoardContents();
}

void GameWindow::onSurrendered()
{
    // 对方认输：我方获胜（m_resultShown 防止随后 TCP 断开再触发 onDisconnected 重复弹窗）
    m_resultShown = true;
    playSfx(QStringLiteral("qrc:/res/win.mp3"));
    showResultDialog(QStringLiteral("对局结束"), QStringLiteral("对方认输了，你获胜！"));

    m_network->stop();
}

void GameWindow::applySkin(const QString& imagePath)
{
    applyBoardBackground(imagePath);
    QSettings settings;
    settings.setValue(QStringLiteral("skin"), imagePath);
    setStatus(QStringLiteral("已更换背景"));
}

void GameWindow::chooseSkinFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择棋盘背景图"), QString(),
        QStringLiteral("图片 (*.jpg *.jpeg *.png *.bmp)"));
    if (!path.isEmpty())
    {
        applySkin(path);
    }
}

void GameWindow::onStatusChanged(const QString& text)
{
    setStatus(text);
}

void GameWindow::playSfx(const QString& qrcPath)
{
    m_sfxPlayer->stop();
    m_sfxPlayer->setSource(QUrl(qrcPath));
    m_sfxPlayer->play();
}

void GameWindow::setConnectedUi(bool connected)
{
    m_connectAct->setEnabled(!connected);
    m_newGameAct->setEnabled(connected);
    m_undoAct->setEnabled(connected);
    m_surrenderAct->setEnabled(connected);
    m_disconnectAct->setEnabled(connected);
}

void GameWindow::setStatus(const QString& text)
{
    statusBar()->showMessage(text);
}

bool GameWindow::myTurn() const
{
    return isBlackTurn() == (m_myKind == CHESS_BLACK);
}

chess_kind_t GameWindow::opponentKind() const
{
    return (m_myKind == CHESS_BLACK) ? CHESS_WHITE : CHESS_BLACK;
}

void GameWindow::updateTurnHint()
{
    if (!m_connected)
    {
        // 未连接：隐藏回合提示（引导文字在状态栏左侧）
        m_turnLabel->setVisible(false);
        return;
    }
    m_turnLabel->setVisible(true);

    if (m_gameOver)
    {
        m_turnLabel->setText(QStringLiteral("对局结束"));
        m_turnLabel->setStyleSheet(
            QStringLiteral("background-color: #d48806; color: white; border-radius: 10px;"
                           "padding: 2px 12px; font-size: 12px; font-weight: bold;"));
        return;
    }

    if (myTurn())
    {
        m_turnLabel->setText(myTurnText());
        m_turnLabel->setStyleSheet(
            QStringLiteral("background-color: #52c41a; color: white; border-radius: 10px;"
                           "padding: 2px 12px; font-size: 12px; font-weight: bold;"));
    }
    else
    {
        m_turnLabel->setText(waitText());
        m_turnLabel->setStyleSheet(
            QStringLiteral("background-color: #8c8c8c; color: white; border-radius: 10px;"
                           "padding: 2px 12px; font-size: 12px; font-weight: bold;"));
    }
}

void GameWindow::showResultDialog(const QString& title, const QString& text)
{
    QMessageBox box(this);
    box.setWindowTitle(title);
    box.setIcon(QMessageBox::Information);
    box.setText(text);
    box.addButton(QStringLiteral("关闭"), QMessageBox::AcceptRole);
    box.exec();
}

void GameWindow::closeResultDialog()
{
    if (m_resultDialog)
    {
        m_resultDialogAutoClosed = true;  // 标记流程接管，exec() 返回后不再执行按钮分支
        m_resultDialog->close();          // 触发 exec() 返回（嵌套事件循环同步处理 close）
    }
}

// ---- OTA 更新 ----

void GameWindow::onCheckUpdateClicked()
{
    m_updateAct->setEnabled(false);
    setStatus(QStringLiteral("正在检查更新…"));
    m_updater->checkForUpdate();
}

void GameWindow::onUpdateAvailable(const QString& version, const QString& assetName, const QString& url)
{
    m_updateAct->setEnabled(true);
    const auto reply = QMessageBox::question(
        this, QStringLiteral("发现新版本"),
        QStringLiteral("发现新版本 v%1（当前 v%2）\n\n%3\n\n是否下载并更新？")
            .arg(version, QCoreApplication::applicationVersion(), assetName),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes);
    if (reply != QMessageBox::Yes)
    {
        setStatus(QStringLiteral("已取消更新"));
        return;
    }
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QDir().mkpath(dir);
    m_updater->download(url, dir);
    setStatus(QStringLiteral("正在下载更新…"));
}

void GameWindow::onUpToDate(const QString& version)
{
    m_updateAct->setEnabled(true);
    QMessageBox::information(this, QStringLiteral("检查更新"),
                             QStringLiteral("已是最新版本 v%1").arg(version));
    setStatus(QStringLiteral("已是最新版本"));
}

void GameWindow::onCheckFailed(const QString& reason)
{
    m_updateAct->setEnabled(true);
    QMessageBox::warning(this, QStringLiteral("检查更新失败"), reason);
    setStatus(QStringLiteral("检查更新失败"));
}

void GameWindow::onDownloadProgress(qint64 received, qint64 total)
{
    if (total > 0)
    {
        const int pct = static_cast<int>(received * 100 / total);
        setStatus(QStringLiteral("正在下载更新… %1%").arg(pct));
    }
}

void GameWindow::onDownloadFinished(const QString& filePath)
{
    m_updateAct->setEnabled(true);
    setStatus(QStringLiteral("更新包已下载：") + filePath);

#if defined(Q_OS_WIN)
    // Windows：解压 + 延迟覆盖脚本（主进程退出后 xcopy 新文件，再重启）
    // 注意：不用 timeout 命令——后台分离进程无控制台，timeout 会直接失败导致不覆盖
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString updDir = appDir + QStringLiteral("/updates");
    QDir().mkpath(updDir + QStringLiteral("/extracted"));

    QProcess::execute(QStringLiteral("powershell"),
                      {QStringLiteral("-NoProfile"), QStringLiteral("-Command"),
                       QStringLiteral("Expand-Archive -Force -LiteralPath '%1' -DestinationPath '%2'")
                           .arg(filePath, updDir + QStringLiteral("/extracted"))});

    QFile batFile(updDir + QStringLiteral("/update.bat"));
    if (batFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream ts(&batFile);
        ts << "@echo off\r\n"
           << "ping -n 3 127.0.0.1 >nul\r\n"
           << "xcopy /y /e /q \"" << updDir << "\\extracted\\*\" \"" << appDir << "\\\"\r\n"
           << "start \"\" \"" << appDir << "\\gobang.exe\"\r\n"
           << "rmdir /s /q \"" << updDir << "\"\r\n";
        batFile.close();
    }
    QProcess::startDetached(updDir + QStringLiteral("/update.bat"));
    close();
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
           << "rm -rf \"$UPD\"\n"
           << "open \"$BUNDLE_PARENT/gobang.app\"\n";
        scriptFile.close();
    }
    QProcess::startDetached(QStringLiteral("/bin/sh"), {scriptPath});
    setStatus(QStringLiteral("正在应用更新，程序即将重启…"));
    close();
#else
    // Linux：安装方式多样（解压目录/发行包），保持手动引导
    QMessageBox::information(
        this, QStringLiteral("更新"),
        QStringLiteral("更新包已下载到：\n%1\n\n请退出程序后，解压并覆盖原安装目录中的文件。")
            .arg(filePath));
#endif
}

void GameWindow::onDownloadFailed(const QString& reason)
{
    m_updateAct->setEnabled(true);
    QMessageBox::warning(this, QStringLiteral("下载失败"),
                         QStringLiteral("更新包下载失败：%1").arg(reason));
    setStatus(QStringLiteral("下载失败"));
}
