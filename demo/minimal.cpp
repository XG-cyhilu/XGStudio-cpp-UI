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

// 最小示例：CreateAppWindow 一行起窗口（DPI、自绘边框、标题栏都由库包好），
// 回调里拿到的 body 就是标题栏下面的内容区。
#include <windows.h>
#include "vcui/vcui.h"

using namespace vcui;

static void BuildPage(Widget& body) {
    auto page = VBox(14.f);
    page->Pad(24.f);

    page->Add(std::make_shared<Label>(L"XGStudio-cpp-UI", 20.f, 600));
    page->Add(std::make_shared<Label>(L"一个窗口，一棵控件树，没有系统控件。", 12.f, 400));

    auto card = std::make_shared<Card>();
    auto box = VBox(12.f);
    box->Add(std::make_shared<Label>(L"输入", 12.f, 600));
    box->Add(std::make_shared<Input>(L"输入点什么…"));

    auto row = HBox(10.f);
    row->Add(std::make_shared<Switch>(true));
    auto ok = std::make_shared<Button>(L"确定", true);
    ok->W = 96.f;
    row->Add(ok);
    box->Add(row);
    card->Add(box);

    page->Add(card);
    body.Add(page);
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    // 只要窗口、不要内容时，连回调都可以省：
    //     return CreateAppWindow(L"空白窗口", 720, 480);
    return CreateAppWindow(L"XGStudio-cpp-UI — Minimal", 520, 340, BuildPage);
}
