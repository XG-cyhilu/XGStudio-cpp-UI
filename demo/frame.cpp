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

// 顶栏导航 + 左侧子导航 + 平坦内容区 的排版打样（无卡片包内容）。
// 用法：VCUIFrameDemo
//   VCUI_DEMO_DARK=1      深色
//   VCUI_NAV_CENTER=1     顶栏导航"最小居中"，否则整栏

#include <windows.h>
#include <vcui/vcui.h>

using namespace vcui;

// 文字一律走 Fluent type ramp：字号 / 字重 / 行高成套拿
static std::shared_ptr<Label> L(std::wstring s, const TypeStyle& sty) {
    auto l = std::make_shared<Label>(std::move(s), sty.size, sty.weight);
    l->LineHeight = sty.lineHeight;
    return l;
}

// 平坦内容区的一行：左标签 + 右值，最后跟一条分隔线
static std::shared_ptr<Widget> FormRow(const std::wstring& label, const std::wstring& value) {
    auto row = HBox(16.f);
    row->H = 48.f;
    row->Cross = Stack::Center;
    auto l = L(label, Type::Body1);
    l->W = 180.f;
    auto v = L(value, Type::Body1Strong);
    v->Grow = 1.f;
    v->Ellipsis = true;
    row->Add(l);
    row->Add(v);
    return row;
}

