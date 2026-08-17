#include "NetworkManager.h"

#include <QCryptographicHash>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>

namespace {

// 判断地址是否属于本机（回环或本机任一网卡地址）
bool isLocalAddress(const QHostAddress& addr)
{
    if (addr.isLoopback())
    {
        return true;
    }
    const QList<QHostAddress> localAddrs = QNetworkInterface::allAddresses();
    for (const QHostAddress& a : localAddrs)
    {
        if (a == addr)
        {
            return true;
        }
    }
    return false;
}

} // namespace

NetworkManager::NetworkManager(QObject* parent)
    : QObject(parent)
{
}

NetworkManager::~NetworkManager()
{
    cleanup();
}

void NetworkManager::start(const QString& password)
{
    if (m_connecting || m_connected)
    {
        return;
    }
    m_abnormalDisconnect = false;  // 新一局连接：清零上次的故障标志

    // 密码哈希：配对过滤 + 握手校验
    m_passwordHash = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex();
    // 随机 nonce（定长 16 hex，字典序 == 数值序），用于跨机角色协商
    const quint64 rand = QRandomGenerator::global()->generate64();
    m_nonce = QByteArray::number(rand, 16).rightJustified(16, '0');

    // ---- UDP 发现（跨机器互找）----
    m_udp = new QUdpSocket(this);
    // ShareAddress 使同机多实例可同时绑定发现端口
    if (!m_udp->bind(QHostAddress::AnyIPv4, kUdpPort,
                     QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
    {
        emit searchFailed(QStringLiteral("UDP 绑定失败：") + m_udp->errorString());
        cleanup();
        return;
    }
    connect(m_udp, &QUdpSocket::readyRead, this, &NetworkManager::onUdpReadyRead);

    // ---- TCP 监听（谁先绑定成功谁可当 HOST）----
    // 本机双开时第二个实例绑定必然失败 → 直连 127.0.0.1 配对，不依赖 UDP 回环投递
    m_tcpServer = new QTcpServer(this);
    if (m_tcpServer->listen(QHostAddress::AnyIPv4, kTcpPort))
    {
        connect(m_tcpServer, &QTcpServer::newConnection, this, &NetworkManager::onNewConnection);
    }
    else
    {
        m_tcpServer->deleteLater();
        m_tcpServer = nullptr;
        // 本机已有服务端：延迟直连本机
        QTimer::singleShot(300, this, [this] {
            if (!m_connecting && !m_connected)
            {
                m_connecting = true;
                m_isHost = false;
                m_peerIp = QHostAddress::LocalHost;
                connectToPeer();
                emit statusChanged(QStringLiteral("发现本机服务端，正在连接…"));
            }
        });
    }

    m_broadcastTimer = new QTimer(this);
    m_broadcastTimer->setInterval(1000);
    connect(m_broadcastTimer, &QTimer::timeout, this, &NetworkManager::broadcastHello);
    m_broadcastTimer->start();

    // 搜索总超时：30s（找到对手后 tryPair 会重置为 10s）
    m_pairTimeout = new QTimer(this);
    m_pairTimeout->setSingleShot(true);
    connect(m_pairTimeout, &QTimer::timeout, this, [this] {
        if (!m_connected)
        {
            m_countdownTimer->stop();
            emit searchFailed(QStringLiteral("配对超时，请确认双方输入相同密码并处于同一局域网"));
            cleanup();
        }
    });
    m_pairTimeout->start(30000);

    // 配对阶段每秒刷新剩余秒数（连接成功后停止）
    m_countdownTimer = new QTimer(this);
    m_countdownTimer->setInterval(1000);
    connect(m_countdownTimer, &QTimer::timeout, this, [this] {
        const int remaining = m_pairTimeout->remainingTime();
        if (remaining <= 0 || m_connected)
        {
            return;
        }
        const int secs = (remaining + 999) / 1000;
        if (m_connecting)
        {
            emit statusChanged(m_isHost
                                   ? QStringLiteral("已找到对手，等待对方连接…（剩余 %1s）").arg(secs)
                                   : QStringLiteral("已找到对手，正在连接…（剩余 %1s）").arg(secs));
        }
        else
        {
            emit statusChanged(QStringLiteral("正在搜索同密码对手…（剩余 %1s）").arg(secs));
        }
    });
    m_countdownTimer->start();

    m_retryTimer = new QTimer(this);
    m_retryTimer->setSingleShot(true);
    connect(m_retryTimer, &QTimer::timeout, this, &NetworkManager::onRetryConnect);

    broadcastHello();
    emit statusChanged(QStringLiteral("正在搜索同密码对手…"));
}

void NetworkManager::stop()
{
    if (m_connected && m_tcp && m_tcp->state() == QAbstractSocket::ConnectedState)
    {
        sendLine("QUIT");
    }
    cleanup();
}

void NetworkManager::broadcastHello()
{
    if (!m_udp)
    {
        return;
    }
    const QByteArray datagram = "GOMOKU1|HELLO|" + m_passwordHash + "|" + m_nonce;

    // 1) 全局广播 + 本机回环（同机双开配对）
    m_udp->writeDatagram(datagram, QHostAddress::Broadcast, kUdpPort);
    m_udp->writeDatagram(datagram, QHostAddress::LocalHost, kUdpPort);

    // 2) 每个活动 IPv4 网卡的子网定向广播（如 192.168.14.255），
    //    部分系统/路由器对 255.255.255.255 不友好，定向广播发现率更高
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : interfaces)
    {
        if (!(iface.flags() & QNetworkInterface::IsUp))
        {
            continue;
        }
        if (iface.flags() & QNetworkInterface::IsLoopBack)
        {
            continue; // 回环已单独发送
        }
        const QList<QNetworkAddressEntry> entries = iface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries)
        {
            const QHostAddress ip = entry.ip();
            const QHostAddress mask = entry.netmask();
            if (ip.protocol() != QAbstractSocket::IPv4Protocol || mask.isNull())
            {
                continue;
            }
            const quint32 broadcast = (ip.toIPv4Address() & mask.toIPv4Address())
                                      | (~mask.toIPv4Address());
            m_udp->writeDatagram(datagram, QHostAddress(broadcast), kUdpPort);
        }
    }
}

