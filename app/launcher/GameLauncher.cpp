#include "GameLauncher.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "../../games/gomoku/GomokuWindow.h"
#include "../../games/xiangqi/XiangqiWindow.h"

GameLauncher::GameLauncher(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("棋类对战 · 选择游戏"));

    auto* title = new QLabel(QStringLiteral("选择游戏"), this);
    title->setAlignment(Qt::AlignCenter);
    QFont tf = title->font();
    tf.setPixelSize(28);
    tf.setBold(true);
    title->setFont(tf);

    m_gomokuCard = new QPushButton(QStringLiteral("五子棋\n局域网对战 · 19×19"), this);
    m_xiangqiCard = new QPushButton(QStringLiteral("象棋\n局域网对战 · 9×10"), this);
    m_gomokuCard->setMinimumSize(260, 130);
    m_xiangqiCard->setMinimumSize(260, 130);
    connect(m_gomokuCard, &QPushButton::clicked, this, &GameLauncher::openGomoku);
    connect(m_xiangqiCard, &QPushButton::clicked, this, &GameLauncher::openXiangqi);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->addWidget(title);
    layout->addStretch();
    layout->addWidget(m_gomokuCard);
    layout->addWidget(m_xiangqiCard);
    layout->addStretch();
    layout->setContentsMargins(40, 24, 40, 40);
    setCentralWidget(central);

    resize(360, 420);
}

void GameLauncher::openGomoku()
{
    auto* w = new GomokuWindow;
    w->setAttribute(Qt::WA_DeleteOnClose);
    connect(w, &QObject::destroyed, this, &GameLauncher::onGameClosed);
    hide();
    w->show();
}

void GameLauncher::openXiangqi()
{
    auto* w = new XiangqiWindow;
    w->setAttribute(Qt::WA_DeleteOnClose);
    connect(w, &QObject::destroyed, this, &GameLauncher::onGameClosed);
    hide();
    w->show();
}

void GameLauncher::onGameClosed()
{
    show();
}
