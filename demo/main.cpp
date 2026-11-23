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

#include <windows.h>
#include <memory>
#include "vcui/vcui.h"

using namespace vcui;

namespace {

std::shared_ptr<Widget> Title(const std::wstring& s) {
    return std::make_shared<Label>(s, 13.f, 600);
}

std::shared_ptr<Widget> CardBox(const std::wstring& title, std::shared_ptr<Widget> body) {
    auto card = std::make_shared<Card>();
    auto box = VBox(14.f);
    box->Add(Title(title));
    box->Add(body);
    card->Add(box);
    return card;
}

std::shared_ptr<Widget> Row(std::vector<std::shared_ptr<Widget>> items, float h, float gap = 12.f) {
    auto row = HBox(gap);
    row->H = h;
    for (auto& i : items) row->Add(i);
    return row;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    Surface app;
    if (!app.Create(L"XGStudio-cpp-UI — Controls", 1180, 840, false)) return 1;

    // 启动状态可由环境变量指定，便于截图与无头复现：
    //   VCUI_DEMO_DARK=1        直接进深色
    //   VCUI_DEMO_PALETTE=github 用 GitHub 色板
    auto env = [](const wchar_t* name) {
        wchar_t buf[64]{};
        GetEnvironmentVariableW(name, buf, 64);
        return std::wstring(buf);
    };
    const bool startDark = env(L"VCUI_DEMO_DARK") == L"1";
    const bool startGh = env(L"VCUI_DEMO_PALETTE") == L"github";

    auto root = VBox(0.f);

    // ---- 顶栏 ----
    auto header = HBox(16.f);
    header->H = 74.f;
    header->Cross = Stack::Center;
    header->PL = 24.f; header->PR = 24.f; header->PT = 16.f; header->PB = 12.f;

    auto brand = VBox(2.f);
    brand->Add(std::make_shared<Label>(L"XGStudio-cpp-UI", 24.f, 600));
    brand->Add(std::make_shared<Label>(L"控件库 · Fluent 2 · Direct2D 自绘", 12.f, 400));
    header->Add(brand);
    header->Add(Spacer());

    auto themeSeg = std::make_shared<Segmented>(std::vector<std::wstring>{ L"浅色", L"深色" }, startDark ? 1 : 0);
    themeSeg->W = 148.f;
    header->Add(themeSeg);

    auto palBtn = std::make_shared<Button>(startGh ? L"色板：GitHub" : L"色板：Microsoft", false);
    palBtn->W = 156.f;
    header->Add(palBtn);

    root->Add(header);

    // ---- 主体 ----
    auto body = HBox(0.f);
    body->Basis = 0.f;
    body->Grow = 1.f;
    root->Add(body);

    auto side = VBox(2.f);
    side->W = 216.f;
    side->PL = 12.f; side->PR = 12.f; side->PT = 8.f;

    const wchar_t* navText[] = { L"导航", L"下载", L"版本", L"整合包", L"设置" };
    const wchar_t* navGlyph[] = { L"\uE700", L"\uE896", L"\uE81C", L"\uE7B8", L"\uE713" };
    std::vector<std::shared_ptr<NavItem>> navItems;
    for (int i = 0; i < 5; ++i) {
        auto n = std::make_shared<NavItem>(navText[i], navGlyph[i]);
        n->Active = (i == 0);
        navItems.push_back(n);
        side->Add(n);
    }
    for (int i = 0; i < 5; ++i) {
        navItems[i]->Clicked = [&app, navItems, i]() {
            for (auto& n : navItems) n->Active = false;
            navItems[i]->Active = true;
            app.Invalidate();
        };
    }

    side->Add(std::make_shared<Divider>());
    auto hint = std::make_shared<Label>(L"控件全部自绘，无系统控件", 11.f, 400);
    hint->PL = 4.f;
    side->Add(hint);

    body->Add(side);

    auto scroll = std::make_shared<ScrollView>();
    scroll->Grow = 1.f;
    scroll->Basis = 0.f;
    body->Add(scroll);

    auto content = VBox(16.f);
    content->PL = 12.f; content->PR = 12.f; content->PT = 8.f; content->PB = 24.f;
    scroll->Add(content);

    // 1. 按钮
    {
        auto b1 = std::make_shared<Button>(L"默认按钮");
        auto b2 = std::make_shared<Button>(L"主色按钮", true);
        auto b3 = std::make_shared<Button>(L"禁用");
        b3->Enabled = false;
        auto ib1 = std::make_shared<IconButton>(L"\uE72C");
        auto ib2 = std::make_shared<IconButton>(L"\uE710", false);
        content->Add(CardBox(L"按钮", Row({ b1, b2, b3, Sizer(8.f), ib1, ib2 }, 36.f)));
    }

    // 2. 文本框
    {
        auto box = VBox(10.f);
        auto i1 = std::make_shared<Input>(L"搜索整合包、Mod、版本…");
        i1->W = 420.f;
        auto i2 = std::make_shared<Input>();
        i2->Text = L"Fabric 0.16.5";
        i2->W = 320.f;
        box->Add(Row({ i1, i2 }, 34.f));
        box->Add(std::make_shared<Label>(L"获得焦点后出现蓝色描边与光标，支持方向键、Backspace、Delete。", 12.f, 400));
        content->Add(CardBox(L"文本框", box));
    }

    // 3. 选择与开关
    {
        auto box = VBox(12.f);
        auto c1 = std::make_shared<Checkbox>(L"启用自动更新", true);
        auto c2 = std::make_shared<Checkbox>(L"仅显示稳定版本", false);
        auto sw = std::make_shared<Switch>(true);
        auto seg = std::make_shared<Segmented>(std::vector<std::wstring>{ L"全部", L"已安装", L"可更新" }, 1);
        seg->W = 260.f;
        box->Add(Row({ c1, c2 }, 28.f, 24.f));
        box->Add(Row({ std::make_shared<Label>(L"液态玻璃开关", 13.f, 400), sw, Sizer(16.f), seg }, 34.f, 16.f));
        content->Add(CardBox(L"选择与开关", box));
    }

    // 4. 进度与状态
    {
        auto box = VBox(12.f);
        auto p1 = std::make_shared<Progress>(.72f);
        p1->H = 6.f;
        auto p2 = std::make_shared<Progress>(.30f);
        p2->H = 6.f;
        auto badges = Row({ std::make_shared<Badge>(L"稳定版", Badge::Success),
                            std::make_shared<Badge>(L"Beta", Badge::Warning),
                            std::make_shared<Badge>(L"不兼容", Badge::Danger),
                            std::make_shared<Badge>(L"新版", Badge::Accent),
                            std::make_shared<Badge>(L"1.20.1", Badge::Neutral) }, 20.f, 8.f);
        auto line = Row({ std::make_shared<Label>(L"下载进度", 12.f, 400), Spacer(),
                          std::make_shared<Label>(L"72%", 12.f, 600) }, 18.f);
        box->Add(line);
        box->Add(p1);
        box->Add(p2);
        box->Add(badges);
        content->Add(CardBox(L"进度与状态", box));
    }

    // 5. 列表
    {
        auto card = std::make_shared<Card>();
        card->PL = 8.f; card->PT = 8.f; card->PR = 8.f; card->PB = 8.f;
        auto box = VBox(10.f);
        auto title = Title(L"列表行");
        title->PL = 8.f;
        box->Add(title);
        auto list = VBox(0.f);
        list->Add(std::make_shared<ListRow>(L"Fabric API", L"0.100.0 · 已安装", L"启用"));
        list->Add(std::make_shared<ListRow>(L"Sodium", L"0.6.1 · 已启用", L"1.20.1"));
        auto sel = std::make_shared<ListRow>(L"Lithium", L"0.12.3 · 已启用", L"1.20.1");
        sel->Selected = true;
        list->Add(sel);
        list->Add(std::make_shared<ListRow>(L"Create", L"0.5.7 · 需要依赖", L"1.20.1"));
        box->Add(list);
        card->Add(box);
        content->Add(card);
    }

    // 6. 下拉与滑块
    {
        auto box = VBox(12.f);
        auto dd = std::make_shared<Dropdown>(std::vector<std::wstring>{
            L"Fabric 0.16.5", L"Quilt 0.26.0", L"Forge 47.3.0", L"NeoForge 21.1.80" }, 0);
        dd->W = 240.f;
        auto dd2 = std::make_shared<Dropdown>(std::vector<std::wstring>{ L"简体中文", L"English" }, 0);
        dd2->W = 140.f;
        auto sl = std::make_shared<Slider>(.42f);
        sl->Grow = 1.f;
        sl->Basis = 0.f;
        auto sl2 = std::make_shared<Slider>(.80f);
        sl2->Grow = 1.f;
        sl2->Basis = 0.f;
        box->Add(Row({ dd, dd2 }, 34.f));
        box->Add(Row({ std::make_shared<Label>(L"内存分配", 12.f, 400), sl }, 26.f));
        box->Add(Row({ std::make_shared<Label>(L"渲染距离", 12.f, 400), sl2 }, 26.f));
        content->Add(CardBox(L"下拉与滑块", box));
    }

    themeSeg->Changed = [&app](int i) {
        app.SetDark(i == 1);
        app.Invalidate();
    };
    static bool gh = startGh;
    palBtn->Clicked = [&app, palBtn]() {
        gh = !gh;
        app.SetTheme(gh ? Palette::GitHub : Palette::Microsoft, app.IsDark());
        palBtn->Text = gh ? L"色板：GitHub" : L"色板：Microsoft";
        app.Invalidate();
    };

    app.SetTheme(startGh ? Palette::GitHub : Palette::Microsoft, startDark);
    app.SetRoot(root);


    return app.Run();
}
