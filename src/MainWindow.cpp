#include "MainWindow.h"

#include <QAction>
#include <QAudioOutput>
#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
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

#include "BoardWidget.h"
#include "NetworkManager.h"
#include "UpdateChecker.h"

namespace {

// 19×19 棋盘（新棋盘底图实测：上边距68 左边距72 格距25.3，600×600）
const int kGradeSize = 19;
const int kMarginX = 72;
const int kMarginY = 68;
const float kChessSize = 25.3f;

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("五子棋 · 局域网对战"));

    // 棋盘模型 + 绘制组件
    m_chess = new Chess(kGradeSize, kMarginX, kMarginY, kChessSize);
    m_board = new BoardWidget(this);
    m_board->setChess(m_chess);
    connect(m_board, &BoardWidget::cellClicked, this, &MainWindow::onCellClicked);

    // 菜单栏操作（macOS 集成到屏幕顶部系统菜单栏；Windows/Linux 显示在窗口顶部）
    m_connectAct = new QAction(QStringLiteral("联机对战"), this);
    m_newGameAct = new QAction(QStringLiteral("新游戏"), this);
    m_undoAct = new QAction(QStringLiteral("悔棋"), this);
    m_surrenderAct = new QAction(QStringLiteral("认输"), this);
    m_disconnectAct = new QAction(QStringLiteral("断开"), this);
    m_newGameAct->setEnabled(false);
    m_undoAct->setEnabled(false);
    m_surrenderAct->setEnabled(false);
    m_disconnectAct->setEnabled(false);
    connect(m_connectAct, &QAction::triggered, this, &MainWindow::onConnectClicked);
    connect(m_newGameAct, &QAction::triggered, this, &MainWindow::onNewGameClicked);
    connect(m_undoAct, &QAction::triggered, this, &MainWindow::onUndoClicked);
    connect(m_surrenderAct, &QAction::triggered, this, &MainWindow::onSurrenderClicked);
    connect(m_disconnectAct, &QAction::triggered, this, &MainWindow::onDisconnectClicked);

    // 菜单栏（macOS 上集成到屏幕顶部系统菜单栏；按类型分菜单，不堆在应用菜单里）
    QMenu* gameMenu = menuBar()->addMenu(QStringLiteral("游戏"));
    gameMenu->addAction(m_connectAct);
    gameMenu->addAction(m_newGameAct);
    gameMenu->addAction(m_undoAct);
    gameMenu->addAction(m_surrenderAct);
    gameMenu->addSeparator();
    gameMenu->addAction(m_disconnectAct);

    // 「皮肤」菜单（内置 4 款棋盘 + 从图片选择）
    QMenu* skinMenu = menuBar()->addMenu(QStringLiteral("皮肤"));
    skinMenu->addAction(QStringLiteral("米白色棋盘"), this,
                        [this] { applySkin(QStringLiteral(":/res/board_1_cream.png")); });
    skinMenu->addAction(QStringLiteral("翡翠绿棋盘"), this,
                        [this] { applySkin(QStringLiteral(":/res/board_2_mint.png")); });
    skinMenu->addAction(QStringLiteral("浅橙色棋盘"), this,
                        [this] { applySkin(QStringLiteral(":/res/board_3_peach.png")); });
    skinMenu->addAction(QStringLiteral("木质棋盘"), this,
                        [this] { applySkin(QStringLiteral(":/res/board_4_wood.png")); });
    skinMenu->addSeparator();
    skinMenu->addAction(QStringLiteral("从图片选择…"), this, [this] { chooseSkinFile(); });
    m_skinAct = skinMenu->menuAction();

    // 「帮助」菜单：检查更新（OTA）
    m_updateAct = new QAction(QStringLiteral("检查更新"), this);
    connect(m_updateAct, &QAction::triggered, this, &MainWindow::onCheckUpdateClicked);
    QMenu* helpMenu = menuBar()->addMenu(QStringLiteral("帮助"));
    helpMenu->addAction(m_updateAct);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->addWidget(m_board);  // 拉伸填满，窗口缩放时棋盘等比跟随
    layout->setContentsMargins(8, 8, 8, 8);
    setCentralWidget(central);

    statusBar()->showMessage(QStringLiteral("未连接 · 点击「联机对战」输入密码配对"));

    // 回合提示：状态栏右下角永久区（左侧为状态消息，一左一右）
    m_turnLabel = new QLabel(this);
    m_turnLabel->setVisible(false);
    statusBar()->addPermanentWidget(m_turnLabel);
    updateTurnHint();

    // 联机
    m_network = new NetworkManager(this);
    connect(m_network, &NetworkManager::statusChanged, this, &MainWindow::onStatusChanged);
    connect(m_network, &NetworkManager::connected, this, &MainWindow::onConnected);
    connect(m_network, &NetworkManager::searchFailed, this, &MainWindow::onSearchFailed);
    connect(m_network, &NetworkManager::moveReceived, this, &MainWindow::onMoveReceived);
    connect(m_network, &NetworkManager::restartRequested, this, &MainWindow::onRestartRequested);
    connect(m_network, &NetworkManager::restartAccepted, this, &MainWindow::onRestartAccepted);
    connect(m_network, &NetworkManager::restartRejected, this, &MainWindow::onRestartRejected);
    connect(m_network, &NetworkManager::undoRequested, this, &MainWindow::onUndoRequested);
    connect(m_network, &NetworkManager::undoAccepted, this, &MainWindow::onUndoAccepted);
    connect(m_network, &NetworkManager::undoRejected, this, &MainWindow::onUndoRejected);
    connect(m_network, &NetworkManager::surrendered, this, &MainWindow::onSurrendered);
    connect(m_network, &NetworkManager::disconnected, this, &MainWindow::onDisconnected);

    // OTA 更新
    m_updater = new UpdateChecker(this);
    connect(m_updater, &UpdateChecker::updateAvailable, this, &MainWindow::onUpdateAvailable);
    connect(m_updater, &UpdateChecker::upToDate, this, &MainWindow::onUpToDate);
    connect(m_updater, &UpdateChecker::checkFailed, this, &MainWindow::onCheckFailed);
    connect(m_updater, &UpdateChecker::downloadProgress, this, &MainWindow::onDownloadProgress);
    connect(m_updater, &UpdateChecker::downloadFinished, this, &MainWindow::onDownloadFinished);
    connect(m_updater, &UpdateChecker::downloadFailed, this, &MainWindow::onDownloadFailed);

    // 恢复上次换肤（默认翡翠绿棋盘）
    QSettings settings;
    const QString skin = settings.value(QStringLiteral("skin"),
                                        QStringLiteral(":/res/board_2_mint.png")).toString();
    m_board->setBackground(skin);

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

    // 窗口尺寸紧凑：无按钮行后高度只需 棋盘(624) + 边距 + 状态栏
    resize(640, 675);
}

