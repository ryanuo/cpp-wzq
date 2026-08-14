# 五子棋局域网对战 - 构建 / 测试 / 版本管理
# 版本号单一数据源: CMakeLists.txt 第 2 行 project(gobang VERSION x.y.z)
# 常用命令:
#   make build                    增量构建
#   make test                     运行测试 (ctest)
#   make run                      启动 app
#   make version                  显示当前版本
#   make version-bump             自动递增 (默认 patch: 0.1.0 -> 0.1.1)
#   make version-bump minor       递增 minor: 0.1.1 -> 0.2.0
#   make version-bump major       递增 major: 0.2.0 -> 1.0.0
#   make version-set VERSION=1.2.3   指定版本号 (格式校验 x.y.z)
#   make tag                      打 v$(VERSION) tag 并推送 (触发 CI 自动发布 Release)
#   make release                  一条龙: bump + commit + push + tag + push

CMAKE      ?= cmake
BUILD_DIR  ?= build
QT_PREFIX  ?= $(HOME)/Qt/6.11.1/macos
CMAKE_ARGS ?= -DCMAKE_PREFIX_PATH=$(QT_PREFIX)

# 从 CMakeLists.txt 提取当前版本 (命令行传入 VERSION= 时优先生效, 供 version-set 用)
# 注意: $(shell ...) 内不能出现任何 () (make 括号计数不识别引号), 故全程用无括号命令
VERSION := $(shell grep '^project' CMakeLists.txt \
            | grep -oE 'VERSION [0-9.]+' | head -1 | cut -d' ' -f2)
TAG      := v$(VERSION)

# macOS 的 sed 需要 -i ''；Linux 是 -i
SED_INPLACE := $(shell uname -s | grep -q Darwin && echo "-i ''" || echo "-i")

APP := $(BUILD_DIR)/gobang.app/Contents/MacOS/gobang

.PHONY: build test run clean version version-bump version-set tag release

build:
	$(CMAKE) -S . -B $(BUILD_DIR) $(CMAKE_ARGS)
	$(CMAKE) --build $(BUILD_DIR) -j8

test:
	cd $(BUILD_DIR) && ctest --output-on-failure

run: build
	$(APP)

clean:
	rm -rf $(BUILD_DIR)

version:
	@echo "当前版本: $(VERSION)   (发布 tag: $(TAG))"

# 用法: make version-bump [major|minor|patch], 默认 patch
# 多余的命令行目标作为 bump 段 (make version-bump minor)
# major/minor/patch 为段参数的空目标, 防止 "No rule to make target" 报错
.PHONY: major minor patch
major minor patch:
	@:

version-bump:
	@kind=$$(echo "$(filter-out $@,$(MAKECMDGOALS))" | tr -d ' '); \
	test -n "$$kind" || kind=patch; \
	case "$$kind" in \
	  major) new=$$(echo "$(VERSION)" | awk -F. '{print $$1+1".0.0"}') ;; \
	  minor) new=$$(echo "$(VERSION)" | awk -F. '{print $$1"."$$2+1".0"}') ;; \
	  patch) new=$$(echo "$(VERSION)" | awk -F. '{print $$1"."$$2"."$$3+1}') ;; \
	  *) echo "无效段: $$kind (可选 major|minor|patch)"; exit 1 ;; \
	esac; \
	$(MAKE) _write_version NEW_VERSION="$$new" FROM="$(VERSION)" && \
	echo "版本: $(VERSION) -> $$new"

# 用法: make version-set VERSION=x.y.z
version-set:
	@test -n "$(VERSION)" || { echo "用法: make version-set VERSION=x.y.z"; exit 1; }; \
	echo "$(VERSION)" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+$$' \
	  || { echo "版本号格式错误: '$(VERSION)' (应为 x.y.z)"; exit 1; }; \
	$(MAKE) _write_version NEW_VERSION="$(VERSION)" FROM="$(VERSION)"

# 内部: 写入新版本号到 CMakeLists.txt 并显示结果
# 注意: project 行是 "VERSION 0.1.0 LANGUAGES CXX)", 版本号后不是 ) —— 只替换 "VERSION x.y.z" 部分
_write_version:
	@sed $(SED_INPLACE) -E '/^project/s/VERSION [0-9.]+/VERSION $(NEW_VERSION)/' \
	  CMakeLists.txt; \
	grep -E '^project' CMakeLists.txt

# 打 tag 并推送 (触发 CI: v* tag -> 三端构建 + 自动发布 Release)
tag:
	@git tag $(TAG) && git push origin $(TAG) && echo "已推送 tag: $(TAG)"

# 一条龙发布: 递增版本 -> 提交 -> 推送 -> 打 tag -> 推送
release: version-bump
	@git add CMakeLists.txt; \
	git commit -m "chore: 升级版本 $(TAG)"; \
	git push origin main; \
	git tag $(TAG) && git push origin $(TAG); \
	echo "发布完成: $(TAG) (CI 构建中, 完成后可在 Releases 下载)"
