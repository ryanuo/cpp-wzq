#pragma once

#include <vector>

enum chess_kind_t {
    CHESS_WHITE = -1,
    CHESS_BLACK = 1
};

struct ChessPos {
    int row;
    int col;

    ChessPos(int r = 0, int c = 0) : row(r), col(c) {}
};

// 纯棋盘模型（原 EasyX 版剥离绘图/音效后保留）
class Chess
{
public:
    Chess(int gradeSize, int marginX, int marginY, float chessSize);

    void init();                                          // 清空棋盘，黑方先行
    bool clickBoard(int x, int y, ChessPos* pos);         // 像素坐标 -> 格点（含越界/占用校验）
    void chessDown(ChessPos* pos, chess_kind_t kind);     // 落子并更新回合
    int getGradeSize() const { return gradeSize; }
    int getMarginX() const { return margin_x; }
    int getMarginY() const { return margin_y; }
    float getChessSize() const { return chessSize; }
    int getChessData(int row, int col) const;
    bool checkOver();                                     // 是否分出胜负（依据最后一手）
    bool isBlackTurn() const { return playerFlag; }       // true=轮到黑方
    bool undoLast();                                      // 撤回最后一手（悔棋），返回是否成功
    int moveCount() const { return static_cast<int>(moveHistory.size()); }

private:
    struct MoveRec {
        int row;
        int col;
        chess_kind_t kind;
    };

    bool checkWin();

    int gradeSize;
    int margin_x;
    int margin_y;
    float chessSize;

    std::vector<std::vector<int>> chessMap;
    std::vector<MoveRec> moveHistory;  // 落子历史（悔棋用）
    bool playerFlag;
    ChessPos lastPos;
};