static std::shared_ptr<Widget> Section(const std::wstring& title) {
    auto s = L(title, Type::Subtitle1);
    s->MT = 8.f;
    return s;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    bool dark = GetEnvironmentVariableW(L"VCUI_DEMO_DARK", nullptr, 0) > 0;
    bool navCenter = GetEnvironmentVariableW(L"VCUI_NAV_CENTER", nullptr, 0) > 0;
    wchar_t pageBuf[8] = {};
    int startPage = GetEnvironmentVariableW(L"VCUI_PAGE", pageBuf, 8) > 0 ? _wtoi(pageBuf) : 0;

    Surface app;
    app.SetMinSize(960, 600);
    app.SetCustomFrame(true);
    if (!app.Create(L"VCUI Frame Demo", 1180, 740, dark)) return 1;

    auto root = VBox(0.f);

    // ---- 顶栏：标题 + 导航（整栏 / 最小居中）+ 窗口按钮 ----
    auto bar = std::make_shared<TitleBar>();
    bar->Title = L"Void Craft";
    bar->Fill = true;
    bar->H = 48.f;
    bar->NavItems = { L"启动", L"版本", L"下载", L"设置" };
    bar->NavCentered = navCenter;
    root->Add(bar);

    // ---- 主体：左侧子导航 + 内容区 ----
    auto body = HBox(0.f);
    body->Grow = 1.f;

    auto sub = VBox(4.f);
    sub->W = 220.f;
    sub->Pad(12.f);

    auto content = VBox(0.f);
    content->Grow = 1.f;
    content->Pad(28.f);

    body->Add(sub);
    body->Add(content);
    root->Add(body);

    // 页面 0：启动（无子导航）
    auto BuildLaunch = [&]() {
        sub->Visible = false;

        auto head = HBox(16.f);
        head->H = 44.f;
        head->Cross = Stack::Center;
        auto ver = L(L"1.20.4", Type::Title1);
        ver->Grow = 1.f;
        auto play = std::make_shared<Button>(L"启动游戏", true);
        play->W = 160.f;
        play->H = 40.f;
        auto open = std::make_shared<Button>(L"打开文件夹");
        open->W = 132.f;
        open->H = 40.f;
        open->MR = 8.f;
        head->Add(ver);
        head->Add(open);
        head->Add(play);
        content->Add(head);

        auto subline = L(L"Fabric 0.15.11   ·   上次游玩 2 小时前   ·   累计 37 小时", Type::Body1);
        subline->MT = 8.f;
        subline->Ellipsis = true;
        content->Add(subline);

        auto div = std::make_shared<Divider>();
        div->MT = 24.f;
        div->MB = 8.f;
        content->Add(div);

        content->Add(Section(L"运行"));
        content->Add(FormRow(L"Java", L"17.0.9 (x64)  ·  C:\\Program Files\\Java\\jdk-17.0.9"));
        auto d1 = std::make_shared<Divider>();
        content->Add(d1);
        content->Add(FormRow(L"内存", L"4096 MB  (上限 4096)"));
        auto d2 = std::make_shared<Divider>();
        content->Add(d2);
        content->Add(FormRow(L"游戏目录", L"版本隔离  ·  versions\\1.20.4-Fabric 0.15.11"));
        auto d3 = std::make_shared<Divider>();
        content->Add(d3);
        content->Add(FormRow(L"加载器", L"Fabric 0.15.11"));

        auto lt = L(L"最近日志", Type::Subtitle1);
        lt->MT = 28.f;
        content->Add(lt);

        auto logScroll = std::make_shared<ScrollView>();
        logScroll->Grow = 1.f;
        logScroll->MT = 10.f;
        auto logBox = VBox(2.f);
        for (int i = 0; i < 30; ++i) {
            wchar_t b[160];
            swprintf_s(b, L"[12:%02d:%02d] [INFO] 校验 assets/objects/%02x/%04d  正常", i, (i * 7) % 60, i % 256, i * 13);
            auto line = L(b, Type::Caption1);
            line->Ellipsis = true;
            logBox->Add(line);
        }
        logScroll->Add(logBox);
        content->Add(logScroll);
    };

    // 页面 1：版本（子导航：已安装 / 可安装 / 整合包）
    auto BuildVersions = [&]() {
        sub->Visible = true;
        sub->Clear();
        auto n1 = std::make_shared<NavItem>(L"已安装", L"\xE8F4");
        n1->Active = true;
        auto n2 = std::make_shared<NavItem>(L"可安装", L"\xE896");
        auto n3 = std::make_shared<NavItem>(L"整合包", L"\xE8F1");
        sub->Add(n1);
        sub->Add(n2);
        sub->Add(n3);

        auto head = HBox(12.f);
        head->H = 44.f;
        head->Cross = Stack::Center;
        auto t = L(L"已安装的版本", Type::Subtitle1);
        t->Grow = 1.f;
        auto search = std::make_shared<Input>(L"搜索版本…");
        search->W = 240.f;
        head->Add(t);
        head->Add(search);
        content->Add(head);

        auto div = std::make_shared<Divider>();
        div->MT = 14.f;
        content->Add(div);

        auto listScroll = std::make_shared<ScrollView>();
        listScroll->Grow = 1.f;
        auto rows = VBox(0.f);
        for (int i = 0; i < 16; ++i) {
            wchar_t t2[96];
            swprintf_s(t2, L"1.%d.4-Fabric 0.15.11-性能优化整合包", 16 + i % 5);
            auto row = std::make_shared<ListRow>(t2, L"1.20.4  ·  Fabric 0.15.11", i == 0 ? L"已选择" : L"3 天前");
            row->H = 52.f;
            row->Selected = (i == 0);
            rows->Add(row);
            auto d = std::make_shared<Divider>();
            rows->Add(d);
        }
        listScroll->Add(rows);
        content->Add(listScroll);
    };

    // 页面 2：下载（无子导航）
    auto BuildDownload = [&]() {
        sub->Visible = false;

        auto t = L(L"下载队列", Type::Subtitle1);
        content->Add(t);

        auto div = std::make_shared<Divider>();
        div->MT = 14.f;
        content->Add(div);

        auto listScroll = std::make_shared<ScrollView>();
        listScroll->Grow = 1.f;
        auto rows = VBox(14.f);
        for (int i = 0; i < 6; ++i) {
            auto box = VBox(6.f);
            auto row = HBox(12.f);
            wchar_t name[96];
            swprintf_s(name, L"libraries\\com\\mojang\\%d.0.3\\asset-%d.jar", i, i);
            auto n = L(name, Type::Body1);
            n->Grow = 1.f;
            n->Ellipsis = true;
            wchar_t pct[32];
            swprintf_s(pct, L"%d%%", 10 + i * 15);
            auto p = L(pct, Type::Caption1);
            row->Add(n);
            row->Add(p);
            auto bar = std::make_shared<Progress>(0.1f + 0.15f * i);
            box->Add(row);
            box->Add(bar);
            rows->Add(box);
        }
        listScroll->Add(rows);
        content->Add(listScroll);
    };

    // 页面 3：设置（子导航：通用 / 外观 / Java / 下载 / 关于）
    auto BuildSettings = [&]() {
        sub->Visible = true;
        sub->Clear();
        auto n1 = std::make_shared<NavItem>(L"通用", L"\xE713");
        n1->Active = true;
        sub->Add(n1);
        sub->Add(std::make_shared<NavItem>(L"外观", L"\xE771"));
        sub->Add(std::make_shared<NavItem>(L"Java 运行时", L"\xE943"));
        sub->Add(std::make_shared<NavItem>(L"下载", L"\xE896"));
        sub->Add(std::make_shared<NavItem>(L"关于", L"\xE946"));

        content->Add(Section(L"启动器"));
        content->Add(FormRow(L"语言", L"简体中文"));
        content->Add(std::make_shared<Divider>());
        content->Add(FormRow(L"启动器版本", L"0.1.0  (dev)"));
        content->Add(std::make_shared<Divider>());
        auto fs = HBox(16.f);
        fs->H = 48.f;
        fs->Cross = Stack::Center;
        auto lab = L(L"启动器启动时检查更新", Type::Body1);
        lab->W = 180.f;
        auto sw = std::make_shared<Switch>(true);
        fs->Add(lab);
        fs->Add(sw);
        content->Add(fs);

        content->Add(Section(L"游戏默认值"));
        content->Add(FormRow(L"默认内存", L"4096 MB"));
        content->Add(std::make_shared<Divider>());
        content->Add(FormRow(L"默认游戏目录", L"版本目录（隔离）"));
    };

    int curPage = -1;
    auto ShowPage = [&](int idx) {
        if (idx == curPage) return;
        curPage = idx;
        content->Clear();
        switch (idx) {
        case 0: BuildLaunch(); break;
        case 1: BuildVersions(); break;
        case 2: BuildDownload(); break;
        default: BuildSettings(); break;
        }
        app.Invalidate();
    };
    bar->NavChanged = [&](int i) { ShowPage(i); };

    app.SetRoot(root);
    ShowPage(startPage);
    bar->NavIndex = startPage;
    return app.Run();
}
