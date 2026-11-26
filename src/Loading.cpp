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

#include "vcui/Controls.h"

#include <algorithm>
#include <cmath>

namespace vcui {

// ---------------- Spinner ----------------

D2D1_SIZE_F Spinner::Desired(float availW, float availH) {
    (void)availW;
    (void)availH;
    float s = W > 0 ? W : Size;
    return D2D1::SizeF(s, H > 0 ? H : Size);
}

bool Spinner::Update(float dt) {
    AnimDirty = true;
    if (!Visible) return false;
    phase_ += dt * Speed;
    phase_ = std::fmod(phase_, 1.f);
    if (phase_ < 0.f) phase_ += 1.f;
    return true;      // 转圈要一直保持 60fps，帧循环靠它醒着
}

void Spinner::Paint(PaintCtx& ctx) {
    D2D1_RECT_F fr = Frame();
    float w = fr.right - fr.left, h = fr.bottom - fr.top;
    if (w <= 0.f || h <= 0.f) return;

    int n = (int)(Dots < 3.f ? 3.f : Dots);
    float r = (std::min)(w, h) * .5f - w / (float)n * .5f;
    if (r < 0.f) return;
    float cx = (fr.left + fr.right) * .5f;
    float cy = (fr.top + fr.bottom) * .5f;
    D2D1_COLOR_F c = Color.a > 0.f ? Color : ctx.t.accent;

    // 最亮的一点在 0 号位，随相位整体旋转；亮度按序号递增，转起来就是"追尾"效果
    for (int i = 0; i < n; ++i) {
        float a = (float)i / (float)n + phase_;
        float ang = a * 6.2831853f;
        float alpha = .18f + .82f * ((float)i / (float)(n - 1));
        D2D1_COLOR_F dot = c;
        dot.a *= alpha;
        float rr = w / (float)n * DotScale * .5f;
        ctx.g.Circle(cx + std::cos(ang) * r, cy + std::sin(ang) * r, rr < 1.f ? 1.f : rr, dot);
    }
}

// ---------------- Skeleton ----------------

D2D1_SIZE_F Skeleton::Desired(float availW, float availH) {
    (void)availH;
    float w = W > 0 ? W : availW;
    return D2D1::SizeF(w, H > 0 ? H : 14.f);
}

bool Skeleton::Update(float dt) {
    AnimDirty = true;
    if (!Visible) return false;
    t_ += dt;
    if (t_ > 3600.f) t_ = std::fmod(t_, Period);   // 别让浮点无限增长
    return true;
}

void Skeleton::Paint(PaintCtx& ctx) {
    D2D1_RECT_F fr = Frame();
    if (fr.right <= fr.left || fr.bottom <= fr.top) return;

    D2D1_COLOR_F base = Base;
    if (base.a <= 0.f) base = ctx.t.dark ? C(1, 1, 1, 1) : C(0, 0, 0, 1);
    float peak = ctx.t.dark ? .13f : .09f;
    float k = .5f - .5f * std::cos((t_ + Delay) / (Period > .05f ? Period : .05f) * 6.2831853f);
    base.a = peak * (.55f + .45f * k);
    ctx.g.RoundRect(fr, Radius, base);
}

} // namespace vcui
