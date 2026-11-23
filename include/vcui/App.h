// Copyright 2026 XGstudio
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "vcui/Surface.h"

namespace vcui {

// 一键起窗口的配置。默认就是"库标准外观"：
// Per-Monitor V2 DPI、自绘边框、库自己的标题栏（三系统按钮 + 可拖拽）。
struct AppOptions {
    std::wstring Title = L"XGStudio-cpp-UI";
    int Width = 960;
    int Height = 640;
    int MinWidth = 320;      // 窗口最小尺寸；不会大于 Width/Height（小窗口也能开）
    int MinHeight = 200;

    bool Dark = false;
    Palette Pal = Palette::Microsoft;
    D2D1_COLOR_F Accent{ 0, 0, 0, 0 };   // alpha <= 0 时用主题默认强调色

    bool CustomFrame = true;             // 自绘边框（无系统标题栏）
    bool TitleBar = true;                // 顶部自绘标题栏
    bool TitleBarFill = false;           // 标题栏是否铺 kp 底色；false 时与页面融合
    float BodyPad = 0.f;                 // 内容区默认内边距

    // 内容搭建。参数是标题栏下面的内容容器（已 Grow=1 铺满剩余区域）；
    // 留空就是空白窗口。回调里做的事和手动 SetRoot 完全一样。
    std::function<void(Widget& body)> Build;
};

// 创建窗口、跑消息循环，退出时返回进程退出码。阻塞直到最后一个窗口关闭。
// 典型用法：return CreateAppWindow({ .Title = L"我的应用", .Width = 900 });
//
// 名字没叫 CreateWindow：<windows.h> 把它 #define 成了 CreateWindowW，会撞名。
VCUI_API int CreateAppWindow(const AppOptions& opt);

// 最简重载：标题 + 尺寸（可选内容搭建）。不传 Build 就是纯空白窗口。
VCUI_API int CreateAppWindow(std::wstring title, int width = 960, int height = 640,
                             std::function<void(Widget& body)> build = nullptr);

// ---- 多窗口 ----
// 只建窗口、不跑循环：同进程开第二个/第三个窗口时用它，
// 把返回的 Surface 一直持有到 RunAppLoop 返回（窗口要靠它活着）。
// 已替你把 DPI 感知、主题、标题栏、内容区都装好，和 CreateAppWindow 完全同构。
VCUI_API std::unique_ptr<Surface> CreateAppSurface(const AppOptions& opt);

// 进程级共享消息循环：同线程所有窗口共用一条，给所有有动画的窗口推帧。
// 最后一个窗口关闭时返回（返回码 0）。重复调用没有意义，一个进程只需一次。
VCUI_API int RunAppLoop();

// 请求退出：关掉全部窗口（每个窗口都会依次收到 DESTROY），循环随即返回
VCUI_API void QuitApp();

} // namespace vcui
