# cpp-wzq OTA 部署（Cloudflare Pages）

自建 OTA 网关：客户端不直连 GitHub，统一走自己的域名，由 Pages Functions 代理检查更新与下载，边缘缓存加速国内访问。

```
设备 → ota.ryanuo.cc → Pages Functions（_worker.js） → Cloudflare Cache → GitHub
```

## 1. 前置条件

- Cloudflare 账号；`ryanuo.cc` 域名 DNS 已托管在 Cloudflare（绑定自定义域名需要）

## 2. 部署（Pages Advanced Mode）

> ⚠️ Pages **不会自动执行普通 `.js`**——把 `ota-worker.js` 拖进 Pages 只会被当静态文件托管（请求全部 404 且带 Pages 默认响应头，说明没生效）。必须用 `_worker.js` 且位于上传目录**根**。

代码：`deploy/pages/_worker.js`（ES module 格式，Pages 专用，含仓库白名单）

1. Dashboard → Workers & Pages → `ota-cqg` 项目 → **Create deployment** → 上传 assets
2. **拖拽整个 `deploy/pages/` 文件夹**（根目录含 `_worker.js` 即自动启用 Advanced Mode）
3. 域名：项目 → **Custom domains** → 绑定 `ota.ryanuo.cc`（确认 CF DNS 中 `ota` 的 CNAME 记录指向 `ota-cqg.pages.dev`）
4. 环境变量：项目 → Settings → **Environment variables**（见下节）

CLI 等价命令：`npx wrangler pages deploy deploy/pages --project-name ota-cqg`

## 3. 环境变量

| 变量 | 必填 | 类型 | 说明 |
|---|---|---|---|
| `ALLOWED_REPOS` | 否 | 普通变量 | 仓库白名单，逗号分隔，如 `ryanuo/cpp-wzq,ryanuo/other-app`；**空 = 允许所有仓库**；配置后非白名单仓库一律 403 |
| `GITHUB_TOKEN` | 否 | 加密变量 | GitHub 个人访问令牌（`public_repo` 权限）。匿名 API 限速 60 次/h/出口 IP（出口 IP 全用户共享，靠缓存缓解）；配 Token 后 5000 次/h |

## 4. 接口与缓存

| 路径 | 上游 | 缓存 TTL | 用途 |
|---|---|---|---|
| `/github-api/<path>` | `api.github.com/<path>` | 5 分钟 | 检查更新（JSON） |
| `/release/<owner>/<repo>/releases/download/<tag>/<file>` | `github.com/...` | 30 天 | 下载安装包 |

- Release 资产会 302 到 `objects.githubusercontent.com`，自动跟随，缓存的是最终内容
- 资产 URL 带 tag，新旧版本天然不冲突
- 已透传 `Range` 头，为客户端断点续传预留
- `/github-api/` 仅接受 `/repos/{owner}/{repo}/...` 路径（防 SSRF）

## 5. 部署后验证

```bash
# 检查更新（应返回最新 release JSON）
curl https://ota.ryanuo.cc/github-api/repos/ryanuo/cpp-wzq/releases/latest

# 下载（-I 看响应头；二次请求应命中缓存 cf-cache-status: HIT）
curl -I https://ota.ryanuo.cc/release/ryanuo/cpp-wzq/releases/download/v0.2.0/gobang-windows-x64.zip

# 白名单行为（配置 ALLOWED_REPOS 后, 非白名单仓库应 403）
curl -i https://ota.ryanuo.cc/github-api/repos/microsoft/vscode/releases/latest | head -1
```

## 6. 新应用接入（只需 2 处）

1. **网关**：环境变量 `ALLOWED_REPOS` 追加 `owner/repo`
2. **客户端**：`UpdateChecker("owner/repo", "资产前缀")`（见 `app/common/GameWindow.cpp:114`）

资产命名规范（`currentAssetName()` 按此拼接）：

| 平台 | 文件名 |
|---|---|
| Windows x64 | `{前缀}windows-x64.zip` |
| macOS | `{前缀}macos.zip` |
| Linux | `{前缀}linux.zip` |

## 7. 注意事项

- **发版可见性**：API 缓存 5 分钟，发布新 Release 后客户端最多延迟 5 分钟看到；紧急时可在 Dashboard → Cache → Purge 手动清掉
- **限速**：匿名 60 次/h/出口 IP，靠缓存基本打不到；用户量大后配 `GITHUB_TOKEN`
- **免费额度**：Pages Functions 免费版 10 万请求/天，个人 OTA 足够
- **安全**：`ALLOWED_REPOS` 建议始终配置（尤其网关公开后），防止他人把你的代理当免费 GitHub 镜像消耗额度
