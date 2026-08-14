// 象棋规则层 headless 自测（ctest 目标）：
// 初始布局 / 各子走法合法性 / 蹩马腿·塞象眼·炮架·兵规则 / 帅对脸 / 吃将胜负 / 回合交替 / 悔棋
// 每个场景从空棋盘精确摆子，避免初始布局满盘干扰
#include <cstdio>

#include "../games/xiangqi/XiangqiChess.h"

namespace {

int g_fail = 0;

#define CHECK(cond, msg)                                                     \
    do                                                                       \
    {                                                                        \
        if (!(cond))                                                         \
        {                                                                    \
            std::printf("FAIL: %s (line %d)\n", msg, __LINE__);              \
            g_fail++;                                                        \
        }                                                                    \
    } while (0)

void clearBoard(XiangqiChess& xq)
{
    for (int r = 0; r < XiangqiChess::kRows; r++)
    {
        for (int c = 0; c < XiangqiChess::kCols; c++)
        {
            xq.setPiece(r, c, PIECE_NONE);
        }
    }
}

} // namespace

int main()
{
    // 1. 初始布局
    XiangqiChess xq;
    CHECK(xq.pieceAt(9, 4) == PIECE_RED_GENERAL, "红帅在底线中央");
    CHECK(xq.pieceAt(0, 4) == PIECE_BLACK_GENERAL, "黑将在底线中央");
    CHECK(xq.pieceAt(9, 0) == PIECE_RED_ROOK && xq.pieceAt(0, 8) == PIECE_BLACK_ROOK, "車位");
    CHECK(xq.pieceAt(8, 1) == PIECE_RED_CANNON && xq.pieceAt(7, 0) == PIECE_RED_PAWN, "炮/兵位");
    CHECK(xq.isRedTurn(), "红方先手");
    CHECK(xq.moveCount() == 0, "初始 0 步");
    CHECK(xq.canSelect(9, 0), "红车可选中");
    CHECK(!xq.canSelect(0, 0), "黑車不可选中(红方回合)");
    CHECK(!xq.canSelect(5, 5), "空格不可选中");

    // 2. 车：直线 + 阻挡
    clearBoard(xq);
    xq.setPiece(9, 0, PIECE_RED_ROOK);
    CHECK(xq.canMove(9, 0, 9, 5), "车横移");
    CHECK(xq.canMove(9, 0, 5, 0), "车纵移");
    CHECK(!xq.canMove(9, 0, 5, 5), "车不可斜走");
    CHECK(!xq.canMove(9, 0, 9, 0), "车不可原地");
    xq.setPiece(9, 4, PIECE_RED_PAWN);
    CHECK(!xq.canMove(9, 0, 9, 6), "车被己方阻挡");
    xq.setPiece(5, 0, PIECE_BLACK_PAWN);
    CHECK(!xq.canMove(9, 0, 3, 0), "车被对方阻挡(不能跳过吃)");
    CHECK(xq.canMove(9, 0, 5, 0), "车可吃对方子");

    // 3. 马：日字 + 蹩马腿
    clearBoard(xq);
    xq.setPiece(5, 5, PIECE_RED_HORSE);
    CHECK(xq.canMove(5, 5, 3, 4), "马跳日(上左)");
    CHECK(xq.canMove(5, 5, 7, 4), "马跳日(下左)");
    CHECK(xq.canMove(5, 5, 4, 7), "马跳日(右长)");
    CHECK(!xq.canMove(5, 5, 4, 5), "马不可走非日字");
    xq.setPiece(4, 5, PIECE_RED_PAWN);  // 蹩马腿(上方)
    CHECK(!xq.canMove(5, 5, 3, 4), "马腿被蹩不能跳");
    CHECK(!xq.canMove(5, 5, 3, 6), "马腿被蹩(同方向另一跳)");
    CHECK(xq.canMove(5, 5, 7, 4), "下方跳不受影响");

    // 4. 相：田字 + 塞象眼 + 不过河
    clearBoard(xq);
    xq.setPiece(5, 5, PIECE_RED_ELEPHANT);
    CHECK(xq.canMove(5, 5, 7, 3), "相飞田(下左)");
    CHECK(xq.canMove(5, 5, 7, 7), "相飞田(下右)");
    CHECK(!xq.canMove(5, 5, 3, 3), "红相不可过河");
    CHECK(!xq.canMove(5, 5, 7, 4), "相不可走非田字");
    xq.setPiece(6, 4, PIECE_RED_PAWN);  // 塞象眼(5,5->7,3 的象眼 6,4)
    CHECK(!xq.canMove(5, 5, 7, 3), "象眼被塞不能飞");

    // 5. 仕：九宫斜一步
    clearBoard(xq);
    xq.setPiece(8, 3, PIECE_RED_ADVISOR);
    CHECK(xq.canMove(8, 3, 9, 4), "仕斜一步");
    CHECK(xq.canMove(8, 3, 7, 4), "仕斜一步(另一向)");
    CHECK(!xq.canMove(8, 3, 8, 4), "仕不可直走");
    CHECK(!xq.canMove(8, 3, 9, 2), "仕不可出九宫");

    // 6. 炮：移动无子 / 吃子隔一个炮架
    clearBoard(xq);
    xq.setPiece(5, 0, PIECE_RED_CANNON);
    xq.setPiece(2, 0, PIECE_BLACK_PAWN);
    CHECK(xq.canMove(5, 0, 5, 4), "炮直移(无子)");
    CHECK(!xq.canMove(5, 0, 2, 0), "炮无炮架不能吃");
    xq.setPiece(4, 0, PIECE_RED_PAWN);  // 炮架
    CHECK(xq.canMove(5, 0, 2, 0), "炮隔一子吃(炮架)");
    xq.setPiece(3, 0, PIECE_RED_PAWN);  // 两个子
    CHECK(!xq.canMove(5, 0, 2, 0), "炮隔两子不能吃");
    xq.setPiece(4, 0, PIECE_NONE);
    xq.setPiece(3, 0, PIECE_NONE);
    CHECK(!xq.canMove(5, 0, 2, 0), "炮无架仍不能吃");

    // 7. 兵：过河前后
    clearBoard(xq);
    xq.setPiece(5, 0, PIECE_RED_PAWN);  // 未过河
    CHECK(xq.canMove(5, 0, 4, 0), "兵前进");
    CHECK(!xq.canMove(5, 0, 5, 1), "兵未过河不可横走");
    CHECK(!xq.canMove(5, 0, 6, 0), "兵不可后退");
    xq.setPiece(4, 0, PIECE_RED_PAWN);  // 过河(红兵 row<=4)
    CHECK(xq.canMove(4, 0, 4, 1), "过河兵可横走");
    CHECK(xq.canMove(4, 0, 3, 0), "过河兵可前进");
    CHECK(!xq.canMove(4, 0, 5, 0), "过河兵仍不可后退");
    CHECK(!xq.canMove(4, 0, 3, 1), "兵不可斜走");

    // 8. 帅：九宫直一步 + 对脸
    clearBoard(xq);
    xq.setPiece(9, 4, PIECE_RED_GENERAL);
    xq.setPiece(0, 4, PIECE_BLACK_GENERAL);
    CHECK(!xq.canMove(9, 4, 8, 4), "将帅对脸不可走");
    xq.setPiece(5, 4, PIECE_RED_PAWN);  // 中间挡子
    CHECK(xq.canMove(9, 4, 8, 4), "中间有子不算对脸");
    CHECK(xq.canMove(9, 4, 9, 3), "帅横移(九宫内)");
    CHECK(!xq.canMove(9, 4, 9, 2), "帅不可出九宫");
    CHECK(!xq.canMove(9, 4, 8, 3), "帅不可斜走");

    // 9. 回合交替 + 不能吃己方
    clearBoard(xq);
    xq.setPiece(9, 0, PIECE_RED_ROOK);
    xq.setPiece(0, 0, PIECE_BLACK_ROOK);
    CHECK(xq.doMove(9, 0, 9, 1), "红车走一步");
    CHECK(!xq.isRedTurn(), "轮到黑方");
    CHECK(!xq.canMove(9, 1, 9, 2), "黑方不能走红子");
    CHECK(xq.doMove(0, 0, 0, 1), "黑车走一步");
    CHECK(xq.isRedTurn(), "回到红方");
    xq.setPiece(9, 2, PIECE_RED_PAWN);
    CHECK(!xq.canMove(9, 1, 9, 2), "不能吃己方");

    // 10. 悔棋（含被吃子恢复）
    xq.init();  // 清空历史
    clearBoard(xq);
    xq.setPiece(9, 0, PIECE_RED_ROOK);
    xq.setPiece(0, 0, PIECE_BLACK_ROOK);
    xq.setPiece(5, 0, PIECE_BLACK_PAWN);
    CHECK(xq.canMove(9, 0, 5, 0), "红车吃黑卒(中间无子)");
    CHECK(xq.doMove(9, 0, 5, 0), "红车吃黑卒执行");
    CHECK(!xq.isRedTurn(), "吃子后轮到黑方");
    CHECK(xq.undoLast(), "悔棋(吃子)");
    CHECK(xq.pieceAt(9, 0) == PIECE_RED_ROOK && xq.pieceAt(5, 0) == PIECE_BLACK_PAWN,
          "悔棋恢复被吃子");
    CHECK(xq.isRedTurn(), "悔棋后回到红方");
    CHECK(xq.moveCount() == 0, "悔棋后 0 步");

    // 11. 吃将即胜
    clearBoard(xq);
    xq.setPiece(9, 0, PIECE_RED_ROOK);
    xq.setPiece(0, 0, PIECE_BLACK_GENERAL);
    CHECK(xq.canMove(9, 0, 0, 0), "车可直吃黑将");
    CHECK(xq.doMove(9, 0, 0, 0), "车吃将");
    CHECK(xq.generalCaptured(), "吃将后分出胜负");

    if (g_fail == 0)
    {
        std::printf("PASS: 象棋规则全部通过\n");
        return 0;
    }
    std::printf("FAIL: %d 项失败\n", g_fail);
    return 1;
}
