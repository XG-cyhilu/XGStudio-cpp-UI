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
#include "vcui/Export.h"

#include <windows.h>
#include <d2d1.h>

namespace vcui {

// ---- 颜色小工具 ----
inline D2D1_COLOR_F C(float r, float g, float b, float a = 1.f) { return D2D1_COLOR_F{ r, g, b, a }; }

inline D2D1_COLOR_F Hex(unsigned rgb, float a = 1.f) {
    return C(((rgb >> 16) & 255) / 255.f, ((rgb >> 8) & 255) / 255.f, (rgb & 255) / 255.f, a);
}

inline D2D1_COLOR_F Mix(D2D1_COLOR_F a, D2D1_COLOR_F b, float k) {
    return C(a.r + (b.r - a.r) * k, a.g + (b.g - a.g) * k,
             a.b + (b.b - a.b) * k, a.a + (b.a - a.a) * k);
}

inline D2D1_COLOR_F A(D2D1_COLOR_F c, float a) { c.a = a; return c; }
inline D2D1_COLOR_F Alpha(D2D1_COLOR_F c, float a) { c.a = a; return c; }
inline D2D1_COLOR_F Lighten(D2D1_COLOR_F c, float k) { return Mix(c, C(1, 1, 1, c.a), k); }
inline D2D1_COLOR_F Darken(D2D1_COLOR_F c, float k) { return Mix(c, C(0, 0, 0, c.a), k); }
inline float Luma(D2D1_COLOR_F c) { return .2126f * c.r + .7152f * c.g + .0722f * c.b; }
inline D2D1_COLOR_F OnColor(D2D1_COLOR_F c) { return Luma(c) > .58f ? Hex(0x0b0b0c) : Hex(0xffffff); }

// ---- 色板 ----
enum class Palette { Microsoft, GitHub };

struct Tokens {
    bool dark = false;
    D2D1_COLOR_F kp{}, bj{}, xt{}, jh{}, wz{};
    D2D1_COLOR_F accent{}, accentHover{}, accentPress{}, onAccent{}, accentSoft{}, accentSel{};
    D2D1_COLOR_F text2{}, text3{}, border{}, divider{}, panel{}, pressed{};
};

// Fluent 2 度量。全部可改，换皮不需要改控件代码。
struct Metrics {
    float radiusBtn = 2, radiusInput = 2, radiusCard = 8, radiusPanel = 8;
    float controlH = 34, rowH = 44, gap = 12, padCard = 24;
    float fLabel = 12, fBody = 14, fSub = 20, fHeading = 28;
    float borderW = 1.f;
    const wchar_t* family = L"Microsoft YaHei";
};

VCUI_API Tokens GetTokens(Palette p, bool dark);
VCUI_API Metrics GetMetrics();

inline D2D1_COLOR_F Overlay(const Tokens& t, float a) { return t.dark ? C(1, 1, 1, a) : C(0, 0, 0, a); }
inline D2D1_COLOR_F Ctrl(const Tokens& t) { return t.dark ? Mix(t.kp, t.wz, .055f) : t.kp; }
inline D2D1_COLOR_F CtrlHover(const Tokens& t) { return t.dark ? Mix(Ctrl(t), t.wz, .06f) : t.xt; }

} // namespace vcui
