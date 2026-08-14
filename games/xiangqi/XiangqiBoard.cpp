#include "XiangqiBoard.h"

#include <QMouseEvent>
#include <QPainter>

XiangqiBoard::XiangqiBoard(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(400, 400);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

// 交叉点坐标（逻辑 600 系）：图片模式按图片网格校准，程序模式用固定边距/格距
QPointF XiangqiBoard::cellPoint(int row, int col) const
{
    const int r = m_flipped ? (XiangqiChess::kRows - 1 - row) : row;
    if (!m_boardBg.isNull())
    {
        const float s = static_cast<float>(kLogicSize) / kImgSize;
        return QPointF((kImgX0 + col * kImgCellX) * s, (kImgY0 + r * kImgCellY) * s);
    }
    return QPointF(kMargin + col * kCell, kMargin + r * kCell);
}

void XiangqiBoard::setFlipped(bool flipped)
{
    m_flipped = flipped;
    update();
}

void XiangqiBoard::setSelected(int row, int col)
{
    m_selRow = row;
    m_selCol = col;
    update();
}

void XiangqiBoard::setLastMove(int row, int col)
{
    m_lastRow = row;
    m_lastCol = col;
    update();
}

void XiangqiBoard::setBackground(const QString& imagePath)
{
    m_boardBg = QPixmap(imagePath);
    update();
}

void XiangqiBoard::setBoardStyle(int style)
{
    m_boardBg = QPixmap();  // 切回程序绘制底色（清除图片底图）
    m_style = style;
    update();
}

void XiangqiBoard::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    const float scale = qMin(width(), height()) / static_cast<float>(kLogicSize);
    painter.scale(scale, scale);

    // 底：图片（用户棋盘图，自带网格）或按风格程序绘制
    if (!m_boardBg.isNull())
    {
        painter.drawPixmap(0, 0, kLogicSize, kLogicSize, m_boardBg);
    }
    else
    {
        // 0 古典宣纸 / 1 青玉翡翠 / 2 柔和粉淡 / 3 沉香红木
        const QColor bg = m_style == 1 ? QColor(203, 227, 217)
                        : m_style == 2 ? QColor(244, 222, 228)
                        : m_style == 3 ? QColor(158, 106, 82)
                                       : QColor(243, 234, 214);
        painter.fillRect(0, 0, kLogicSize, kLogicSize, bg);
        drawBoard(painter);  // 程序绘制模式才画网格（图片模式用图片自带网格）
    }

    if (!m_chess)
    {
        return;
    }
    // 棋子
    for (int r = 0; r < XiangqiChess::kRows; r++)
    {
        for (int c = 0; c < XiangqiChess::kCols; c++)
        {
            const int piece = m_chess->pieceAt(r, c);
            if (piece != PIECE_NONE)
            {
                drawPiece(painter, r, c, piece);
            }
        }
    }

    // 选中高亮（最上层）
    if (m_selRow >= 0 && m_selCol >= 0)
    {
        const QPointF p = cellPoint(m_selRow, m_selCol);
        painter.setPen(QPen(QColor(230, 40, 40), 3));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(p, 26, 26);
    }
    // 最后一手标记
    if (m_lastRow >= 0 && m_lastCol >= 0)
    {
        const QPointF p = cellPoint(m_lastRow, m_lastCol);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(230, 40, 40));
        painter.drawEllipse(p, 4, 4);
    }
}