void NetworkManager::onUdpReadyRead()
{
    while (m_udp->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = m_udp->receiveDatagram();
        const QByteArray data = datagram.data();
        const QList<QByteArray> parts = data.split('|');
        if (parts.size() != 4 || parts[0] != "GOMOKU1" || parts[1] != "HELLO")
        {
            continue;
        }
        if (parts[3] == m_nonce)
        {
            continue; // 自己的广播
        }
        if (parts[2] != m_passwordHash)
        {
            continue; // 密码不同，忽略
        }
        tryPair(datagram.senderAddress(), parts[3]);
    }
}

void NetworkManager::tryPair(const QHostAddress& peerIp, const QByteArray& peerNonce)
{
    if (m_connecting || m_connected)
    {
        return;
    }
    m_connecting = true;
    m_peerIp = peerIp;
    m_broadcastTimer->stop();

    // 对端是否在本机（同机双开：UDP 回环投递的对端 IP 是本机地址，
    // 此时出站连接会连到自己的监听端口造成自我连接死锁）
    const bool peerIsLocal = isLocalAddress(peerIp);

    if (m_tcpServer && !peerIsLocal)
    {
        // 跨机器且双方都持有监听端口 → nonce 小者执黑（HOST），等待对方连接
        m_isHost = (m_nonce < peerNonce);
        if (m_isHost)
        {
            emit statusChanged(QStringLiteral("已找到对手，等待对方连接…"));
        }
        else
        {
            emit statusChanged(QStringLiteral("已找到对手，正在连接…"));
            connectToPeer();
        }
    }
    else
    {
        // 本机双开：持有监听端口者执黑并等待接入；监听失败者执白直连对方
        m_isHost = (m_tcpServer != nullptr);
        emit statusChanged(m_isHost ? QStringLiteral("已找到对手，等待对方连接…")
                                    : QStringLiteral("已找到对手，正在连接…"));
        if (!m_isHost)
        {
            connectToPeer();
        }
    }

    m_pairTimeout->start(10000);
}

void NetworkManager::connectToPeer()
{
    m_tcp = new QTcpSocket(this);
    connect(m_tcp, &QTcpSocket::readyRead, this, &NetworkManager::onTcpReadyRead);
    connect(m_tcp, &QTcpSocket::disconnected, this, &NetworkManager::onTcpDisconnected);
    connect(m_tcp, &QTcpSocket::errorOccurred, this, &NetworkManager::onTcpError);
    connect(m_tcp, &QTcpSocket::connected, this, &NetworkManager::onTcpConnected);
    m_tcp->connectToHost(m_peerIp, kTcpPort);
}

