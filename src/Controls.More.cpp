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

#include "vcui/Surface.h"
#include <algorithm>
#include <cmath>

namespace vcui {

// ---------------- NavItem ----------------

NavItem::NavItem(std::wstring text, std::wstring glyph)
    : Text(std::move(text)), Glyph(std::move(glyph)) {
    H = 38.f;
    Interactive = true;
    PL = 14.f;
    PR = 12.f;
}

D2D1_SIZE_F NavItem::Desired(float availW, float availH) {
    (void)availH;
    return D2D1::SizeF(W > 0 ? W : availW, H > 0 ? H : 38.f);
}

void NavItem::Paint(PaintCtx& ctx) {
    const Tokens& t = ctx.t;
    float rad = 4.f;
    float hov = HoverA();

    if (Active) ctx.g.RoundRect(fr_, rad, t.accentSoft);
    if (!Active && hov > .01f) ctx.g.RoundRect(fr_, rad, A(CtrlHover(t), hov));

    if (Active) {
        D2D1_RECT_F bar{ fr_.left, fr_.top + 6.f, fr_.left + 3.f, fr_.bottom - 6.f };
        ctx.g.RoundRect(bar, 1.5f, t.accent);
    }

    float ix = fr_.left + PL;
    if (!Glyph.empty()) {
        D2D1_RECT_F gr{ ix, fr_.top, ix + 18.f, fr_.bottom };
        ctx.g.Glyph(gr, Glyph, 16.f, Active ? t.accent : Mix(t.text2, t.wz, hov));
        ix += 26.f;
    }

    D2D1_RECT_F tr{ ix, fr_.top, fr_.right - PR, fr_.bottom };
    ctx.g.Text(tr, Text, 13.f, Active ? 600 : 400,
               Active ? t.wz : Mix(t.text2, t.wz, hov),
               HAlign::Left, VAlign::Middle);
}

void NavItem::OnClick() {
    if (!Enabled) return;
    // 同一父容器下的 NavItem 视为单选组：点谁谁亮，兄弟自动熄灭
    if (Parent) {
        for (auto& s : Parent->Children()) {
            auto* n = dynamic_cast<NavItem*>(s.get());
            if (n && n != this) n->Active = false;
        }
    }
    Active = true;
    if (Clicked) Clicked();
}

void NavItem::OnKeyDown(int vk) {
    if (vk == VK_SPACE || vk == VK_RETURN) { OnClick(); return; }
    if (vk != VK_UP && vk != VK_DOWN && vk != VK_LEFT && vk != VK_RIGHT) return;
    if (!Parent || !Host) return;

    // 同父容器内的 NavItem 是一个单选组：方向键顺着走，焦点与选中一起移
    std::vector<NavItem*> sib;
    for (auto& s : Parent->Children()) {
        if (auto* n = dynamic_cast<NavItem*>(s.get()))
            if (n->Visible && n->Enabled) sib.push_back(n);
    }
    if (sib.size() < 2) return;
    int idx = -1;
    for (size_t i = 0; i < sib.size(); ++i) if (sib[i] == this) { idx = int(i); break; }
    if (idx < 0) return;
    int dir = (vk == VK_DOWN || vk == VK_RIGHT) ? 1 : -1;
    int n = int(sib.size());
    NavItem* next = sib[(idx + dir + n) % n];

    Host->SetFocus(next);
    // 切换页面时回调可能把整组 NavItem 重建（旧对象就此销毁），所以这里必须是
    // 本函数最后一个动到 this / next 的操作，之后直接 return。
    next->OnClick();
}

// ---------------- ListRow ----------------

ListRow::ListRow(std::wstring title, std::wstring sub, std::wstring right)
    : Title(std::move(title)), Sub(std::move(sub)), Right(std::move(right)) {
    H = 56.f;
    PL = 14.f;
    PR = 14.f;
    Interactive = true;   // 行本身是控件：不置位的话点击会被判定"没命中"，弹层里的菜单项点不动
}

D2D1_SIZE_F ListRow::Desired(float availW, float availH) {
    (void)availH;
    return D2D1::SizeF(W > 0 ? W : availW, H > 0 ? H : 56.f);
}

void ListRow::Paint(PaintCtx& ctx) {
    const Tokens& t = ctx.t;
    float hov = HoverA();

    if (Selected)         ctx.g.RoundRect(fr_, 4.f, t.accentSel);
    else if (hov > .01f)  ctx.g.RoundRect(fr_, 4.f, A(CtrlHover(t), hov));

    if (Selected) {
        D2D1_RECT_F bar{ fr_.left, fr_.top + 8.f, fr_.left + 3.f, fr_.bottom - 8.f };
        ctx.g.RoundRect(bar, 1.5f, t.accent);
    }

    float x = fr_.left + PL;
    float rightW = 0.f;
    if (!Right.empty()) {
        rightW = Tw(Right, 12.f, 400) + 10.f;
        ctx.g.Text(D2D1_RECT_F{ fr_.right - PR - rightW, fr_.top, fr_.right - PR, fr_.bottom },
                   Right, 12.f, 400, t.text2, HAlign::Right, VAlign::Middle);
    }

    if (Sub.empty()) {
        ctx.g.TextTrim(D2D1_RECT_F{ x, fr_.top, fr_.right - PR - rightW, fr_.bottom },
                       Title, 14.f, Selected ? 600 : 400, t.wz, HAlign::Left, VAlign::Middle);
    } else {
        float cy = (fr_.top + fr_.bottom) * .5f;
        ctx.g.TextTrim(D2D1_RECT_F{ x, cy - 20.f, fr_.right - PR - rightW, cy - 2.f },
                       Title, 14.f, Selected ? 600 : 400, t.wz, HAlign::Left, VAlign::Bottom);
        ctx.g.TextTrim(D2D1_RECT_F{ x, cy + 1.f, fr_.right - PR - rightW, cy + 19.f },
                       Sub, 12.f, 400, t.text2, HAlign::Left, VAlign::Top);
    }
}

// ---------------- IconButton ----------------

IconButton::IconButton(std::wstring glyph, bool ghost)
    : Glyph(std::move(glyph)), Ghost(ghost) {
    W = 32.f;
    H = 32.f;
    Interactive = true;
}

D2D1_SIZE_F IconButton::Desired(float, float) {
    return D2D1::SizeF(W > 0 ? W : 32.f, H > 0 ? H : 32.f);
}

void IconButton::Paint(PaintCtx& ctx) {
    const Tokens& t = ctx.t;
    float rad = 4.f;
    float hov = HoverA(), prs = PressA();

    if (!Ghost) {
        D2D1_COLOR_F fill = Mix(Ctrl(t), CtrlHover(t), hov);
        fill = Mix(fill, t.pressed, prs);
        ctx.g.RoundRect(fr_, rad, fill);
        ctx.g.StrokeRect(fr_, rad, t.border, 1.f);
    } else if (hov > .01f) {
        D2D1_COLOR_F fill = Mix(CtrlHover(t), t.pressed, prs);
        ctx.g.RoundRect(fr_, rad, A(fill, hov));
    }

    ctx.g.Glyph(fr_, Glyph, GlyphSize, Enabled ? t.wz : t.text3, HAlign::Center, VAlign::Middle);
}

void IconButton::OnKeyDown(int vk) {
    if (!Enabled) return;
    if (vk == VK_SPACE || vk == VK_RETURN) OnClick();
}

// ---------------- Dropdown ----------------

Dropdown::Dropdown(std::vector<std::wstring> items, int selected)
    : Items(std::move(items)), Selected(selected) {
    H = 34.f;
    W = 180.f;
    Interactive = true;
    FocusRing = false;   // 展开/聚焦时自身已是 accent 描边
}

D2D1_SIZE_F Dropdown::Desired(float availW, float availH) {
    (void)availW; (void)availH;
    return D2D1::SizeF(W > 0 ? W : 180.f, H > 0 ? H : 34.f);
}

bool Dropdown::Update(float dt) {
    bool changed = Widget::Update(dt);
    // 定长时间线（150ms）+ smoothstep，避免指数尾部拖沓、收尾突然消失
    if (aLinear != (Open ? 1.f : 0.f)) {
        float step = dt / .15f;
        aLinear = Open ? std::min(1.f, aLinear + step) : std::max(0.f, aLinear - step);
        AnimDirty = true;
        changed = true;
        if (menu_) menu_->AnimDirty = true;   // 菜单自己的透明度也变了，必须一起进脏矩形
    }
    float e = aLinear * aLinear * (3.f - 2.f * aLinear);       // smoothstep
    aOpen = e;
    if (menu_) {
        menu_->Opacity = aOpen;
        if (!Open && aLinear <= 0.f) {
            if (Host) {
                Host->ClearDismissIf(this);
                Host->RemoveOverlayDeferred(menu_.get());
                if (catcher_) {
                    Host->RemoveOverlayDeferred(catcher_.get());
                    catcher_.reset();
                }
            }
            menu_.reset();
        }
    }
    return changed;
}

void Dropdown::Paint(PaintCtx& ctx) {
    const Tokens& t = ctx.t;
    float rad = ctx.m.radiusInput;
    float hov = HoverA();

    ctx.g.RoundRect(fr_, rad, Mix(Ctrl(t), CtrlHover(t), hov));
    ctx.g.StrokeRect(fr_, rad, (Open || Focus) ? t.accent : t.border,
                     (Open || Focus) ? 1.6f : 1.f);

    float pad = 12.f;
    float cx = fr_.right - pad - 5.f;
    float cy = (fr_.top + fr_.bottom) * .5f;

    std::wstring label = (Selected >= 0 && Selected < (int)Items.size()) ? Items[Selected] : L"";
    ctx.g.Text(D2D1_RECT_F{ fr_.left + pad, fr_.top, cx - 10.f, fr_.bottom }, label, 13.f, 400,
               t.wz, HAlign::Left, VAlign::Middle);

    // 箭头随展开状态翻转
    float d = aOpen * 4.f;
    ctx.g.Line(cx - 4.f, cy - 2.f + d, cx, cy + 2.f - d, t.text2, 1.4f);
    ctx.g.Line(cx, cy + 2.f - d, cx + 4.f, cy - 2.f + d, t.text2, 1.4f);
}

void Dropdown::OnClickAt(float, float) {
    if (!Enabled) return;
    if (Open) { CloseMenu(); return; }
    OpenMenu();
}

void Dropdown::OpenMenu() {
    if (Items.empty() || !Host) return;

    // 上一次淡出还没走完就再点开：先把旧弹层摘掉，避免同一控件挂两个菜单
    if (menu_) {
        Host->RemoveOverlay(menu_.get());
        menu_.reset();
    }
    if (catcher_) {
        Host->RemoveOverlay(catcher_.get());
        catcher_.reset();
    }
    menuRows_.clear();
    aLinear = 0.f;
    aOpen = 0.f;

    float menuW = MenuWidth > 0 ? MenuWidth : (fr_.right - fr_.left);
    float rowH = 34.f;
    float menuH = float(Items.size()) * rowH + 8.f;

    auto card = std::make_shared<Card>();
    card->PL = card->PT = card->PR = card->PB = 4.f;
    card->Radius = 12.f;   // Fluent 2 X-Large：浮层
    card->Shadow = true;

    auto box = VBox(0.f);
    card->Add(box);

    for (size_t i = 0; i < Items.size(); ++i) {
        auto row = std::make_shared<ListRow>(Items[i], L"", L"");
        row->H = rowH;
        row->PL = 10.f;
        row->PR = 10.f;
        row->Selected = (int(i) == Selected);
        int idx = int(i);
        auto self = this;
        row->Clicked = [self, idx]() {
            self->Selected = idx;
            if (self->Changed) self->Changed(idx);
            self->CloseMenu();
        };
        box->Add(row);
        menuRows_.push_back(row.get());   // 键盘换选中项时要改这些行的 Selected
    }

    card->X = fr_.left;
    card->Y = fr_.bottom + 4.f;
    card->W = menuW;
    card->H = menuH;

    card->Opacity = 0.f;       // 先透明再挂上去：否则第一帧会以全不透明画出来，看起来闪一下

    // 全屏点击毯垫在菜单下面：点在菜单外时由它吃掉，只关菜单，不会点到底下的控件/弹窗
    auto self0 = this;
    catcher_ = std::make_shared<Widget>();
    catcher_->FillHost = true;
    catcher_->Interactive = true;
    catcher_->Clicked = [self0] { self0->CloseMenu(); };
    Host->AddOverlay(catcher_);

    menu_ = card;
    Open = true;
    Host->AddOverlay(card);
    Host->PlayAnim();          // 菜单从 aOpen=0 淡入，必须唤醒帧循环
    Host->SetDismiss([this]() { CloseMenu(); }, this);
}

void Dropdown::SyncMenuSelection() {
    for (size_t i = 0; i < menuRows_.size(); ++i)
        if (auto* r = dynamic_cast<ListRow*>(menuRows_[i]))
            r->Selected = (int(i) == Selected);
}

void Dropdown::OnKeyDown(int vk) {
    if (!Enabled || Items.empty()) return;
    int n = (int)Items.size();
    switch (vk) {
    case VK_SPACE:
    case VK_RETURN:
    case VK_F4:                    // F4 / Alt+Down 是 Windows 惯例的展开键
        if (Open) CloseMenu(); else OpenMenu();
        return;
    case VK_ESCAPE:
        if (Open) CloseMenu();
        return;
    case VK_DOWN:
    case VK_UP: {
        if (!Open) { OpenMenu(); return; }
        int d = (vk == VK_DOWN) ? 1 : -1;
        int sel = std::clamp(Selected + d, 0, n - 1);
        if (sel == Selected) return;
        Selected = sel;
        SyncMenuSelection();
        if (Changed) Changed(Selected);   // 展开时用方向键即时生效（同 WinUI ComboBox）
        if (Host) { Host->PlayAnim(); Host->Invalidate(); }
        return;
    }
    case VK_HOME:
    case VK_END:
        if (!Open) return;
        Selected = (vk == VK_HOME) ? 0 : n - 1;
        SyncMenuSelection();
        if (Changed) Changed(Selected);
        if (Host) Host->Invalidate();
        return;
    default:
        return;
    }
}

void Dropdown::CloseMenu() {
    Open = false;
    menuRows_.clear();
    if (Host) {
        Host->ClearDismissIf(this);
        Host->PlayAnim();      // 淡出同样要帧循环来推 aOpen
    }
    // 菜单等淡出动画结束再销毁（见 Update）
}

// ---------------- TitleBar ----------------

namespace {

// 标题栏按钮：无边框、悬停亮底；关闭按钮悬停变红（Win11 风格）。
class TitleButton : public Widget {
public:
    std::wstring Glyph;
    bool Danger = false;