MainWindow::~MainWindow() = default;

void MainWindow::startOnline(const QString& password)
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

void MainWindow::closeEvent(QCloseEvent* event)
{
    m_network->stop();
    event->accept();
}

void MainWindow::onConnectClicked()
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

void MainWindow::onNewGameClicked()
{
    // 对局进行中（已落子且未分胜负）：点「新游戏」视为认输
    if (m_connected && m_chess->moveCount() > 0 && !m_gameOver)
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

    // 对局已结束（结果窗「再来一局」/平局窗）：发重开请求，需对方同意
    if (m_gameOver)
    {
        if (m_restartPending)
        {
            setStatus(QStringLiteral("已发送重开请求，等待对方同意…"));
            return;
        }
        if (m_connected)
        {
            m_restartPending = true;
            m_network->sendRestartRequest();
            setStatus(QStringLiteral("已发送重开请求，等待对方同意…"));
        }
        return;
    }

    // 未开局：本地重开（双方棋盘本就为空，无需通知对方）
    resetBoard();
}

void MainWindow::onDisconnectClicked()
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

void MainWindow::onSurrenderClicked()
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

void MainWindow::onCellClicked(int row, int col)
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
        setStatus(QStringLiteral("等待对方落子…"));
        return;
    }
    if (m_chess->getChessData(row, col) != 0)
    {
        return; // 已有棋子
    }

    ChessPos pos(row, col);
    m_chess->chessDown(&pos, m_myKind);
    m_board->repaintBoard();
    m_board->setLastMove(row, col);
    m_downSound->play();
    m_network->sendMove(row, col);
    updateTurnHint();
    checkGameEnd(m_myKind);
}