void NetworkManager::onNewConnection()
{
    // 若正在出站连接（本机双开的竞争场景），放弃出站、接受入站：持有监听端口者执黑
    if (m_tcp)
    {
        if (m_tcp->state() == QAbstractSocket::ConnectedState)
        {
            // 已配对/已连接：拒绝多余连接
            if (m_tcpServer->hasPendingConnections())
            {
                QTcpSocket* extra = m_tcpServer->nextPendingConnection();
                extra->abort();
                extra->deleteLater();
            }
            return;
        }
        // 出站未成功（Connecting/Unconnected/Closing）：放弃出站，改接入站
        m_tcp->disconnect(this); // 避免 abort 触发 cleanup
        m_tcp->abort();
        m_tcp->deleteLater();
        m_tcp = nullptr;
        m_retryTimer->stop();
    }

    m_tcp = m_tcpServer->nextPendingConnection();
    m_tcpServer->close(); // 只接受一个对手

    connect(m_tcp, &QTcpSocket::readyRead, this, &NetworkManager::onTcpReadyRead);
    connect(m_tcp, &QTcpSocket::disconnected, this, &NetworkManager::onTcpDisconnected);
    connect(m_tcp, &QTcpSocket::errorOccurred, this, &NetworkManager::onTcpError);

    m_connecting = true;
    m_isHost = true; // 入站连接定局：我方执黑
    m_broadcastTimer->stop();
    m_pairTimeout->start(10000);

    sendLine("HELLO " + m_passwordHash);
    checkHandshake();
}

void NetworkManager::onTcpConnected()
{
    if (m_tcpServer)
    {
        m_tcpServer->close(); // 出站成功即 CLIENT，不再接受入站
    }
    sendLine("HELLO " + m_passwordHash);
    checkHandshake();
}

void NetworkManager::onTcpReadyRead()
{
    m_tcpBuffer += m_tcp->readAll();
    while (true)
    {
        const int idx = m_tcpBuffer.indexOf('\n');
        if (idx < 0)
        {
            break;
        }
        const QByteArray line = m_tcpBuffer.left(idx).trimmed();
        m_tcpBuffer.remove(0, idx + 1);
        if (!line.isEmpty())
        {
            handleLine(line);
        }
    }
}

void NetworkManager::handleLine(const QByteArray& line)
{
    const QList<QByteArray> parts = line.split(' ');
    if (parts.isEmpty())
    {
        return;
    }

    if (parts[0] == "HELLO")
    {
        if (parts.size() == 2 && parts[1] == m_passwordHash)
        {
            m_peerHelloOk = true;
            checkHandshake();
        }
        else
        {
            emit searchFailed(QStringLiteral("密码校验失败，已断开连接"));
            sendLine("ERR");
            m_tcp->disconnectFromHost();
        }
    }
    else if (parts[0] == "MOVE" && m_connected)
    {
        // 五子棋: MOVE <row> <col>；象棋: MOVE <fr> <fc> <tr> <tc>
        if (parts.size() == 5)
        {
            bool ok = false;
            const int fr = parts[1].toInt(&ok);
            const int fc = ok ? parts[2].toInt(&ok) : 0;
            const int tr = ok ? parts[3].toInt(&ok) : 0;
            const int tc = ok ? parts[4].toInt(&ok) : 0;
            if (ok)
            {
                emit moveFromToReceived(fr, fc, tr, tc);
            }
        }
        else if (parts.size() == 3)
        {
            bool ok = false;
            const int row = parts[1].toInt(&ok);
            const int col = ok ? parts[2].toInt(&ok) : 0;
            if (ok)
            {
                emit moveReceived(row, col);
            }
        }
    }
    else if (parts[0] == "RESTART_REQ" && m_connected)
    {
        emit restartRequested();
    }
    else if (parts[0] == "UNDO" && m_connected)
    {
        emit undoRequested();
    }
    else if (parts[0] == "UNDO_OK" && m_connected)
    {
        emit undoAccepted();
    }
    else if (parts[0] == "UNDO_NO" && m_connected)
    {
        emit undoRejected();
    }
    else if (parts[0] == "SURRENDER" && m_connected)
    {
        emit surrendered();
    }
    else if (parts[0] == "QUIT")
    {
        cleanup(); // 对端退出
    }
    else if (parts[0] == "ERR")
    {
        emit searchFailed(QStringLiteral("密码校验失败，已断开连接"));
        cleanup();
    }
}

