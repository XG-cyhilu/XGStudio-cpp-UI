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
#include "vcui/Surface.h"
#include "vcui/Controls.h"
#include "vcui/VirtualList.h"
#include <cmath>
#include <algorithm>

namespace vcui {

// cubic-bezier(0.1, 0.9, 0.2, 1)：起步快、收尾慢（Fluent decelerate）
static float BezAxis(float t, float a1, float a2) {
    float u = 1.f - t;
    return 3.f * u * u * t * a1 + 3.f * u * t * t * a2 + t * t * t;
}

float EaseDecelerate(float p) {
    if (p <= 0.f) return 0.f;
    if (p >= 1.f) return 1.f;
    const float x1 = 0.1f, y1 = 0.9f, x2 = 0.2f, y2 = 1.f;
    // 先按 x 反解参数 t（贝塞尔曲线 x(t) 单调），再取 y(t)
    float lo = 0.f, hi = 1.f, t = p;
    for (int i = 0; i < 20; ++i) {
        float x = BezAxis(t, x1, x2);
        if (x < p) lo = t; else hi = t;
        t = (lo + hi) * .5f;
    }
    return BezAxis(t, y1, y2);
}

Widget::~Widget() {
    // 被销毁的控件必须让宿主机把悬停/按下/焦点/提示这些裸指针放开，
    // 否则"切页重建子导航"这类操作会留下悬空指针，下一帧绘制就画到已释放内存。
    if (Host) Host->ForgetWidget(this);
}

Widget& Widget::Add(std::shared_ptr<Widget> child) {
    if (!child) return *this;
    child->Parent = this;
    // 子树可能是离线搭好再挂上来的（比如"先 Add 到 col，再把 col 塞进 ScrollView"），
    // 必须把 Host / GFX 递归补进去，否则里层控件拿不到 Surface（下拉框就无法挂 Overlay）。
    if (Host || GFX) {
        std::shared_ptr<Widget> keep = child;
        std::function<void(Widget*)> walk = [&](Widget* w) {
            w->Host = Host;
            if (GFX) w->GFX = GFX;
            for (auto& k : w->Children()) walk(k.get());
        };
        walk(keep.get());
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
    // 先按自身裁剪判定：被滚出视口的子项虽然在 fr_ 上还"存在"，但不该再响应点击
    if (Clip) {
        D2D1_RECT_F c = Content();
        if (x < c.left || x >= c.right || y < c.top || y >= c.bottom) return nullptr;
    }
    for (auto it = kids_.rbegin(); it != kids_.rend(); ++it) {
        if (Widget* h = (*it)->Hit(x, y)) {
            // 命中纯装饰子节点（文字、图标）时上溯到真正的控件，
            // 否则点击会被 Label 吃掉
            while (h && !h->Interactive) h = h->Parent;
            return h;
        }
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

void Widget::FadeIn(float dur, float dy, float delay) {
    AnimDur = dur > 0.f ? dur : .252f;
    AnimDy = dy;
    AnimDelay = delay > 0.f ? delay : 0.f;
    // 重启动画时必须沿用上一次的基准不透明度：此刻 Opacity 可能还停在 0，
    // 若拿它当基准，动画终点就成了 0，内容会永远看不见。
    AnimBaseOpacity = (AnimT >= 0.f) ? AnimBaseOpacity : Opacity;
    AnimBaseMT = (AnimT >= 0.f) ? AnimBaseMT : MT;
    AnimT = 0.f;
    Opacity = 0.f;                     // 延迟期内也是透明，避免闪一下
    MT = AnimBaseMT + AnimDy;
    AnimDirty = true;
    if (Host) Host->PlayAnim();  // 唤醒帧循环（平时是睡着的）
}

void Widget::StaggerIn(float dur, float dy, float step, float maxDelay) {
    float d = 0.f;
    auto next = [&]() { float v = std::min(d, maxDelay); d += step; return v; };

    for (auto& k : kids_) {
        // 虚拟列表只让视口内的行存在，交给它自己给那几行错峰
        if (auto* vl = dynamic_cast<VirtualList*>(k.get())) {
            vl->AnimateIn(dur, dy, step, maxDelay);
            continue;
        }
        // 滚动列表里真正"一行一行出现"的是 ScrollView 内容容器的直接子项
        auto* sv = dynamic_cast<ScrollView*>(k.get());
        if (sv && !sv->Children().empty()) {
            auto* inner = sv->Children()[0].get();
            for (auto& r : inner->Children()) r->FadeIn(dur, dy, next());
        } else {
            k->FadeIn(dur, dy, next());
        }
    }
    if (Host) Host->PlayAnim();
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

    // 入场：Fluent decelerate 贝塞尔，含各自的延迟
    if (AnimT >= 0.f) {
        AnimT += dt;
        float local = AnimT - AnimDelay;
        AnimDirty = true;
        changed = true;
        if (local < 0.f) {
            Opacity = 0.f;                        // 还没轮到自己
            MT = AnimBaseMT + AnimDy;
        } else {
            float p = local / AnimDur;
            if (p > 1.f) p = 1.f;
            float e = EaseDecelerate(p);
            Opacity = AnimBaseOpacity * e;
            MT = AnimBaseMT + AnimDy * (1.f - e);
            if (p >= 1.f) {
                AnimT = -1.f;
                Opacity = AnimBaseOpacity;
                MT = AnimBaseMT;
            }
        }
    }

    // 不可见的子树不参与动画推进：否则隐藏的转圈/骨架会一直把帧循环拉醒
    for (auto& k : kids_) if (k->Visible) changed |= k->Update(dt);
    return changed;
}

float Widget::Tw(const std::wstring& s, float px, int weight) const {    if (s.empty()) return 0.f;
    if (GFX) return GFX->TextW(s, px, weight);
    float w = 0.f;
    for (auto c : s) w += (c < 128 ? px * .55f : px);
    return w;
}

} // namespace vcui
