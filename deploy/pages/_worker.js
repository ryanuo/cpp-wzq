/**
 * cpp-wzq OTA 代理 —— Cloudflare Pages Advanced Mode (_worker.js)
 * ------------------------------------------------------------------
 * 架构: 设备 -> ota-cqg.pages.dev -> Pages Functions -> Cloudflare Cache -> GitHub
 *
 * 部署（Pages 不会自动运行普通 .js, 必须用本文件, 且放在上传目录根）:
 *   方式1（Dashboard, 无需装工具）: ota-cqg 项目 -> Create deployment ->
 *       上传 assets -> 拖拽整个 deploy/pages/ 文件夹
 *       （根目录含 _worker.js 即自动启用 Advanced Mode）
 *   方式2（CLI）: npx wrangler pages deploy deploy/pages --project-name ota-cqg
 *
 * 环境变量（Pages 项目 -> Settings -> Environment variables）:
 *   ALLOWED_REPOS  仓库白名单逗号分隔, 空 = 允许所有; 配置后非白名单仓库 403
 *   GITHUB_TOKEN   加密变量, GitHub 个人访问令牌(public_repo 权限),
 *                  匿名 API 限速 60 次/h/出口IP, 配 Token 后 5000 次/h
 *
 * 域名: Pages 项目 -> Custom domains 绑定 ota.ryanuo.cc
 *       （推荐, 客户端 C++ 端 kWorkerOrigin 保持不变）
 */

const GITHUB_API_ORIGIN = "https://api.github.com";
const GITHUB_DOWNLOAD_ORIGIN = "https://github.com";

const USER_AGENT = "gobang-updater";
const ACCEPT_API = "application/vnd.github+json";

// API JSON 缓存 5 分钟（发版后最多延迟 5 分钟可见, 可手动 purge）
const API_CACHE_TTL = 300;
// Release 资产缓存 30 天（资产按 tag 不可变, 新旧版本 URL 不同, 缓存天然不冲突）
const ASSET_CACHE_TTL = 2592000;

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    const path = url.pathname;

    if (path.startsWith("/github-api/")) {
      return proxyGithubApi(request, path, url.search, env);
    }

    if (path.startsWith("/release/")) {
      return proxyRelease(request, path, env);
    }

    return new Response("Not Found", { status: 404 });
  },
};

/**
 * 代理 GitHub REST API（检查更新）
 * /github-api/repos/ryanuo/cpp-wzq/releases/latest
 */
async function proxyGithubApi(request, path, search, env) {
  const apiPath = path.slice("/github-api".length); // 保留前导 "/"

  // 只允许 api.github.com 下的普通路径, 防 SSRF
  if (!apiPath.startsWith("/") || apiPath.includes("..")) {
    return new Response("Bad Request", { status: 400 });
  }

  // 白名单: 仅允许 /repos/{owner}/{repo}/... 路径下的授权仓库
  const repoMatch = apiPath.match(/^\/repos\/([^/]+)\/([^/]+)\//);
  if (!repoMatch || !isRepoAllowed(env, repoMatch[1], repoMatch[2])) {
    return new Response("Forbidden", { status: 403 });
  }

  const headers = new Headers({
    "User-Agent": USER_AGENT,
    Accept: ACCEPT_API,
  });

  // 配置了 GITHUB_TOKEN 加密变量时带上, 提升 API 限速额度
  if (env.GITHUB_TOKEN) {
    headers.set("Authorization", `Bearer ${env.GITHUB_TOKEN}`);
  }

  const response = await fetch(GITHUB_API_ORIGIN + apiPath + search, {
    headers,
    cf: {
      cacheEverything: true,
      cacheTtlByStatus: {
        "200-299": API_CACHE_TTL,
        "404": 60,
        "403": 60, // 限速时短暂缓存, 避免客户端反复触发
        "500-599": 0, // 5xx 不缓存, 立即回源
      },
    },
  });

  const out = new Response(response.body, {
    status: response.status,
    headers: response.headers,
  });
  out.headers.set("Cache-Control", `public, max-age=${API_CACHE_TTL}`);
  return out;
}

/**
 * 代理 GitHub Release 资产下载
 * /release/ryanuo/cpp-wzq/releases/download/v1.2.0/gobang-windows-x64.zip
 *
 * 注意: GitHub 资产会 302 到 objects.githubusercontent.com,
 *       fetch 默认 redirect:"follow" 自动跟随, 缓存的是最终内容。
 */
async function proxyRelease(request, path, env) {
  const match = path.match(
    /^\/release\/([^/]+)\/([^/]+)\/releases\/download\/([^/]+)\/(.+)$/
  );
  if (!match) {
    return new Response("Bad Request", { status: 400 });
  }

  const [, owner, repo, tag, fileRaw] = match;

  // 白名单: 非授权仓库一律拒绝
  if (!isRepoAllowed(env, owner, repo)) {
    return new Response("Forbidden", { status: 403 });
  }

  let file;
  try {
    file = decodeURIComponent(fileRaw); // 文件名可能被 QUrl 编码（空格/# 等）
  } catch {
    file = fileRaw;
  }

  const upstream =
    `${GITHUB_DOWNLOAD_ORIGIN}/${owner}/${repo}` +
    `/releases/download/${tag}/${file}`;

  const headers = new Headers();

  // 透传 Range, 为客户端断点续传预留（后续 C++ 端发 Range: bytes=N-）
  const range = request.headers.get("Range");
  if (range) {
    headers.set("Range", range);
  }

  const response = await fetch(upstream, {
    headers,
    redirect: "follow",
    cf: {
      cacheEverything: true,
      cacheTtlByStatus: {
        "200-299": ASSET_CACHE_TTL,
        "404": 60,
        "403": 60,
        "500-599": 0,
      },
    },
  });

  const out = new Response(response.body, {
    status: response.status,
    headers: response.headers,
  });
  out.headers.set("Cache-Control", "public, max-age=31536000, immutable");
  return out;
}

/**
 * 仓库白名单: env.ALLOWED_REPOS = "owner/repo,owner/repo2"（逗号分隔）
 * 空/未配置 = 允许所有仓库（单应用默认行为）
 * 新应用接入: 在环境变量里追加 "owner/repo" 即可
 */
function isRepoAllowed(env, owner, repo) {
  const raw = (env.ALLOWED_REPOS || "").trim();
  if (!raw) return true;
  const key = `${owner.toLowerCase()}/${repo.toLowerCase()}`;
  return raw
    .split(",")
    .map((s) => s.trim().toLowerCase())
    .includes(key);
}