void NetworkManager::checkHandshake()
{
    if (m_peerHelloOk && m_tcp && m_tcp->state() == QAbstractSocket::ConnectedState)
    {
        m_connected = true;
        m_pairTimeout->stop();
        m_countdownTimer->stop();
        // 通用文案不带颜色（五子棋黑/白、象棋红/黑由 GameWindow::onConnected 的 sideName 提供）
        emit statusChanged(m_isHost ? QStringLiteral("已连接 · 配对成功（先手）")
                                    : QStringLiteral("已连接 · 配对成功（后手）"));
        emit connected(m_isHost);
    }
}

void NetworkManager::sendLine(const QByteArray& line)
{
    if (m_tcp && m_tcp->state() == QAbstractSocket::ConnectedState)
    {
        m_tcp->write(line + '\n');
    }
}

void NetworkManager::sendMove(int row, int col)
{
    if (m_connected)
    {
        sendLine("MOVE " + QByteArray::number(row) + " " + QByteArray::number(col));
    }
}

void NetworkManager::sendMoveFromTo(int fr, int fc, int tr, int tc)
{
    if (m_connected)
    {
        sendLine("MOVE " + QByteArray::number(fr) + " " + QByteArray::number(fc) + " "
                 + QByteArray::number(tr) + " " + QByteArray::number(tc));
    }
}

void NetworkManager::sendRestartRequest()
{
    if (m_connected)
    {
        sendLine("RESTART_REQ");
    }
}

void NetworkManager::sendUndo()
{
    if (m_connected)
    {
        sendLine("UNDO");
    }
}

void NetworkManager::sendUndoReply(bool accept)
{
    if (m_connected)
    {
        sendLine(accept ? "UNDO_OK" : "UNDO_NO");
    }
}

void NetworkManager::sendSurrender()
{
    if (m_connected)
    {
        sendLine("SURRENDER");
    }
}

void NetworkManager::onTcpDisconnected()
{
    if (m_connecting || m_connected)
    {
        cleanup();
    }
}

void NetworkManager::onTcpError(QAbstractSocket::SocketError error)
{
    if (m_connected)
    {
        // 对局中 socket 异常：仅"对端正常关闭连接"视为对端断开（判认输）；
        // 其余（本地断网/超时/未知错误）标记 abnormal，避免双方同时判对方认输
        if (error != QAbstractSocket::RemoteHostClosedError)
        {
            m_abnormalDisconnect = true;
        }
        return;
    }
    // 对方服务端可能尚未就绪，重试连接
    if (m_retryCount++ < 20)
    {
        m_retryTimer->start(500);
    }
    else if (m_peerIp == QHostAddress::LocalHost)
    {
        // 本机直连失败：放弃本机通道，继续 UDP 搜索局域网
        m_connecting = false;
        m_retryCount = 0;
        m_peerIp = QHostAddress();
        if (m_tcp)
        {
            m_tcp->disconnect(this);
            m_tcp->deleteLater();
            m_tcp = nullptr;
        }
        emit statusChanged(QStringLiteral("本机无匹配对手，继续搜索局域网…"));
    }
    else
    {
        emit searchFailed(QStringLiteral("连接失败，请重试"));
        cleanup();
    }
}

void NetworkManager::onRetryConnect()
{
    if (m_tcp && !m_connected)
    {
        m_tcp->connectToHost(m_peerIp, kTcpPort);
    }
}

void NetworkManager::cleanup()
{
    const bool wasConnected = m_connected;
    m_connected = false;
    m_connecting = false;
    m_peerHelloOk = false;
    m_retryCount = 0;

    if (m_tcp)
    {
        m_tcp->disconnect(this);
        m_tcp->abort();
        m_tcp->deleteLater();
        m_tcp = nullptr;
    }
    if (m_tcpServer)
    {
        m_tcpServer->close();
        m_tcpServer->deleteLater();
        m_tcpServer = nullptr;
    }
    if (m_udp)
    {
        m_udp->disconnect(this);
        m_udp->deleteLater();
        m_udp = nullptr;
    }
    if (m_broadcastTimer)
    {
        m_broadcastTimer->stop();
        m_broadcastTimer->deleteLater();
        m_broadcastTimer = nullptr;
    }
    if (m_pairTimeout)
    {
        m_pairTimeout->stop();
        m_pairTimeout->deleteLater();
        m_pairTimeout = nullptr;
    }
    if (m_countdownTimer)
    {
        m_countdownTimer->stop();
        m_countdownTimer->deleteLater();
        m_countdownTimer = nullptr;
    }
    if (m_retryTimer)
    {
        m_retryTimer->stop();
        m_retryTimer->deleteLater();
        m_retryTimer = nullptr;
    }
    m_tcpBuffer.clear();

    if (wasConnected)
    {
        emit disconnected();
    }
}
