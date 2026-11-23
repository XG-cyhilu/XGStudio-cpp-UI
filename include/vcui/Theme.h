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

#pragma once
#include "vcui/Color.h"

namespace vcui {

// 一套设计语言（Fluent 2 底座），度量参数化，换皮只改这里。
struct VCUI_API Theme {
    Palette pal = Palette::Microsoft;
    bool dark = false;
    D2D1_COLOR_F accent = Hex(0x0071EA);
    Tokens t{};
    Metrics m{};

    void Refresh();
    void Set(Palette p, bool isDark);
    void SetDark(bool isDark) { Set(pal, isDark); }
    void SetAccent(D2D1_COLOR_F a);
    void ToggleDark() { dark = !dark; Refresh(); }
};

} // namespace vcui
