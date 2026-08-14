#include "Chess.h"

#include <cmath>

Chess::Chess(int gradeSize, int marginX, int marginY, float chessSize)
{
    this->gradeSize = gradeSize;
    this->margin_x = marginX;
    this->margin_y = marginY;
    this->chessSize = chessSize;
    playerFlag = CHESS_BLACK;

    for (int i = 0; i < gradeSize; i++)
    {
        std::vector<int> row;
        for (int j = 0; j < gradeSize; j++)
        {
            row.push_back(0);
        }
        chessMap.push_back(row);
    }
}

void Chess::init()
{
    for (int i = 0; i < gradeSize; i++)
    {
        for (int j = 0; j < gradeSize; j++)
        {
            chessMap[i][j] = 0;
        }
    }
    moveHistory.clear();
    playerFlag = true; // 黑方先行
}

bool Chess::clickBoard(int x, int y, ChessPos* pos)
{
    int col = (x - margin_x) / chessSize;
    int row = (y - margin_y) / chessSize;

    // 越界守卫（原代码缺，极端点击会越界访问 chessMap）
    if (row < 0 || row >= gradeSize || col < 0 || col >= gradeSize)
    {
        return false;
    }

    int leftTopPosX = margin_x + chessSize * col;
    int leftTopPosY = margin_y + chessSize * row;
    int offset = chessSize * 0.4;

    int len;
    bool ret = false;

    do {
        len = sqrt((x - leftTopPosX) * (x - leftTopPosX) + (y - leftTopPosY) * (y - leftTopPosY));
        if (len < offset)
        {
            pos->row = row;
            pos->col = col;
            if (chessMap[pos->row][pos->col] == 0)
            {
                ret = true;
            }
            break;
        }
        int x2 = leftTopPosX + chessSize;
        int y2 = leftTopPosY;
        len = sqrt((x - x2) * (x - x2) + (y - y2) * (y - y2));
        if (len < offset)
        {
            pos->row = row;
            pos->col = col + 1;
            if (chessMap[pos->row][pos->col] == 0)
            {
                ret = true;
            }
            break;
        }
        x2 = leftTopPosX;
        y2 = leftTopPosY + chessSize;
        len = sqrt((x - x2) * (x - x2) + (y - y2) * (y - y2));
        if (len < offset)
        {
            pos->row = row + 1;
            pos->col = col;
            if (chessMap[pos->row][pos->col] == 0)
            {
                ret = true;
            }
            break;
        }
        x2 = leftTopPosX + chessSize;
        y2 = leftTopPosY + chessSize;
        len = sqrt((x - x2) * (x - x2) + (y - y2) * (y - y2));
        if (len < offset)
        {
            pos->row = row + 1;
            pos->col = col + 1;
            if (chessMap[pos->row][pos->col] == 0)
            {
                ret = true;
            }
            break;
        }
    } while (0);
    return ret;
}

void Chess::chessDown(ChessPos* pos, chess_kind_t kind)
{
    lastPos = *pos;
    chessMap[pos->row][pos->col] = kind;
    // 黑方落子后轮到白方；白方落子后轮到黑方
    playerFlag = (kind == CHESS_WHITE);
    moveHistory.push_back({pos->row, pos->col, kind});
}

bool Chess::undoLast()
{
    if (moveHistory.empty())
    {
        return false;
    }
    const MoveRec m = moveHistory.back();
    moveHistory.pop_back();
    chessMap[m.row][m.col] = 0;
    // lastPos 回到再前一手（空盘时置无效，checkWin 依赖 lastPos，悔棋后不会立即判胜）
    lastPos = moveHistory.empty() ? ChessPos(-1, -1)
                                  : ChessPos(moveHistory.back().row, moveHistory.back().col);
    // 回合恢复：撤回黑子 → 轮黑；撤回白子 → 轮白
    playerFlag = (m.kind == CHESS_BLACK);
    return true;
}

int Chess::getChessData(int row, int col) const
{
    return chessMap[row][col];
}

bool Chess::checkOver()
{
    return checkWin();
}

bool Chess::checkWin()
{
    int row = lastPos.row;
    int col = lastPos.col;

    for (int i = 0; i < 5; i++)
    {
        if (col - i >= 0 &&
            col - i + 4 < gradeSize &&
            chessMap[row][col - i] == chessMap[row][col - i + 1] &&
            chessMap[row][col - i] == chessMap[row][col - i + 2] &&
            chessMap[row][col - i] == chessMap[row][col - i + 3] &&
            chessMap[row][col - i] == chessMap[row][col - i + 4])
        {
            return true;
        }
    }
    for (int i = 0; i < 5; i++)
    {
        if (
            row - i >= 0 && row - i + 4 < gradeSize &&
            chessMap[row - i][col] == chessMap[row - i + 1][col] &&
            chessMap[row - i][col] == chessMap[row - i + 2][col] &&
            chessMap[row - i][col] == chessMap[row - i + 3][col] &&
            chessMap[row - i][col] == chessMap[row - i + 4][col])
        {
            return true;
        }
    }
    for (int i = 0; i < 5; i++)
    {
        if (
            row + i < gradeSize && row + i - 4 >= 0 &&
            col - i >= 0 && col - i + 4 < gradeSize &&
            chessMap[row + i][col - i] == chessMap[row + i - 1][col - i + 1] &&
            chessMap[row + i][col - i] == chessMap[row + i - 2][col - i + 2] &&
            chessMap[row + i][col - i] == chessMap[row + i - 3][col - i + 3] &&
            chessMap[row + i][col - i] == chessMap[row + i - 4][col - i + 4])
        {
            return true;
        }
    }
    for (int i = 0; i < 5; i++)
    {
        if (
            row - i >= 0 && row - i + 4 < gradeSize &&
            col - i >= 0 && col - i + 4 < gradeSize &&
            chessMap[row - i][col - i] == chessMap[row - i + 1][col - i + 1] &&
            chessMap[row - i][col - i] == chessMap[row - i + 2][col - i + 2] &&
            chessMap[row - i][col - i] == chessMap[row - i + 3][col - i + 3] &&
            chessMap[row - i][col - i] == chessMap[row - i + 4][col - i + 4])
        {
            return true;
        }
    }

    return false;
}