void XiangqiBoard::drawBoard(QPainter& painter)
{
    painter.setRenderHint(QPainter::Antialiasing, false);
    // 深色底（红木）用浅色线/文字，浅色底用深色
    const bool dark = (m_style == 3) && m_boardBg.isNull();
    const QColor line = dark ? QColor(235, 205, 178) : QColor(90, 60, 30);
    const QColor text = dark ? QColor(235, 205, 178) : QColor(120, 80, 40);
    painter.setPen(QPen(line, 1.6));

    // 横线 10 条
    for (int r = 0; r < XiangqiChess::kRows; r++)
    {
        painter.drawLine(cellPoint(r, 0), cellPoint(r, XiangqiChess::kCols - 1));
    }
    // 竖线 9 条（中间行断开为楚河汉界）
    for (int c = 0; c < XiangqiChess::kCols; c++)
    {
        painter.drawLine(cellPoint(0, c), cellPoint(4, c));
        painter.drawLine(cellPoint(5, c), cellPoint(XiangqiChess::kRows - 1, c));
    }
    // 九宫斜线（红：7-9 行；黑：0-2 行）
    painter.drawLine(cellPoint(7, 3), cellPoint(9, 5));
    painter.drawLine(cellPoint(7, 5), cellPoint(9, 3));
    painter.drawLine(cellPoint(0, 3), cellPoint(2, 5));
    painter.drawLine(cellPoint(0, 5), cellPoint(2, 3));

    // 楚河汉界
    painter.setRenderHint(QPainter::Antialiasing, true);
    QFont f = painter.font();
    f.setPixelSize(34);
    f.setBold(true);
    painter.setFont(f);
    painter.setPen(text);
    const QPointF p0 = cellPoint(0, 0);
    const QPointF p4 = cellPoint(4, 0);
    const QPointF p5 = cellPoint(5, 0);
    painter.drawText(QRectF(p0.x(), p4.y(), kCell * 4, kCell),
                     Qt::AlignCenter, QStringLiteral("楚　河"));
    painter.drawText(QRectF(p0.x(), p5.y(), kCell * 4, kCell),
                     Qt::AlignCenter, QStringLiteral("汉　界"));
}

void XiangqiBoard::drawPiece(QPainter& painter, int row, int col, int piece)
{
    const QPointF p = cellPoint(row, col);
    const bool red = XiangqiChess::pieceBelongs(piece, true);
    // 半径按模式自适应：图片底图行距 50.4（逻辑）< 程序模式 60，防棋子压相邻行
    const int radius = m_boardBg.isNull() ? 25 : 22;

    painter.setRenderHint(QPainter::Antialiasing, true);
    // 圆底
    painter.setPen(QPen(red ? QColor(150, 40, 30) : QColor(60, 60, 60), 2));
    painter.setBrush(red ? QColor(248, 220, 200) : QColor(230, 230, 225));
    painter.drawEllipse(p, radius, radius);

    // 汉字
    QFont f = painter.font();
    f.setPixelSize(radius * 2 - 24);
    f.setBold(true);
    painter.setFont(f);
    painter.setPen(red ? QColor(170, 30, 20) : QColor(40, 40, 40));
    painter.drawText(QRectF(p.x() - radius, p.y() - radius, radius * 2, radius * 2),
                     Qt::AlignCenter, QString::fromUtf8(XiangqiChess::pieceName(piece)));
}

void XiangqiBoard::mousePressEvent(QMouseEvent* event)
{
    if (!m_chess || event->button() != Qt::LeftButton)
    {
        return;
    }
    const float scale = qMin(width(), height()) / static_cast<float>(kLogicSize);
    const QPointF logic(event->pos().x() / scale, event->pos().y() / scale);

    // 找最近的交叉点（图片网格按图校准，可能非均匀）
    int bestRow = -1, bestCol = -1;
    float bestDist = 1e9f;
    for (int r = 0; r < XiangqiChess::kRows; r++)
    {
        for (int c = 0; c < XiangqiChess::kCols; c++)
        {
            const QPointF p = cellPoint(r, c);
            const float dx = p.x() - logic.x();
            const float dy = p.y() - logic.y();
            const float d = dx * dx + dy * dy;
            if (d < bestDist)
            {
                bestDist = d;
                bestRow = r;
                bestCol = c;
            }
        }
    }
    if (bestRow >= 0 && bestDist < 30 * 30)
    {
        // bestRow/bestCol 已是模型坐标（cellPoint 内部已完成视角翻转），直接上报
        emit cellClicked(bestRow, bestCol);
    }
}
