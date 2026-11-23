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

// 自绘边框 + 标题栏 + 网格 + 滚动条 + 省略号，一套"启动器主界面"的排版打样。
// 用法：VCUIFrameDemo [dark]

#include <windows.h>
#include <vcui/vcui.h>

using namespace vcui;

static std::shared_ptr<Widget> MakeInfoCard(const wchar_t* title, const wchar_t* value) {
    auto card = std::make_shared<Card>();
    card->H = 104.f;
    auto box = VBox(6.f);

    auto l1 = std::make_shared<Label>(title, 12.f, 400);
    auto l2 = std::make_shared<Label>(value, 22.f, 600);
    box->Add(l1);
    box->Add(l2);
    card->Add(box);
    return card;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    bool dark = GetEnvironmentVariableW(L"VCUI_DEMO_DARK", nullptr, 0) > 0;

    Surface app;
    app.SetMinSize(980, 620);
    app.SetCustomFrame(true);        // 无边框：客户区吃掉整窗，标题栏自己画
    if (!app.Create(L"VCUI Frame Demo", 1180, 740, dark)) return 1;

    auto root = std::make_shared<Widget>();

    // ---- 标题栏 ----
    auto bar = std::make_shared<TitleBar>();
    bar->Title = L"Void Craft Launcher";
    bar->Fill = true;
    bar->H = 40.f;
    root->Add(bar);

    // ---- 下面：左栏 + 主区 ----
    auto body = HBox(0.f);
    body->Grow = 1.f;

    // 左栏
    auto side = std::make_shared<Widget>();
    side->W = 240.f;
    auto sideBox = VBox(0.f);
    sideBox->Pad(12.f);

    auto folders = std::make_shared<Dropdown>(std::vector<std::wstring>{ L"主目录", L"D:\\Games\\MC-1.20" }, 0);
    folders->H = 34.f;
    sideBox->Add(folders);
    sideBox->Add(Spacer(10.f));

    auto list = std::make_shared<ScrollView>();
    list->Grow = 1.f;
    auto rows = VBox(0.f);
    for (int i = 0; i < 18; ++i) {
        wchar_t buf[96];
        swprintf_s(buf, L"%d.20.4-Fabric 0.15.11-性能优化整合包", 20 - (i % 6));
        auto row = std::make_shared<ListRow>(buf, L"1.20.4", L"2h");
        row->H = 56.f;
        row->Selected = (i == 0);
        rows->Add(row);
    }
    list->Add(rows);
    sideBox->Add(list);

    auto addBtn = std::make_shared<Button>(L"新建实例", true);
    addBtn->H = 36.f;
    addBtn->MT = 12.f;
    sideBox->Add(addBtn);

    side->Add(sideBox);

    // 主区
    auto main = std::make_shared<Widget>();
    main->Grow = 1.f;
    auto page = VBox(12.f);
    page->Pad(24.f);

    auto hero = std::make_shared<Card>();
    hero->H = 180.f;
    auto heroBox = VBox(8.f);
    heroBox->Cross = Stack::Start;
    auto ver = std::make_shared<Label>(L"1.20.4", 32.f, 600);
    auto meta = std::make_shared<Label>(L"Fabric 0.15.11  ·  上次游玩 2 小时前", 14.f, 400);
    meta->Ellipsis = true;   // 版本名过长时尾部省略
    heroBox->Add(ver);
    heroBox->Add(meta);
    hero->Add(heroBox);

    auto grid = std::make_shared<Grid>(2, 12.f);
    grid->Add(MakeInfoCard(L"Java", L"17.0.9  (x64)"));
    grid->Add(MakeInfoCard(L"内存", L"4096 MB"));
    grid->Add(MakeInfoCard(L"游戏目录", L"版本隔离"));
    grid->Add(MakeInfoCard(L"加载器", L"Fabric"));

    page->Add(hero);
    page->Add(grid);

    auto logCard = std::make_shared<Card>();
    logCard->Grow = 1.f;
    auto log = std::make_shared<ScrollView>();
    auto logText = VBox(2.f);
    for (int i = 0; i < 40; ++i) {
        wchar_t buf[128];
        swprintf_s(buf, L"[%02d:%02d:%02d] [INFO] 正在校验资源文件 assets/objects/%02x/%04d", 12, i, i % 60, i % 256, i);
        auto line = std::make_shared<Label>(buf, 12.f, 400);
        line->Ellipsis = true;
        logText->Add(line);
    }
    log->Add(logText);
    logCard->Add(log);
    page->Add(logCard);

    main->Add(page);

    body->Add(side);
    body->Add(main);
    root->Add(body);

    app.SetRoot(root);
    return app.Run();
}
