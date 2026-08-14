#include "GameLauncher.h"

#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include "../../games/gomoku/GomokuWindow.h"
#include "../../games/xiangqi/XiangqiWindow.h"

namespace {

// 简约图标：五子棋 = 黑白棋子；象棋 = 圆形棋子 + 汉字
QPixmap gomokuIcon(int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    const float r = size * 0.30f;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(40, 40, 42));  // 黑子
    p.drawEllipse(QPointF(size * 0.36f, size * 0.36f), r, r);
    p.setBrush(QColor(252, 252, 252));  // 白子
    p.setPen(QPen(QColor(200, 200, 205), size * 0.04f));
    p.drawEllipse(QPointF(size * 0.66f, size * 0.66f), r, r);
    return pm;
}

QPixmap xiangqiIcon(int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    const float r = size * 0.40f;
    p.setBrush(QColor(198, 152, 104));  // 棋子木色
    p.setPen(QPen(QColor(118, 82, 50), size * 0.05f));
    p.drawEllipse(QPointF(size * 0.5f, size * 0.5f), r, r);
    QFont f = p.font();
    f.setPixelSize(static_cast<int>(size * 0.46f));
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(150, 45, 30));
    p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, QStringLiteral("将"));
    return pm;
}

// 创建一张游戏卡片（图标 + 标题 + 副标题）
QPushButton* makeCard(const QPixmap& icon, const QString& name, const QString& subtitle,
                      QWidget* parent)
{
    auto* card = new QPushButton(parent);
    card->setObjectName(QStringLiteral("gameCard"));
    card->setCursor(Qt::PointingHandCursor);
    card->setMinimumSize(280, 92);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* iconLabel = new QLabel(card);
    iconLabel->setPixmap(icon);
    iconLabel->setFixedSize(56, 56);
    iconLabel->setStyleSheet(QStringLiteral("background: transparent;"));

    auto* nameLabel = new QLabel(name, card);
    QFont nf = nameLabel->font();
    nf.setPixelSize(20);
    nf.setBold(true);
    nameLabel->setFont(nf);
    nameLabel->setStyleSheet(QStringLiteral("color: #1d1d1f; background: transparent;"));

    auto* subLabel = new QLabel(subtitle, card);
    QFont sf = subLabel->font();
    sf.setPixelSize(13);
    subLabel->setFont(sf);
    subLabel->setStyleSheet(QStringLiteral("color: #8a8a8e; background: transparent;"));

    auto* textLayout = new QVBoxLayout;
    textLayout->setSpacing(2);
    textLayout->addWidget(nameLabel);
    textLayout->addWidget(subLabel);

    auto* row = new QHBoxLayout(card);
    row->setContentsMargins(20, 14, 20, 14);
    row->setSpacing(16);
    row->addWidget(iconLabel);
    row->addLayout(textLayout);
    row->addStretch();

    // 柔和投影
    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 3);
    shadow->setColor(QColor(0, 0, 0, 26));
    card->setGraphicsEffect(shadow);

    return card;
}

} // namespace

GameLauncher::GameLauncher(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("棋类对战 · 选择游戏"));
    setStyleSheet(QStringLiteral(
        "QMainWindow { background: #f1f1f3; }\n"
        "QPushButton#gameCard { background: #ffffff; border: 1px solid #e3e3e6;"
        " border-radius: 16px; }\n"
        "QPushButton#gameCard:hover { background: #fbfbfd; border-color: #cfcfd6; }\n"
        "QPushButton#gameCard:pressed { background: #f4f4f7; }"));

    auto* title = new QLabel(QStringLiteral("选择游戏"), this);
    title->setAlignment(Qt::AlignCenter);
    QFont tf = title->font();
    tf.setPixelSize(34);
    tf.setBold(true);
    title->setFont(tf);
    title->setStyleSheet(QStringLiteral("color: #1d1d1f; background: transparent;"));

    m_gomokuCard = makeCard(gomokuIcon(56), QStringLiteral("五子棋"),
                            QStringLiteral("局域网对战 · 19×19"), this);
    m_xiangqiCard = makeCard(xiangqiIcon(56), QStringLiteral("象棋"),
                             QStringLiteral("局域网对战 · 9×10"), this);
    connect(m_gomokuCard, &QPushButton::clicked, this, &GameLauncher::openGomoku);
    connect(m_xiangqiCard, &QPushButton::clicked, this, &GameLauncher::openXiangqi);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setSpacing(16);
    layout->setContentsMargins(48, 40, 48, 48);
    layout->addSpacing(8);
    layout->addWidget(title);
    layout->addSpacing(20);
    layout->addWidget(m_gomokuCard);
    layout->addWidget(m_xiangqiCard);
    layout->addStretch();
    setCentralWidget(central);

    setFixedSize(400, 420);
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
