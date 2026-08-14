cmake_minimum_required(VERSION 3.21)
project(gomoku-ai VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Multimedia)
qt_standard_project_setup()

add_executable(gomoku-ai
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/BoardWidget.cpp
    src/BoardWidget.h
    src/NetworkManager.cpp
    src/NetworkManager.h
    src/UpdateChecker.cpp
    src/UpdateChecker.h
    src/Chess.cpp
    src/Chess.h
    resources.qrc
)

target_link_libraries(gomoku-ai PRIVATE Qt6::Widgets Qt6::Network Qt6::Multimedia)

# 版本号注入（OTA 更新对比用）
target_compile_definitions(gomoku-ai PRIVATE GOMOKU_APP_VERSION="${PROJECT_VERSION}")

# macOS 打包为 .app bundle（macdeployqt 需要）；Windows 隐藏控制台窗口
set_target_properties(gomoku-ai PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

enable_testing()

add_executable(gomoku_net_test
    qtests/net_test.cpp
    src/NetworkManager.cpp
    src/NetworkManager.h
)

target_link_libraries(gomoku_net_test PRIVATE Qt6::Network)

add_test(NAME net_test COMMAND gomoku_net_test)

add_executable(gomoku_chess_test
    qtests/chess_test.cpp
    src/Chess.cpp
    src/Chess.h
)

target_include_directories(gomoku_chess_test PRIVATE src)

add_test(NAME chess_test COMMAND gomoku_chess_test)

# OTA 更新逻辑单测（版本比较 / 资产匹配）
add_executable(gomoku_update_test
    qtests/update_test.cpp
    src/UpdateChecker.cpp
    src/UpdateChecker.h
)

target_link_libraries(gomoku_update_test PRIVATE Qt6::Network)

target_include_directories(gomoku_update_test PRIVATE src)

add_test(NAME update_test COMMAND gomoku_update_test)
