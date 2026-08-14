#include "XiangqiChess.h"

#include <cstdlib>  // abs()（Linux gcc 需要显式引入）

namespace {

// 初始布局（row 0 = 黑方底线 → row 9 = 红方底线）
// clang-format off
const int kInitialBoard[10][9] = {
    {-5, -4, -3, -2, -1, -2, -3, -4, -5},  // 0 黑底线：車馬象士將士象馬車
    { 0,  0,  0,  0,  0,  0,  0,  0,  0},  // 1 空行
    { 0, -6,  0,  0,  0,  0,  0, -6,  0},  // 2 黑炮
    {-7,  0, -7,  0, -7,  0, -7,  0, -7},  // 3 黑卒
    { 0,  0,  0,  0,  0,  0,  0,  0,  0},  // 4 空
    { 0,  0,  0,  0,  0,  0,  0,  0,  0},  // 5 楚河汉界
    { 7,  0,  7,  0,  7,  0,  7,  0,  7},  // 6 红兵
    { 0,  6,  0,  0,  0,  0,  0,  6,  0},  // 7 红炮
    { 0,  0,  0,  0,  0,  0,  0,  0,  0},  // 8 空行
    { 5,  4,  3,  2,  1,  2,  3,  4,  5},  // 9 红底线：車馬相仕帥仕相馬車
};
// clang-format on

} // namespace

XiangqiChess::XiangqiChess()
{
    init();
}

void XiangqiChess::init()
{
    board_.assign(kRows, std::vector<int>(kCols, 0));
    for (int r = 0; r < kRows; r++)
    {
        for (int c = 0; c < kCols; c++)
        {
            board_[r][c] = kInitialBoard[r][c];
        }
    }
    redTurn = true;
    history_.clear();
}

int XiangqiChess::pieceAt(int row, int col) const
{
    return board_[row][col];
}

int XiangqiChess::generalOf(bool red) const
{
    const int target = red ? PIECE_RED_GENERAL : PIECE_BLACK_GENERAL;
    for (int r = 0; r < kRows; r++)
    {
        for (int c = 0; c < kCols; c++)
        {
            if (board_[r][c] == target)
            {
                return r;
            }
        }
    }
    return -1;
}

bool XiangqiChess::inBoard(int r, int c)
{
    return r >= 0 && r < kRows && c >= 0 && c < kCols;
}

bool XiangqiChess::inPalace(int r, int c, bool red)
{
    // 九宫：红方 rows 7-9、黑方 rows 0-2，cols 3-5
    if (c < 3 || c > 5)
    {
        return false;
    }
    return red ? (r >= 7 && r <= 9) : (r >= 0 && r <= 2);
}

bool XiangqiChess::pieceBelongs(int piece, bool red)
{
    return red ? (piece > 0) : (piece < 0);
}

bool XiangqiChess::sameSide(int a, int b)
{
    return (a > 0 && b > 0) || (a < 0 && b < 0);
}

const char* XiangqiChess::pieceName(int piece)
{
    switch (piece)
    {
    case PIECE_RED_GENERAL: return "帥";
    case PIECE_RED_ADVISOR: return "仕";
    case PIECE_RED_ELEPHANT: return "相";
    case PIECE_RED_HORSE: return "馬";
    case PIECE_RED_ROOK: return "車";
    case PIECE_RED_CANNON: return "炮";
    case PIECE_RED_PAWN: return "兵";
    case PIECE_BLACK_GENERAL: return "將";
    case PIECE_BLACK_ADVISOR: return "士";
    case PIECE_BLACK_ELEPHANT: return "象";
    case PIECE_BLACK_HORSE: return "馬";
    case PIECE_BLACK_ROOK: return "車";
    case PIECE_BLACK_CANNON: return "砲";
    case PIECE_BLACK_PAWN: return "卒";
    default: return "";
    }
}

bool XiangqiChess::canSelect(int row, int col) const
{
    if (!inBoard(row, col))
    {
        return false;
    }
    return pieceBelongs(board_[row][col], redTurn);
}