    explicit TitleButton(std::wstring g, bool danger = false)
        : Glyph(std::move(g)), Danger(danger) {
        W = 46.f;
        H = 40.f;
        Interactive = true;
    }

    D2D1_SIZE_F Desired(float, float) override {
        return D2D1::SizeF(W > 0 ? W : 46.f, H > 0 ? H : 40.f);
    }

    void Paint(PaintCtx& ctx) override {
        float hov = HoverA(), prs = PressA();
        if (Danger) {
            if (hov > .01f) {
                D2D1_COLOR_F red = Mix(Hex(0xC42B1C), Hex(0xB0281A), prs);
                red.a = hov;
                ctx.g.FillRect(fr_, red);
            }
            ctx.g.Glyph(fr_, Glyph, 10.f, Mix(ctx.t.text2, Hex(0xFFFFFF), hov));
        } else {
            if (hov > .01f) ctx.g.FillRect(fr_, A(CtrlHover(ctx.t), hov));
            ctx.g.Glyph(fr_, Glyph, 10.f, Mix(ctx.t.text2, ctx.t.wz, hov));
        }
    }

    CursorShape CursorAt(float, float) const override { return CursorShape::Hand; }
};

// 顶栏导航项：圆角胶囊，选中用 accentSoft + accent 文字。
class NavChip : public Widget {
public:
    std::wstring Text;
    bool Active = false;

