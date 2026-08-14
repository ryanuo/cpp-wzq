#pragma once

#include <QPixmap>
#include <QString>
#include <QWidget>

#include "Chess.h"

// 棋盘绘制组件：底图 + 黑白棋子，鼠标点击换算格点
// 支持等比缩放（窗口拉大时棋盘跟随放大）与换肤（自定义背景图）
class BoardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BoardWidget(QWidget* parent = nullptr);

    void setChess(Chess* chess) { m_chess = chess; }
    void repaintBoard() { update(); }

    // 最后一手高亮（红点标记），落子后调用；悔棋/重开用 clearLastMove 清除
    void setLastMove(int row, int col)
    {
        m_lastRow = row;
        m_lastCol = col;
        update();
    }
    void clearLastMove()
    {
        m_lastRow = -1;
        m_lastCol = -1;
        update();
    }

    // 换肤：设置背景图（任意尺寸，绘制时等比缩放铺满），qrc 路径或本地文件
    void setBackground(const QString& imagePath);

    // 逻辑棋盘尺寸（600x600 逻辑坐标，实际渲染按 scale 缩放）
    static constexpr int kLogicSize = 600;

signals:
    void cellClicked(int row, int col);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    // 当前缩放系数 = min(宽,高)/600，绘制与鼠标换算共用
    float scaleFactor() const;

    Chess* m_chess = nullptr;
    QPixmap m_boardBg;
    QPixmap m_blackPiece;
    QPixmap m_whitePiece;
    int m_lastRow = -1;  // 最后一手高亮（-1 = 无）
    int m_lastCol = -1;
};
