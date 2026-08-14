#pragma once

#include <QMainWindow>

class QPushButton;

// 游戏选择首页：五子棋 / 象棋 卡片入口
// 点击进入对应游戏窗口；游戏窗口关闭后自动返回首页
class GameLauncher : public QMainWindow
{
    Q_OBJECT

public:
    explicit GameLauncher(QWidget* parent = nullptr);

private slots:
    void openGomoku();
    void openXiangqi();
    void onGameClosed();  // 游戏窗口关闭 -> 重新显示首页

private:
    QPushButton* m_gomokuCard = nullptr;
    QPushButton* m_xiangqiCard = nullptr;
};
