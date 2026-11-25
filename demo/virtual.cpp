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

// 虚拟列表演示：一百万条也只建视口里那十几行，滚轮 / 拖动滚动条 / Home / End 都能用。
// 顶部那行统计是"存活行数 · 累计新建 · 累计复用"，用来肉眼确认行数没有随数据量增长。
#include <windows.h>
#include "vcui/vcui.h"

using namespace vcui;

// 演示用的每帧钩子（正式界面别为了统计数字强行保持 60fps）
struct Ticker : Widget {
    std::function<bool(float)> Fn;
    bool Update(float dt) override { return Fn ? Fn(dt) : false; }
};

static int g_built = 0, g_reused = 0;

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    return CreateAppWindow(L"XGStudio-cpp-UI — VirtualList", 860, 620, [](Widget& body) {
        auto page = VBox(10.f);
        page->Pad(20.f);

        auto head = HBox(12.f);
        head->Add(std::make_shared<Label>(L"VirtualList", 20.f, 600));
        auto stat = std::make_shared<Label>(L"", 12.f, 400);
        stat->Grow = 1.f;
        stat->Align = HAlign::Right;
        head->Add(stat);
        page->Add(head);

        auto list = std::make_shared<VirtualList>();
        list->RowHeight = 64.f;
        list->Gap = 6.f;
        list->ItemCount = 1000000;
        list->Grow = 1.f;

        list->Build = [](int i) {
            ++g_built;
            auto row = std::make_shared<ListRow>(L"条目 #" + std::to_wstring(i + 1),
                                                 L"这一行是虚拟化出来的，滚走就回收",
                                                 std::to_wstring(i + 1));
            row->Id = L"row";
            return row;
        };
        list->Bind = [](Widget& w, int i) {
            ++g_reused;
            auto* r = dynamic_cast<ListRow*>(&w);
            if (!r) return false;
            r->Title = L"条目 #" + std::to_wstring(i + 1);
            r->Right = std::to_wstring(i + 1);
            return true;
        };
        list->AnimateIn();

        auto bar = std::make_shared<Progress>(0.f);
        bar->H = 3.f;

        auto tick = std::make_shared<Ticker>();
        tick->Fn = [list, stat, bar](float) {
            float maxS = list->MaxScroll();
            bar->Value = maxS > .5f ? list->ScrollY / maxS : 0.f;
            stat->Text = L"存活行 " + std::to_wstring(list->LiveRows()) +
                         L" · 新建 " + std::to_wstring(g_built) +
                         L" · 复用 " + std::to_wstring(g_reused) +
                         L" · 第 " + std::to_wstring(list->FirstVisible() + 1) + L" 行起";
            return true;
        };

        page->Add(list);
        page->Add(bar);
        page->Add(tick);
        body.Add(page);
    });
}
