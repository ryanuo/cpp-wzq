#pragma once

#include <QMainWindow>

class QAction;
class QPushButton;
class UpdateChecker;

// 游戏选择首页：五子棋 / 象棋 卡片入口
// 点击进入对应游戏窗口；游戏窗口关闭后自动返回首页
// 菜单栏：帮助（检查更新 / 关于）
class GameLauncher : public QMainWindow
{
    Q_OBJECT

public:
    explicit GameLauncher(QWidget* parent = nullptr);

private slots:
    void openGomoku();
    void openXiangqi();
    void onGameClosed();  // 游戏窗口关闭 -> 重新显示首页
    // 帮助菜单
    void onCheckUpdateClicked();
    void onAboutClicked();
    // OTA（UpdateChecker 信号）
    void onUpdateAvailable(const QString& version, const QString& assetName,
                           const QString& url, qint64 size);
    void onUpToDate(const QString& version);
    void onCheckFailed(const QString& reason);
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished(const QString& filePath);
    void onDownloadFailed(const QString& reason);

private:
    QPushButton* m_gomokuCard = nullptr;
    QPushButton* m_xiangqiCard = nullptr;
    QAction* m_updateAct = nullptr;  // 「检查更新」菜单项（下载期间禁用防重复）
    UpdateChecker* m_updater = nullptr;
};
