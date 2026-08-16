#include "GomokuBoard.h"

#include <QMouseEvent>
#include <QPainter>

GomokuBoard::GomokuBoard(QWidget* parent)
    : QWidget(parent)
{
    // 可缩放：初始 600x600，允许拉大/拉小（等比）
    setMinimumSize(400, 400);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 资源来自 resource.qrc（原 EasyX 的 loadimage 改为 Qt 资源），默认翡翠绿棋盘
    setBackground(QStringLiteral(":/res/board_2_mint.png"));
    m_blackPiece = QPixmap(QStringLiteral(":/res/black.png"));
    m_whitePiece = QPixmap(QStringLiteral(":/res/white.png"));
}

void GomokuBoard::setBackground(const QString& imagePath)
{
    m_boardBg = QPixmap(imagePath);
    update();
}

float GomokuBoard::scaleFactor() const
{
    const float s = qMin(width(), height()) / static_cast<float>(kLogicSize);
    return qMax(0.1f, s);
}

QPointF GomokuBoard::boardOrigin() const
{
    // 棋盘按 scale 缩放后的实际像素边长，超出控件宽/高的部分均分到两侧 -> 居中
    const float boardPx = kLogicSize * scaleFactor();
    return QPointF((width() - boardPx) / 2.0f, (height() - boardPx) / 2.0f);
}

void GomokuBoard::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    const float scale = scaleFactor();
    const QPointF origin = boardOrigin();

    painter.save();
    painter.translate(origin);
    painter.scale(scale, scale);

    // 背景图（按逻辑尺寸 600x600 铺满，scale 变换下自动等比缩放）
    if (!m_boardBg.isNull())
    {
        painter.drawPixmap(0, 0, kLogicSize, kLogicSize, m_boardBg);
    }

    if (m_chess)
    {
        const int gradeSize = m_chess->getGradeSize();
        const int marginX = m_chess->getMarginX();
        const int marginY = m_chess->getMarginY();
        const float chessSize = m_chess->getChessSize();

        for (int row = 0; row < gradeSize; row++)
        {
            for (int col = 0; col < gradeSize; col++)
            {
                const int kind = m_chess->getChessData(row, col);
                if (kind == 0)
                {
                    continue;
                }

                // 与原版一致：棋子左上角 = 交点 - 半格（逻辑坐标，随 scale 缩放）
                const int x = marginX + chessSize * col - 0.5f * chessSize;
                const int y = marginY + chessSize * row - 0.5f * chessSize;
                const QPixmap& piece = (kind == CHESS_BLACK) ? m_blackPiece : m_whitePiece;
                painter.drawPixmap(x, y, static_cast<int>(chessSize), static_cast<int>(chessSize), piece);
            }
        }
    }

    painter.restore();

    // 最后一手红点标记（画在棋子上方中心，随 scale 缩放）
    if (m_chess && m_lastRow >= 0 && m_lastCol >= 0)
    {
        const int gradeSize = m_chess->getGradeSize();
        if (m_lastRow < gradeSize && m_lastCol < gradeSize)
        {
            painter.save();
            painter.translate(origin);
            painter.scale(scale, scale);
            painter.setBrush(QColor(230, 40, 40));
            painter.setPen(Qt::NoPen);
            const float x = m_chess->getMarginX() + m_chess->getChessSize() * m_lastCol;
            const float y = m_chess->getMarginY() + m_chess->getChessSize() * m_lastRow;
            painter.drawEllipse(QPointF(x, y), 3.5f, 3.5f);
            painter.restore();
        }
    }
}

void GomokuBoard::mousePressEvent(QMouseEvent* event)
{
    if (!m_chess || event->button() != Qt::LeftButton)
    {
        return;
    }

    // 屏幕坐标 -> 逻辑坐标（先减居中偏移，再除以缩放系数）
    const float scale = scaleFactor();
    const QPointF origin = boardOrigin();
    const int logicX = static_cast<int>((event->pos().x() - origin.x()) / scale);
    const int logicY = static_cast<int>((event->pos().y() - origin.y()) / scale);

    ChessPos pos;
    if (m_chess->clickBoard(logicX, logicY, &pos))
    {
        emit cellClicked(pos.row, pos.col);
    }
}