void MainWindow::onConnected(bool isHost)
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
    setWindowTitle(QStringLiteral("五子棋 · 局域网对战 - 对手 ") + peerIp);
    updateTurnHint();
}

void MainWindow::onMoveReceived(int row, int col)
{
    if (!m_connected || m_gameOver)
    {
        return;
    }
    if (m_chess->getChessData(row, col) != 0)
    {
        return; // 已占
    }

    ChessPos pos(row, col);
    m_chess->chessDown(&pos, opponentKind());
    m_board->repaintBoard();
    m_board->setLastMove(row, col);
    m_downSound->play();
    updateTurnHint();
    checkGameEnd(opponentKind());
}

void MainWindow::onRestartRequested()
{
    if (!m_connected)
    {
        return;
    }
    // 仅对局已结束才弹确认；对局中收到视为误发，忽略
    if (!m_gameOver)
    {
        return;
    }
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("再来一局"));
    box.setText(QStringLiteral("对方请求再来一局，是否同意？"));
    QPushButton* yes = box.addButton(QStringLiteral("同意"), QMessageBox::AcceptRole);
    box.addButton(QStringLiteral("拒绝"), QMessageBox::RejectRole);
    box.exec();

    const bool accept = (box.clickedButton() == yes);
    m_network->sendRestartReply(accept);
    if (accept)
    {
        resetBoard();
        setStatus(QStringLiteral("对方请求重开，已同意 · 新的一局"));
    }
    else
    {
        setStatus(QStringLiteral("已拒绝对方的重开请求"));
    }
}

void MainWindow::onRestartAccepted()
{
    m_restartPending = false;
    resetBoard();
    updateTurnHint();
    setStatus(QStringLiteral("对方同意重开 · 新的一局"));
}

void MainWindow::onRestartRejected()
{
    m_restartPending = false;
    setStatus(QStringLiteral("对方拒绝了重开请求"));
}

void MainWindow::onDisconnected()
{
    // 对局中且非本地主动断开、且未弹过结果 → 对方认输（QUIT 或掉线）
    const bool inGame = m_chess->moveCount() > 0;
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
    setWindowTitle(QStringLiteral("五子棋 · 局域网对战"));

    if (opponentLeft)
    {
        playSfx(QStringLiteral("qrc:/res/win.mp3"));
        showResultDialog(QStringLiteral("对局结束"),
                         QStringLiteral("对方已断开连接，视为认输，你获胜！"));
    }
    setStatus(opponentLeft ? QStringLiteral("对方已断开（视为认输），你获胜") : QStringLiteral("连接已断开"));
    updateTurnHint();
}

void MainWindow::onSearchFailed(const QString& reason)
{
    // 配对失败/中断：恢复「联机对战」按钮，允许重新尝试
    setConnectedUi(false);
    setStatus(reason);
}

void MainWindow::onUndoClicked()
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
    if (m_chess->moveCount() == 0)
    {
        setStatus(QStringLiteral("还没有落子，无法悔棋"));
        return;
    }
    m_undoPending = true;
    m_network->sendUndo();
    setStatus(QStringLiteral("已发送悔棋请求，等待对方回复…"));
}

void MainWindow::onUndoRequested()
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

void MainWindow::onUndoAccepted()
{
    m_undoPending = false;
    doUndo();
    setStatus(QStringLiteral("对方同意悔棋"));
}

void MainWindow::onUndoRejected()
{
    m_undoPending = false;
    setStatus(QStringLiteral("对方拒绝了悔棋"));
}

