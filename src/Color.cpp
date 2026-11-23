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

#include "vcui/Color.h"

#include <cmath>

namespace vcui {

Tokens GetTokens(Palette p, bool dark) {
    Tokens t;
    t.dark = dark;
    if (p == Palette::Microsoft) {
        if (dark) { t.kp = Hex(0x2b2b2b); t.bj = Hex(0x202020); t.xt = Hex(0x2d2d2d); t.wz = Hex(0xf7f7f7); }
        else { t.kp = Hex(0xfbfbfb); t.bj = Hex(0xf3f3f3); t.xt = Hex(0xeaeaea); t.wz = Hex(0x1b1b1b); }
    } else {
        if (dark) { t.kp = Hex(0x0c1117); t.bj = Hex(0x000409); t.xt = Hex(0x1e242a); t.wz = Hex(0xeef6fc); }
        else { t.kp = Hex(0xffffff); t.bj = Hex(0xf5f8fa); t.xt = Hex(0xf2f3f5); t.wz = Hex(0x1e2328); }
    }

    t.accent = Hex(0x0071EA);
    t.onAccent = Luma(t.accent) > .58f ? Hex(0x0b0b0c) : Hex(0xffffff);
    t.accentHover = Lighten(t.accent, .08f);
    t.accentPress = Darken(t.accent, .10f);
    t.accentSoft = A(t.accent, .12f);
    t.accentSel = A(t.accent, .18f);

    t.text2 = Mix(t.wz, t.bj, .45f);
    t.text3 = Mix(t.wz, t.bj, .65f);
    t.border = dark ? Overlay(t, .14f) : Mix(t.wz, t.kp, .84f);
    t.divider = dark ? Overlay(t, .08f) : Mix(t.wz, t.kp, .90f);
    t.pressed = Mix(t.xt, t.wz, dark ? .08f : .07f);
    t.panel = Mix(t.kp, t.bj, .14f);
    return t;
}

Metrics GetMetrics() {
    Metrics m;
    m.radiusBtn = 2; m.radiusInput = 2; m.radiusCard = 8; m.radiusPanel = 8;
    m.controlH = 34; m.rowH = 44; m.gap = 12; m.padCard = 24;
    m.fCaption = 10; m.fLabel = 12; m.fBody = 14; m.fSub = 20; m.fHeading = 28; m.fTitle = 32;
    m.borderW = 1.f;
    m.family = L"Microsoft YaHei";
    return m;
}

namespace Type {
const TypeStyle Caption2        { 10.f, 14.f, 400, L"Caption 2" };
const TypeStyle Caption2Strong  { 10.f, 14.f, 600, L"Caption 2 Strong" };
const TypeStyle Caption1        { 12.f, 16.f, 400, L"Caption 1" };
const TypeStyle Caption1Strong  { 12.f, 16.f, 600, L"Caption 1 Strong" };
const TypeStyle Caption1Stronger{ 12.f, 16.f, 700, L"Caption 1 Stronger" };
const TypeStyle Body1           { 14.f, 20.f, 400, L"Body 1" };
const TypeStyle Body1Strong     { 14.f, 20.f, 600, L"Body 1 Strong" };
const TypeStyle Subtitle2       { 16.f, 22.f, 600, L"Subtitle 2" };
const TypeStyle Subtitle1       { 20.f, 26.f, 600, L"Subtitle 1" };
const TypeStyle Title3          { 24.f, 32.f, 600, L"Title 3" };
const TypeStyle Title2          { 28.f, 36.f, 600, L"Title 2" };
const TypeStyle Title1          { 32.f, 40.f, 600, L"Title 1" };
const TypeStyle LargeTitle      { 40.f, 52.f, 600, L"Large Title" };
const TypeStyle Display         { 68.f, 92.f, 600, L"Display" };
}

TypeStyle TypeFor(float px) {
    static const TypeStyle ramp[] = {
        Type::Caption2, Type::Caption1, Type::Body1, Type::Subtitle2, Type::Subtitle1,
        Type::Title3, Type::Title2, Type::Title1, Type::LargeTitle, Type::Display,
    };
    const TypeStyle* best = &ramp[0];
    float d = 1e9f;
    for (const auto& s : ramp) {
        float cur = std::abs(s.size - px);
        if (cur < d) { d = cur; best = &s; }
    }
    TypeStyle r = *best;
    if (px != r.size && r.size > 0.f) {      // 非标准字号，按比例缩放行高
        float k = px / r.size;
        r.size = px;
        r.lineHeight = r.lineHeight * k;
    }
    r.name = best->name;
    return r;
}

float LineH(float px) { return TypeFor(px).lineHeight; }

} // namespace vcui
