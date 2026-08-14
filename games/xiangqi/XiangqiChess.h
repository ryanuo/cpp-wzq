#pragma once

#include <vector>

#include "../common/ChessTypes.h"

// 棋子类型（红 > 0，黑 < 0，0 = 空）
enum xq_piece_t {
    PIECE_NONE = 0,
    PIECE_RED_GENERAL = 1,    // 帅
    PIECE_RED_ADVISOR = 2,    // 仕
    PIECE_RED_ELEPHANT = 3,   // 相
    PIECE_RED_HORSE = 4,      // 马
    PIECE_RED_ROOK = 5,       // 车
    PIECE_RED_CANNON = 6,     // 炮
    PIECE_RED_PAWN = 7,       // 兵
    PIECE_BLACK_GENERAL = -1, // 将
    PIECE_BLACK_ADVISOR = -2, // 士
    PIECE_BLACK_ELEPHANT = -3,// 象
    PIECE_BLACK_HORSE = -4,   // 马
    PIECE_BLACK_ROOK = -5,    // 車
    PIECE_BLACK_CANNON = -6,  // 砲
    PIECE_BLACK_PAWN = -7,    // 卒
};

// 象棋规则层（纯逻辑，可单测）：9 列 × 10 行，红方先手
// 行 0 = 黑方底线，行 9 = 红方底线；胜负：吃掉对方帅/将即胜（简化版）
class XiangqiChess
{
public:
    static constexpr int kCols = 9;
    static constexpr int kRows = 10;

    XiangqiChess();

    void init();                              // 初始布子，红方先手
    int pieceAt(int row, int col) const;      // 该格棋子（0 = 空）
    bool isRedTurn() const { return redTurn; }
    int moveCount() const { return static_cast<int>(history_.size()); }
    int generalOf(bool red) const;            // 帅/将所在行（-1 = 已被吃）

    bool canSelect(int row, int col) const;   // 当前回合的己方棋子
    bool canMove(int fr, int fc, int tr, int tc) const;  // 走法合法性（含吃子）
    bool doMove(int fr, int fc, int tr, int tc);  // 执行落子（非法返回 false）
    bool undoLast();                          // 悔棋
    bool generalCaptured() const;             // 已吃对方帅/将（胜负）
    void setPiece(int row, int col, int piece);  // 测试辅助：直接摆子（不切换回合）

    // 规则辅助（供测试/绘制）
    static bool inBoard(int r, int c);
    static bool inPalace(int r, int c, bool red);       // 九宫
    static bool pieceBelongs(int piece, bool red);      // 属于红方
    static bool sameSide(int a, int b);                 // 同色棋子
    static const char* pieceName(int piece);            // "帥/車/馬…"

private:
    struct MoveRec {
        int fromRow, fromCol, toRow, toCol;
        int captured;
        bool redMoved;
    };

    bool canMoveRook(int fr, int fc, int tr, int tc) const;
    bool canMoveHorse(int fr, int fc, int tr, int tc) const;
    bool canMoveElephant(int fr, int fc, int tr, int tc) const;
    bool canMoveAdvisor(int fr, int fc, int tr, int tc) const;
    bool canMoveGeneral(int fr, int fc, int tr, int tc) const;
    bool canMoveCannon(int fr, int fc, int tr, int tc) const;
    bool canMovePawn(int fr, int fc, int tr, int tc) const;
    bool generalsFacing(int fr, int fc, int tr, int tc) const;  // 走子后是否将帅对脸
    int countBetween(int r1, int c1, int r2, int c2) const;     // 两点间直线上的棋子数

    std::vector<std::vector<int>> board_;  // [row][col]
    bool redTurn = true;
    std::vector<MoveRec> history_;
};
