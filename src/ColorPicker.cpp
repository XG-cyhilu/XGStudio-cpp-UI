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

namespace {
const float kSwatchH = 30.f;    // 色块 + 输入那一行
const float kDotsH = 22.f;      // 预设色板行

D2D1_COLOR_F HueColor(float h) { return HsvToRgb(h, 1.f, 1.f); }
} // namespace

ColorPicker::ColorPicker() { Init(); }

ColorPicker::ColorPicker(D2D1_COLOR_F c) : Color(c) { Init(); }

void ColorPicker::Init() {
    Interactive = true;
    FocusRing = false;
    hex_ = std::make_shared<Input>(L"#RRGGBB");
    hex_->Text = ToHex(Color);
    hex_->H = kSwatchH;
    hex_->Changed = [this](const std::wstring& s) {
        D2D1_COLOR_F c{};
        if (!ParseHex(s, c)) return;      // 输入还没成型，先不打断
        SetColor(c, true);
    };
    Add(hex_);
    SetColor(Color, false);
}

D2D1_RECT_F ColorPicker::SquareRc() const {
    D2D1_RECT_F c = Content();
    return D2D1::RectF(c.left, c.top, c.right, c.top + SquareH);
}

D2D1_RECT_F ColorPicker::HueRc() const {
    D2D1_RECT_F sq = SquareRc();
    return D2D1::RectF(sq.left, sq.bottom + Spacing, sq.right, sq.bottom + Spacing + HueH);
}

D2D1_RECT_F ColorPicker::HexRc() const {
    D2D1_RECT_F hue = HueRc();
    return D2D1::RectF(hue.left, hue.bottom + Spacing, hue.right, hue.bottom + Spacing + kSwatchH);
}

D2D1_RECT_F ColorPicker::SwatchRc() const {
    D2D1_RECT_F hex = HexRc();
    return D2D1::RectF(hex.left, hex.bottom + Spacing, hex.right, hex.bottom + Spacing + kDotsH);
}

D2D1_SIZE_F ColorPicker::Desired(float availW, float availH) {
    (void)availH;
    float w = W > 0 ? W : 320.f;
    float h = SquareH + Spacing + HueH;
    if (ShowHex) h += Spacing + kSwatchH;
    if (!Swatches.empty()) h += Spacing + kDotsH;
    h += PT + PB;
    return D2D1::SizeF(w + PL + PR, h);
}

void ColorPicker::LayoutChildren() {
    if (!hex_) return;
    D2D1_RECT_F hex = HexRc();
    float left = hex.left + kSwatchH + 8.f;
    hex_->Arrange(D2D1::RectF(left, hex.top, hex.right, hex.bottom));
}

void ColorPicker::Apply(float h, float s, float v, bool notify) {
    hue_ = h;
    sat_ = s;
    val_ = v;
    Color = HsvToRgb(h, s, v);
    SyncHex();
    if (notify && Changed) Changed(Color);
}

void ColorPicker::SyncHex() {
    if (!hex_) return;
    std::wstring want = ToHex(Color);
    if (hex_->Text != want) {
        hex_->Text = want;
        hex_->Caret = (int)want.size();
        hex_->SelAnchor = hex_->Caret;
    }
}

void ColorPicker::SetColor(D2D1_COLOR_F c, bool notify) {
    c.a = 1.f;
    float h = hue_, s = 0.f, v = 0.f;
    RgbToHsv(c, h, s, v);      // 灰阶时 h 保持原值，避免拖动中色相跳回 0
    hue_ = h;
    sat_ = s;
    val_ = v;
    Color = c;
    SyncHex();
    if (notify && Changed) Changed(Color);
}

void ColorPicker::PickAt(float x, float y) {
    if (drag_ == 1) {
        D2D1_RECT_F sq = SquareRc();
        float w = sq.right - sq.left;
        float h = sq.bottom - sq.top;
        float s = w > 0.f ? (x - sq.left) / w : 0.f;
        float v = h > 0.f ? 1.f - (y - sq.top) / h : 0.f;
        s = (std::min)(1.f, (std::max)(0.f, s));
        v = (std::min)(1.f, (std::max)(0.f, v));
        Apply(hue_, s, v, true);
    } else if (drag_ == 2) {
        D2D1_RECT_F hr = HueRc();
        float w = hr.right - hr.left;
        float t = w > 0.f ? (x - hr.left) / w : 0.f;
        t = (std::min)(1.f, (std::max)(0.f, t));
        Apply(t * 360.f, sat_, val_, true);
    }
}

