#pragma once

#include <QMainWindow>

#include "Chess.h"

class BoardWidget;
class NetworkManager;
class QLabel;
class QPushButton;
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
    void onRestartReceived();
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
    void showResultDialog(const QString& title, const QString& text, const QString& imagePath);
    void updateTurnLabel();                     // 回合提示（谁下/轮到你/等待）
    bool myTurn() const;
    chess_kind_t opponentKind() const;

    Chess* m_chess = nullptr;
    BoardWidget* m_board = nullptr;
    NetworkManager* m_network = nullptr;
    UpdateChecker* m_updater = nullptr;
    QLabel* m_turnLabel = nullptr;
    QPushButton* m_connectBtn = nullptr;
    QPushButton* m_newGameBtn = nullptr;
    QPushButton* m_undoBtn = nullptr;
    QPushButton* m_surrenderBtn = nullptr;
    QPushButton* m_skinBtn = nullptr;
    QPushButton* m_updateBtn = nullptr;
    QPushButton* m_disconnectBtn = nullptr;
    bool m_undoPending = false;  // 已发悔棋请求，等待对方回复
    bool m_localDisconnect = false;  // 本次断开是否本地主动（主动断开不弹"对方认输"）

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
