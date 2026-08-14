#pragma once

#include "../common/GameWindow.h"

class GomokuBoard;
class GomokuChess;

// 五子棋窗口：继承 GameWindow 公共框架，实现五子棋落子/胜负/皮肤
class GomokuWindow : public GameWindow
{
    Q_OBJECT

public:
    explicit GomokuWindow(QWidget* parent = nullptr);

protected:
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
    GomokuChess* m_chess = nullptr;
    GomokuBoard* m_board = nullptr;
};
