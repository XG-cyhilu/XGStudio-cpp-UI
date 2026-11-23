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

    // 顶栏：标题 + 导航 + 窗口按钮
    auto bar = std::make_shared<TitleBar>();
    bar->Id = L"titlebar";
    bar->H = 48.f;
    bar->Title = L"Void Craft";
    bar->NavItems = { L"启动", L"版本", L"下载", L"设置" };
    root->Add(bar);

    auto body = HBox(0.f); body->Id = L"body"; body->Grow = 1.f;

    // 左侧子导航（版本页可见）
    auto sub = VBox(4.f); sub->Id = L"subnav"; sub->W = 220.f; sub->Pad(12.f);
    for (int i = 0; i < 3; ++i) {
        auto n = std::make_shared<NavItem>(L"子导航", L"\xE8F4");
        n->Active = (i == 0);
        sub->Add(n);
    }
    body->Add(sub);

    // 内容区（平坦：无卡片，标题 + 表单行 + 日志滚动）
    auto content = VBox(0.f); content->Id = L"content"; content->Grow = 1.f; content->Pad(28.f);

    auto head = HBox(16.f); head->Id = L"head"; head->H = 52.f; head->Cross = Stack::Center;
    auto ver = std::make_shared<Label>(L"1.20.4", 32.f, 600); ver->Grow = 1.f;
    head->Add(ver);
    auto btn = std::make_shared<Button>(L"启动游戏", true); btn->Id = L"play"; btn->W = 160.f;
    head->Add(btn);
    content->Add(head);

    content->Add(std::make_shared<Divider>());
    for (int i = 0; i < 4; ++i) {
        auto row = HBox(16.f); row->Id = L"formrow"; row->H = 48.f; row->Cross = Stack::Center;
        auto l = std::make_shared<Label>(L"Java", 14.f, 400); l->W = 180.f;
        auto v = std::make_shared<Label>(L"17.0.9 (x64)", 14.f, 600); v->Grow = 1.f; v->Ellipsis = true;
        row->Add(l);
        row->Add(v);
        content->Add(row);
        content->Add(std::make_shared<Divider>());
    }

    auto logScroll = std::make_shared<ScrollView>(); logScroll->Id = L"logscroll"; logScroll->Grow = 1.f;
    auto logRows = VBox(2.f); logRows->Id = L"logrows";
    for (int i = 0; i < 30; ++i)
        logRows->Add(std::make_shared<Label>(L"[INFO] assets/objects/00/0001 正常", 12.f, 400));
    logScroll->Add(logRows);
    content->Add(logScroll);

    body->Add(content);
    root->Add(body);

    D2D1_RECT_F all{ 0, 0, W, H };
    root->Arrange(all);
    Dump(root.get(), 0, false, all);
    std::printf("\n越界节点：%d\n", g_bad);
    return g_bad == 0 ? 0 : 2;
}
