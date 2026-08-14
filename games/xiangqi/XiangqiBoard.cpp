#include "XiangqiBoard.h"

#include <QMouseEvent>
#include <QPainter>

namespace {

// 交叉点坐标（逻辑 600×600）
inline QPoint pt(int row, int col)
{
    return QPoint(XiangqiBoard::kMargin + col * XiangqiBoard::kCell,
                  XiangqiBoard::kMargin + row * XiangqiBoard::kCell);
}

} // namespace

XiangqiBoard::XiangqiBoard(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(400, 400);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
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

    // 底：图片（预留）或按风格程序绘制
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
    }

    drawBoard(painter);

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
        const QPoint p = pt(m_selRow, m_selCol);
        painter.setPen(QPen(QColor(230, 40, 40), 3));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(p, 26, 26);
    }
    // 最后一手标记
    if (m_lastRow >= 0 && m_lastCol >= 0)
    {
        const QPoint p = pt(m_lastRow, m_lastCol);
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
        painter.drawLine(pt(r, 0), pt(r, XiangqiChess::kCols - 1));
    }
    // 竖线 9 条（中间行断开为楚河汉界）
    for (int c = 0; c < XiangqiChess::kCols; c++)
    {
        painter.drawLine(pt(0, c), pt(4, c));
        painter.drawLine(pt(5, c), pt(XiangqiChess::kRows - 1, c));
    }
    // 九宫斜线（红：7-9 行；黑：0-2 行）
    painter.drawLine(pt(7, 3), pt(9, 5));
    painter.drawLine(pt(7, 5), pt(9, 3));
    painter.drawLine(pt(0, 3), pt(2, 5));
    painter.drawLine(pt(0, 5), pt(2, 3));

    // 楚河汉界
    painter.setRenderHint(QPainter::Antialiasing, true);
    QFont f = painter.font();
    f.setPixelSize(34);
    f.setBold(true);
    painter.setFont(f);
    painter.setPen(text);
    painter.drawText(QRect(pt(0, 0).x(), pt(4, 0).y(), kCell * 4, kCell),
                     Qt::AlignCenter, QStringLiteral("楚　河"));
    painter.drawText(QRect(pt(0, 5).x(), pt(5, 0).y() + 0, kCell * 4, kCell),
                     Qt::AlignCenter, QStringLiteral("汉　界"));
}

void XiangqiBoard::drawPiece(QPainter& painter, int row, int col, int piece)
{
    const QPoint p = pt(row, col);
    const bool red = XiangqiChess::pieceBelongs(piece, true);

    painter.setRenderHint(QPainter::Antialiasing, true);
    // 圆底
    painter.setPen(QPen(red ? QColor(150, 40, 30) : QColor(60, 60, 60), 2));
    painter.setBrush(red ? QColor(248, 220, 200) : QColor(230, 230, 225));
    painter.drawEllipse(p, 25, 25);

    // 汉字
    QFont f = painter.font();
    f.setPixelSize(26);
    f.setBold(true);
    painter.setFont(f);
    painter.setPen(red ? QColor(170, 30, 20) : QColor(40, 40, 40));
    painter.drawText(QRect(p.x() - 25, p.y() - 25, 50, 50), Qt::AlignCenter,
                     QString::fromUtf8(XiangqiChess::pieceName(piece)));
}

void XiangqiBoard::mousePressEvent(QMouseEvent* event)
{
    if (!m_chess || event->button() != Qt::LeftButton)
    {
        return;
    }
    const float scale = qMin(width(), height()) / static_cast<float>(kLogicSize);
    const int logicX = static_cast<int>(event->pos().x() / scale);
    const int logicY = static_cast<int>(event->pos().y() / scale);
    const int col = (logicX - kMargin + kCell / 2) / kCell;
    const int row = (logicY - kMargin + kCell / 2) / kCell;
    if (row >= 0 && row < XiangqiChess::kRows && col >= 0 && col < XiangqiChess::kCols)
    {
        emit cellClicked(row, col);
    }
}
