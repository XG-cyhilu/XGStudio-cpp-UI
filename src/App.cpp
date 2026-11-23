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

namespace vcui {

int CreateAppWindow(const AppOptions& opt) {
    // DPI 感知必须在任何窗口创建之前设置；重复调用无副作用
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    Surface app;
    // 最小尺寸不能把请求尺寸顶上去，否则小窗口会被强行放大
    int minW = opt.MinWidth < opt.Width ? opt.MinWidth : opt.Width;
    int minH = opt.MinHeight < opt.Height ? opt.MinHeight : opt.Height;
    app.SetMinSize(minW, minH);
    app.SetCustomFrame(opt.CustomFrame);
    if (!app.Create(opt.Title, opt.Width, opt.Height, opt.Dark)) return 1;

    // Create 之后再刷新主题：窗口类与首帧都用最终主题
    app.Th().Set(opt.Pal, opt.Dark);
    if (opt.Accent.a > 0.f) app.SetAccent(opt.Accent);

    auto root = VBox(0.f);

    if (opt.TitleBar) {
        auto bar = std::make_shared<TitleBar>();
        bar->Title = opt.Title;
        bar->Fill = opt.TitleBarFill;
        bar->OnCloseRequested = [&app]() {
            app.Close();
            return true;
        };
        root->Add(bar);
    }

    auto body = VBox(0.f);
    body->Grow = 1.f;                 // 吃掉标题栏以下全部高度
    body->Pad(opt.BodyPad);
    if (opt.Build) opt.Build(*body);
    root->Add(body);

    app.SetRoot(root);
    return app.Run();
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
