#pragma once

// 公共棋类枚举/结构（五子棋与象棋共用）
// 约定：CHESS_BLACK = 先手方（五子棋黑方 / 象棋红方），CHESS_WHITE = 后手方
enum chess_kind_t {
    CHESS_WHITE = -1,
    CHESS_BLACK = 1
};

struct ChessPos {
    int row;
    int col;

    ChessPos(int r = 0, int c = 0) : row(r), col(c) {}
};
