# cpp-wzq · 棋类局域网对战（五子棋 + 象棋）

基于 **Qt Widgets + CMake + C++17** 的双人局域网对战应用，启动进入游戏选择首页：
**五子棋**（19×19）与 **中国象棋**（9×10，走子规则完整）。两人输入相同密码即可在同一局域网（或同机双开）自动配对开战，无需任何服务器。

<img src="app/games/gomoku/README.png" width="300" />

## ✨ 功能

| 功能 | 说明 |
|---|---|
| 🎮 游戏选择首页 | 启动后选择五子棋 / 象棋，游戏窗口关闭自动返回 |
| 🖥 局域网自动配对 | UDP 广播发现（全局 + 回环 + 子网定向广播），输入相同密码即自动匹配 |
| 🔐 密码校验 | SHA-256 哈希过滤 + TCP 双向握手校验，密码不符自动断开 |
| ♟ 五子棋 | 19×19 标准棋盘，四款换肤（米白 / 翡翠绿 / 浅橙 / 木质） |
| 🐘 中国象棋 | 9×10 完整走子规则：蹩马腿 / 塞象眼 / 炮架 / 过河兵 / 将帅对脸，**吃将即胜**（简化，可后续升级） |
| 🎨 象棋换肤 | 4 款棋盘底图（古典宣纸 / 青玉翡翠 / 柔和粉淡 / 沉香红木），可加载本地图片，重启记忆 |
| 📐 窗口等比缩放 | 任意拉伸窗口，棋盘等比放大缩小，点击坐标自动换算 |
| ↩️ 悔棋 | 请求-确认制，双方撤回最后一手 |
| 🔄 再来一局 | 需对方同意（RESTART_REQ/OK/NO），拒绝则保留局面 |
| 🏳 认输 | 对局中点「认输」或「新游戏」均视为认输；对方断开视为认输 |
| 🔊 音效与背景乐 | 落子/开始/胜负音效 + 循环背景乐（两游戏共享） |
| 🚀 OTA 自动更新 | 检查更新走 GitHub 加速镜像（gh-proxy.com 等），失败自动切官方源，再失败提示手动下载 |

## 📥 下载

从 **[Releases](https://github.com/ryanuo/cpp-wzq/releases)** 下载对应系统的压缩包（**一个包内含两个游戏**）：

| 平台 | 文件 | 说明 |
|---|---|---|
| Windows | `gobang-windows-x64.zip` | 解压后运行 `gobang.exe` |
| macOS | `gobang-macos.zip` | 解压后打开 `gobang.app` |
| Linux | `gobang-linux.zip` | 解压后运行 `./gobang` |

> 发布版本：推送 `v*` tag 自动触发 CI 三端构建并生成 Release（见下方「发布新版本」）。

## 🚀 使用

1. 启动程序 → 首页选择「五子棋」或「象棋」
2. 两台电脑接入**同一局域网**（如同一路由器/WiFi），或同一台电脑双开
3. 双方输入**相同的密码**点击「联机对战」→ 自动配对成功（先绑定者执黑/红先手）
4. 状态栏左侧显示连接状态与对手 IP，右侧显示回合提示（轮到你 / 等待对方）
5. 对局中可用菜单操作：游戏（联机对战 / 新游戏 / 悔棋 / 认输 / 断开）、皮肤、帮助（检查更新）
6. 象棋为两段式操作：先点击选中己方棋子（红色高亮），再点击目标格走子

**跨设备连不上时**：检查 Windows 防火墙是否放行（首次运行弹窗请允许）、路由器是否开启 AP 隔离、两机是否同一网段。

## 🛠 构建

依赖：Qt 6.5+（Widgets / Network / Multimedia 模块）、CMake 3.21+、支持 C++17 的编译器。

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build -j8
ctest --test-dir build --output-on-failure   # 运行全部测试
```

- 主程序：`gobang`（macOS 生成 `gobang.app`）
- 测试（4 个）：`net_test`（联机协议）、`gomoku_test`（五子棋规则）、`xiangqi_test`（象棋走法/规则）、`update_test`（OTA 版本解析）

## 📁 结构

```
cpp-wzq/
├── CMakeLists.txt             # 公共库 + 主程序 + 4 个测试目标
├── Makefile                   # build/test/run/version/release 快捷命令
├── app/
│   ├── main.cpp               # 入口
│   ├── launcher/GameLauncher.*  # 首页：五子棋/象棋卡片选择
│   └── common/
│       ├── GameWindow.*       # 游戏窗口基类（菜单/状态栏/音效/OTA/联机状态机）
│       ├── NetworkManager.*   # UDP 发现 + TCP 传输 + 密码配对 + 角色协商
│       ├── UpdateChecker.*    # OTA：镜像 fallback + 超时 + 手动下载兜底
│       ├── ChessTypes.h       # 公共棋类枚举（先手/后手约定）
│       ├── resource.qrc       # 公共资源（音效）
│       └── res/               # 音效文件
├── games/
│   ├── gomoku/                # 五子棋
│   │   ├── GomokuWindow.*     # 窗口（落子/胜负/皮肤）
│   │   ├── GomokuBoard.*      # 棋盘绘制（等比缩放）
│   │   ├── GomokuChess.*      # 棋盘模型（判胜/悔棋历史栈）
│   │   ├── resource.qrc       # 五子棋资源
│   │   └── res/               # 4 款棋盘 + 棋子贴图
│   └── xiangqi/               # 中国象棋
│       ├── XiangqiWindow.*    # 窗口（两段式选子走子）
│       ├── XiangqiBoard.*     # 棋盘绘制（网格按底图校准，棋子 QPainter 汉字）
│       ├── XiangqiChess.*     # 规则层（走法合法性/胜负，纯逻辑可单测）
│       ├── resource.qrc       # 象棋资源
│       └── res/               # 4 款棋盘底图
├── qtests/                    # net / gomoku / xiangqi / update 测试
└── docs/architecture.md       # 系统架构图（mermaid）
```

## 🔌 协议

- **UDP 45231**：广播发现 `GOMOKU1|HELLO|sha256(密码)|nonce`
- **TCP 45232**：对局传输 `HELLO / MOVE / RESTART_REQ / RESTART_OK / RESTART_NO / UNDO / UNDO_OK / UNDO_NO / SURRENDER / QUIT`
- **MOVE**：五子棋 `MOVE <row> <col>`；象棋 `MOVE <fr> <fc> <tr> <tc>`（起止格，选子本地完成）
- 角色协商：nonce 小者执黑（HOST），双方独立计算必然一致；象棋红方 = 先手方

## 📦 发布新版本

版本号单一数据源 = `CMakeLists.txt` 的 `project(gobang VERSION x.y.z)`，编译期注入供 OTA 对比。

```bash
make version              # 查看当前版本
make version-bump         # 0.1.3 -> 0.1.4（或 version-bump minor / major）
make version-set VERSION=1.0.0   # 指定版本号
make release              # 一条龙：bump(默认 patch) + commit + push + 打 v* tag + push
```

推送 `v*` tag 后，CI 自动构建 Windows / macOS / Linux 三端并发布到 GitHub Releases。

> 国内 OTA：检查更新与下载优先走加速镜像（gh-proxy.com 等，按实测可用性维护在 `UpdateChecker.cpp`），全部失败后兜底官方直连，仍失败则提示手动打开 Releases 页下载。
