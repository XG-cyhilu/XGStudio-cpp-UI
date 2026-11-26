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

// 一键起窗口：把"Surface + 自绘边框 + 标题栏 + 内容区"这套固定骨架收成一个入口。
// 入口名是 CreateAppWindow 而不是 CreateWindow：后者被 windows.h 定义成宏了。

#include "vcui/App.h"

#include <dwmapi.h>
#include <vector>

namespace vcui {

namespace {

// 组装窗口骨架（DPI → Surface → 主题 → TitleBar + body），已 Create 好、未跑循环。
std::unique_ptr<Surface> BuildWindow(const AppOptions& opt) {
    // DPI 感知必须在任何窗口创建之前设置；重复调用无副作用
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    auto app = std::make_unique<Surface>();
    // 最小尺寸不能把请求尺寸顶上去，否则小窗口会被强行放大
    int minW = opt.MinWidth < opt.Width ? opt.MinWidth : opt.Width;
    int minH = opt.MinHeight < opt.Height ? opt.MinHeight : opt.Height;
    app->SetMinSize(minW, minH);
    app->SetCustomFrame(opt.CustomFrame);
    if (!app->Create(opt.Title, opt.Width, opt.Height, opt.Dark)) return nullptr;

    // Create 之后再刷新主题：窗口类与首帧都用最终主题
    app->Th().Set(opt.Pal, opt.Dark);
    if (opt.Accent.a > 0.f) app->SetAccent(opt.Accent);

    auto root = VBox(0.f);

    if (opt.TitleBar) {
        auto bar = std::make_shared<TitleBar>();
        bar->Title = opt.Title;
        bar->Fill = opt.TitleBarFill;
        Surface* host = app.get();
        bar->OnCloseRequested = [host]() {
            host->Close();
            return true;
        };
        root->Add(bar);
    }

    auto body = VBox(0.f);
    body->Grow = 1.f;                 // 吃掉标题栏以下全部高度
    body->Pad(opt.BodyPad);
    root->Add(body);

    // 先把骨架挂到 Surface（AttachTree 会注入 Host/GFX），再跑用户的 Build：
    // 这样回调里 body.Host / body.GFX 已经可用，能立刻查主题、挂弹层、开新窗口。
    app->SetRoot(root);
    if (opt.Build) opt.Build(*body);
    return app;
}

} // namespace

std::unique_ptr<Surface> CreateAppSurface(const AppOptions& opt) {
    return BuildWindow(opt);
}

int RunAppLoop() {
    MSG msg{};
    LARGE_INTEGER freq, last;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);
    ULONGLONG lastFrameTick = 0;

    for (;;) {
        // 消息优先：把所有已排队的消息处理干净（含本进程所有窗口）
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return (int)msg.wParam;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        // 有没有窗口需要推帧（入场动画、悬停缓动、光标闪烁都算）
        bool anyAnim = false;
        for (Surface* s : detail::LiveSurfaces()) {
            if (s->Animating()) { anyAnim = true; break; }
        }

        if (anyAnim) {
            // 帧节奏上限 ~120fps：同一帧内的多次标脏（滚轮、鼠标移动）合并成一次绘制
            ULONGLONG tick = GetTickCount64();
            if (tick - lastFrameTick < 8) {
                MsgWaitForMultipleObjectsEx(0, nullptr, (DWORD)(8 - (tick - lastFrameTick)),
                                            QS_ALLINPUT, MWMO_INPUTAVAILABLE);
                continue;
            }
            lastFrameTick = tick;
            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);
            float dt = float(now.QuadPart - last.QuadPart) / float(freq.QuadPart);
            last = now;
            if (dt <= 0.f) dt = .008f;

            std::vector<HWND> dirty;
            // 拷一份再遍历：Pump 里可能因为对话框关闭而销毁覆盖层甚至窗口
            std::vector<Surface*> live = detail::LiveSurfaces();
            for (Surface* s : live) {
                if (!s->Animating()) continue;
                s->Pump(dt);
                if (HWND h = s->Hwnd()) dirty.push_back(h);
            }
            for (HWND h : dirty) UpdateWindow(h);   // 只处理各自标脏的那块
            DwmFlush();                              // 跟 DWM 合成节拍对齐，锁帧不抖
        } else {
            MsgWaitForMultipleObjectsEx(0, nullptr, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            last = LARGE_INTEGER{};
            QueryPerformanceCounter(&last);
        }
    }
}

void QuitApp() {
    // 拷贝一份：Close 会同步触发 DESTROY，注册表在回调里就被改写
    std::vector<Surface*> live = detail::LiveSurfaces();
    for (Surface* s : live) s->Close();
}

int CreateAppWindow(const AppOptions& opt) {
    auto app = CreateAppSurface(opt);
    if (!app) return 1;
    return RunAppLoop();
}

int CreateAppWindow(std::wstring title, int width, int height,
                    std::function<void(Widget& body)> build) {
    AppOptions o;
    o.Title = std::move(title);
    o.Width = width;
    o.Height = height;
    o.Build = std::move(build);
    return CreateAppWindow(o);
}

} // namespace vcui
