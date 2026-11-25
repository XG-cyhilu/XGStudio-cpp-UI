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

#include "vcui/VirtualList.h"
#include "vcui/Surface.h"

#include <algorithm>
#include <cmath>

namespace vcui {

VirtualList::VirtualList() {
    Clip = true;
    Grow = 1.f;
    Interactive = true;
    FocusRing = false;
}

float VirtualList::Pitch() const {
    return (std::max)(1.f, RowHeight + Gap);
}

float VirtualList::ViewHeight() const {
    D2D1_RECT_F in = Content();
    return in.bottom - in.top;
}

float VirtualList::ContentHeight() const {
    if (ItemCount <= 0) return 0.f;
    return ItemCount * Pitch() - Gap;
}

float VirtualList::MaxScroll() const {
    return (std::max)(0.f, ContentHeight() - ViewHeight());
}

int VirtualList::LiveRows() const {
    int n = 0;
    for (const auto& s : slots_) if (s.index >= 0) ++n;
    return n;
}

D2D1_SIZE_F VirtualList::Desired(float availW, float availH) {
    // 同样不能直接返回可用尺寸：父容器会以为已经占满，Grow 的剩余空间变 0。
    float cw = W > 0 ? W : availW;
    float ch;
    if (H > 0) ch = H;
    else {
        float c = ContentHeight();
        ch = (availH > 0.f && availH < 1e5f) ? (std::min)(c, availH) : c;
    }
    return D2D1::SizeF(cw, ch);
}

void VirtualList::ParkAll() {
    for (auto& s : slots_) {
        s.index = -1;
        if (s.w) s.w->Visible = false;
    }
    first_ = last_ = -1;
}

void VirtualList::Reset() {
    for (auto& s : slots_) {
        if (s.w) s.w->Detach();
        s.w.reset();
        s.index = -1;
    }
    slots_.clear();
    laidOutCount_ = -1;
    first_ = last_ = -1;
    ScrollY = TargetY = 0.f;
    AnimDirty = true;
}

int VirtualList::AcquireSlot() {
    for (int i = 0; i < (int)slots_.size(); ++i)
        if (slots_[i].index < 0 && slots_[i].w) return i;
    // 先找空槽复用（旧行留着给 Bind 改写）；没有就扩一个
    for (int i = 0; i < (int)slots_.size(); ++i)
        if (slots_[i].index < 0) return i;
    slots_.push_back(Slot{});
    return (int)slots_.size() - 1;
}

void VirtualList::AnimateIn(float dur, float dy, float step, float maxDelay) {
    enterDur_ = dur;
    enterDy_ = dy;
    enterStep_ = step;
    enterMaxDelay_ = maxDelay;
    enterPending_ = true;
    AnimDirty = true;
    if (Host) { Host->KickAnim(); Host->PlayAnim(); }
}

void VirtualList::LayoutChildren() {
    D2D1_RECT_F in = Content();
    float gutter = ShowBar ? (BarW + 6.f) : 0.f;
    float right = in.right - gutter;
    float pitch = Pitch();

    if (ItemCount != laidOutCount_) {
        ParkAll();
        laidOutCount_ = ItemCount;
    }

    float maxS = MaxScroll();
    ScrollY = std::clamp(ScrollY, 0.f, maxS);
    TargetY = std::clamp(TargetY, 0.f, maxS);

    if (ItemCount <= 0 || in.bottom <= in.top) {
        ParkAll();
        return;
    }

    int first = (int)std::floor(ScrollY / pitch) - Overscan;
    int last = (int)std::floor((ScrollY + (in.bottom - in.top) - .01f) / pitch) + Overscan;
    first = (std::max)(0, first);
    last = (std::min)(ItemCount - 1, last);
    if (last < first) { ParkAll(); return; }
    first_ = first;
    last_ = last;

    // 1) 视口外的槽全部停用（控件留着，等下一个 index 来复用）
    for (auto& s : slots_)
        if (s.index >= 0 && (s.index < first || s.index > last)) {
            s.index = -1;
            if (s.w) s.w->Visible = false;
        }

    // 2) 给每个可见 index 认领一个槽，认领不到就造新行
    float delay = 0.f;
    for (int i = first; i <= last; ++i) {
        int cur = -1;
        for (int k = 0; k < (int)slots_.size(); ++k)
            if (slots_[k].index == i) { cur = k; break; }
        if (cur >= 0) continue;   // 已经在位

        int slotIdx = AcquireSlot();
        auto& s = slots_[slotIdx];
        bool reused = false;
        if (s.w && Bind) reused = Bind(*s.w, i);
        else if (s.w) { s.w->Detach(); s.w.reset(); }   // 没有 Bind：内容改不了，只能重造
        if (reused) {
            ++recycled_;
        } else {
            auto w = Build ? Build(i) : nullptr;
            if (!w) { s.index = -1; continue; }
            s.w = w;
            Add(w);
            // 新行只有结构，内容同样要 Bind 一次（Build 只管搭骨架）
            if (Bind) Bind(*w, i);
        }
        s.w->Visible = true;
        s.index = i;

        // 入场动画：按先后错峰。FadeIn 会改 MT，位移由下一轮 Arrange 生效。
        if (enterPending_) {
            s.w->FadeIn(enterDur_, enterDy_, (std::min)(delay, enterMaxDelay_));
            delay += enterStep_;
        }
    }
    if (enterPending_ && enterDur_ > 0.f) {
        enterPending_ = false;
        if (Host) { Host->KickAnim(); Host->PlayAnim(); }
    }

    // 3) 排位置：行顶 = 视口顶 - 滚动偏移 + index * 行距（再加动画位移 MT）
    for (auto& s : slots_) {
        if (s.index < 0 || !s.w) continue;
        float top = in.top - ScrollY + s.index * pitch + s.w->MT;
        s.w->Arrange(D2D1_RECT_F{ in.left, top, right, top + RowHeight });
    }
}

bool VirtualList::Update(float dt) {
    bool changed = Widget::Update(dt);

    if (ScrollY != TargetY) {
        float k = 1.f - std::exp(-dt / .085f);
        ScrollY += (TargetY - ScrollY) * k;
        if (std::abs(TargetY - ScrollY) < .5f) ScrollY = TargetY;
        LayoutChildren();
        AnimDirty = true;
        changed = true;
    }

    // 行入场动画每帧都在改 MT，位移要重新排一次才看得见
    bool anim = false;
    for (auto& s : slots_)
        if (s.index >= 0 && s.w && s.w->AnimT >= 0.f) { anim = true; break; }
    if (anim) {
        LayoutChildren();
        AnimDirty = true;
        changed = true;
    }

    if (barHold_ > 0.f) {
        barHold_ -= dt;
        if (barHold_ < 0.f) barHold_ = 0.f;
    }
    float target = (draggingBar_ || Hover || barHold_ > 0.f) ? 1.f : 0.f;
    if (MaxScroll() <= .5f) target = 0.f;
    if (aBar != target) {
        float k = 1.f - std::exp(-dt / .12f);
        aBar += (target - aBar) * k;
        if (std::abs(target - aBar) < .01f) aBar = target;
        AnimDirty = true;
        changed = true;
    }
    return changed;
}

void VirtualList::ScrollToIndex(int index, bool center) {
    if (ItemCount <= 0) return;
    index = std::clamp(index, 0, ItemCount - 1);
    float pitch = Pitch();
    float next = index * pitch;
    if (center) next -= (ViewHeight() - RowHeight) * .5f;
    TargetY = std::clamp(next, 0.f, MaxScroll());
    barHold_ = .9f;
    if (Host) { Host->KickAnim(); Host->PlayAnim(); }
}

void VirtualList::EnsureVisible(int index) {
    if (ItemCount <= 0) return;
    index = std::clamp(index, 0, ItemCount - 1);
    float pitch = Pitch();
    float pad = 6.f;
    float top = index * pitch;
    float bottom = top + RowHeight;
    float next = TargetY;
    if (top < TargetY + pad) next = top - pad;
    else if (bottom > TargetY + ViewHeight() - pad) next = bottom - ViewHeight() + pad;
    next = std::clamp(next, 0.f, MaxScroll());
    if (std::abs(next - TargetY) < .5f) return;
    TargetY = next;
    barHold_ = .9f;
    if (Host) { Host->KickAnim(); Host->PlayAnim(); }
}

void VirtualList::OnWheel(float dy, float, float) {
    TargetY -= dy * Pitch();
    TargetY = std::clamp(TargetY, 0.f, MaxScroll());
    barHold_ = .9f;
}

void VirtualList::OnKeyDown(int vk) {
    float pitch = Pitch();
    float page = ViewHeight() * .9f;
    float next = TargetY;
    switch (vk) {
    case VK_UP:    next -= pitch; break;
    case VK_DOWN:  next += pitch; break;
    case VK_PRIOR: next -= page; break;
    case VK_NEXT:  next += page; break;
    case VK_HOME:  next = 0.f; break;
    case VK_END:   next = MaxScroll(); break;
    default: return;
    }
    next = std::clamp(next, 0.f, MaxScroll());
    if (std::abs(next - TargetY) < .5f && std::abs(next - ScrollY) < .5f) return;
    TargetY = next;
    barHold_ = .9f;
    if (Host) { Host->PlayAnim(); Host->InvalidateRectD(fr_); }
}

void VirtualList::OnPressAt(float x, float y) {
    Widget::OnPress();
    if (!ShowBar) return;
    D2D1_RECT_F th = ThumbRect();
    if (x >= th.left - 2.f && x <= th.right + 2.f && y >= th.top && y < th.bottom) {
        draggingBar_ = true;
        grab_ = y - th.top;
        TargetY = ScrollY;
        barHold_ = .9f;
    }
}

void VirtualList::OnDrag(float, float y) {
    if (!draggingBar_) return;
    D2D1_RECT_F tr = TrackRect();
    D2D1_RECT_F th = ThumbRect();
    float travel = (tr.bottom - tr.top) - (th.bottom - th.top);
    if (travel <= 1.f) return;
    float top = y - grab_;
    float r = std::clamp((top - tr.top) / travel, 0.f, 1.f);
    ScrollY = TargetY = r * MaxScroll();
    LayoutChildren();
    barHold_ = .9f;
    AnimDirty = true;
}

void VirtualList::OnRelease(bool) {
    Widget::OnRelease(true);
    draggingBar_ = false;
}

CursorShape VirtualList::CursorAt(float, float) const { return CursorShape::Arrow; }

D2D1_RECT_F VirtualList::TrackRect() const {
    D2D1_RECT_F in = Content();
    return D2D1_RECT_F{ in.right - BarW, in.top + 2.f, in.right, in.bottom - 2.f };
}

D2D1_RECT_F VirtualList::ThumbRect() const {
    D2D1_RECT_F tr = TrackRect();
    float trackH = tr.bottom - tr.top;
    float ch = ContentHeight();
    float vh = ViewHeight();
    float maxS = MaxScroll();
    float w = 2.f + (BarW - 2.f) * aBar;
    float hh = ch > 0.f ? trackH * (vh / ch) : trackH;
    hh = std::clamp(hh, 24.f, trackH);
    float y = tr.top + (maxS > .5f ? (ScrollY / maxS) * (trackH - hh) : 0.f);
    float x = tr.right - 2.f - w;
    return D2D1_RECT_F{ x, y, x + w, y + hh };
}

void VirtualList::Paint(PaintCtx& ctx) {
    if (!Visible || !ShowBar || MaxScroll() <= .5f) return;
    D2D1_RECT_F th = ThumbRect();
    D2D1_COLOR_F c = ctx.t.text3;
    c.a = .30f + .30f * aBar;
    ctx.g.RoundRect(th, (th.right - th.left) * .5f, c);
}

} // namespace vcui
