# Copyright 2026 XGstudio
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)
project(XGStudio-cpp-UI VERSION 1.0.0 LANGUAGES CXX)

# XGStudio-cpp-UI —— C++ / Direct2D + Win32 + DirectWrite 的自绘控件库
#
# 选项：
#   VCUI_BUILD_SHARED  构建 DLL（默认静态库）
#   VCUI_BUILD_DEMO    构建演示程序（默认开）
#   VCUI_STATIC_RUNTIME  MSVC 静态运行时（/MT），便于单文件分发

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(VCUI_BUILD_SHARED "Build VCUI as a shared library" OFF)
option(VCUI_BUILD_DEMO "Build the VCUI demo application" ON)
option(VCUI_STATIC_RUNTIME "Link the MSVC runtime statically (/MT)" OFF)

set(VCUI_SOURCES
    src/Color.cpp
    src/Theme.cpp
    src/Gfx.cpp
    src/App.cpp
    src/Widget.cpp
    src/Layout.cpp
    src/Controls.cpp
    src/Controls.More.cpp
    src/Feedback.cpp
    src/Surface.cpp)

if (VCUI_BUILD_SHARED)
    add_library(vcui SHARED ${VCUI_SOURCES})
    target_compile_definitions(vcui PUBLIC VCUI_DLL)
    target_compile_definitions(vcui PRIVATE VCUI_EXPORTS)
    # 公开头里有 std 容器成员，DLL 边界上的 C4251 是预期内的噪声
    target_compile_options(vcui PRIVATE /wd4251)
else()
    add_library(vcui STATIC ${VCUI_SOURCES})
endif()

add_library(VCUI::vcui ALIAS vcui)

# 公开头里有 C++17 的 inline 变量（vcui::Version），标准必须传给使用方，
# 否则 MSVC 默认 /std:c++14 的工程一 include 就报错。
target_compile_features(vcui PUBLIC cxx_std_17)

target_include_directories(vcui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_compile_definitions(vcui PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(vcui PRIVATE /utf-8 /W3)
if (VCUI_STATIC_RUNTIME)
    target_compile_options(vcui PRIVATE /MT)
endif()

target_link_libraries(vcui PUBLIC d2d1 dwrite dwmapi uxtheme user32 gdi32 ole32 imm32)

set_target_properties(vcui PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    DEBUG_POSTFIX d)

if (VCUI_BUILD_DEMO)
    function(vcui_add_demo name source)
        add_executable(${name} WIN32 ${source})
        target_link_libraries(${name} PRIVATE VCUI::vcui)
        target_compile_definitions(${name} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_compile_options(${name} PRIVATE /utf-8 /W3)
        if (VCUI_STATIC_RUNTIME)
            target_compile_options(${name} PRIVATE /MT)
        endif()
    endfunction()

    vcui_add_demo(VCUIDemo demo/main.cpp)        # 全控件展示
    vcui_add_demo(VCUIMinimal demo/minimal.cpp)  # 最小用法示例
    vcui_add_demo(VCUIMultiWin demo/multiwin.cpp)  # 多窗口 + 共享消息循环
    vcui_add_demo(VCUIFrameDemo demo/frame.cpp)  # 自绘边框 + 启动器排版打样

    # 无窗口布局自检：只算几何，打印每个节点的 frame 并统计越界
    add_executable(VCUILayoutCheck tools/layoutcheck.cpp)
    target_link_libraries(VCUILayoutCheck PRIVATE VCUI::vcui)
    target_compile_options(VCUILayoutCheck PRIVATE /utf-8 /W3)
endif()

# ---- 安装 / 导出，供 find_package(VCUI) 使用 ----
include(GNUInstallDirs)
install(TARGETS vcui EXPORT VCUITargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/vcui DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT VCUITargets
        NAMESPACE VCUI::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VCUI)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/VCUIConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/VCUIConfig.cmake" "include(\"\${CMAKE_CURRENT_LIST_DIR}/VCUITargets.cmake\")\n")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/VCUIConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/VCUIConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VCUI)