void MainWindow::doUndo()
{
    if (m_chess->undoLast())
    {
        m_board->repaintBoard();
        m_board->clearLastMove();  // 悔棋后无最后一手标记
        updateTurnHint();
    }
}

void MainWindow::onSurrendered()
{
    // 对方认输：我方获胜（m_resultShown 防止随后 TCP 断开再触发 onDisconnected 重复弹窗）
    m_resultShown = true;
    playSfx(QStringLiteral("qrc:/res/win.mp3"));
    showResultDialog(QStringLiteral("对局结束"), QStringLiteral("对方认输了，你获胜！"));

    m_network->stop();
}

void MainWindow::applySkin(const QString& imagePath)
{
    m_board->setBackground(imagePath);
    QSettings settings;
    settings.setValue(QStringLiteral("skin"), imagePath);
    setStatus(QStringLiteral("已更换背景"));
}

void MainWindow::chooseSkinFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择棋盘背景图"), QString(),
        QStringLiteral("图片 (*.jpg *.jpeg *.png *.bmp)"));
    if (!path.isEmpty())
    {
        applySkin(path);
    }
}

void MainWindow::onStatusChanged(const QString& text)
{
    setStatus(text);
}

void MainWindow::resetBoard()
{
    m_gameOver = false;
    m_resultShown = false;
    m_restartPending = false;
    m_chess->init();
    m_board->repaintBoard();
    m_board->clearLastMove();
    updateTurnHint();
}

void MainWindow::checkGameEnd(chess_kind_t lastKind)
{
    if (m_chess->checkOver())
    {
        m_gameOver = true;
        m_resultShown = true;
        const bool iWin = (lastKind == m_myKind);
        playSfx(iWin ? QStringLiteral("qrc:/res/win.mp3") : QStringLiteral("qrc:/res/lose.mp3"));

        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("对局结束"));
        box.setIcon(QMessageBox::Information);
        box.setText(iWin ? QStringLiteral("你赢了！") : QStringLiteral("你输了！"));
        QPushButton* again = box.addButton(QStringLiteral("再来一局"), QMessageBox::AcceptRole);
        box.addButton(QStringLiteral("断开"), QMessageBox::RejectRole);
        box.exec();

        if (box.clickedButton() == again)
        {
            onNewGameClicked();
        }
        else
        {
            // 断开连接：对局已结束不视为认输（m_resultShown 已防误判），留在窗口可重新联机
            m_network->stop();
        }
        return;
    }

    // 棋盘下满未分胜负 -> 平局
    bool full = true;
    const int size = m_chess->getGradeSize();
    for (int r = 0; r < size && full; r++)
    {
        for (int c = 0; c < size; c++)
        {
            if (m_chess->getChessData(r, c) == 0)
            {
                full = false;
                break;
            }
        }
    }
    if (full)
    {
        m_gameOver = true;
        m_resultShown = true;
        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("对局结束"));
        box.setIcon(QMessageBox::Information);
        box.setText(QStringLiteral("平局！"));
        QPushButton* again = box.addButton(QStringLiteral("再来一局"), QMessageBox::AcceptRole);
        box.addButton(QStringLiteral("断开"), QMessageBox::RejectRole);
        box.exec();

        if (box.clickedButton() == again)
        {
            onNewGameClicked();
        }
        else
        {
            // 断开连接：对局已结束不视为认输，留在窗口可重新联机
            m_network->stop();
        }
    }
}

void MainWindow::playSfx(const QString& qrcPath)
{
    m_sfxPlayer->stop();
    m_sfxPlayer->setSource(QUrl(qrcPath));
    m_sfxPlayer->play();
}

void MainWindow::setConnectedUi(bool connected)
{
    m_connectAct->setEnabled(!connected);
    m_newGameAct->setEnabled(connected);
    m_undoAct->setEnabled(connected);
    m_surrenderAct->setEnabled(connected);
    m_disconnectAct->setEnabled(connected);
}

void MainWindow::setStatus(const QString& text)
{
    statusBar()->showMessage(text);
}

