// Chess 模型单元测试（悔棋/回合/落子），headless，无 Qt 依赖
#include <cstdio>

#include "Chess.h"

static int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("FAIL: %s (line %d)\n", #cond, __LINE__);            \
            g_failures++;                                                    \
        }                                                                    \
    } while (0)

int main()
{
    Chess chess(13, 29, 29, 45);
    chess.init();

    // 空盘：无子可悔
    CHECK(chess.moveCount() == 0);
    CHECK(!chess.undoLast());

    // 黑落子
    ChessPos p1(6, 6);
    chess.chessDown(&p1, CHESS_BLACK);
    CHECK(chess.getChessData(6, 6) == CHESS_BLACK);
    CHECK(!chess.isBlackTurn());  // 黑落子后轮白
    CHECK(chess.moveCount() == 1);

    // 白落子
    ChessPos p2(6, 7);
    chess.chessDown(&p2, CHESS_WHITE);
    CHECK(chess.getChessData(6, 7) == CHESS_WHITE);
    CHECK(chess.isBlackTurn());  // 白落子后轮黑

    // 悔棋撤回白
    CHECK(chess.undoLast());
    CHECK(chess.getChessData(6, 7) == 0);
    CHECK(!chess.isBlackTurn());  // 撤回白 → 轮白
    CHECK(chess.moveCount() == 1);
    CHECK(!chess.checkOver());

    // 悔棋撤回黑
    CHECK(chess.undoLast());
    CHECK(chess.getChessData(6, 6) == 0);
    CHECK(chess.isBlackTurn());  // 撤回黑 → 轮黑
    CHECK(chess.moveCount() == 0);
    CHECK(!chess.undoLast());  // 已空盘

    // 悔棋后 lastPos 回到上一手：连下 3 手撤回 2 手，checkOver 依据 lastPos 不应误判
    ChessPos a(3, 3);
    ChessPos b(4, 4);
    ChessPos c(5, 5);
    chess.chessDown(&a, CHESS_BLACK);
    chess.chessDown(&b, CHESS_WHITE);
    chess.chessDown(&c, CHESS_BLACK);
    chess.undoLast();  // 撤 (5,5) 黑
    chess.undoLast();  // 撤 (4,4) 白
    CHECK(chess.getChessData(5, 5) == 0);
    CHECK(chess.getChessData(4, 4) == 0);
    CHECK(chess.getChessData(3, 3) == CHESS_BLACK);
    CHECK(chess.isBlackTurn() == false);  // 撤白后轮白
    CHECK(!chess.checkOver());            // lastPos 是 (3,3) 黑，未成五连

    // 新游戏清空历史
    chess.init();
    CHECK(chess.moveCount() == 0);

    if (g_failures == 0)
    {
        std::printf("ALL CHESS TESTS PASSED\n");
        return 0;
    }
    std::printf("%d FAILURE(S)\n", g_failures);
    return 1;
}