bool XiangqiChess::canMove(int fr, int fc, int tr, int tc) const
{
    if (!inBoard(fr, fc) || !inBoard(tr, tc))
    {
        return false;
    }
    if (fr == tr && fc == tc)
    {
        return false; // 原地不动
    }
    const int piece = board_[fr][fc];
    if (piece == PIECE_NONE || !pieceBelongs(piece, redTurn))
    {
        return false; // 无子或不是己方
    }
    const int target = board_[tr][tc];
    if (sameSide(piece, target))
    {
        return false; // 不能吃己方
    }

    bool legal = false;
    switch (piece)
    {
    case PIECE_RED_ROOK: case PIECE_BLACK_ROOK:
        legal = canMoveRook(fr, fc, tr, tc); break;
    case PIECE_RED_HORSE: case PIECE_BLACK_HORSE:
        legal = canMoveHorse(fr, fc, tr, tc); break;
    case PIECE_RED_ELEPHANT: case PIECE_BLACK_ELEPHANT:
        legal = canMoveElephant(fr, fc, tr, tc); break;
    case PIECE_RED_ADVISOR: case PIECE_BLACK_ADVISOR:
        legal = canMoveAdvisor(fr, fc, tr, tc); break;
    case PIECE_RED_GENERAL: case PIECE_BLACK_GENERAL:
        legal = canMoveGeneral(fr, fc, tr, tc); break;
    case PIECE_RED_CANNON: case PIECE_BLACK_CANNON:
        legal = canMoveCannon(fr, fc, tr, tc); break;
    case PIECE_RED_PAWN: case PIECE_BLACK_PAWN:
        legal = canMovePawn(fr, fc, tr, tc); break;
    default:
        break;
    }
    if (!legal)
    {
        return false;
    }

    // 走子后不能将帅对脸（吃帅/将除外——吃将直接结束）
    if (target == PIECE_RED_GENERAL || target == PIECE_BLACK_GENERAL)
    {
        return true;
    }
    return !generalsFacing(fr, fc, tr, tc);
}

bool XiangqiChess::doMove(int fr, int fc, int tr, int tc)
{
    if (!canMove(fr, fc, tr, tc))
    {
        return false;
    }
    MoveRec rec;
    rec.fromRow = fr; rec.fromCol = fc;
    rec.toRow = tr; rec.toCol = tc;
    rec.captured = board_[tr][tc];
    rec.redMoved = redTurn;

    board_[tr][tc] = board_[fr][fc];
    board_[fr][fc] = PIECE_NONE;
    redTurn = !redTurn;
    history_.push_back(rec);
    return true;
}

bool XiangqiChess::undoLast()
{
    if (history_.empty())
    {
        return false;
    }
    const MoveRec rec = history_.back();
    history_.pop_back();
    board_[rec.fromRow][rec.fromCol] = board_[rec.toRow][rec.toCol];
    board_[rec.toRow][rec.toCol] = rec.captured;
    redTurn = rec.redMoved;
    return true;
}

bool XiangqiChess::generalCaptured() const
{
    return generalOf(true) < 0 || generalOf(false) < 0;
}

void XiangqiChess::setPiece(int row, int col, int piece)
{
    board_[row][col] = piece;
}

int XiangqiChess::countBetween(int r1, int c1, int r2, int c2) const
{
    int n = 0;
    if (r1 == r2) // 横向
    {
        const int step = (c2 > c1) ? 1 : -1;
        for (int c = c1 + step; c != c2; c += step)
        {
            if (board_[r1][c] != PIECE_NONE)
            {
                n++;
            }
        }
    }
    else if (c1 == c2) // 纵向
    {
        const int step = (r2 > r1) ? 1 : -1;
        for (int r = r1 + step; r != r2; r += step)
        {
            if (board_[r][c1] != PIECE_NONE)
            {
                n++;
            }
        }
    }
    return n;
}

bool XiangqiChess::canMoveRook(int fr, int fc, int tr, int tc) const
{
    if (fr != tr && fc != tc)
    {
        return false; // 必须直线
    }
    return countBetween(fr, fc, tr, tc) == 0;
}