bool MainWindow::myTurn() const
{
    return m_chess->isBlackTurn() == (m_myKind == CHESS_BLACK);
}

chess_kind_t MainWindow::opponentKind() const
{
    return (m_myKind == CHESS_BLACK) ? CHESS_WHITE : CHESS_BLACK;
}

void MainWindow::updateTurnHint()
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

    const bool black = (m_myKind == CHESS_BLACK);
    if (myTurn())
    {
        m_turnLabel->setText(QStringLiteral("● 轮到你落子（%1）")
                                 .arg(black ? QStringLiteral("黑") : QStringLiteral("白")));
        m_turnLabel->setStyleSheet(
            QStringLiteral("background-color: #52c41a; color: white; border-radius: 10px;"
                           "padding: 2px 12px; font-size: 12px; font-weight: bold;"));
    }
    else
    {
        m_turnLabel->setText(QStringLiteral("○ 等待对方落子…"));
        m_turnLabel->setStyleSheet(
            QStringLiteral("background-color: #8c8c8c; color: white; border-radius: 10px;"
                           "padding: 2px 12px; font-size: 12px; font-weight: bold;"));
    }
}

void MainWindow::showResultDialog(const QString& title, const QString& text)
{
    QMessageBox box(this);
    box.setWindowTitle(title);
    box.setIcon(QMessageBox::Information);
    box.setText(text);
    box.addButton(QStringLiteral("关闭"), QMessageBox::AcceptRole);
    box.exec();
}

// ---- OTA 更新 ----

void MainWindow::onCheckUpdateClicked()
{
    m_updateAct->setEnabled(false);
    setStatus(QStringLiteral("正在检查更新…"));
    m_updater->checkForUpdate();
}

void MainWindow::onUpdateAvailable(const QString& version, const QString& assetName, const QString& url)
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

void MainWindow::onUpToDate(const QString& version)
{
    m_updateAct->setEnabled(true);
    QMessageBox::information(this, QStringLiteral("检查更新"),
                             QStringLiteral("已是最新版本 v%1").arg(version));
    setStatus(QStringLiteral("已是最新版本"));
}

void MainWindow::onCheckFailed(const QString& reason)
{
    m_updateAct->setEnabled(true);
    QMessageBox::warning(this, QStringLiteral("检查更新失败"), reason);
    setStatus(QStringLiteral("检查更新失败"));
}

void MainWindow::onDownloadProgress(qint64 received, qint64 total)
{
    if (total > 0)
    {
        const int pct = static_cast<int>(received * 100 / total);
        setStatus(QStringLiteral("正在下载更新… %1%").arg(pct));
    }
}

void MainWindow::onDownloadFinished(const QString& filePath)
{
    m_updateAct->setEnabled(true);
    setStatus(QStringLiteral("更新包已下载：") + filePath);

#if defined(Q_OS_WIN)
    // Windows：自动解压 + 覆盖脚本（主进程退出后 xcopy 新文件，再重启）
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
           << "timeout /t 2 /nobreak >nul\r\n"
           << "xcopy /y /e /q \"" << updDir << "\\extracted\\*\" \"" << appDir << "\\\"\r\n"
           << "start \"\" \"" << appDir << "\\gobang.exe\"\r\n"
           << "del /q \"" << updDir << "\\update.bat\"\r\n";
        batFile.close();
    }
    QProcess::startDetached(updDir + QStringLiteral("/update.bat"));
    close();
#else
    // macOS/Linux：运行中的程序无法自覆盖，引导用户解压覆盖
    QMessageBox::information(
        this, QStringLiteral("更新"),
        QStringLiteral("更新包已下载到：\n%1\n\n请退出程序后，解压并覆盖原安装目录中的文件。")
            .arg(filePath));
#endif
}

void MainWindow::onDownloadFailed(const QString& reason)
{
    m_updateAct->setEnabled(true);
    QMessageBox::warning(this, QStringLiteral("下载失败"),
                         QStringLiteral("更新包下载失败：%1").arg(reason));
    setStatus(QStringLiteral("下载失败"));
}
