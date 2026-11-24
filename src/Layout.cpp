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
#include "vcui/Surface.h"
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
        // Grow 子项在布局阶段才决定实际高度（只吃剩余空间），但 Desired 仍按固有
        // 尺寸参与，容器才不会被算成"迷你高度"而让整页塌陷。
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
    // Desired 不该超过可用尺寸，否则父容器把这份"撑爆"的尺寸当基准，
    // 兄弟/自身就被挤出窗口（Grow 分配的 freeSpace 会直接算成 0）。
    if (availW > 0.f && w > availW) w = availW;
    if (availH > 0.f && h > availH) h = availH;
    return D2D1::SizeF(w, h);
}

void Stack::LayoutChildren() {
    D2D1_RECT_F in = Content();
    if (Direction == Col) LayoutCol(in); else LayoutRow(in);
}

void Stack::LayoutCol(D2D1_RECT_F in) {
    float avail = in.bottom - in.top;
    float usedFixed = 0.f, growTotal = 0.f;
    int n = 0;
    float aw = in.right - in.left;

    std::vector<float> bases, widths;
    bases.reserve(kids_.size());
    widths.reserve(kids_.size());
    for (auto& k : kids_) {
        if (!k->Visible) { bases.push_back(0.f); widths.push_back(0.f); continue; }
        D2D1_SIZE_F d = k->Desired(aw, avail);
        float cw = k->W > 0 ? k->W : (Cross == Stretch ? aw - k->ML - k->MR : d.width);
        float ch = k->H > 0 ? k->H : d.height;
        float b = k->Basis >= 0 ? k->Basis : (k->Grow > 0.f ? 0.f : ch);
        bases.push_back(b);
        widths.push_back(cw);
        usedFixed += b + k->MT + k->MB;
        growTotal += k->Grow;
        ++n;
    }
    if (n > 1) usedFixed += Gap * float(n - 1);

    // Grow 子项只分"非 Grow 兄弟排完之后剩下的空间"，否则它会按固有高度占满，
    // 把排在它后面的兄弟顶出容器（表现在滚动容器覆盖其它 flex 区域）。
    float freeSpace = max(0.f, avail - usedFixed);
    float y = in.top;

    for (size_t i = 0; i < kids_.size(); ++i) {
        auto& k = kids_[i];
        if (!k->Visible) continue;
        float h = bases[i];
        if (growTotal > 0.f && k->Grow > 0.f) h = freeSpace * (k->Grow / growTotal);

        float cw = widths[i];
        float ox = 0.f;
        if (Cross == Center) ox = (aw - cw - k->ML - k->MR) * .5f;
        else if (Cross == End) ox = aw - cw - k->MR;

        D2D1_RECT_F fr{ in.left + k->ML + ox, y + k->MT, in.left + k->ML + ox + cw, y + k->MT + h };
        // 兜底：任何子项都不许超出容器内容区，否则会把同一容器里的兄弟顶出窗口
        if (fr.bottom > in.bottom) fr.bottom = in.bottom > fr.top ? in.bottom : fr.top;
        k->Arrange(fr);
        y = fr.bottom + k->MB + Gap;
    }
}

