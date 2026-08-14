#include "GomokuWindow.h"

#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

#include "GomokuBoard.h"
#include "GomokuChess.h"
#include "../common/NetworkManager.h"

namespace {

// 19×19 棋盘（新棋盘底图实测：上边距68 左边距72 格距25.3，600×600）
const int kGradeSize = 19;
const int kMarginX = 72;
const int kMarginY = 68;
const float kChessSize = 25.3f;

} // namespace

GomokuWindow::GomokuWindow(QWidget* parent)
    : GameWindow(QStringLiteral("五子棋 · 局域网对战"), parent)
{
    // 棋盘模型 + 绘制组件
    m_chess = new GomokuChess(kGradeSize, kMarginX, kMarginY, kChessSize);
    m_board = new GomokuBoard(this);
    m_board->setChess(m_chess);
    connect(m_board, &GomokuBoard::cellClicked, this, &GomokuWindow::onCellClicked);
    m_centralLayout->addWidget(m_board);
    m_boardWidget = m_board;

    // 皮肤菜单（基类菜单栏的「皮肤」项）
    fillSkinMenu(m_skinMenu);

    // 恢复上次换肤（默认翡翠绿棋盘）
    QSettings settings;
    const QString skin = settings.value(QStringLiteral("skin"),
                                        QStringLiteral(":/res/board_2_mint.png")).toString();
    m_board->setBackground(skin);
}

void GomokuWindow::fillSkinMenu(QMenu* skinMenu)
{
    skinMenu->addAction(QStringLiteral("米白色棋盘"), this,
                        [this] { applySkin(QStringLiteral(":/res/board_1_cream.png")); });
    skinMenu->addAction(QStringLiteral("翡翠绿棋盘"), this,
                        [this] { applySkin(QStringLiteral(":/res/board_2_mint.png")); });
    skinMenu->addAction(QStringLiteral("浅橙色棋盘"), this,
                        [this] { applySkin(QStringLiteral(":/res/board_3_peach.png")); });
    skinMenu->addAction(QStringLiteral("木质棋盘"), this,
                        [this] { applySkin(QStringLiteral(":/res/board_4_wood.png")); });
    skinMenu->addSeparator();
    skinMenu->addAction(QStringLiteral("从图片选择…"), this, [this] { chooseSkinFile(); });
}

bool GomokuWindow::canPlace(int row, int col) const
{
    return m_chess->getChessData(row, col) == 0;
}

int GomokuWindow::moveCount() const
{
    return m_chess->moveCount();
}

void GomokuWindow::placePiece(int row, int col, chess_kind_t kind)
{
    ChessPos pos(row, col);
    m_chess->chessDown(&pos, kind);
    m_board->repaintBoard();
    m_board->setLastMove(row, col);
}

void GomokuWindow::checkGameEnd(chess_kind_t lastKind)
{
    if (m_chess->checkOver())
    {
        m_gameOver = true;
        m_resultShown = true;
        const bool iWin = (lastKind == m_myKind);
        playSfx(iWin ? QStringLiteral("qrc:/res/win.mp3") : QStringLiteral("qrc:/res/lose.mp3"));

        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("对局结束"));
        box.setIcon(QMessageBox::Information);
        box.setText(iWin ? QStringLiteral("你赢了！") : QStringLiteral("你输了！"));
        QPushButton* again = box.addButton(QStringLiteral("再来一局"), QMessageBox::AcceptRole);
        box.addButton(QStringLiteral("断开"), QMessageBox::RejectRole);
        box.exec();

        if (box.clickedButton() == again)
        {
            onNewGameClicked();
        }
        else
        {
            // 断开连接：对局已结束不视为认输（m_resultShown 已防误判），留在窗口可重新联机
            m_network->stop();
        }
        return;
    }

    // 棋盘下满未分胜负 -> 平局
    bool full = true;
    const int size = m_chess->getGradeSize();
    for (int r = 0; r < size && full; r++)
    {
        for (int c = 0; c < size; c++)
        {
            if (m_chess->getChessData(r, c) == 0)
            {
                full = false;
                break;
            }
        }
    }
    if (full)
    {
        m_gameOver = true;
        m_resultShown = true;
        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("对局结束"));
        box.setIcon(QMessageBox::Information);
        box.setText(QStringLiteral("平局！"));
        QPushButton* again = box.addButton(QStringLiteral("再来一局"), QMessageBox::AcceptRole);
        box.addButton(QStringLiteral("断开"), QMessageBox::RejectRole);
        box.exec();

        if (box.clickedButton() == again)
        {
            onNewGameClicked();
        }
        else
        {
            // 断开连接：对局已结束不视为认输，留在窗口可重新联机
            m_network->stop();
        }
    }
}

void GomokuWindow::resetBoardContents()
{
    m_chess->init();
    m_board->repaintBoard();
    m_board->clearLastMove();
}

bool GomokuWindow::undoLastMove()
{
    if (m_chess->undoLast())
    {
        m_board->repaintBoard();
        m_board->clearLastMove();  // 悔棋后无最后一手标记
        return true;
    }
    return false;
}

bool GomokuWindow::isBlackTurn() const
{
    return m_chess->isBlackTurn();
}

QString GomokuWindow::myTurnText() const
{
    return QStringLiteral("● 轮到你落子（%1）")
        .arg(m_myKind == CHESS_BLACK ? QStringLiteral("黑") : QStringLiteral("白"));
}

QString GomokuWindow::waitText() const
{
    return QStringLiteral("○ 等待对方落子…");
}

void GomokuWindow::applyBoardBackground(const QString& imagePath)
{
    m_board->setBackground(imagePath);
}
