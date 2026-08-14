#pragma once

#include <QMainWindow>

#include "Chess.h"

class BoardWidget;
class NetworkManager;
class QAction;
class QLabel;
class QMediaPlayer;
class QAudioOutput;
class QSoundEffect;
class UpdateChecker;

// 主窗口：联机对战唯一模式（黑方 HOST 先手，白方 CLIENT 后手）
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void startOnline(const QString& password); // 供 --online 参数调用

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onConnectClicked();
    void onNewGameClicked();
    void onDisconnectClicked();
    void onUndoClicked();
    void onSurrenderClicked();
    void onCellClicked(int row, int col);
    void onConnected(bool isHost);
    void onMoveReceived(int row, int col);
    void onRestartRequested();   // 对方请求再来一局（弹确认框）
    void onRestartAccepted();    // 对方同意重开
    void onRestartRejected();    // 对方拒绝重开
    void onSearchFailed(const QString& reason);
    void onDisconnected();
    void onStatusChanged(const QString& text);
    void onUndoRequested();
    void onUndoAccepted();
    void onUndoRejected();
    void onSurrendered();
    void onCheckUpdateClicked();
    void onUpdateAvailable(const QString& version, const QString& assetName, const QString& url);
    void onUpToDate(const QString& version);
    void onCheckFailed(const QString& reason);
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished(const QString& filePath);
    void onDownloadFailed(const QString& reason);

private:
    void resetBoard();
    void checkGameEnd(chess_kind_t lastKind);
    void playSfx(const QString& qrcPath);
    void setStatus(const QString& text);
    void setConnectedUi(bool connected);
    void doUndo();
    void applySkin(const QString& imagePath);   // 换肤（qrc 路径或本地文件）
    void chooseSkinFile();
    void showResultDialog(const QString& title, const QString& text);
    void updateTurnHint();                      // 回合提示（绘制在棋盘顶部横幅）
    bool myTurn() const;
    chess_kind_t opponentKind() const;

    Chess* m_chess = nullptr;
    BoardWidget* m_board = nullptr;
    NetworkManager* m_network = nullptr;
    UpdateChecker* m_updater = nullptr;
    QLabel* m_turnLabel = nullptr;   // 回合提示（状态栏右下角永久区）
    QAction* m_connectAct = nullptr; // 操作按钮（菜单栏，macOS 集成系统菜单栏）
    QAction* m_newGameAct = nullptr;
    QAction* m_undoAct = nullptr;
    QAction* m_surrenderAct = nullptr;
    QAction* m_skinAct = nullptr;    // 皮肤菜单
    QAction* m_updateAct = nullptr;
    QAction* m_disconnectAct = nullptr;
    bool m_undoPending = false;  // 已发悔棋请求，等待对方回复
    bool m_restartPending = false;  // 已发重开请求，等待对方回复
    bool m_localDisconnect = false;  // 本次断开是否本地主动（主动断开不弹"对方认输"）
    bool m_resultShown = false;  // 本次对局结果已弹窗（防止对方认输后再触发断线弹窗）

    chess_kind_t m_myKind = CHESS_BLACK;
    bool m_connected = false;
    bool m_gameOver = false;

    QMediaPlayer* m_bgPlayer = nullptr;
    QAudioOutput* m_bgAudio = nullptr;
    QMediaPlayer* m_sfxPlayer = nullptr;
    QAudioOutput* m_sfxAudio = nullptr;
    QSoundEffect* m_startSound = nullptr;
    QSoundEffect* m_downSound = nullptr;
};
