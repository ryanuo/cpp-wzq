#pragma once

#include "../common/GameWindow.h"

class XiangqiBoard;
class XiangqiChess;

// 象棋窗口：继承 GameWindow 公共框架
// 交互为两段式（选中己方棋子 -> 点击目标格走子），MOVE 协议发起止格
class XiangqiWindow : public GameWindow
{
    Q_OBJECT

public:
    explicit XiangqiWindow(QWidget* parent = nullptr);

protected:
    // 覆盖基类落子入口：两段式选子
    void onCellClicked(int row, int col) override;
    void onMoveFromToReceived(int fr, int fc, int tr, int tc) override;

    void fillSkinMenu(QMenu* skinMenu) override;
    bool canPlace(int row, int col) const override;
    int moveCount() const override;
    void placePiece(int row, int col, chess_kind_t kind) override;
    void checkGameEnd(chess_kind_t lastKind) override;
    void resetBoardContents() override;
    bool undoLastMove() override;
    bool isBlackTurn() const override;
    QString myTurnText() const override;
    QString waitText() const override;
    void applyBoardBackground(const QString& imagePath) override;

private:
    void doLocalMove(int fr, int fc, int tr, int tc, bool notify);
    void saveSkinPath(const QString& path);  // 记住象棋皮肤选择（qrc 路径或本地文件）

    XiangqiChess* m_chess = nullptr;
    XiangqiBoard* m_board = nullptr;
    int m_selectedRow = -1;  // 当前选中的棋子（-1 = 未选中）
    int m_selectedCol = -1;
};
