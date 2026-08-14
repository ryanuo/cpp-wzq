# 五子棋局域网对战 — 系统架构图

## 1. 总体架构

```mermaid
flowchart TB
    subgraph LAN["局域网 192.168.14.0/24（同机双开亦支持）"]
        subgraph PC1["电脑 A（192.168.14.x）— gomoku-ai"]
            direction TB
            MW1["MainWindow<br/>对局编排 · 悔棋确认 · 胜负对话框 · 对手IP显示"]
            BW1["BoardWidget<br/>棋盘绘制(QPainter) · 鼠标落子换算"]
            CH1["Chess<br/>棋盘模型 · 五连判胜 · 落子历史栈(悔棋)"]
            NM1["NetworkManager<br/>UDP发现 · TCP传输 · 密码配对 · 角色协商"]
            RES1["resources.qrc<br/>board.jpg / black·white.png / 音效"]
            MW1 --> BW1
            MW1 --> CH1
            MW1 --> NM1
            BW1 --> CH1
            BW1 -. 加载资源 .-> RES1
        end
        subgraph PC2["电脑 B（192.168.14.y）— gomoku-ai"]
            direction TB
            MW2["MainWindow"]
            BW2["BoardWidget"]
            CH2["Chess"]
            NM2["NetworkManager"]
            RES2["resources.qrc"]
            MW2 --> BW2
            MW2 --> CH2
            MW2 --> NM2
            BW2 --> CH2
            BW2 -. 加载资源 .-> RES2
        end
        UDP["UDP 45231<br/>广播发现<br/>全局255.255.255.255 + 回环 + 子网定向广播(192.168.14.255)"]
        TCP["TCP 45232<br/>对局传输<br/>HELLO / MOVE / RESTART / UNDO / UNDO_OK / UNDO_NO / SURRENDER / QUIT"]
        NM1 <--> UDP
        NM2 <--> UDP
        NM1 <--> TCP
        NM2 <--> TCP
    end
```

## 2. 配对与对局时序

```mermaid
sequenceDiagram
    autonumber
    participant A as 电脑A NetworkManager
    participant B as 电脑B NetworkManager

    Note over A,B: 双方输入相同密码后启动
    loop 每 1s 广播
        A->>B: GOMOKU1|HELLO|sha256(密码)|nonce_A (UDP 45231)
        B->>A: GOMOKU1|HELLO|sha256(密码)|nonce_B (UDP 45231)
    end
    Note over A,B: 密码哈希一致 → 非本机地址 → nonce 协商角色<br/>nonce 小者 = HOST(执黑先手, TCP服务端) / 大者 = CLIENT(执白, TCP客户端)
    B->>A: TCP 连接 45232
    A->>B: HELLO sha256(密码)（双向握手校验）
    B->>A: HELLO sha256(密码)
    Note over A,B: 握手通过 → 已连接（状态栏显示对手 IP）

    A->>B: MOVE 6 6（黑落子）
    Note over A,B: 双方各自落子+判胜，棋盘一致
    B->>A: MOVE 6 7（白落子）
    A->>B: UNDO（悔棋请求）
    B->>A: UNDO_OK（同意，双方撤回最后一手）
    A->>B: SURRENDER（认输，对局结束并断开）
    Note over A,B: 对局中断开 = 对方认输（弹框确认，你获胜）
    A->>B: RESTART（再来一局，双方重置）
    A->>B: QUIT（退出联机）
```

## 3. 关键机制

| 机制 | 说明 |
|---|---|
| 密码配对 | 密码 SHA-256 哈希做广播过滤 + TCP 双向握手校验，哈希不符即断开 |
| 角色协商 | 双方用同一组 (nonce_A, nonce_B) 独立计算，nonce 小者执黑，结果必然一致 |
| 同机双开 | UDP `ShareAddress` 共享绑定 + TCP 绑定竞争（先绑定者执黑），不依赖 UDP 回环投递 |
| 跨设备发现 | 三路广播：全局 255.255.255.255 + 本机回环 + 每个活动网卡的子网定向广播（如 192.168.14.255） |
| 悔棋 | 请求-确认制：UNDO → 对方弹窗 → UNDO_OK 双方撤回最后一手（Chess 落子历史栈） |
| 认输/断开 | SURRENDER 协议；对局中断开视为对方认输（弹框确认获胜）；本地断开需确认 |
| 配对倒计时 | 搜索/连接阶段状态栏每秒显示剩余秒数（30s 搜索 / 10s 配对） |
| 换肤 | 菜单 4 款内置棋盘（米白/翡翠绿/浅橙/木质，600×600 19 路）+ 本地图片（QSettings 持久化），背景任意尺寸等比铺满 |
| 棋盘规格 | 19×19；上边距 68、左边距 72、格距 25.3（按底图实测，棋子 <1px 对齐格点） |
| 等比缩放 | 窗口缩放时棋盘 QPainter scale 变换等比跟随（逻辑 600×600，鼠标坐标反算） |
| 容错 | 配对超时/失败后恢复「联机对战」按钮可重试；对端退出/掉线自动清理 |
