cmake_minimum_required(VERSION 3.21)
project(gobang VERSION 0.3.9 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Multimedia)
qt_standard_project_setup()

# ============================================================
# 公共库（两个游戏共享：联机 / OTA / 游戏窗口基类 / 公共类型）
# ============================================================
add_library(gobang_common STATIC
    app/common/GameWindow.cpp
    app/common/GameWindow.h
    app/common/NetworkManager.cpp
    app/common/NetworkManager.h
    app/common/UpdateChecker.cpp
    app/common/UpdateChecker.h
    app/common/UpdateInstaller.cpp
    app/common/UpdateInstaller.h
    app/common/ChessTypes.h
)
target_link_libraries(gobang_common PUBLIC Qt6::Widgets Qt6::Network Qt6::Multimedia)
target_include_directories(gobang_common PUBLIC app/common)

# ============================================================
# 主程序：游戏选择首页 + 五子棋 + 象棋
# ============================================================
add_executable(gobang
    app/main.cpp
    app/launcher/GameLauncher.cpp
    app/launcher/GameLauncher.h
    games/gomoku/GomokuWindow.cpp
    games/gomoku/GomokuWindow.h
    games/gomoku/GomokuBoard.cpp
    games/gomoku/GomokuBoard.h
    games/gomoku/GomokuChess.cpp
    games/gomoku/GomokuChess.h
    games/xiangqi/XiangqiWindow.cpp
    games/xiangqi/XiangqiWindow.h
    games/xiangqi/XiangqiBoard.cpp
    games/xiangqi/XiangqiBoard.h
    games/xiangqi/XiangqiChess.cpp
    games/xiangqi/XiangqiChess.h
    app/common/resource.qrc
    games/gomoku/resource.qrc
    games/xiangqi/resource.qrc
)
target_link_libraries(gobang PRIVATE gobang_common)

# 版本号注入（OTA 更新对比用）
target_compile_definitions(gobang PRIVATE GOBANG_APP_VERSION="${PROJECT_VERSION}")

# macOS 打包为 .app bundle（macdeployqt 需要）；Windows 隐藏控制台窗口
set_target_properties(gobang PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# ============================================================
# 应用图标
#   macOS: .icns 放进 bundle Resources + Info.plist 引用（CFBundleIconFile）
#   Windows: 显式 .rc 编译嵌入 exe（RC_ICONS 属性需 CMake 3.22+ 且部分环境
#            静默失效，实测 exe 无 RT_ICON；.rc 任何版本都编译）
#   窗口/任务栏: gobang-1024.png 经 resource.qrc 以 :/res/icon.png 加载（main.cpp setWindowIcon）
# 重新生成: python3 scripts/make-icons.py（Pillow + iconutil；ico 为 BMP 格式兼容 rc.exe）
# ============================================================
target_sources(gobang PRIVATE app/icon/gobang.icns)
set_source_files_properties(app/icon/gobang.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
set_target_properties(gobang PROPERTIES MACOSX_BUNDLE_ICON_FILE gobang.icns)
if(WIN32)
    target_sources(gobang PRIVATE app/icon/gobang.rc)
    # rc.exe 按 include 路径找 gobang.ico
    target_include_directories(gobang PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/app/icon")
endif()

# ============================================================
# 测试
# ============================================================
enable_testing()

# 联机协议（公共）
add_executable(gobang_net_test
    qtests/net_test.cpp
    app/common/NetworkManager.cpp
    app/common/NetworkManager.h
)
target_link_libraries(gobang_net_test PRIVATE Qt6::Network)
target_include_directories(gobang_net_test PRIVATE app/common)
add_test(NAME net_test COMMAND gobang_net_test)

# 五子棋规则
add_executable(gobang_gomoku_test
    qtests/gomoku_test.cpp
    games/gomoku/GomokuChess.cpp
    games/gomoku/GomokuChess.h
)
target_include_directories(gobang_gomoku_test PRIVATE games/gomoku app/common)
add_test(NAME gomoku_test COMMAND gobang_gomoku_test)

# 象棋规则
add_executable(gobang_xiangqi_test
    qtests/xiangqi_test.cpp
    games/xiangqi/XiangqiChess.cpp
    games/xiangqi/XiangqiChess.h
)
target_include_directories(gobang_xiangqi_test PRIVATE games/xiangqi app/common)
add_test(NAME xiangqi_test COMMAND gobang_xiangqi_test)

# OTA（公共）
add_executable(gobang_update_test
    qtests/update_test.cpp
    app/common/UpdateChecker.cpp
    app/common/UpdateChecker.h
)
target_link_libraries(gobang_update_test PRIVATE Qt6::Network)
target_include_directories(gobang_update_test PRIVATE app/common)
add_test(NAME update_test COMMAND gobang_update_test)
