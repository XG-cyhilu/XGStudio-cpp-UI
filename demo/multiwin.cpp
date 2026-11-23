// Copyright 2026 XGstudio
//
// SPDX-License-Identifier: Apache-2.0
//
// 多窗口示例：同进程开两个窗口，共用一条消息循环。
// 关键点：CreateAppSurface 只建窗不阻塞，返回的 Surface 要一直持有到
// RunAppLoop 返回（窗口靠它活着）；最后一个窗口关闭时循环才会退出。

#include "vcui/vcui.h"

using namespace vcui;

static std::shared_ptr<Widget> BuildMainBody(Surface* host) {
    auto col = VBox(12.f);
    auto title = std::make_shared<Label>(L"主窗口");
    title->UseType(Type::Subtitle1);
    col->Add(title);

    auto hint = std::make_shared<Label>(L"这个进程同时开了两个窗口，共用一条消息循环。");
    if (host) hint->Color = host->Th().t.text2;
    col->Add(hint);

    auto row = HBox(8.f);
    auto open = std::make_shared<Button>(L"再开一个窗口", true);
    open->MinWidth = 140.f;
    open->Clicked = []() {
        // 运行时再开窗口：多窗口的核心用法。Surface 由静态表持有，
        // 窗口活多久它就活多久（关窗即销毁）。
        AppOptions o;
        o.Title = L"运行时新建窗口";
        o.Width = 460;
        o.Height = 280;
        o.Build = [](Widget& body) {
            auto c = VBox(10.f);
            auto lb = std::make_shared<Label>(L"我是运行时创建的新窗口。");
            lb->UseType(Type::Body1);
            c->Add(lb);
            c->Add(Spacer(0.f));
            body.Add(c);
        };
        static std::vector<std::shared_ptr<Surface>> extra;
        if (auto s = CreateAppSurface(o)) extra.push_back(std::shared_ptr<Surface>(std::move(s)));
    };
    row->Add(open);

    auto quit = std::make_shared<Button>(L"退出全部");
    quit->MinWidth = 100.f;
    quit->Clicked = []() { QuitApp(); };
    row->Add(quit);
    col->Add(row);

    auto sp = std::make_shared<Widget>();
    sp->Grow = 1.f;
    col->Add(sp);
    return col;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    AppOptions a;
    a.Title = L"XGStudio-cpp-UI — 多窗口";
    a.Width = 620;
    a.Height = 380;
    // Build 在树挂载之后执行，body.Host 已经就绪
    a.Build = [](Widget& body) {
        Surface* host = body.Host;
        auto col = BuildMainBody(host);
        body.Add(col);
    };

    auto w1 = CreateAppSurface(a);
    if (!w1) return 1;

    AppOptions b;
    b.Title = L"XGStudio-cpp-UI — 第二个窗口";
    b.Width = 420;
    b.Height = 260;
    b.Build = [](Widget& body) {
        auto c = VBox(8.f);
        c->Add(std::make_shared<Label>(L"启动时就存在的第二个窗口。"));
        c->Add(std::make_shared<Progress>(.6f));
        body.Add(c);
    };
    auto w2 = CreateAppSurface(b);
    if (!w2) return 1;

    return RunAppLoop();   // 两个窗口一起跑，全部关掉才返回
}
