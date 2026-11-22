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

namespace vcui {

Tokens GetTokens(Palette p, bool dark) {
    Tokens t;
    t.dark = dark;
    if (p == Palette::Microsoft) {
        if (dark) { t.kp = Hex(0x2b2b2b); t.bj = Hex(0x202020); t.xt = Hex(0x2d2d2d); t.jh = Hex(0x323232); t.wz = Hex(0xf7f7f7); }
        else { t.kp = Hex(0xfbfbfb); t.bj = Hex(0xf3f3f3); t.xt = Hex(0xeaeaea); t.jh = Hex(0xededed); t.wz = Hex(0x1b1b1b); }
    } else {
        if (dark) { t.kp = Hex(0x0c1117); t.bj = Hex(0x000409); t.xt = Hex(0x1e242a); t.jh = Hex(0x14191f); t.wz = Hex(0xeef6fc); }
        else { t.kp = Hex(0xffffff); t.bj = Hex(0xf5f8fa); t.xt = Hex(0xf2f3f5); t.jh = Hex(0xe9edf1); t.wz = Hex(0x1e2328); }
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
    m.fLabel = 12; m.fBody = 14; m.fSub = 20; m.fHeading = 28;
    m.borderW = 1.f;
    m.family = L"Microsoft YaHei";
    return m;
}

} // namespace vcui
