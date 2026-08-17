#pragma once

#include <QMap>
#include <QPixmap>
#include <QWidget>

#include "XiangqiChess.h"

class QTimer;

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
    void setFlipped(bool flipped);             // 视角翻转：自己执黑时上下翻转（自己棋子在下）
    // 走子动画：棋子从 (fr,fc) 平滑移到 (tr,tc)（piece 为该子棋值），消除瞬移生硬感
    // captured: 目标格被吃的对方棋子（无吃子传 PIECE_NONE），动画期间显示在目标格待被覆盖
    void startMoveAnimation(int fr, int fc, int tr, int tc, int piece, int captured);
    void stopAnimation();                // 停止走子动画（悔棋/清盘时调用）

    // 棋盘底图（用户提供图片后调用；未设置时用 QPainter 按风格绘制底色）
    void setBackground(const QString& imagePath);
    // 程序绘制底色风格：0 古典宣纸 / 1 青玉翡翠 / 2 柔和粉淡 / 3 沉香红木（图片未设置时生效）
    void setBoardStyle(int style);

    // 棋子皮肤："" = 经典（圆形 + 汉字程序绘制）；"stype_1/2/3" = 对应图片套（res/pieces/）
    void setPieceSkin(const QString& skin);

    // 逻辑棋盘尺寸（600×600：格距 60，边距 30）
    static constexpr int kLogicSize = 600;
    static constexpr int kMargin = 30;
    static constexpr int kCell = 60;

    // 图片底图（512×512 用户棋盘图）的网格交叉点像素坐标（按图校准，4 款通用）
    // 竖线 x = kImgX0 + col*kImgCellX；横线 y = kImgY0 + row*kImgCellY
    static constexpr int kImgX0 = 68;
    static constexpr int kImgCellX = 47;
    static constexpr int kImgY0 = 64;
    static constexpr int kImgCellY = 43;
    static constexpr int kImgSize = 512;

signals:
    void cellClicked(int row, int col);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QPointF cellPoint(int row, int col) const;  // 交叉点坐标（逻辑 600 系）
    QPointF boardOrigin() const;        // 棋盘左上角偏移（居中：宽/高超出棋盘的部分均分）
    void drawBoard(QPainter& painter);   // 程序绘制模式的网格/九宫/楚河汉界（图片模式不画）
    void drawPiece(QPainter& painter, int row, int col, int piece);
    void drawPieceAt(QPainter& painter, const QPointF& pos, int piece);  // 在指定逻辑坐标画棋子（动画用）
    QPixmap piecePixmap(int piece);      // 图片皮肤：取棋子图（按枚举缓存）
    QString pieceImagePath(int piece);   // 图片皮肤：棋子枚举 -> 资源路径（按当前皮肤目录）
    QPixmap m_selBoxRed;                 // 红方选中框（r_box: 70×70, 内容 53px 红环）
    QPixmap m_selBoxBlack;               // 黑方选中框（b_box: 38×38, 满铺黑环）

    XiangqiChess* m_chess = nullptr;
    QPixmap m_boardBg;   // 棋盘底图（非空时优先于程序绘制）
    int m_style = 0;     // 程序绘制底色风格（0-3）
    bool m_flipped = false;  // 视角翻转（黑方视角：上下颠倒）
    QString m_pieceSkin;      // 棋子皮肤：空 = 经典文字，非空 = 图片皮肤名
    QMap<int, QPixmap> m_piecePixmaps;  // 图片皮肤缓存（棋子枚举 -> 图）
    int m_selRow = -1;
    int m_selCol = -1;
    int m_lastRow = -1;
    int m_lastCol = -1;
    // 走子动画状态
    int m_animPiece = PIECE_NONE;  // 动画中的棋子（PIECE_NONE = 无动画）
    int m_animToRow = -1;          // 动画目标格（模型坐标，动画期间隐藏该格棋子）
    int m_animToCol = -1;
    int m_animCaptured = PIECE_NONE;  // 目标格被吃的对方棋子（动画期间显示，飞行棋子覆盖）
    QPointF m_animFrom;            // 起点（逻辑坐标）
    QPointF m_animTo;              // 终点（逻辑坐标）
    qint64 m_animStartMs = 0;      // 动画开始时间戳（QElapsedTimer 基准）
    static constexpr int kAnimDurationMs = 220;  // 走子动画时长
    QTimer* m_animTimer = nullptr; // 动画刷新定时器
};
