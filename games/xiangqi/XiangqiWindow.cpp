#include "XiangqiWindow.h"

#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSoundEffect>
#include <QVBoxLayout>

#include "XiangqiBoard.h"
#include "XiangqiChess.h"
#include "../common/NetworkManager.h"

XiangqiWindow::XiangqiWindow(QWidget* parent)
    : GameWindow(QStringLiteral("象棋 · 局域网对战"), parent)
{
    // 棋盘模型 + 绘制组件
    m_chess = new XiangqiChess;
    m_board = new XiangqiBoard(this);
    m_board->setChess(m_chess);
    connect(m_board, &XiangqiBoard::cellClicked, this, &XiangqiWindow::onCellClicked);
    m_centralLayout->addWidget(m_board);
    m_boardWidget = m_board;

    // 皮肤菜单（4 款图片底图 + 从图片选择）
    fillSkinMenu(m_skinMenu);

    // 恢复上次象棋皮肤（默认古典宣纸）
    QSettings settings;
    m_board->setBackground(settings.value(
        QStringLiteral("xiangqi_skin"), QStringLiteral(":/res/board_xq_paper.png")).toString());
}

void XiangqiWindow::fillSkinMenu(QMenu* skinMenu)
{
    // 4 款棋盘底图（用户提供）
    skinMenu->addAction(QStringLiteral("古典宣纸"), this,
                        [this] { saveSkinPath(QStringLiteral(":/res/board_xq_paper.png")); });
    skinMenu->addAction(QStringLiteral("青玉翡翠"), this,
                        [this] { saveSkinPath(QStringLiteral(":/res/board_xq_jade.png")); });
    skinMenu->addAction(QStringLiteral("柔和粉淡"), this,
                        [this] { saveSkinPath(QStringLiteral(":/res/board_xq_pink.png")); });
    skinMenu->addAction(QStringLiteral("沉香红木"), this,
                        [this] { saveSkinPath(QStringLiteral(":/res/board_xq_redwood.png")); });
    skinMenu->addSeparator();
    skinMenu->addAction(QStringLiteral("从图片选择…"), this, [this] { chooseSkinFile(); });
}

void XiangqiWindow::saveSkinPath(const QString& path)
{
    m_board->setBackground(path);
    QSettings settings;
    settings.setValue(QStringLiteral("xiangqi_skin"), path);
    setStatus(QStringLiteral("已更换背景"));
}

void XiangqiWindow::onCellClicked(int row, int col)
{
    if (!m_connected)
    {
        setStatus(QStringLiteral("未连接 · 请先点击「联机对战」输入密码配对"));
        return;
    }
    if (m_gameOver)
    {
        return;
    }
    if (!myTurn())
    {
        setStatus(waitText());
        return;
    }

    // 两段式：先选子，再点目标格
    if (m_selectedRow < 0)
    {
        if (m_chess->canSelect(row, col))
        {
            m_selectedRow = row;
            m_selectedCol = col;
            m_board->setSelected(row, col);
            setStatus(QStringLiteral("已选中「%1」，点击目标位置走子")
                          .arg(QString::fromUtf8(XiangqiChess::pieceName(m_chess->pieceAt(row, col)))));
        }
        else if (m_chess->pieceAt(row, col) != PIECE_NONE)
        {
            setStatus(QStringLiteral("不是你的棋子"));
        }
        return;
    }

    // 已选中：目标格
    const int fr = m_selectedRow;
    const int fc = m_selectedCol;
    if (m_chess->canMove(fr, fc, row, col))
    {
        m_selectedRow = m_selectedCol = -1;
        doLocalMove(fr, fc, row, col, true);
        checkGameEnd(m_myKind);
        return;
    }
    // 非法目标：点己方其他子则切换选中
    if (m_chess->canSelect(row, col))
    {
        m_selectedRow = row;
        m_selectedCol = col;
        m_board->setSelected(row, col);
    }
    else
    {
        setStatus(QStringLiteral("该棋子无法走到目标位置"));
    }
}

void XiangqiWindow::onMoveFromToReceived(int fr, int fc, int tr, int tc)
{
    if (!m_connected || m_gameOver)
    {
        return;
    }
    if (!m_chess->canMove(fr, fc, tr, tc))
    {
        return; // 对端消息非法，忽略
    }
    doLocalMove(fr, fc, tr, tc, false);
    checkGameEnd(opponentKind());
}

void XiangqiWindow::doLocalMove(int fr, int fc, int tr, int tc, bool notify)
{
    m_chess->doMove(fr, fc, tr, tc);
    m_board->repaintBoard();
    m_board->clearSelected();
    m_board->setLastMove(tr, tc);
    m_downSound->play();
    if (notify)
    {
        m_network->sendMoveFromTo(fr, fc, tr, tc);
    }
    updateTurnHint();
}

bool XiangqiWindow::canPlace(int row, int col) const
{
    // 基类 onCellClicked 路径不用（象棋覆盖）；兜底实现
    return row >= 0 && row < XiangqiChess::kRows && col >= 0 && col < XiangqiChess::kCols;
}

int XiangqiWindow::moveCount() const
{
    return m_chess->moveCount();
}

void XiangqiWindow::placePiece(int row, int col, chess_kind_t kind)
{
    Q_UNUSED(row);
    Q_UNUSED(col);
    Q_UNUSED(kind);
    // 象棋走子走 doLocalMove（起止格），基类路径不用
}

void XiangqiWindow::checkGameEnd(chess_kind_t lastKind)
{
    if (m_chess->generalCaptured())
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
            m_network->stop();
        }
    }
}

void XiangqiWindow::resetBoardContents()
{
    m_chess->init();
    m_selectedRow = m_selectedCol = -1;
    m_board->repaintBoard();
    m_board->clearSelected();
    m_board->clearLastMove();
}

bool XiangqiWindow::undoLastMove()
{
    if (m_chess->undoLast())
    {
        m_selectedRow = m_selectedCol = -1;
        m_board->repaintBoard();
        m_board->clearSelected();
        m_board->clearLastMove();
        return true;
    }
    return false;
}

bool XiangqiWindow::isBlackTurn() const
{
    // 约定：CHESS_BLACK = 先手方 = 象棋红方
    return m_chess->isRedTurn();
}

QString XiangqiWindow::myTurnText() const
{
    return QStringLiteral("● 轮到你走子（%1）")
        .arg(m_myKind == CHESS_BLACK ? QStringLiteral("红") : QStringLiteral("黑"));
}

QString XiangqiWindow::waitText() const
{
    return QStringLiteral("○ 等待对方走子…");
}

void XiangqiWindow::applyBoardBackground(const QString& imagePath)
{
    m_board->setBackground(imagePath);
}
