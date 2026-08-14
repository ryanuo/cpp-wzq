# cpp-wzq · 五子棋局域网对战

基于 **Qt Widgets + CMake + C++17** 的双人五子棋联机对战应用。两人输入相同密码即可在同一局域网（或同机双开）自动配对开战，无需任何服务器。

![棋盘](res/board_1_cream.png)

## ✨ 功能

| 功能 | 说明 |
|---|---|
| 🖥 局域网自动配对 | UDP 广播发现（全局 + 回环 + 子网定向广播），输入相同密码即自动匹配 |
| 🔐 密码校验 | SHA-256 哈希过滤 + TCP 双向握手校验，密码不符自动断开 |
| ♟ 19×19 棋盘 | 标准 19 路棋盘，棋子精确对齐格点 |
| 🎨 四款换肤 | 米白 / 翡翠绿 / 浅橙 / 木质，可加载本地图片，重启记忆 |
| 📐 窗口等比缩放 | 任意拉伸窗口，棋盘等比放大缩小，点击坐标自动换算 |
| ↩️ 悔棋 | 请求-确认制，双方撤回最后一手 |
| 🏳 认输 | 对局中点「认输」或「新游戏」均视为认输；对方断开视为认输 |
| ⏱ 配对倒计时 | 搜索/连接阶段实时显示剩余秒数 |
| 🔊 音效与背景乐 | 落子/开始/胜负音效 + 循环背景乐 |

## 📥 下载

从 **[Releases](https://github.com/ryanuo/cpp-wzq/releases)** 下载对应系统的压缩包：

| 平台 | 文件 | 说明 |
|---|---|---|
| Windows | `gobang-windows-x64.zip` | 解压后运行 `gobang.exe` |
| macOS | `gobang-macos.zip` | 解压后打开 `gobang.app` |
| Linux | `gobang-linux.zip` | 解压后运行 `./gobang` |

> 发布版本：在仓库打 tag（如 `v0.1.0`）后自动触发 CI 构建并生成 Release。

## 🚀 使用

1. 两台电脑接入**同一局域网**（如同一个路由器/WiFi），或同一台电脑双开
2. 双方各自启动程序，点击 **「联机对战」**，输入**相同的密码**
3. 自动配对成功后：先绑定者执黑先手，后加入者执白
4. 状态栏显示连接状态、剩余配对秒数和**对手 IP**
5. 对局中可使用：悔棋（对方确认）、认输、新游戏（对局中视为认输）、断开（视为认输）

**跨设备连不上时**：检查 Windows 防火墙是否放行（首次运行弹窗请允许）、路由器是否开启 AP 隔离、两机是否同一网段（192.168.14.x）。

## 🛠 构建

依赖：Qt 6.5+（Widgets / Network / Multimedia 模块）、CMake 3.21+、支持 C++17 的编译器。

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build -j8
ctest --test-dir build --output-on-failure   # 运行测试（协议 + 悔棋逻辑）
```

- 主程序：`gobang`
- 测试：`gobang_net_test`（配对/MOVE/悔棋/认输/断开协议）、`gobang_chess_test`（落子/悔棋/回合逻辑）

## 📁 结构

```
cpp-wzq/
├── CMakeLists.txt        # 主程序 + 两个测试目标
├── resources.qrc         # 棋盘/棋子/音效资源（alias 显式命名）
├── src/
│   ├── main.cpp          # 入口（支持 --online <密码> 直连）
│   ├── MainWindow.*      # 对局编排、悔棋/认输确认、换肤、对手 IP
│   ├── BoardWidget.*     # 棋盘绘制（等比缩放）、鼠标落子换算
│   ├── Chess.*           # 棋盘模型（判胜/悔棋历史栈）
│   └── NetworkManager.*  # UDP 发现 + TCP 传输 + 密码配对 + 角色协商
├── qtests/               # net_test / chess_test
├── res/                  # 4 款棋盘 + 棋子 + 音效
└── docs/architecture.md  # 系统架构图（mermaid）
```

## 🔌 协议

- **UDP 45231**：广播发现 `GOMOKU1|HELLO|sha256(密码)|nonce`
- **TCP 45232**：对局传输 `HELLO / MOVE / RESTART / UNDO / UNDO_OK / UNDO_NO / SURRENDER / QUIT`
- 角色协商：nonce 小者执黑（HOST），双方独立计算必然一致

## 📦 发布新版本

```bash
git tag v0.1.0
git push origin v0.1.0
```

推送 `v*` tag 后，CI 自动构建 Windows / macOS / Linux 三端并发布到 GitHub Releases，可直接下载对应系统版本。
