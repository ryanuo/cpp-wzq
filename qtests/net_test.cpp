// 联机协议 headless 自测（ctest 目标）：
// 1. 同进程两实例输入相同密码 -> 自动配对成功，主客角色互异
// 2. MOVE 双向往返
// 3. RESTART 通知
// 4. 不同密码不配对
// 全部通过返回 0，否则返回 1。
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

#include <cstdio>

#include "../src/NetworkManager.h"

namespace {

// 轮询事件循环直到条件满足或超时
bool waitUntil(const std::function<bool()>& cond, int timeoutMs = 10000)
{
    QElapsedTimer timer;
    timer.start();
    while (!cond())
    {
        QCoreApplication::processEvents();
        if (timer.elapsed() > timeoutMs)
        {
            return false;
        }
        QThread::msleep(20);
    }
    return true;
}

bool g_aConnected = false;
bool g_bConnected = false;
bool g_aGotMove = false;
bool g_bGotMove = false;
bool g_aGotRestart = false;

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    NetworkManager a;
    NetworkManager b;

    QObject::connect(&a, &NetworkManager::connected, [](bool) { g_aConnected = true; });
    QObject::connect(&b, &NetworkManager::connected, [](bool) { g_bConnected = true; });
    QObject::connect(&a, &NetworkManager::moveReceived, [](int r, int c) {
        if (r == 6 && c == 6) g_aGotMove = true;
    });
    QObject::connect(&b, &NetworkManager::moveReceived, [](int r, int c) {
        if (r == 5 && c == 5) g_bGotMove = true;
    });
    QObject::connect(&b, &NetworkManager::restartReceived, [] { g_aGotRestart = true; });

    QObject::connect(&a, &NetworkManager::statusChanged, [](const QString& s) {
        std::printf("  [A] %s\n", s.toUtf8().constData());
    });
    QObject::connect(&b, &NetworkManager::statusChanged, [](const QString& s) {
        std::printf("  [B] %s\n", s.toUtf8().constData());
    });

    // 1. 相同密码配对
    a.start("secret123");
    b.start("secret123");
    if (!waitUntil([] { return g_aConnected && g_bConnected; }))
    {
        std::printf("FAIL: 相同密码未能在 10s 内配对\n");
        return 1;
    }
    if (a.isHost() == b.isHost())
    {
        std::printf("FAIL: 角色协商错误，双方角色相同\n");
        return 1;
    }
    if (a.peerAddress().isNull() || b.peerAddress().isNull())
    {
        std::printf("FAIL: 对手 IP 不可见\n");
        return 1;
    }
    std::printf("PASS: 同密码配对成功 (A isHost=%d, B isHost=%d), 对手 IP 可见 (A->%s, B->%s)\n",
                a.isHost(), b.isHost(),
                a.peerAddress().toString().toUtf8().constData(),
                b.peerAddress().toString().toUtf8().constData());

    // 2. MOVE 双向往返
    a.sendMove(5, 5);
    if (!waitUntil([] { return g_bGotMove; }))
    {
        std::printf("FAIL: A -> B 的 MOVE 未到达\n");
        return 1;
    }
    b.sendMove(6, 6);
    if (!waitUntil([] { return g_aGotMove; }))
    {
        std::printf("FAIL: B -> A 的 MOVE 未到达\n");
        return 1;
    }
    std::printf("PASS: MOVE 双向往返正常\n");

    // 3. RESTART 通知
    a.sendRestart();
    if (!waitUntil([] { return g_aGotRestart; }))
    {
        std::printf("FAIL: RESTART 未到达\n");
        return 1;
    }
    std::printf("PASS: RESTART 通知正常\n");

    // 3.5 悔棋协议往返 (UNDO / UNDO_OK / UNDO_NO)
    bool aGotUndo = false;
    bool bGotUndo = false;
    bool aGotUndoOk = false;
    bool bGotUndoOk = false;
    bool bGotUndoNo = false;
    QObject::connect(&a, &NetworkManager::undoRequested, [&] { aGotUndo = true; });
    QObject::connect(&b, &NetworkManager::undoRequested, [&] { bGotUndo = true; });
    QObject::connect(&a, &NetworkManager::undoAccepted, [&] { aGotUndoOk = true; });
    QObject::connect(&b, &NetworkManager::undoAccepted, [&] { bGotUndoOk = true; });
    QObject::connect(&b, &NetworkManager::undoRejected, [&] { bGotUndoNo = true; });

    a.sendUndo();
    if (!waitUntil([&] { return bGotUndo; }))
    {
        std::printf("FAIL: UNDO 请求未到达 B\n");
        return 1;
    }
    b.sendUndoReply(true);
    if (!waitUntil([&] { return aGotUndoOk; }))
    {
        std::printf("FAIL: UNDO_OK 未到达 A\n");
        return 1;
    }
    b.sendUndo();
    if (!waitUntil([&] { return aGotUndo; }))
    {
        std::printf("FAIL: UNDO 请求未到达 A\n");
        return 1;
    }
    a.sendUndoReply(false);
    if (!waitUntil([&] { return bGotUndoNo; }))
    {
        std::printf("FAIL: UNDO_NO 未到达 B\n");
        return 1;
    }
    std::printf("PASS: 悔棋协议往返 (UNDO/UNDO_OK/UNDO_NO) 正常\n");

    // 3.6 认输协议
    bool bGotSurrender = false;
    bool aGotSurrender = false;
    QObject::connect(&a, &NetworkManager::surrendered, [&] { aGotSurrender = true; });
    QObject::connect(&b, &NetworkManager::surrendered, [&] { bGotSurrender = true; });
    a.sendSurrender();
    if (!waitUntil([&] { return bGotSurrender; }))
    {
        std::printf("FAIL: SURRENDER 未到达 B\n");
        return 1;
    }
    b.sendSurrender();
    if (!waitUntil([&] { return aGotSurrender; }))
    {
        std::printf("FAIL: SURRENDER 未到达 A\n");
        return 1;
    }
    std::printf("PASS: 认输协议往返 (SURRENDER) 正常\n");

    // 4. 断开后对端收到 disconnected
    bool bGotDisconnect = false;
    QObject::connect(&b, &NetworkManager::disconnected, [&] { bGotDisconnect = true; });
    a.stop();
    if (!waitUntil([&] { return bGotDisconnect; }))
    {
        std::printf("FAIL: 对端未收到断开通知\n");
        return 1;
    }
    std::printf("PASS: 断开通知正常\n");
    b.stop();

    // 5. 不同密码不配对
    NetworkManager c;
    NetworkManager d;
    bool cConnected = false;
    bool dConnected = false;
    QObject::connect(&c, &NetworkManager::connected, [&](bool) { cConnected = true; });
    QObject::connect(&d, &NetworkManager::connected, [&](bool) { dConnected = true; });
    c.start("password-a");
    d.start("password-b");
    if (waitUntil([&] { return cConnected || dConnected; }, 4000))
    {
        std::printf("FAIL: 不同密码竟然配对成功\n");
        c.stop();
        d.stop();
        return 1;
    }
    c.stop();
    d.stop();
    std::printf("PASS: 不同密码不配对\n");

    std::printf("ALL TESTS PASSED\n");
    return 0;
}
