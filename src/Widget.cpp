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

#include "vcui/Widget.h"
#include <cmath>

namespace vcui {

Widget::~Widget() = default;

Widget& Widget::Add(std::shared_ptr<Widget> child) {
    if (!child) return *this;
    child->Parent = this;
    child->Host = Host;
    if (!child->Id.empty()) {
        // 允许按 id 查找（Surface 挂载时会递归补齐 Host）
    }
    kids_.push_back(std::move(child));
    return *this;
}

void Widget::Clear() {
    kids_.clear();
}

void Widget::Detach() {
    if (!Parent) return;
    auto& sib = Parent->kids_;
    for (auto it = sib.begin(); it != sib.end(); ++it) {
        if (it->get() == this) { sib.erase(it); break; }
    }
    Parent = nullptr;
}

D2D1_RECT_F Widget::Content() const {
    return D2D1_RECT_F{ fr_.left + PL, fr_.top + PT, fr_.right - PR, fr_.bottom - PB };
}

void Widget::Arrange(D2D1_RECT_F fr) {
    fr_ = fr;
    LayoutChildren();
}

D2D1_SIZE_F Widget::Desired(float /*availW*/, float /*availH*/) {
    return D2D1::SizeF(W > 0 ? W : 0.f, H > 0 ? H : 0.f);
}

bool Widget::HitTest(float x, float y) const {
    return x >= fr_.left && x < fr_.right && y >= fr_.top && y < fr_.bottom;
}

Widget* Widget::Hit(float x, float y) {
    if (!Visible) return nullptr;
    for (auto it = kids_.rbegin(); it != kids_.rend(); ++it) {
        if (Widget* h = (*it)->Hit(x, y)) {
            // 命中纯装饰子节点（文字、图标）时上溯到真正的控件，
            // 否则点击会被 Label 吃掉
            while (h && !h->Interactive) h = h->Parent;
            return h;
        }
    }
    if (Clip) {
        D2D1_RECT_F c = Content();
        if (x < c.left || x >= c.right || y < c.top || y >= c.bottom) return nullptr;
    }
    return HitTest(x, y) && Enabled && Interactive ? this : nullptr;
}

void Widget::OnClick() {
    if (Clicked) Clicked();
}

void Widget::Paint(PaintCtx& ctx) {
    if (!Visible) return;
    if (Bg.a > 0.f && fr_.right > fr_.left && fr_.bottom > fr_.top)
        ctx.g.FillRect(fr_, Bg);
}

void Widget::PaintTree(PaintCtx& ctx) {
    if (!Visible) return;
    float savedAlpha = ctx.g.alpha;
    ctx.g.alpha *= Opacity;
    Paint(ctx);
    if (Clip) ctx.g.PushClip(Content());
    for (auto& k : kids_) k->PaintTree(ctx);
    if (Clip) ctx.g.PopClip();
    ctx.g.alpha = savedAlpha;
}

std::shared_ptr<Widget> Sizer(float w, float h) {
    auto s = std::make_shared<Widget>();
    s->W = w;
    s->H = h;
    return s;
}

bool Widget::Update(float dt) {
    bool changed = false;
    auto approach = [&](float& v, float target, float tau) {
        if (v == target) return;
        float k = 1.f - std::exp(-dt / tau);
        v += (target - v) * k;
        if (std::abs(target - v) < .004f) v = target;
        changed = true;
        AnimDirty = true;
    };
    approach(aHover, Hover && Enabled ? 1.f : 0.f, .10f);
    approach(aPress, Press ? 1.f : 0.f, .06f);
    for (auto& k : kids_) changed |= k->Update(dt);
    return changed;
}

float Widget::Tw(const std::wstring& s, float px, int weight) const {    if (s.empty()) return 0.f;
    if (GFX) return GFX->TextW(s, px, weight);
    float w = 0.f;
    for (auto c : s) w += (c < 128 ? px * .55f : px);
    return w;
}

} // namespace vcui
