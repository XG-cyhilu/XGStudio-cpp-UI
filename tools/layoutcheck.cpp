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

// 无窗口布局自检：只算几何、不建窗口，用来验证容器分配是否越界。
// 用法：VCUILayoutCheck [宽 高]，输出每个节点的 frame，并标出越界的节点。

#include <vcui/vcui.h>
#include <cstdio>
#include <string>
#include <vector>

using namespace vcui;

static int g_bad = 0;

static void Dump(Widget* w, int depth, bool inScroll, D2D1_RECT_F parent) {
    D2D1_RECT_F f = w->Frame();
    bool outW = !inScroll && (f.right > parent.right + .5f || f.left < parent.left - .5f);
    bool outH = !inScroll && (f.bottom > parent.bottom + .5f || f.top < parent.top - .5f);
    if (outW || outH) ++g_bad;
    std::wstring ind(depth * 2, L' ');
    std::printf("%ls%-22ls frame=(%.1f,%.1f)-(%.1f,%.1f) %s%s\n",
                ind.c_str(), w->Id.empty() ? L"?" : w->Id.c_str(),
                f.left, f.top, f.right, f.bottom,
                outW ? "OVF-X" : "", outH ? "OVF-Y" : "");
    // 滚动容器里的内容本来就可以比视口高，不算越界
    bool scroll = dynamic_cast<ScrollView*>(w) != nullptr;
    for (auto& k : w->Children()) Dump(k.get(), depth + 1, inScroll || scroll, f);
}

int main(int argc, char** argv) {
    float W = 1180.f, H = 740.f;
    if (argc >= 3) { W = (float)atof(argv[1]); H = (float)atof(argv[2]); }

    auto root = VBox(0.f); root->Id = L"root";

    auto bar = std::make_shared<TitleBar>();
    bar->Id = L"titlebar";
    bar->H = 40.f;
    root->Add(bar);

    auto body = HBox(0.f); body->Id = L"body"; body->Grow = 1.f;

    auto side = VBox(0.f); side->Id = L"side"; side->W = 240.f;
    auto sideBox = VBox(0.f); sideBox->Id = L"sideBox"; sideBox->Grow = 1.f; sideBox->Pad(12.f);
    auto folders = std::make_shared<Dropdown>(std::vector<std::wstring>{ L"a", L"b" }, 0);
    folders->Id = L"folders";
    sideBox->Add(folders);
    auto list = std::make_shared<ScrollView>(); list->Id = L"list"; list->Grow = 1.f;
    auto rows = VBox(0.f); rows->Id = L"rows";
    for (int i = 0; i < 18; ++i) {
        auto row = std::make_shared<ListRow>(L"1.20.4-Fabric", L"1.20.4", L"2h");
        row->H = 56.f;
        rows->Add(row);
    }
    list->Add(rows);
    sideBox->Add(list);
    side->Add(sideBox);
    body->Add(side);

    auto mainV = VBox(0.f); mainV->Id = L"main"; mainV->Grow = 1.f;
    auto page = VBox(12.f); page->Id = L"page"; page->Grow = 1.f; page->Pad(24.f);

    auto hero = std::make_shared<Card>(); hero->Id = L"hero"; hero->H = 180.f;
    auto heroBox = VBox(8.f);
    heroBox->Add(std::make_shared<Label>(L"1.20.4", 32.f, 600));
    heroBox->Add(std::make_shared<Label>(L"Fabric 0.15.11", 14.f, 400));
    hero->Add(heroBox);
    page->Add(hero);

    auto grid = std::make_shared<Grid>(2, 12.f); grid->Id = L"grid";
    for (int i = 0; i < 4; ++i) {
        auto card = std::make_shared<Card>(); card->Id = L"card"; card->H = 104.f;
        card->Add(VBox(6.f));   // 空内容
        grid->Add(card);
    }
    page->Add(grid);

    auto logCard = std::make_shared<Card>(); logCard->Id = L"logCard"; logCard->Grow = 1.f;
    logCard->Add(std::make_shared<Label>(L"log", 12.f, 400));
    page->Add(logCard);

    mainV->Add(page);
    body->Add(mainV);
    root->Add(body);

    D2D1_RECT_F all{ 0, 0, W, H };
    root->Arrange(all);
    Dump(root.get(), 0, false, all);
    std::printf("\n越界节点：%d\n", g_bad);
    return g_bad == 0 ? 0 : 2;
}