void ColorPicker::OnPressAt(float x, float y) {
    drag_ = 0;
    D2D1_RECT_F sq = SquareRc();
    D2D1_RECT_F hr = HueRc();
    D2D1_RECT_F sw = SwatchRc();

    // 预设色板：一行圆点，点中即切换
    if (!Swatches.empty() && y >= sw.top && y <= sw.bottom && x >= sw.left && x <= sw.right) {
        float step = 26.f;
        int idx = (int)((x - sw.left) / step);
        if (idx >= 0 && idx < (int)Swatches.size()) {
            SetColor(Hex(Swatches[(size_t)idx]), true);
        }
        return;
    }
    if (x >= sq.left && x <= sq.right && y >= sq.top && y <= sq.bottom) {
        drag_ = 1;
        PickAt(x, y);
    } else if (x >= hr.left && x <= hr.right && y >= hr.top && y <= hr.bottom) {
        drag_ = 2;
        PickAt(x, y);
    }
}

void ColorPicker::OnDrag(float x, float y) { PickAt(x, y); }

void ColorPicker::OnRelease(bool) { drag_ = 0; }

void ColorPicker::Paint(PaintCtx& ctx) {
    const Tokens& t = ctx.t;
    D2D1_RECT_F sq = SquareRc();
    D2D1_RECT_F hr = HueRc();

    // 饱和度/明度方块：色相底色 + 白色横向渐变 + 黑色纵向渐变
    ctx.g.FillRect(sq, HueColor(hue_));
    ctx.g.FillLinear(sq, { { 0.f, C(1, 1, 1, 1) }, { 1.f, C(1, 1, 1, 0) } }, true);
    ctx.g.FillLinear(sq, { { 0.f, C(0, 0, 0, 0) }, { 1.f, C(0, 0, 0, 1) } }, false);
    ctx.g.StrokeRect(sq, 0.f, t.border, 1.f);

    // 方块上的圆点：白圈 + 细暗描边，深色浅色都看得见
    float cx = sq.left + sat_ * (sq.right - sq.left);
    float cy = sq.top + (1.f - val_) * (sq.bottom - sq.top);
    ctx.g.StrokeCircle(cx, cy, 6.5f, C(0, 0, 0, .45f), 1.f);
    ctx.g.StrokeCircle(cx, cy, 6.f, C(1, 1, 1, 1), 2.f);
    ctx.g.Circle(cx, cy, 3.f, Color);

    // 色相条
    ctx.g.FillLinear(hr, {
        { 0.f, HsvToRgb(0.f, 1, 1) },   { 1.f / 6.f, HsvToRgb(60.f, 1, 1) },
        { 2.f / 6.f, HsvToRgb(120.f, 1, 1) }, { .5f, HsvToRgb(180.f, 1, 1) },
        { 4.f / 6.f, HsvToRgb(240.f, 1, 1) }, { 5.f / 6.f, HsvToRgb(300.f, 1, 1) },
        { 1.f, HsvToRgb(360.f, 1, 1) } }, true);
    ctx.g.StrokeRect(hr, 0.f, t.border, 1.f);

    // 色相滑块：白色竖条
    float hx = hr.left + (hue_ / 360.f) * (hr.right - hr.left);
    D2D1_RECT_F knob{ hx - 2.5f, hr.top - 3.f, hx + 2.5f, hr.bottom + 3.f };
    ctx.g.RoundRect(knob, 2.f, C(1, 1, 1, 1));
    ctx.g.StrokeRect(knob, 2.f, C(0, 0, 0, .5f), 1.f);

    if (ShowHex) {
        D2D1_RECT_F hex = HexRc();
        D2D1_RECT_F chip{ hex.left, hex.top + 1.f, hex.left + kSwatchH - 2.f, hex.top + kSwatchH - 1.f };
        ctx.g.RoundRect(chip, 3.f, Color);
        ctx.g.StrokeRect(chip, 3.f, t.border, 1.f);
    }

    if (!Swatches.empty()) {
        D2D1_RECT_F sw = SwatchRc();
        float cy2 = (sw.top + sw.bottom) / 2.f;
        for (size_t i = 0; i < Swatches.size(); ++i) {
            float x = sw.left + 10.f + (float)i * 26.f;
            D2D1_COLOR_F c = Hex(Swatches[i]);
            ctx.g.Circle(x, cy2, 8.f, c);
            bool on = (c.r == Color.r && c.g == Color.g && c.b == Color.b);
            ctx.g.StrokeCircle(x, cy2, 8.f, on ? t.wz : t.border, on ? 2.f : 1.f);
        }
    }
}

} // namespace vcui
