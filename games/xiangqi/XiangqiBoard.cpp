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
    // 翻转视角 = 从对面看：行、列同时镜像（旋转 180°），仅翻行会导致左右错位
    // （黑方视角右侧实际是模型左侧，红方看到的位置会左右相反）
    const int r = m_flipped ? (XiangqiChess::kRows - 1 - row) : row;
    const int c = m_flipped ? (XiangqiChess::kCols - 1 - col) : col;
    if (!m_boardBg.isNull())
    {
        const float s = static_cast<float>(kLogicSize) / kImgSize;
        return QPointF((kImgX0 + c * kImgCellX) * s, (kImgY0 + r * kImgCellY) * s);
    }
    return QPointF(kMargin + c * kCell, kMargin + r * kCell);
}

void XiangqiBoard::setFlipped(bool flipped)
{
    m_flipped = flipped;
    update();
}

void XiangqiBoard::setPieceSkin(const QString& skin)
{
    m_pieceSkin = skin;
    m_piecePixmaps.clear();  // 切皮肤失效缓存
    update();
}

QString XiangqiBoard::pieceImagePath(int piece)
{
    const bool red = XiangqiChess::pieceBelongs(piece, true);
    const char* name = nullptr;
    switch (qAbs(piece))
    {
    case PIECE_RED_GENERAL:  name = "j"; break;  // 将/帅
    case PIECE_RED_ADVISOR:  name = "s"; break;  // 士/仕
    case PIECE_RED_ELEPHANT: name = "x"; break;  // 相/象
    case PIECE_RED_HORSE:    name = "m"; break;  // 马
    case PIECE_RED_ROOK:     name = "c"; break;  // 车
    case PIECE_RED_CANNON:   name = "p"; break;  // 炮
    case PIECE_RED_PAWN:     name = "z"; break;  // 兵/卒
    default: return QString();
    }
    // m_pieceSkin 为皮肤目录名（stype_1/2/3）
    return QStringLiteral(":/res/pieces/%1/%2_%3.png")
        .arg(m_pieceSkin)
        .arg(red ? QStringLiteral("r") : QStringLiteral("b"))
        .arg(QLatin1String(name));
}

QPixmap XiangqiBoard::piecePixmap(int piece)
{
    auto it = m_piecePixmaps.constFind(piece);
    if (it != m_piecePixmaps.constEnd())
    {
        return it.value();
    }
    const QPixmap pm(pieceImagePath(piece));
    m_piecePixmaps.insert(piece, pm);  // 缺失资源会得到空图，缓存避免重复加载
    return pm;
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
        painter.save();
        if (m_flipped)
        {
            // 翻转视角：底图旋转 180°（水平+垂直镜像），与 cellPoint 行列翻转后的
            // 棋子位置对齐；只镜像单方向会导致棋子与底图网格错位
            painter.translate(kLogicSize, kLogicSize);
            painter.rotate(180);
        }
        painter.drawPixmap(0, 0, kLogicSize, kLogicSize, m_boardBg);
        painter.restore();
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

    // 图片棋子皮肤（54×54 原图缩放到棋子直径；对方棋子旋转 180° 保持文字物理朝向）
    if (!m_pieceSkin.isEmpty())
    {
        const QPixmap pm = piecePixmap(piece);
        if (!pm.isNull())
        {
            const int size = radius * 2;
            painter.save();
            painter.translate(p.x(), p.y());
            if (red == m_flipped)
            {
                painter.rotate(180);  // 对方棋子字朝对方，自己这侧看反（与经典文字一致）
            }
            painter.drawPixmap(-size / 2, -size / 2, size, size, pm);
            painter.restore();
            return;
        }
        // 皮肤资源缺失：回退经典文字绘制
    }

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
    painter.save();
    if (red == m_flipped)
    {
        // 物理正确的棋子文字方向：对方棋子字朝向对方那侧，自己这侧看是反的
        //   黑方（翻转）视角：红方棋子反、自己黑子正
        //   红方（正向）视角：黑方棋子反、自己红子正
        painter.translate(p.x(), p.y());
        painter.rotate(180);
        painter.translate(-p.x(), -p.y());
    }
    painter.drawText(QRectF(p.x() - radius, p.y() - radius, radius * 2, radius * 2),
                     Qt::AlignCenter, QString::fromUtf8(XiangqiChess::pieceName(piece)));
    painter.restore();
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
