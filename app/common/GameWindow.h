#pragma once

#include <QMainWindow>
#include <QPointer>

#include "ChessTypes.h"

class GameWindow;
class NetworkManager;
class QAction;
class QLabel;
class QMenu;
class QMediaPlayer;
class QAudioOutput;
class QMessageBox;
class QSoundEffect;
class QVBoxLayout;
class UpdateChecker;

// 游戏窗口公共基类：封装两个游戏完全相同的部分
//   - 菜单栏（游戏菜单 + 帮助-检查更新；皮肤菜单由子类填充）
//   - 状态栏（左侧状态消息 + 右侧回合提示胶囊）
//   - 音效（落子/开始/胜负/背景乐）
//   - OTA 更新（UpdateChecker 挂载）
//   - 联机状态机（密码配对 / MOVE / 悔棋 / 再来一局 / 认输 / 断线 / 防重入标志）
// 子类只实现棋盘创建、落子规则、胜负判定与回合文案。
class GameWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit GameWindow(const QString& title, QWidget* parent = nullptr);
    ~GameWindow() override;

    void startOnline(const QString& password); // 供 --online 参数调用

protected:
    void closeEvent(QCloseEvent* event) override;
    void applySkin(const QString& imagePath);   // 换肤（qrc 路径或本地文件）
    void chooseSkinFile();

protected:
    // ---- 子类实现 ----
    virtual void fillSkinMenu(QMenu* skinMenu) = 0;        // 皮肤菜单项
    virtual bool canPlace(int row, int col) const = 0;     // 该格是否可落子（未占用等）
    virtual int moveCount() const = 0;                     // 已落子数（新游戏=认输/悔棋判断用）
    virtual void placePiece(int row, int col, chess_kind_t kind) = 0; // 规则层落子 + 棋盘重绘 + 最后一手高亮
    virtual void checkGameEnd(chess_kind_t lastKind) = 0;  // 胜负/平局判定 + 结果弹窗
    virtual void resetBoardContents() = 0;                 // 清盘：规则层 init + 重绘 + 清高亮
    virtual bool undoLastMove() = 0;                       // 悔棋：撤回最后一手 + 重绘 + 清高亮，返回是否成功
    virtual bool isBlackTurn() const = 0;                  // 当前是否先手方回合（五子棋黑 / 象棋红）
    virtual QString myTurnText() const = 0;                // 回合提示："● 轮到你落子（黑）" 等
    virtual QString waitText() const = 0;                  // 回合提示："○ 等待对方落子…"
    virtual void applyBoardBackground(const QString& imagePath) = 0; // 换肤：转调具体棋盘的 setBackground

protected slots:
    // 联机/菜单通用槽（基类实现，协议层逻辑）
    void onConnectClicked();
    void onNewGameClicked();
    void onDisconnectClicked();
    void onUndoClicked();
    void onSurrenderClicked();
    void onConnected(bool isHost);
    void onSearchFailed(const QString& reason);
    void onDisconnected();
    void onStatusChanged(const QString& text);
    void onRestartRequested();
    void onRestartAccepted();
    void onRestartRejected();
    void onUndoRequested();
    void onUndoAccepted();
    void onUndoRejected();
    void onSurrendered();
    // OTA
    void onCheckUpdateClicked();
    void onUpdateAvailable(const QString& version, const QString& assetName, const QString& url);
    void onUpToDate(const QString& version);
    void onCheckFailed(const QString& reason);
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished(const QString& filePath);
    void onDownloadFailed(const QString& reason);

protected:
    // 落子入口（子类棋盘 cellClicked 连接到此；含连接/回合/占用检查后转子类实现）
    virtual void onCellClicked(int row, int col);
    virtual void onMoveReceived(int row, int col);
    // 象棋走子（起止格；五子棋不用，默认忽略）
    virtual void onMoveFromToReceived(int fr, int fc, int tr, int tc);

    void updateTurnHint();                    // 状态栏回合提示胶囊
    void setStatus(const QString& text);
    void playSfx(const QString& qrcPath);
    void showResultDialog(const QString& title, const QString& text);
    // 自动关闭结果弹窗（收到重开请求/对方同意/拒绝/断开时调用，防止残留框
    // 被手动关闭时误走「断开」分支触发 m_network->stop()）
    void closeResultDialog();
    bool myTurn() const;
    chess_kind_t opponentKind() const;
    void setConnectedUi(bool connected);
    void doUndo();
    void resetBoard();                        // 复位对局标志 + 调 resetBoardContents（子类清盘）

    QWidget* m_boardWidget = nullptr;  // 子类棋盘（布局用）
    QVBoxLayout* m_centralLayout = nullptr;  // central 布局（子类把棋盘 addWidget 进来）
    QMenu* m_skinMenu = nullptr;       // 皮肤菜单（子类 fillSkinMenu 填充）
    NetworkManager* m_network = nullptr;
    UpdateChecker* m_updater = nullptr;
    QLabel* m_turnLabel = nullptr;     // 回合提示（状态栏右下角永久区）
    QAction* m_connectAct = nullptr;
    QAction* m_newGameAct = nullptr;
    QAction* m_undoAct = nullptr;
    QAction* m_surrenderAct = nullptr;
    QAction* m_skinAct = nullptr;
    QAction* m_updateAct = nullptr;
    QAction* m_disconnectAct = nullptr;
    QSoundEffect* m_startSound = nullptr;  // 落子/开始音效（子类播放）
    QSoundEffect* m_downSound = nullptr;
    bool m_undoPending = false;        // 已发悔棋请求，等待对方回复
    bool m_restartPending = false;     // 已发重开请求，等待对方回复
    bool m_localDisconnect = false;    // 本次断开是否本地主动
    bool m_resultShown = false;        // 本次对局结果已弹窗
    QPointer<QMessageBox> m_resultDialog;      // 当前结果弹窗（checkGameEnd 弹出，可被流程自动关闭）
    bool m_resultDialogAutoClosed = false;     // 结果弹窗是否被流程自动关闭（非用户点按钮）
    chess_kind_t m_myKind = CHESS_BLACK;
    bool m_connected = false;
    bool m_gameOver = false;

private:
    QMediaPlayer* m_bgPlayer = nullptr;
    QAudioOutput* m_bgAudio = nullptr;
    QMediaPlayer* m_sfxPlayer = nullptr;
    QAudioOutput* m_sfxAudio = nullptr;
};
