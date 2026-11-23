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
#include <functional>
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
    // 子导航点一下要真的换内容，所以按 tab 重建：sub 列表 + content 正文
    int verTab = 0;
    std::function<void(int)> ShowVersions;
    ShowVersions = [&](int tab) {
        verTab = tab;
        sub->Visible = true;
        sub->Clear();
        const wchar_t* names[3] = { L"已安装", L"可安装", L"整合包" };
        const wchar_t* glyphs[3] = { L"\xE8F4", L"\xE896", L"\xE8F1" };
        for (int i = 0; i < 3; ++i) {
            auto n = std::make_shared<NavItem>(names[i], glyphs[i]);
            n->Active = (i == tab);
            n->Clicked = [&, i]() { if (verTab != i) { ShowVersions(i); app.Invalidate(); } };
            sub->Add(n);
        }

        content->Clear();
        auto head = HBox(12.f);
        head->H = 44.f;
        head->Cross = Stack::Center;
        auto t = L(tab == 0 ? L"已安装的版本" : (tab == 1 ? L"可安装的版本" : L"整合包"),
                  Type::Subtitle1);
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
            std::wstring sub1, right;
            if (tab == 0) {
                swprintf_s(t2, L"1.%d.4-Fabric 0.15.11-性能优化整合包", 16 + i % 5);
                sub1 = L"1.20.4  ·  Fabric 0.15.11";
                right = (i == 0 ? L"已选择" : L"3 天前");
            } else if (tab == 1) {
                swprintf_s(t2, L"1.%d.%d  ·  正式版", 16 + i % 6, i % 3);
                sub1 = L"未安装  ·  约 1.2 GB";
                right = L"下载";
            } else {
                swprintf_s(t2, L"流萤 1.20.1 整合包 v%d", 1 + i % 4);
                sub1 = L"207+ Mod  ·  已安装";
                right = L"打开";
            }
            auto row = std::make_shared<ListRow>(t2, sub1, right);
            row->H = 52.f;
            row->Selected = (tab == 0 && i == 0);
            rows->Add(row);
            rows->Add(std::make_shared<Divider>());
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
    int setTab = 0;
    std::function<void(int)> ShowSettings;
    ShowSettings = [&](int tab) {
        setTab = tab;
        sub->Visible = true;
        sub->Clear();
        const wchar_t* names[5] = { L"通用", L"外观", L"Java 运行时", L"下载", L"关于" };
        const wchar_t* glyphs[5] = { L"\xE713", L"\xE771", L"\xE943", L"\xE896", L"\xE946" };
        for (int i = 0; i < 5; ++i) {
            auto n = std::make_shared<NavItem>(names[i], glyphs[i]);
            n->Active = (i == tab);
            n->Clicked = [&, i]() { if (setTab != i) { ShowSettings(i); app.Invalidate(); } };
            sub->Add(n);
        }

        content->Clear();
        auto SwitchRow = [&](const std::wstring& label, bool on) {
            auto fs = HBox(16.f);
            fs->H = 48.f;
            fs->Cross = Stack::Center;
            auto lab = L(label, Type::Body1);
            lab->W = 180.f;
            fs->Add(lab);
            fs->Add(std::make_shared<Switch>(on));
            content->Add(fs);
        };

        switch (tab) {
        case 0:
            content->Add(Section(L"启动器"));
            content->Add(FormRow(L"语言", L"简体中文"));
            content->Add(std::make_shared<Divider>());
            content->Add(FormRow(L"启动器版本", L"0.1.0  (dev)"));
            content->Add(std::make_shared<Divider>());
            SwitchRow(L"启动器启动时检查更新", true);
            content->Add(Section(L"游戏默认值"));
            content->Add(FormRow(L"默认内存", L"4096 MB"));
            content->Add(std::make_shared<Divider>());
            content->Add(FormRow(L"默认游戏目录", L"版本目录（隔离）"));
            break;
        case 1:
            content->Add(Section(L"外观"));
            content->Add(FormRow(L"主题", L"跟随系统（浅色）"));
            content->Add(std::make_shared<Divider>());
            content->Add(FormRow(L"界面缩放", L"100%"));
            content->Add(std::make_shared<Divider>());
            SwitchRow(L"窗口动画", true);
            SwitchRow(L"紧凑列表行", false);
            break;
        case 2:
            content->Add(Section(L"Java 运行时"));
            content->Add(FormRow(L"当前版本", L"17.0.9  (x64)"));
            content->Add(std::make_shared<Divider>());
            content->Add(FormRow(L"路径", L"C:\\Program Files\\Java\\jdk-17.0.9"));
            content->Add(std::make_shared<Divider>());
            SwitchRow(L"启动时自动检测 Java", true);
            SwitchRow(L"允许使用系统内置 JRE", false);
            break;
        case 3:
            content->Add(Section(L"下载"));
            content->Add(FormRow(L"并发连接数", L"32"));
            content->Add(std::make_shared<Divider>());
            content->Add(FormRow(L"下载源", L"BMCLAPI（镜像）"));
            content->Add(std::make_shared<Divider>());
            SwitchRow(L"下载完校验 SHA-1", true);
            SwitchRow(L"失败自动重试 3 次", true);
            break;
        default:
            content->Add(Section(L"关于"));
            content->Add(FormRow(L"启动器", L"Void Craft Launcher 0.1.0"));
            content->Add(std::make_shared<Divider>());
            content->Add(FormRow(L"UI 库", L"XGStudio-cpp-UI"));
            content->Add(std::make_shared<Divider>());
            content->Add(FormRow(L"许可证", L"AGPL-3.0"));
            break;
        }
    };

    int curPage = -1;
    auto ShowPage = [&](int idx) {
        if (idx == curPage) return;
        curPage = idx;
        content->Clear();
        switch (idx) {
        case 0: BuildLaunch(); break;
        case 1: ShowVersions(verTab); break;
        case 2: BuildDownload(); break;
        default: ShowSettings(setTab); break;
        }
        app.Invalidate();
    };
    bar->NavChanged = [&](int i) { ShowPage(i); };

    app.SetRoot(root);
    ShowPage(startPage);
    bar->NavIndex = startPage;
    return app.Run();
}
