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

// 反馈类控件：悬停提示 / 提示条（Toast、InfoBar） / 模态对话框。
// 三者都是覆盖层，由 Surface 托管，不参与页面布局。

#include "vcui/Controls.h"

#include "vcui/Surface.h"
#include <algorithm>
#include <cmath>

namespace vcui {

namespace {

// 语气色：提示条左侧竖条与图标用它
D2D1_COLOR_F ToneColor(const Tokens& t, InfoTone tone) {
    switch (tone) {
    case InfoTone::Success: return Hex(0x0F7B0F);
    case InfoTone::Warning: return Hex(0x9D5D00);
    case InfoTone::Danger: return Hex(0xC42B1C);
    default: return t.accent;
    }
}

// Segoe MDL2 图标：信息 / 勾 / 警告三角 / 错误圆
const wchar_t* ToneGlyph(InfoTone tone) {
    switch (tone) {
    case InfoTone::Success: return L"\xE930";   // Completed
    case InfoTone::Warning: return L"\xE7BA";   // Warning
    case InfoTone::Danger: return L"\xE783";    // Error
    default: return L"\xE946";                  // Info
    }
}

} // namespace

// ---------------- Tooltip ----------------

D2D1_SIZE_F Tooltip::Desired(float, float) {
    float w = W > 0 ? W : (Tw(Text, 12.f, 400) + 26.f);
    return D2D1::SizeF(w, H > 0 ? H : 28.f);
}

void Tooltip::Paint(PaintCtx& ctx) {
    const Tokens& t = ctx.t;
    float r = 4.f;
    ctx.g.Shadow(fr_, r, 6.f, 2.f, C(0, 0, 0, t.dark ? .26f : .07f));
    ctx.g.RoundRect(fr_, r, t.dark ? Mix(t.kp, t.wz, .09f) : t.kp);
    ctx.g.StrokeRect(fr_, r, t.border, 1.f);
    ctx.g.TextTrim(D2D1_RECT_F{ fr_.left + 10.f, fr_.top, fr_.right - 10.f, fr_.bottom }, Text,
                   12.f, 400, t.wz, HAlign::Left, VAlign::Middle);
}

// ---------------- InfoBar ----------------

InfoBar::InfoBar(std::wstring text, InfoTone tone, float lifeMs)
    : Text(std::move(text)), ToneKind(tone), LifeMs(lifeMs) {
    Interactive = true;
    H = 52.f;
    lastMs_ = GetTickCount64();
}

D2D1_SIZE_F InfoBar::Desired(float, float) {
    if (W > 0) return D2D1::SizeF(W, H > 0 ? H : 52.f);
    // 文字左边留给竖条+图标（42），右边留给关闭按钮（40），再留 4 余量防半像素截断
    float w = Tw(Text, 14.f, 400) + 86.f;
    return D2D1::SizeF(std::min(460.f, std::max(260.f, w)), H > 0 ? H : 52.f);
}

void InfoBar::TickClock() {
    unsigned long long now = GetTickCount64();
    unsigned long long d = now - lastMs_;
    lastMs_ = now;
    if (!Exiting && !Hover) holdMs_ += float(d);
}

bool InfoBar::Update(float dt) {
    bool changed = Widget::Update(dt);

    float inTarget = Exiting ? 0.f : 1.f;
    if (aIn != inTarget) {
        // 进入 0.22s / 退出 0.16s
        float k = 1.f - std::exp(-dt / (Exiting ? .16f : .22f));
        aIn += (inTarget - aIn) * k;
        if (std::abs(inTarget - aIn) < .004f) aIn = inTarget;
        changed = true;
        AnimDirty = true;
    }
    aOut = 1.f - aIn;
    if (Exiting && aIn <= 0.f) {
        if (Host) Host->RemoveOverlayDeferred(this);
        if (Closed) { auto fn = Closed; Closed = nullptr; fn(); }
    }

    Opacity = aIn;
    // 右侧滑入：从 +24dip 处推入到位
    X = baseX + 24.f * (1.f - aIn);
    return changed;
}

void InfoBar::Paint(PaintCtx& ctx) {
    const Tokens& t = ctx.t;
    D2D1_COLOR_F accent = ToneColor(t, ToneKind);

    // 卡片底：深浅色都用 kp，靠边框和阴影跟背景分层
    ctx.g.RoundRect(fr_, 4.f, t.dark ? Mix(t.kp, t.wz, .06f) : t.kp);
    ctx.g.StrokeRect(fr_, 4.f, t.border, 1.f);
    // 左侧语气竖条
    ctx.g.RoundRect(D2D1_RECT_F{ fr_.left, fr_.top + 1.f, fr_.left + 3.f, fr_.bottom - 1.f },
                    1.5f, accent);
    // 图标
    ctx.g.Glyph(D2D1_RECT_F{ fr_.left + 12.f, fr_.top, fr_.left + 36.f, fr_.bottom },
                ToneGlyph(ToneKind), 15.f, accent);
    // 正文（留出关闭按钮位置）
    ctx.g.TextTrim(D2D1_RECT_F{ fr_.left + 42.f, fr_.top, fr_.right - 40.f, fr_.bottom },
                   Text, 14.f, 400, t.wz, HAlign::Left, VAlign::Middle);
    // 关闭
    bool hovClose = Hover;   // 简化：整条悬停时 ✕ 提亮
    ctx.g.Glyph(D2D1_RECT_F{ fr_.right - 34.f, fr_.top, fr_.right - 6.f, fr_.bottom }, L"\xE711",
                12.f, hovClose ? t.wz : t.text2);
}

void InfoBar::OnClickAt(float x, float) {
    if (x >= fr_.right - 34.f) StartExit();
}

CursorShape InfoBar::CursorAt(float x, float) const {
    return x >= fr_.right - 34.f ? CursorShape::Hand : CursorShape::Arrow;
}

void InfoBar::StartExit() {
    if (Exiting) return;
    Exiting = true;
    if (Host) Host->PlayAnim();
}

// ---------------- Dialog ----------------

Dialog::Dialog() {
    Interactive = true;   // 吃掉遮罩上的点击，避免穿透到页面
    FillHost = true;
    FocusRing = false;    // 卡片按钮自己会画，遮罩层不需要环
}

D2D1_SIZE_F Dialog::Desired(float, float) {
    return D2D1::SizeF(W > 0 ? W : 1180.f, H > 0 ? H : 740.f);
}

void Dialog::LayoutChildren() {
    if (!card_) return;
    float w = fr_.right - fr_.left, h = fr_.bottom - fr_.top;
    float cw = std::min(CardWidth, w - 32.f);
    D2D1_SIZE_F d = card_->Desired(cw, h - 32.f);
    float ch = d.height;
    if (ch > h - 32.f) ch = h - 32.f;
    cardH_ = ch;
    float x = fr_.left + (w - cw) * .5f;
    float y = fr_.top + (h - ch) * .5f;
    card_->Arrange(D2D1_RECT_F{ x, y, x + cw, y + ch });
}

void Dialog::Paint(PaintCtx& ctx) {
    // 压暗遮罩：深色下用更重的黑，浅色下略轻
    ctx.g.FillRect(fr_, Overlay(ctx.t, ctx.t.dark ? .55f : .42f));
}

void Dialog::OnClickAt(float x, float y) {
    if (!DismissOnScrim || !card_) return;
    D2D1_RECT_F c = card_->Frame();
    if (x >= c.left && x < c.right && y >= c.top && y < c.bottom) return;   // 点在卡片上
    Close();
}

void Dialog::Close() {
    if (Dead) return;
    auto fn = Closed;
    Closed = nullptr;
    if (Host) {
        Host->ClearDismissIf(this);
        Host->RemoveOverlayDeferred(this);
        Host->PlayAnim();
    }
    if (fn) fn();
}

std::shared_ptr<Dialog> Dialog::Open(Surface* host, std::wstring title, std::wstring message,
                                     std::vector<DialogAction> actions, bool dismissOnScrim,
                                     float cardWidth) {
    auto dlg = std::make_shared<Dialog>();
    if (!host) return dlg;
    dlg->Title = std::move(title);
    dlg->Message = std::move(message);
    dlg->Actions = std::move(actions);
    dlg->DismissOnScrim = dismissOnScrim;
    dlg->CardWidth = cardWidth;
    if (dlg->Actions.empty()) dlg->Actions.push_back(DialogAction{ L"确定", true, false, nullptr });

    D2D1_SIZE_F sz = host->ClientSize();
    dlg->W = sz.width;
    dlg->H = sz.height;
    dlg->X = 0.f;
    dlg->Y = 0.f;

    float cw = std::min(cardWidth, sz.width - 32.f);
    float innerW = cw - 48.f;        // 卡片左右各 24 内边距

    auto card = std::make_shared<Card>();
    card->Pad(24.f);
    card->Radius = 12.f;
    card->Shadow = true;
    dlg->card_ = card;
    dlg->Add(card);

    auto col = VBox(10.f);
    card->Add(col);

    // 标题
    auto titleLb = std::make_shared<Label>(dlg->Title);
    titleLb->UseType(Type::Subtitle1);
    col->Add(titleLb);

    // 正文：按卡片宽度实际排版量高（Label 在固定宽度里自动换行）
    if (!dlg->Message.empty()) {
        auto msg = std::make_shared<Label>(dlg->Message, 14.f, 400);
        msg->UseType(Type::Body1);
        msg->Wrap = true;
        msg->W = innerW;
        msg->Color = host->Th().t.text2;
        if (auto lay = host->G().Layout(dlg->Message, 14.f, 400, L"Microsoft YaHei",
                                        HAlign::Left, VAlign::Top, innerW, 4096.f, true)) {
            DWRITE_TEXT_METRICS tm{};
            lay->GetMetrics(&tm);
            msg->H = tm.height > 1.f ? tm.height : LineH(14.f);
        }
        col->Add(msg);
    }

    col->Add(Spacer(6.f));

    // 按钮行：右对齐
    auto row = HBox(8.f);
    auto gap = std::make_shared<Widget>();
    gap->Grow = 1.f;
    row->Add(gap);
    Widget* firstBtn = nullptr;
    for (auto& a : dlg->Actions) {
        auto btn = std::make_shared<Button>(a.Text, a.Primary);
        btn->Danger = a.Danger;
        btn->MinWidth = 76.f;
        std::function<void()> cb = a.OnClick;
        auto d = dlg.get();
        btn->Clicked = [cb, d]() {
            if (cb) cb();
            d->Close();
        };
        row->Add(btn);
        if (!firstBtn || a.Primary) firstBtn = btn.get();   // 主按钮优先接管焦点
    }
    col->Add(row);

    host->AddOverlay(dlg);
    // Esc 关闭：借 Surface 的 dismiss 通道（遮罩覆盖全窗，不会误触发外点收起）
    std::shared_ptr<Dialog> keep = dlg;
    host->SetDismiss([keep]() { keep->Close(); }, dlg.get());
    // 键盘可用：打开即把焦点给主按钮，回车直接确认、Tab 可在按钮间切换
    if (firstBtn) host->SetFocus(firstBtn);
    host->PlayAnim();
    host->Invalidate();
    return dlg;
}

} // namespace vcui
