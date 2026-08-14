#pragma once

#include <QPixmap>
#include <QWidget>

#include "XiangqiChess.h"

// 象棋棋盘：9×10 网格绘制（黄木底 + 楚河汉界 + 九宫），棋子 QPainter 圆形 + 汉字
// 棋盘底图预留（用户图片后补，setBackground 同五子棋换肤机制）
class XiangqiBoard : public QWidget
{
    Q_OBJECT

public:
    explicit XiangqiBoard(QWidget* parent = nullptr);

    void setChess(XiangqiChess* chess) { m_chess = chess; }
    void repaintBoard() { update(); }

    void setSelected(int row, int col);        // 选中棋子高亮（-1,-1 清除）
    void clearSelected() { setSelected(-1, -1); }
    void setLastMove(int row, int col);        // 最后一手目标格标记
    void clearLastMove() { m_lastRow = m_lastCol = -1; update(); }

    // 棋盘底图（用户提供图片后调用；未设置时用 QPainter 按风格绘制底色）
    void setBackground(const QString& imagePath);
    // 程序绘制底色风格：0 古典宣纸 / 1 青玉翡翠 / 2 柔和粉淡 / 3 沉香红木（图片未设置时生效）
    void setBoardStyle(int style);

    // 逻辑棋盘尺寸（600×600：格距 60，边距 30）
    static constexpr int kLogicSize = 600;
    static constexpr int kMargin = 30;
    static constexpr int kCell = 60;

signals:
    void cellClicked(int row, int col);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void drawBoard(QPainter& painter);   // 网格/九宫/楚河汉界
    void drawPiece(QPainter& painter, int row, int col, int piece);

    XiangqiChess* m_chess = nullptr;
    QPixmap m_boardBg;   // 棋盘底图（非空时优先于程序绘制）
    int m_style = 0;     // 程序绘制底色风格（0-3）
    int m_selRow = -1;
    int m_selCol = -1;
    int m_lastRow = -1;
    int m_lastCol = -1;
};