void Stack::LayoutRow(D2D1_RECT_F in) {
    float avail = in.right - in.left;
    float usedFixed = 0.f, growTotal = 0.f;
    int n = 0;
    float ah = in.bottom - in.top;

    std::vector<float> bases, heights;
    bases.reserve(kids_.size());
    heights.reserve(kids_.size());
    for (auto& k : kids_) {
        if (!k->Visible) { bases.push_back(0.f); heights.push_back(0.f); continue; }
        D2D1_SIZE_F d = k->Desired(avail, ah);
        float cw = k->W > 0 ? k->W : d.width;
        float ch = k->H > 0 ? k->H : (Cross == Stretch ? ah - k->MT - k->MB : d.height);
        float b = k->Basis >= 0 ? k->Basis : (k->Grow > 0.f ? 0.f : cw);
        bases.push_back(b);
        heights.push_back(ch);
        usedFixed += b + k->ML + k->MR;
        growTotal += k->Grow;
        ++n;
    }
    if (n > 1) usedFixed += Gap * float(n - 1);

    float freeSpace = max(0.f, avail - usedFixed);
    float x = in.left;

    for (size_t i = 0; i < kids_.size(); ++i) {
        auto& k = kids_[i];
        if (!k->Visible) continue;
        float w = bases[i];
        if (growTotal > 0.f && k->Grow > 0.f) w = freeSpace * (k->Grow / growTotal);

        float ch = heights[i];
        float oy = 0.f;
        if (Cross == Center) oy = (ah - ch - k->MT - k->MB) * .5f;
        else if (Cross == End) oy = ah - ch - k->MB;

        D2D1_RECT_F fr{ x + k->ML, in.top + k->MT + oy, x + k->ML + w, in.top + k->MT + oy + ch };
        if (fr.right > in.right) fr.right = in.right > fr.left ? in.right : fr.left;
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

// ---------------- Grid ----------------

D2D1_SIZE_F Grid::Desired(float availW, float availH) {
    int cols = Cols > 0 ? Cols : 1;
    float aw = availW - PL - PR;

    // 内容宽按"最宽的一列 × 列数"估：返回内容尺寸，不返回可用尺寸，
    // 否则父容器的 Grow 分配会把 freeSpace 算成 0 并把兄弟挤出窗口。
    float maxCell = 0.f;
    std::vector<float> rowH;
    int i = 0;
    for (auto& k : kids_) {
        if (!k->Visible) continue;
        D2D1_SIZE_F d = k->Desired(aw, availH - PT - PB);
        float kw = k->W > 0 ? k->W : d.width;
        maxCell = max(maxCell, kw + k->ML + k->MR);
        float kh = k->H > 0 ? k->H : d.height;
        kh += k->MT + k->MB;
        if (i % cols == 0) rowH.push_back(kh);
        else rowH.back() = max(rowH.back(), kh);
        ++i;
    }
    float w = cols > 1 ? maxCell * float(cols) + GapX * float(cols - 1) : maxCell;
    float h = PT + PB;
    for (float v : rowH) h += v;
    if (rowH.size() > 1) h += GapY * float(rowH.size() - 1);
    float fw = W > 0 ? W : w + PL + PR;
    float fh = H > 0 ? H : h;
    if (availW > 0.f && fw > availW) fw = availW;
    if (availH > 0.f && fh > availH) fh = availH;
    return D2D1::SizeF(fw, fh);
}

void Grid::LayoutChildren() {
    D2D1_RECT_F in = Content();
    int cols = Cols > 0 ? Cols : 1;
    float aw = in.right - in.left;
    float cellW = cols > 1 ? (aw - GapX * float(cols - 1)) / float(cols) : aw;
    if (cellW < 1.f) cellW = 1.f;

    std::vector<Widget*> vis;
    for (auto& k : kids_)
        if (k->Visible) vis.push_back(k.get());
    size_t n = vis.size();
    if (n == 0) return;

    // 行高按该行最高的子项算，这样同一行的卡片底部对齐
    std::vector<float> rowH((n + size_t(cols) - 1) / size_t(cols), 0.f);
    for (size_t i = 0; i < n; ++i) {
        D2D1_SIZE_F d = vis[i]->Desired(cellW, in.bottom - in.top);
        float kh = vis[i]->H > 0 ? vis[i]->H : d.height;
        float& rh = rowH[i / size_t(cols)];
        rh = max(rh, kh + vis[i]->MT + vis[i]->MB);
    }

    float y = in.top;
    for (size_t r = 0; r < rowH.size(); ++r) {
        for (int c = 0; c < cols; ++c) {
            size_t i = r * size_t(cols) + size_t(c);
            if (i >= n) break;
            Widget* k = vis[i];
            float x = in.left + float(c) * (cellW + GapX);
            float kh = k->H > 0 ? k->H : rowH[r] - k->MT - k->MB;
            k->Arrange(D2D1_RECT_F{ x + k->ML, y + k->MT,
                                    x + k->ML + cellW - k->ML - k->MR, y + k->MT + kh });
        }
        y += rowH[r] + GapY;
    }
}

// ---------------- ScrollView ----------------

ScrollView::ScrollView() {
    Clip = true;
    Grow = 1.f;
    Interactive = true;
    FocusRing = false;   // 焦点只是为了键盘滚动，不该给整个视口套一圈描边
}

D2D1_SIZE_F ScrollView::Desired(float availW, float availH) {
    // 取内容尺寸并夹到可用尺寸：返回"可用尺寸"会把父容器的 Grow 计算
    // 带偏（父以为这个子项已经占满，freeSpace 直接变 0，兄弟被挤出窗口）。
    float cw = availW, ch = availH;
    if (!kids_.empty()) {
        D2D1_SIZE_F d = kids_[0]->Desired(availW, availH);
        cw = std::min(availW, d.width > 0.f ? d.width : availW);
        ch = std::min(availH, d.height > 0.f ? d.height : availH);
    }
    return D2D1::SizeF(W > 0 ? W : cw, H > 0 ? H : ch);
}

float ScrollView::ViewHeight() const {
    D2D1_RECT_F in = Content();
    return in.bottom - in.top;
}

float ScrollView::ContentHeight() const {
    if (kids_.empty()) return 0.f;
    D2D1_RECT_F f = kids_[0]->Frame();
    return f.bottom - f.top;
}

float ScrollView::MaxScroll() const {
    return max(0.f, ContentHeight() - ViewHeight());
}

void ScrollView::LayoutChildren() {
    if (kids_.empty()) return;
    D2D1_RECT_F in = Content();
    // 右侧留出滚动条沟槽，内容不会盖住滑块（也保证滑块能收到点击）
    float gutter = ShowBar ? (BarW + 6.f) : 0.f;
    auto& content = kids_[0];
    float ch = content->H > 0 ? content->H
                              : content->Desired(in.right - in.left - gutter, 1e6f).height;
    float maxScroll = max(0.f, ch - (in.bottom - in.top));
    ScrollY = std::clamp(ScrollY, 0.f, maxScroll);
    TargetY = std::clamp(TargetY, 0.f, maxScroll);
    content->Arrange(D2D1_RECT_F{ in.left, in.top - ScrollY, in.right - gutter, in.top - ScrollY + ch });
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
    if (barHold_ > 0.f) {
        barHold_ -= dt;
        if (barHold_ < 0.f) barHold_ = 0.f;
    }
    float target = (draggingBar_ || Hover || barHold_ > 0.f) ? 1.f : 0.f;
    if (MaxScroll() <= .5f) target = 0.f;   // 内容不足一屏就不显示
    if (aBar != target) {
        float k = 1.f - std::exp(-dt / .12f);
        aBar += (target - aBar) * k;
        if (std::abs(target - aBar) < .01f) aBar = target;
        AnimDirty = true;
        changed = true;
    }
    return changed;
}

void ScrollView::OnWheel(float dy, float, float) {
    TargetY -= dy * 48.f;
    TargetY = std::clamp(TargetY, 0.f, MaxScroll());
    barHold_ = .9f;
}

void ScrollView::OnKeyDown(int vk) {
    float page = ViewHeight() * .9f;
    float next = TargetY;
    switch (vk) {
    case VK_UP:    next -= 48.f; break;
    case VK_DOWN:  next += 48.f; break;
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

void ScrollView::ScrollIntoView(D2D1_RECT_F rc) {
    D2D1_RECT_F in = Content();
    float pad = 6.f;              // 别让焦点控件贴着视口边缘
    float next = TargetY;
    // rc 是内容在绝对坐标下当前的位置，直接和视口比
    if (rc.top < in.top + pad) next -= (in.top + pad) - rc.top;
    else if (rc.bottom > in.bottom - pad) next += rc.bottom - (in.bottom - pad);
    next = std::clamp(next, 0.f, MaxScroll());
    if (std::abs(next - TargetY) < .5f) return;
    TargetY = next;
    barHold_ = .9f;
}

void ScrollView::OnPressAt(float x, float y) {
    Widget::OnPress();
    if (!ShowBar) return;
    D2D1_RECT_F th = ThumbRect();
    if (x >= th.left - 2.f && x <= th.right + 2.f && y >= th.top && y < th.bottom) {
        draggingBar_ = true;
        grab_ = y - th.top;
        TargetY = ScrollY;   // 按住就停掉缓动
        barHold_ = .9f;
    }
}

void ScrollView::OnDrag(float, float y) {
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

void ScrollView::OnRelease(bool) {
    Widget::OnRelease(true);
    draggingBar_ = false;
}

CursorShape ScrollView::CursorAt(float, float) const { return CursorShape::Arrow; }

D2D1_RECT_F ScrollView::TrackRect() const {
    D2D1_RECT_F in = Content();
    return D2D1_RECT_F{ in.right - BarW, in.top + 2.f, in.right, in.bottom - 2.f };
}

D2D1_RECT_F ScrollView::ThumbRect() const {
    D2D1_RECT_F tr = TrackRect();
    float trackH = tr.bottom - tr.top;
    float ch = ContentHeight();
    float vh = ViewHeight();
    float maxS = MaxScroll();
    float w = 2.f + (BarW - 2.f) * aBar;          // 静默 2px，悬停 6px
    float hh = ch > 0.f ? trackH * (vh / ch) : trackH;
    hh = std::clamp(hh, 24.f, trackH);
    float y = tr.top + (maxS > .5f ? (ScrollY / maxS) * (trackH - hh) : 0.f);
    float x = tr.right - 2.f - w;
    return D2D1_RECT_F{ x, y, x + w, y + hh };
}

void ScrollView::Paint(PaintCtx& ctx) {
    if (!Visible || !ShowBar || MaxScroll() <= .5f) return;
    // 静止时也是一条 2px 细线（alpha 压低），悬停/滚动时加粗到 BarW
    D2D1_RECT_F th = ThumbRect();
    D2D1_COLOR_F c = ctx.t.text3;
    c.a = .30f + .30f * aBar;
    ctx.g.RoundRect(th, (th.right - th.left) * .5f, c);
}

} // namespace vcui
