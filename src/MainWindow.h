#pragma once

#include <QMainWindow>

#include "Chess.h"

class BoardWidget;
class NetworkManager;
class QPushButton;
class QMediaPlayer;
class QAudioOutput;
class QSoundEffect;

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

private:
    void resetBoard();
    void checkGameEnd(chess_kind_t lastKind);
    void playSfx(const QString& qrcPath);
    void setStatus(const QString& text);
    void setConnectedUi(bool connected);
    void doUndo();
    void applySkin(const QString& imagePath);   // 换肤（qrc 路径或本地文件）
    void chooseSkinFile();
    bool myTurn() const;
    chess_kind_t opponentKind() const;

    Chess* m_chess = nullptr;
    BoardWidget* m_board = nullptr;
    NetworkManager* m_network = nullptr;
    QPushButton* m_connectBtn = nullptr;
    QPushButton* m_newGameBtn = nullptr;
    QPushButton* m_undoBtn = nullptr;
    QPushButton* m_surrenderBtn = nullptr;
    QPushButton* m_skinBtn = nullptr;
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