    explicit NavChip(std::wstring t) : Text(std::move(t)) {
        H = 30.f;
        Interactive = true;
    }

    D2D1_SIZE_F Desired(float, float) override {
        return D2D1::SizeF(W > 0 ? W : 80.f, H > 0 ? H : 30.f);
    }

    void Paint(PaintCtx& ctx) override {
        float hov = HoverA();
        if (Active) ctx.g.RoundRect(fr_, 6.f, ctx.t.accentSoft);
        else if (hov > .01f) ctx.g.RoundRect(fr_, 6.f, A(CtrlHover(ctx.t), hov));
        ctx.g.Text(fr_, Text, 13.f, Active ? 600 : 400,
                   Active ? ctx.t.accent : Mix(ctx.t.text2, ctx.t.wz, hov),
                   HAlign::Center, VAlign::Middle);
    }

    CursorShape CursorAt(float, float) const override { return CursorShape::Hand; }
};

} // namespace

TitleBar::TitleBar() {
    H = 40.f;
    Caption = true;        // 非按钮区域整条都是窗口拖动区
    Interactive = false;   // 让点击冒泡到 Caption 判定

    min_ = std::make_shared<TitleButton>(L"\xE921");                 // ChromeMinimize
    max_ = std::make_shared<TitleButton>(L"\xE922");                 // ChromeMaximize
    close_ = std::make_shared<TitleButton>(L"\xE8BB", true);         // ChromeClose

    min_->Clicked = [this] { if (Host) Host->Minimize(); };
    max_->Clicked = [this] { if (Host) Host->ToggleMaximize(); };
    close_->Clicked = [this] {
        if (OnCloseRequested && OnCloseRequested()) return;
        if (Host) Host->Close();
    };

    Add(min_);
    Add(max_);
    Add(close_);
}

D2D1_SIZE_F TitleBar::Desired(float availW, float) {
    return D2D1::SizeF(W > 0 ? W : availW, H > 0 ? H : 40.f);
}

void TitleBar::SyncNav() {
    if (nav_.size() == NavItems.size()) {
        for (size_t i = 0; i < nav_.size(); ++i)
            static_cast<NavChip*>(nav_[i].get())->Text = NavItems[i];
        return;
    }
    // 项数变了就重建子项（按钮的 shared_ptr 一直在，重新挂上去即可）
    nav_.clear();
    Clear();
    Add(min_);
    Add(max_);
    Add(close_);
    for (size_t i = 0; i < NavItems.size(); ++i) {
        auto chip = std::make_shared<NavChip>(NavItems[i]);
        size_t idx = i;
        chip->Clicked = [this, idx] {
            NavIndex = (int)idx;
            if (NavChanged) NavChanged(NavIndex);
            if (Host) Host->Invalidate();
        };
        nav_.push_back(chip);
        Add(chip);
    }
}

void TitleBar::LayoutChildren() {
    float bh = fr_.bottom - fr_.top;
    float x = fr_.right;
    Widget* order[] = { close_.get(), max_.get(), min_.get() };
    bool shown[] = { ShowClose, ShowMaximize, ShowMinimize };
    for (int i = 0; i < 3; ++i) {
        order[i]->Visible = shown[i];
        if (!shown[i]) continue;
        float bw = BtnWidth > 0 ? BtnWidth : 46.f;
        x -= bw;
        order[i]->W = bw;
        order[i]->H = bh;
        order[i]->Arrange(D2D1_RECT_F{ x, fr_.top, x + bw, fr_.top + bh });
    }

    SyncNav();
    if (nav_.empty()) return;

    // 先量宽，再决定起点（整栏跟在标题后，最小居中则整体居中）
    std::vector<float> ws;
    float total = 0.f;
    for (auto& n : nav_) {
        auto* c = static_cast<NavChip*>(n.get());
        float w = Tw(c->Text, 13.f, 400) + 24.f;
        ws.push_back(w);
        total += w + NavGap;
    }
    total -= NavGap;

    float leftBound = fr_.left + 16.f;
    if (!Title.empty()) leftBound += Tw(Title, 12.f, 400) + 24.f;
    float start = NavCentered ? fr_.left + ((x - fr_.left) - total) * .5f : leftBound;
    if (start < leftBound) start = leftBound;

    float chipH = 30.f;
    float y = fr_.top + (bh - chipH) * .5f;
    float cx = start;
    for (size_t i = 0; i < nav_.size(); ++i) {
        auto* c = static_cast<NavChip*>(nav_[i].get());
        c->Active = (int(i) == NavIndex);
        c->W = ws[i];
        c->H = chipH;
        c->Arrange(D2D1_RECT_F{ cx, y, cx + ws[i], y + chipH });
        cx += ws[i] + NavGap;
    }
}

void TitleBar::Paint(PaintCtx& ctx) {
    if (!Visible) return;
    if (Fill) ctx.g.FillRect(fr_, ctx.t.kp);

    // 最大化/还原图标跟随窗口状态
    TitleButton* mb = static_cast<TitleButton*>(max_.get());
    mb->Glyph = (Host && Host->Maximized()) ? L"\xE923" : L"\xE922";

    if (!Title.empty()) {
        D2D1_RECT_F tr{ fr_.left + 16.f, fr_.top, fr_.right * .5f, fr_.bottom };
        ctx.g.TextTrim(tr, Title, 12.f, 400, ctx.t.text2, HAlign::Left, VAlign::Middle);
    }
}

} // namespace vcui
