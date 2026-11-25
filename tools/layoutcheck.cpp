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
#include <cmath>
#include <cstdlib>

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

    // 回归：连续两次入场动画不能把终点写成 0；错峰延迟必须递增；缓动端点与单调性正确
    content->StaggerIn();
    float prevDelay = -1.f, firstDelay = -1.f, lastDelay = 0.f;
    int staggered = 0;
    for (auto& k : content->Children()) {
        if (k->AnimT < 0.f) continue;
        if (firstDelay < 0.f) firstDelay = k->AnimDelay;
        if (k->AnimDelay + 1e-4f < prevDelay) { std::printf("错峰延迟乱序：%.3f 在 %.3f 之后\n", k->AnimDelay, prevDelay); g_bad++; }
        prevDelay = k->AnimDelay;
        lastDelay = (std::max)(lastDelay, k->AnimDelay);
        ++staggered;
    }
    for (int i = 0; i <= 120; ++i) root->Update(1.f / 60.f);   // 跑到所有项都结束
    bool animOk = true;
    for (auto& k : content->Children())
        if (k->AnimT >= 0.f || k->Opacity < .99f) animOk = false;
    float e0 = EaseDecelerate(0.f), e1 = EaseDecelerate(1.f), e5 = EaseDecelerate(.5f);
    bool easeOk = std::abs(e0) < 1e-3f && std::abs(e1 - 1.f) < 1e-3f && e5 > .5f;
    for (int i = 1; i <= 100; ++i)
        if (EaseDecelerate(i / 100.f) < EaseDecelerate((i - 1) / 100.f) - 1e-4f) easeOk = false;
    std::printf("入场回归：%d 项错峰，首次延迟=%.2fs 末次=%.2fs；缓动 e(0)=%.2f e(.5)=%.2f e(1)=%.2f %s\n",
                staggered, firstDelay < 0 ? 0.f : firstDelay, lastDelay, e0, e5, e1,
                (animOk && easeOk && staggered > 0) ? "OK" : "FAIL");

    // 虚拟列表回归：六万条也只该存在视口内的十几行；滚动/跳转后行数与索引仍然对得上
    int vlBad = 0;
    auto vl = std::make_shared<VirtualList>();
    vl->Id = L"virtlist";
    vl->H = 400.f;
    vl->RowHeight = 56.f;
    vl->Gap = 6.f;
    vl->ItemCount = 60000;
    int built = 0;
    vl->Build = [&built](int i) {
        ++built;
        auto r = std::make_shared<ListRow>(L"item", L"sub", L"");   // 内容交给 Bind 填，Build 只搭结构
        r->Id = L"vrow";
        return r;
    };
    vl->Bind = [](Widget& w, int i) {
        if (auto* lr = dynamic_cast<ListRow*>(&w)) lr->Right = std::to_wstring(i);
        return true;
    };
    vl->Arrange(D2D1_RECT_F{ 0, 0, 900, 400 });
    int live = vl->LiveRows();
    int expectMax = (int)std::ceil(400.f / 62.f) + 2 * vl->Overscan + 2;
    std::printf("虚拟列表：总条数=60000 实建=%d 存活=%d 首行=%d 末行=%d 内容高=%.0f\n",
                built, live, vl->FirstVisible(), vl->LastVisible(), vl->ContentHeight());
    if (live > expectMax) { std::printf("  存活行超过视口应有数量（%d > %d）\n", live, expectMax); ++vlBad; }
    if (vl->FirstVisible() < 0 || vl->LastVisible() < vl->FirstVisible()) { std::printf("  可见区间无效\n"); ++vlBad; }
    // 滚到一半：行数不涨，复用计数上涨，行里绑的是新索引
    vl->ScrollToIndex(30000);
    for (int i = 0; i < 60; ++i) vl->Update(1.f / 60.f);
    int live2 = vl->LiveRows();
    std::printf("  跳到第 30000 条后：存活=%d 累计新建=%d 复用=%d 首行=%d\n",
                live2, built, vl->RecycledCount(), vl->FirstVisible());
    if (live2 > expectMax) { std::printf("  滚动后存活行失控\n"); ++vlBad; }
    if (vl->RecycledCount() <= 0 || built > expectMax + 6) {
        std::printf("  没有复用旧行（新建=%d 复用=%d）\n", built, vl->RecycledCount());
        ++vlBad;
    }
    bool boundOk = true;
    for (auto& k : vl->Children()) {
        if (!k->Visible) continue;
        auto* lr = dynamic_cast<ListRow*>(k.get());
        if (!lr) { boundOk = false; continue; }
        // 复用后行内容必须换成新索引：Right 应该落在当前可见区间内
        wchar_t* end = nullptr;
        long v = std::wcstol(lr->Right.c_str(), &end, 10);
        if (end == lr->Right.c_str() || v < vl->FirstVisible() || v > vl->LastVisible()) boundOk = false;
    }
    if (!boundOk) { std::printf("  复用行的内容没跟着索引更新\n"); ++vlBad; }
    // 滚动到底：最后一行必须落在内容高度之内
    vl->EnsureVisible(vl->ItemCount - 1);
    for (int i = 0; i < 120; ++i) vl->Update(1.f / 60.f);
    int lastIdx = vl->LastVisible();
    std::printf("  滚到底：末行=%d（应为 %d）滚动偏移=%.0f/%.0f\n",
                lastIdx, vl->ItemCount - 1, vl->ScrollY, vl->MaxScroll());
    if (lastIdx != vl->ItemCount - 1 || std::abs(vl->ScrollY - vl->MaxScroll()) > 1.f) {
        std::printf("  滚到底没对齐\n");
        ++vlBad;
    }
    std::printf("虚拟列表回归：%s\n", vlBad == 0 ? "OK" : "FAIL");

    return (g_bad == 0 && animOk && easeOk && staggered > 0 && vlBad == 0) ? 0 : 2;
}