bool XiangqiChess::canMoveHorse(int fr, int fc, int tr, int tc) const
{
    const int dr = tr - fr;
    const int dc = tc - fc;
    if (!((abs(dr) == 2 && abs(dc) == 1) || (abs(dr) == 1 && abs(dc) == 2)))
    {
        return false; // 日字
    }
    // 蹩马腿：沿长边方向一步的格不能有子
    if (abs(dr) == 2)
    {
        const int legRow = fr + dr / 2;
        if (board_[legRow][fc] != PIECE_NONE)
        {
            return false;
        }
    }
    else
    {
        const int legCol = fc + dc / 2;
        if (board_[fr][legCol] != PIECE_NONE)
        {
            return false;
        }
    }
    return true;
}

bool XiangqiChess::canMoveElephant(int fr, int fc, int tr, int tc) const
{
    const int dr = tr - fr;
    const int dc = tc - fc;
    if (abs(dr) != 2 || abs(dc) != 2)
    {
        return false; // 田字
    }
    // 塞象眼：对角中间格
    if (board_[fr + dr / 2][fc + dc / 2] != PIECE_NONE)
    {
        return false;
    }
    // 不过河：红相行 5-9，黑象行 0-4
    if (pieceBelongs(board_[fr][fc], true) && tr < 5)
    {
        return false;
    }
    if (pieceBelongs(board_[fr][fc], false) && tr > 4)
    {
        return false;
    }
    return true;
}

bool XiangqiChess::canMoveAdvisor(int fr, int fc, int tr, int tc) const
{
    if (abs(tr - fr) != 1 || abs(tc - fc) != 1)
    {
        return false; // 斜一步
    }
    return inPalace(tr, tc, pieceBelongs(board_[fr][fc], true));
}

bool XiangqiChess::canMoveGeneral(int fr, int fc, int tr, int tc) const
{
    if (abs(tr - fr) + abs(tc - fc) != 1)
    {
        return false; // 直一步
    }
    return inPalace(tr, tc, pieceBelongs(board_[fr][fc], true));
}

bool XiangqiChess::canMoveCannon(int fr, int fc, int tr, int tc) const
{
    if (fr != tr && fc != tc)
    {
        return false; // 必须直线
    }
    const int between = countBetween(fr, fc, tr, tc);
    const int target = board_[tr][tc];
    if (target == PIECE_NONE)
    {
        return between == 0; // 移动：中间无子
    }
    return between == 1; // 吃子：恰好隔一个
}

bool XiangqiChess::canMovePawn(int fr, int fc, int tr, int tc) const
{
    const bool red = pieceBelongs(board_[fr][fc], true);
    const int dr = tr - fr;
    const int dc = tc - fc;
    const int forward = red ? -1 : 1;  // 红向上（row 减），黑向下（row 加）
    if (dc == 0 && dr == forward)
    {
        return true; // 前进一步
    }
    // 过河后可横走（红兵 row<=4，黑卒 row>=5）
    const bool crossed = red ? (fr <= 4) : (fr >= 5);
    if (crossed && dr == 0 && abs(dc) == 1)
    {
        return true;
    }
    return false;
}

bool XiangqiChess::generalsFacing(int fr, int fc, int tr, int tc) const
{
    // 临时模拟走子（不记录历史），检查将帅是否同列且中间无子
    auto tmp = board_;
    tmp[tr][tc] = tmp[fr][fc];
    tmp[fr][fc] = PIECE_NONE;

    int rRed = -1, rBlack = -1, cRed = -1, cBlack = -1;
    for (int r = 0; r < kRows; r++)
    {
        for (int c = 0; c < kCols; c++)
        {
            if (tmp[r][c] == PIECE_RED_GENERAL)
            {
                rRed = r;
                cRed = c;
            }
            else if (tmp[r][c] == PIECE_BLACK_GENERAL)
            {
                rBlack = r;
                cBlack = c;
            }
        }
    }
    if (rRed < 0 || rBlack < 0 || cRed != cBlack)
    {
        return false; // 一方被吃（吃将场景已前置处理）或不同列
    }
    const int lo = (rRed < rBlack) ? rRed : rBlack;
    const int hi = (rRed > rBlack) ? rRed : rBlack;
    for (int r = lo + 1; r < hi; r++)
    {
        if (tmp[r][cRed] != PIECE_NONE)
        {
            return false; // 中间有子相隔，不算对脸
        }
    }
    return true; // 将帅同列且中间无子 = 对脸
}
