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

#include "vcui/Layout.h"
#include <algorithm>
#include <cmath>

using std::max;

namespace vcui {

// ---------------- Stack ----------------

D2D1_SIZE_F Stack::Desired(float availW, float availH) {
    float mainSum = 0.f;
    float crossMax = 0.f;
    int n = 0;
    float aw = availW - PL - PR;
    float ah = availH - PT - PB;

    for (auto& k : kids_) {
        if (!k->Visible) continue;
        D2D1_SIZE_F d = k->Desired(aw, ah);
        float cw = k->W > 0 ? k->W : d.width;
        float ch = k->H > 0 ? k->H : d.height;
        float b = k->Basis >= 0 ? k->Basis : (Direction == Col ? ch : cw);
        mainSum += b + (Direction == Col ? k->MT + k->MB : k->ML + k->MR);
        crossMax = max(crossMax, (Direction == Col ? cw + k->ML + k->MR : ch + k->MT + k->MB));
        ++n;
    }
    if (n > 1) mainSum += Gap * float(n - 1);

    float w = (Direction == Col ? crossMax : mainSum) + PL + PR;
    float h = (Direction == Col ? mainSum : crossMax) + PT + PB;
    if (W > 0) w = W;
    if (H > 0) h = H;
    return D2D1::SizeF(w, h);
}

void Stack::LayoutChildren() {
    D2D1_RECT_F in = Content();
    if (Direction == Col) LayoutCol(in); else LayoutRow(in);
}

void Stack::LayoutCol(D2D1_RECT_F in) {
    float avail = in.bottom - in.top;
    float used = 0.f, growTotal = 0.f;
    int n = 0;
    float aw = in.right - in.left;

    std::vector<float> bases, widths;
    bases.reserve(kids_.size());
    for (auto& k : kids_) {
        if (!k->Visible) { bases.push_back(0.f); widths.push_back(0.f); continue; }
        D2D1_SIZE_F d = k->Desired(aw, avail);
        float cw = k->W > 0 ? k->W : (Cross == Stretch ? aw - k->ML - k->MR : d.width);
        float ch = k->H > 0 ? k->H : d.height;
        float b = k->Basis >= 0 ? k->Basis : ch;
        bases.push_back(b);
        widths.push_back(cw);
        used += b + k->MT + k->MB;
        growTotal += k->Grow;
        ++n;
    }
    if (n > 1) used += Gap * float(n - 1);

    float freeSpace = max(0.f, avail - used);
    float y = in.top;

    for (size_t i = 0; i < kids_.size(); ++i) {
        auto& k = kids_[i];
        if (!k->Visible) continue;
        float h = bases[i];
        if (growTotal > 0.f && k->Grow > 0.f) h += freeSpace * (k->Grow / growTotal);

        float cw = widths[i];
        float ox = 0.f;
        if (Cross == Center) ox = (aw - cw - k->ML - k->MR) * .5f;
        else if (Cross == End) ox = aw - cw - k->MR;

        D2D1_RECT_F fr{ in.left + k->ML + ox, y + k->MT, in.left + k->ML + ox + cw, y + k->MT + h };
        k->Arrange(fr);
        y = fr.bottom + k->MB + Gap;
    }
}

void Stack::LayoutRow(D2D1_RECT_F in) {
    float avail = in.right - in.left;
    float used = 0.f, growTotal = 0.f;
    int n = 0;
    float ah = in.bottom - in.top;

    std::vector<float> bases, heights;
    bases.reserve(kids_.size());
    for (auto& k : kids_) {
        if (!k->Visible) { bases.push_back(0.f); heights.push_back(0.f); continue; }
        D2D1_SIZE_F d = k->Desired(avail, ah);
        float cw = k->W > 0 ? k->W : d.width;
        float ch = k->H > 0 ? k->H : (Cross == Stretch ? ah - k->MT - k->MB : d.height);
        float b = k->Basis >= 0 ? k->Basis : cw;
        bases.push_back(b);
        heights.push_back(ch);
        used += b + k->ML + k->MR;
        growTotal += k->Grow;
        ++n;
    }
    if (n > 1) used += Gap * float(n - 1);

    float freeSpace = max(0.f, avail - used);
    float x = in.left;

    for (size_t i = 0; i < kids_.size(); ++i) {
        auto& k = kids_[i];
        if (!k->Visible) continue;
        float w = bases[i];
        if (growTotal > 0.f && k->Grow > 0.f) w += freeSpace * (k->Grow / growTotal);

        float ch = heights[i];
        float oy = 0.f;
        if (Cross == Center) oy = (ah - ch - k->MT - k->MB) * .5f;
        else if (Cross == End) oy = ah - ch - k->MB;

        D2D1_RECT_F fr{ x + k->ML, in.top + k->MT + oy, x + k->ML + w, in.top + k->MT + oy + ch };
        k->Arrange(fr);
        x = fr.right + k->MR + Gap;
    }
}

std::shared_ptr<Stack> VBox(float gap) { return std::make_shared<Stack>(Stack::Col, gap); }
std::shared_ptr<Stack> HBox(float gap) { return std::make_shared<Stack>(Stack::Row, gap); }

void Absolute::LayoutChildren() {
    D2D1_RECT_F in = Content();
    for (auto& k : Children()) {
        if (!k->Visible) continue;
        k->Arrange(D2D1_RECT_F{ in.left + k->X, in.top + k->Y,
                                in.left + k->X + k->W, in.top + k->Y + k->H });
    }
}

// ---------------- Card ----------------

Card::Card() {
    PL = PT = PR = PB = 20.f;
}

D2D1_SIZE_F Card::Desired(float availW, float availH) {
    float cw = 0.f, ch = 0.f;
    float aw = availW - PL - PR, ah = availH - PT - PB;
    for (auto& k : kids_) {
        if (!k->Visible) continue;
        D2D1_SIZE_F d = k->Desired(aw, ah);
        float kw = k->W > 0 ? k->W : d.width;
        float kh = k->H > 0 ? k->H : d.height;
        cw = max(cw, kw + k->ML + k->MR);
        ch += kh + k->MT + k->MB;
    }
    return D2D1::SizeF(W > 0 ? W : cw + PL + PR, H > 0 ? H : ch + PT + PB);
}

void Card::LayoutChildren() {
    D2D1_RECT_F in = Content();
    float y = in.top;
    float aw = in.right - in.left;
    for (auto& k : kids_) {
        if (!k->Visible) continue;
        D2D1_SIZE_F d = k->Desired(aw, in.bottom - in.top);
        float kw = k->W > 0 ? k->W : aw - k->ML - k->MR;
        float kh = k->H > 0 ? k->H : d.height;
        D2D1_RECT_F fr{ in.left + k->ML, y + k->MT, in.left + k->ML + kw, y + k->MT + kh };
        k->Arrange(fr);
        y = fr.bottom + k->MB;
    }
}

void Card::Paint(PaintCtx& ctx) {
    if (!Visible) return;
    float r = Radius >= 0 ? Radius : ctx.m.radiusCard;
    if (Shadow) {
        ctx.g.Shadow(fr_, r, 10.f, 3.f, C(0, 0, 0, ctx.t.dark ? .28f : .08f));
    }
    D2D1_COLOR_F fill = Fill.a > 0.f ? Fill : ctx.t.kp;
    ctx.g.RoundRect(fr_, r, fill);
    if (Border) ctx.g.StrokeRect(fr_, r, ctx.t.border, 1.f);
}

// ---------------- Spacer ----------------

std::shared_ptr<Widget> Spacer(float size) {
    auto s = std::make_shared<Widget>();
    s->W = 0.f;
    s->H = size > 0 ? size : 0.f;
    if (size <= 0) s->Grow = 1.f;
    return s;
}

// ---------------- Divider ----------------

Divider::Divider() { H = 1.f; }

D2D1_SIZE_F Divider::Desired(float availW, float availH) {
    return Vertical ? D2D1::SizeF(1.f, availH) : D2D1::SizeF(availW, 1.f);
}

void Divider::Paint(PaintCtx& ctx) {
    if (Vertical) ctx.g.Line(fr_.left + .5f, fr_.top, fr_.left + .5f, fr_.bottom, ctx.t.divider, 1.f);
    else ctx.g.Line(fr_.left, fr_.top + .5f, fr_.right, fr_.top + .5f, ctx.t.divider, 1.f);
}

// ---------------- ScrollView ----------------

ScrollView::ScrollView() {
    Clip = true;
    Grow = 1.f;
    Interactive = true;
}

D2D1_SIZE_F ScrollView::Desired(float availW, float availH) {
    return D2D1::SizeF(W > 0 ? W : availW, H > 0 ? H : availH);
}

void ScrollView::LayoutChildren() {
    if (kids_.empty()) return;
    D2D1_RECT_F in = Content();
    auto& content = kids_[0];
    float ch = content->H > 0 ? content->H : content->Desired(in.right - in.left, in.bottom - in.top).height;
    float maxScroll = max(0.f, ch - (in.bottom - in.top));
    if (ScrollY > maxScroll) ScrollY = maxScroll;
    if (ScrollY < 0.f) ScrollY = 0.f;
    if (TargetY > maxScroll) TargetY = maxScroll;
    if (TargetY < 0.f) TargetY = 0.f;
    content->Arrange(D2D1_RECT_F{ in.left, in.top - ScrollY, in.right, in.top - ScrollY + ch });
}

bool ScrollView::Update(float dt) {
    bool changed = Widget::Update(dt);
    if (ScrollY != TargetY) {
        float k = 1.f - std::exp(-dt / .085f);
        ScrollY += (TargetY - ScrollY) * k;
        if (std::abs(TargetY - ScrollY) < .5f) ScrollY = TargetY;
        LayoutChildren();
        AnimDirty = true;
        changed = true;
    }
    return changed;
}

void ScrollView::OnWheel(float dy, float, float) {
    TargetY -= dy * 48.f;
    D2D1_RECT_F in = Content();
    float viewH = in.bottom - in.top;
    float ch = kids_.empty() ? 0.f : kids_[0]->Frame().bottom - kids_[0]->Frame().top;
    float maxScroll = max(0.f, ch - viewH);
    TargetY = std::clamp(TargetY, 0.f, maxScroll);
}

void ScrollView::Paint(PaintCtx& ctx) {
    if (!Visible) return;
}

} // namespace vcui
