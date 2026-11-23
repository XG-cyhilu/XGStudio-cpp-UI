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
#include <imm.h>

namespace vcui {

// ---------------- Label ----------------

D2D1_SIZE_F Label::Desired(float availW, float availH) {
    (void)availH;
    float w = W > 0 ? W : Tw(Text, Size, Weight);
    float h = H > 0 ? H : (LineHeight > 0.f ? LineHeight : LineH(Size));
    // Wrap 且宽度已定：按实际排版的行数算高，多行文本才不会被压成一行
    if (W > 0 && Wrap && GFX) {
        if (auto lay = GFX->Layout(Text, Size, Weight, L"Microsoft YaHei", Align, VAlignMode, W,
                                   4096.f, true)) {
            DWRITE_TEXT_METRICS tm{};
            lay->GetMetrics(&tm);
            if (tm.height > 1.f) h = tm.height;
        }
    }
    return D2D1::SizeF(w, h);
}

void Label::Paint(PaintCtx& ctx) {
    if (!Visible || Text.empty()) return;
    D2D1_COLOR_F col = Color.a > 0.f ? Color : ctx.t.wz;
    if (Ellipsis) {
        ctx.g.TextTrim(fr_, Text, Size, Weight, col, Align, VAlignMode);
    } else {
        ctx.g.Text(fr_, Text, Size, Weight, col, Align, VAlignMode, Wrap);
    }
}

// ---------------- Button ----------------

Button::Button(std::wstring text, bool primary) : Text(std::move(text)), Primary(primary) {
    H = primary ? 36.f : 34.f;
    Interactive = true;
}

D2D1_SIZE_F Button::Desired(float availW, float availH) {
    (void)availW; (void)availH;
    float w = W;
    if (w <= 0) {
        float tw = Tw(Text, 14.f, 400);
        w = std::max(MinWidth, tw + 34.f);
        if (!Glyph.empty()) w += 22.f;
    }
    return D2D1::SizeF(w, H > 0 ? H : 34.f);
}

void Button::Paint(PaintCtx& ctx) {
    const Tokens& t = ctx.t;
    float r = ctx.m.radiusBtn;
    float hov = HoverA(), prs = PressA();

    D2D1_COLOR_F fill, fg, brd{ 0, 0, 0, 0 };
    if (Danger) {
        // 危险操作：平时红字描边，悬停/按下变红底白字（Win11 危险按钮的观感）
        fill = Mix(Ctrl(t), Hex(0xC42B1C), hov);
        fill = Mix(fill, Hex(0xB0281A), prs);
        fg = Mix(Hex(0xC42B1C), Hex(0xFFFFFF), hov);
        brd = A(Hex(0xC42B1C), .55f + .45f * std::max(hov, prs));
    } else if (Primary) {
        fill = Mix(t.accent, t.accentHover, hov);
        fill = Mix(fill, t.accentPress, prs);
        fg = t.onAccent;
    } else {
        fill = Mix(Ctrl(t), CtrlHover(t), hov);
        fill = Mix(fill, t.pressed, prs);
        fg = t.wz;
        brd = Mix(t.border, t.accent, Focus ? .45f : 0.f);
    }
    if (!Enabled) { fill = t.xt; fg = t.text3; brd = t.border; }

    ctx.g.RoundRect(fr_, r, fill);
    if (brd.a > 0.f) ctx.g.StrokeRect(fr_, r, brd, 1.f);

    if (!Glyph.empty()) {
        D2D1_RECT_F gr{ fr_.left + 12.f, fr_.top, fr_.left + 30.f, fr_.bottom };
        ctx.g.Glyph(gr, Glyph, 14.f, Primary ? fg : (Enabled ? t.wz : t.text3));
        ctx.g.Text(D2D1_RECT_F{ fr_.left + 34.f, fr_.top, fr_.right - 12.f, fr_.bottom },
                   Text, 13.f, Primary ? 600 : 400, fg, HAlign::Left, VAlign::Middle);
    } else {
        ctx.g.Text(fr_, Text, 13.f, Primary ? 600 : 400, fg, HAlign::Center, VAlign::Middle);
    }
}

void Button::OnKeyDown(int vk) {
    // Fluent 按钮：空格与回车都算激活；禁用时不响应
    if (!Enabled) return;
    if (vk == VK_SPACE || vk == VK_RETURN) OnClick();
}

// ---------------- Checkbox ----------------

Checkbox::Checkbox(std::wstring text, bool on) : Text(std::move(text)), On(on) {
    H = 26.f;
    Interactive = true;
}

D2D1_SIZE_F Checkbox::Desired(float availW, float availH) {
    (void)availW; (void)availH;
    return D2D1::SizeF(W > 0 ? W : 20.f + 10.f + Tw(Text, 13.f, 400), H > 0 ? H : 26.f);
}

bool Checkbox::Update(float dt) {
    bool changed = Widget::Update(dt);
    if (aOn < 0.f) aOn = On ? 1.f : 0.f;
    float target = On ? 1.f : 0.f;
    if (aOn != target) {
        float k = 1.f - std::exp(-dt / .12f);
        aOn += (target - aOn) * k;
        if (std::abs(target - aOn) < .004f) aOn = target;
        changed = true;
        AnimDirty = true;
    }
    return changed;
}

void Checkbox::Paint(PaintCtx& ctx) {
    const Tokens& t = ctx.t;
    float hov = HoverA(), prs = PressA();
    float on = aOn < 0.f ? (On ? 1.f : 0.f) : aOn;
    float box = 20.f;
    float top = fr_.top + ((fr_.bottom - fr_.top) - box) * .5f;
    D2D1_RECT_F rc{ fr_.left, top, fr_.left + box, top + box };

    D2D1_COLOR_F ctrl = Mix(Ctrl(t), CtrlHover(t), hov);
    D2D1_COLOR_F acc = Mix(t.accent, t.accentHover, hov);
    acc = Mix(acc, t.accentPress, prs);
    D2D1_COLOR_F fill = Mix(ctrl, acc, on);
    D2D1_COLOR_F brd = Mix(Mix(t.border, t.wz, .12f + .12f * hov), acc, on);
    if (!Enabled) { fill = t.xt; brd = t.border; }

    ctx.g.RoundRect(rc, 2.5f, fill);
    ctx.g.StrokeRect(rc, 2.5f, brd, 1.f);

    if (on > .02f) {
        D2D1_COLOR_F cc = Enabled ? t.onAccent : t.text3;
        cc.a *= on;
        float cx = rc.left + box * .5f, cy = (rc.top + rc.bottom) * .5f;
        float s = .6f + .4f * on;   // 勾先小后到位
        ctx.g.Line(cx - 4.2f * s, cy + .6f, cx - 1.2f * s, cy + 3.4f * s, cc, 1.6f);
        ctx.g.Line(cx - 1.2f * s, cy + 3.4f * s, cx + 4.6f * s, cy - 3.2f * s, cc, 1.6f);
    }

    if (!Text.empty()) {
        ctx.g.Text(D2D1_RECT_F{ rc.right + 10.f, fr_.top, fr_.right, fr_.bottom },
                   Text, 13.f, 400, Enabled ? t.wz : t.text3, HAlign::Left, VAlign::Middle);
    }
}

void Checkbox::OnClick() {
    if (!Enabled) return;
    On = !On;
    if (Changed) Changed(On);
}

void Checkbox::OnKeyDown(int vk) {
    if (vk == VK_SPACE) OnClick();
}

// ---------------- Switch ----------------

D2D1_SIZE_F Switch::Desired(float, float) {
    return D2D1::SizeF(40.f, 26.f);
}

bool Switch::Update(float dt) {
    bool changed = Widget::Update(dt);
    if (aKnob < 0.f) aKnob = On ? 1.f : 0.f;
    float target = On ? 1.f : 0.f;
    if (aKnob != target) {
        float k = 1.f - std::exp(-dt / .11f);
        aKnob += (target - aKnob) * k;
        if (std::abs(target - aKnob) < .004f) aKnob = target;
        changed = true;
        AnimDirty = true;
    }
    return changed;
}

void Switch::Paint(PaintCtx& ctx) {
    const Tokens& t = ctx.t;
    float hov = HoverA(), prs = PressA();
    float w = 40.f, th = 20.f;
    D2D1_RECT_F tr{ fr_.left, fr_.top + ((fr_.bottom - fr_.top) - th) * .5f, fr_.left + w,
                    fr_.top + ((fr_.bottom - fr_.top) - th) * .5f + th };

    D2D1_COLOR_F track;
    if (!On) {
        track = Mix(t.text3, t.wz, .10f + .10f * hov);
    } else {
        track = Mix(t.accent, t.accentHover, hov);
        track = Mix(track, t.accentPress, prs);
    }
    if (!Enabled) track = Mix(track, t.bj, .45f);
    ctx.g.RoundRect(tr, th * .5f, track);

    float k = aKnob < 0.f ? (On ? 1.f : 0.f) : aKnob;
    float knobR = 6.f + ((Hover || Press) ? .5f : 0.f);
    float inset = knobR + 3.f;
    float kx = tr.left + inset + (tr.right - tr.left - inset * 2.f) * k;
    D2D1_COLOR_F knob = On ? t.onAccent : (ctx.t.dark ? Hex(0xc9c9c9) : Hex(0x5f5f5f));
    ctx.g.Circle(kx, (tr.top + tr.bottom) * .5f, knobR, knob);
}

void Switch::OnClick() {
    if (!Enabled) return;
    On = !On;
    if (Changed) Changed(On);
}

void Switch::OnKeyDown(int vk) {
    if (vk == VK_SPACE) OnClick();
}

// ---------------- Input ----------------

Input::Input() {
    H = 34.f;
    W = -1.f;   // 由布局决定宽度（单行时自然宽 240）
    Interactive = true;
    FocusRing = false;   // 焦点已有 accent 描边 + 光标，再套环会双线
}

Input::Input(std::wstring placeholder) : Placeholder(std::move(placeholder)) {
    H = 34.f;
    W = -1.f;
    Interactive = true;
    FocusRing = false;
}

D2D1_SIZE_F Input::Desired(float availW, float availH) {
    (void)availH;
    float w = W > 0 ? std::min(W, MaxWidth) : 240.f;
    return D2D1::SizeF(w, H > 0 ? H : 34.f);
}

void Input::ClampCaret() {
    int n = (int)Text.size();
    if (Caret < 0) Caret = 0;
    if (Caret > n) Caret = n;
    if (SelAnchor < 0) SelAnchor = 0;
    if (SelAnchor > n) SelAnchor = n;
}

void Input::DelSelection() {
    int lo = SelLo(), hi = SelHi();
    if (lo == hi) return;
    Text.erase(Text.begin() + lo, Text.begin() + hi);
    Caret = lo;
    SelAnchor = lo;
    if (Changed) Changed(Text);
}

int Input::PosFromX(float x) {
    float pad = 12.f;
    float rel = x - (fr_.left + pad);
    if (rel <= 0.f) return 0;
    int best = 0;
    float bestD = 1e9f;
    for (int i = 0; i <= (int)Text.size(); ++i) {
        float w = Tw(Text.substr(0, (size_t)i), 14.f, 400);
        float d = std::abs(w - rel);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

bool Input::Update(float dt) {
    bool changed = Widget::Update(dt);
    if (Focus) {
        blinkT_ += dt;
        float ph = std::fmod(blinkT_, 1.06f);
        bool on = ph < .55f;
        if (on != blinkOn_) { blinkOn_ = on; changed = true; AnimDirty = true; }
    } else if (!blinkOn_) {
        blinkOn_ = true;
    }
    return changed;
}

void Input::Paint(PaintCtx& ctx) {
    const Tokens& t = ctx.t;
    const Metrics& m = ctx.m;
    float rad = m.radiusInput;
    float hov = HoverA();

    ctx.g.RoundRect(fr_, rad, Ctrl(t));
    ctx.g.StrokeRect(fr_, rad, Focus ? t.accent : Mix(t.border, t.wz, .16f * hov),
                     Focus ? 1.6f : 1.f);

    float pad = 12.f;
    D2D1_RECT_F tr{ fr_.left + pad, fr_.top, fr_.right - pad, fr_.bottom };
    float textY = fr_.top + ((fr_.bottom - fr_.top) - LineH(m.fBody)) * .5f;

    if (Text.empty() && !Placeholder.empty() && comp_.empty()) {
        ctx.g.Text(tr, Placeholder, m.fBody, 400, t.text2, HAlign::Left, VAlign::Middle);
    }

    // 选区
    int lo = SelLo(), hi = SelHi();
    if (lo != hi && !Text.empty()) {
        float x1 = tr.left + Tw(Text.substr(0, (size_t)lo), m.fBody, 400);
        float x2 = tr.left + Tw(Text.substr(0, (size_t)hi), m.fBody, 400);
        ctx.g.FillRect(D2D1_RECT_F{ x1, textY - 2.f, x2, textY + LineH(m.fBody) + 2.f },
                       A(t.accent, .28f));
    }

    if (!Text.empty())
        ctx.g.Text(tr, Text, m.fBody, 400, t.wz, HAlign::Left, VAlign::Middle);

    // 输入法组合串（带下划线）
    float caretX = tr.left + Tw(Text.substr(0, (size_t)Caret), m.fBody, 400);
    if (!comp_.empty()) {
        ctx.g.Text(D2D1_RECT_F{ caretX, fr_.top, tr.right, fr_.bottom }, comp_, m.fBody, 600,
                   t.wz, HAlign::Left, VAlign::Middle);
        float cw = Tw(comp_, m.fBody, 600);
        ctx.g.Line(caretX, textY + m.fBody * 1.35f, caretX + cw, textY + m.fBody * 1.35f,
                   t.accent, 1.f);
    }

    if (Focus && blinkOn_) {
        float top = textY - 1.f;
        ctx.g.Line(caretX + .5f, top, caretX + .5f, top + LineH(m.fBody) + 2.f, t.wz, 1.f);
    }
}

void Input::OnFocus(bool on) {
    blinkT_ = 0.f;
    blinkOn_ = true;
    if (on) {
        ClampCaret();
        Caret = (int)Text.size();
        SelAnchor = Caret;
    }
}

void Input::OnChar(wchar_t c) {
    if (c < 32) return;
    if (imeActive_) return;   // 组合中的字符由 WM_IME_COMPOSITION 处理
    DelSelection();
    undo_ = Text;
    Text.insert(Text.begin() + Caret, c);
    ++Caret;
    SelAnchor = Caret;
    blinkT_ = 0.f;
    blinkOn_ = true;
    if (Changed) Changed(Text);
}

void Input::OnKeyDown(int vk) {
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    ClampCaret();

    if (ctrl) {
        switch (vk) {
        case 'A':
            SelAnchor = 0;
            Caret = (int)Text.size();
            return;
        case 'C': {
            int lo = SelLo(), hi = SelHi();
            if (lo == hi) return;
            std::wstring sel = Text.substr(lo, hi - lo);
            if (OpenClipboard(Host ? Host->Hwnd() : nullptr)) {
                EmptyClipboard();
                HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (sel.size() + 1) * sizeof(wchar_t));
                if (mem) {
                    void* p = GlobalLock(mem);
                    memcpy(p, sel.c_str(), (sel.size() + 1) * sizeof(wchar_t));
                    GlobalUnlock(mem);
                    SetClipboardData(CF_UNICODETEXT, mem);
                }
                CloseClipboard();
            }
            return;
        }
        case 'X': {
            int lo = SelLo(), hi = SelHi();
            if (lo == hi) return;
            std::wstring sel = Text.substr(lo, hi - lo);
            if (OpenClipboard(Host ? Host->Hwnd() : nullptr)) {
                EmptyClipboard();
                HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (sel.size() + 1) * sizeof(wchar_t));
                if (mem) {
                    void* p = GlobalLock(mem);
                    memcpy(p, sel.c_str(), (sel.size() + 1) * sizeof(wchar_t));
                    GlobalUnlock(mem);
                    SetClipboardData(CF_UNICODETEXT, mem);
                }
                CloseClipboard();
            }
            undo_ = Text;
            DelSelection();
            if (Changed) Changed(Text);
            return;
        }
        case 'V': {
            if (!OpenClipboard(Host ? Host->Hwnd() : nullptr)) return;
            if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
                const wchar_t* p = (const wchar_t*)GlobalLock(h);
                if (p) {
                    std::wstring add(p);
                    // 去掉换行，单行输入
                    for (auto& ch : add) if (ch == L'\r' || ch == L'\n' || ch == L'\t') ch = L' ';
                    undo_ = Text;
                    DelSelection();
                    Text.insert(Caret, add);
                    Caret += (int)add.size();
                    SelAnchor = Caret;
                    if (Changed) Changed(Text);
                    GlobalUnlock(h);
                }
            }
            CloseClipboard();
            return;
        }
        case 'Z':
            if (!undo_.empty()) {
                Text = undo_;
                undo_.clear();
                Caret = (int)Text.size();
                SelAnchor = Caret;
                if (Changed) Changed(Text);
            }
            return;
        default: break;
        }
    }

    switch (vk) {
    case VK_BACK:
        undo_ = Text;
        if (SelLo() != SelHi()) { DelSelection(); }
        else if (Caret > 0) { Text.erase(Text.begin() + (Caret - 1)); --Caret; SelAnchor = Caret; }
        if (Changed) Changed(Text);
        break;
    case VK_DELETE:
        undo_ = Text;
        if (SelLo() != SelHi()) { DelSelection(); }
        else if (Caret < (int)Text.size()) { Text.erase(Text.begin() + Caret); }
        if (Changed) Changed(Text);
        break;
    case VK_LEFT:
        Caret = std::max(0, Caret - 1);
        if (!shift) SelAnchor = Caret;
        break;
    case VK_RIGHT:
        Caret = std::min((int)Text.size(), Caret + 1);
        if (!shift) SelAnchor = Caret;
        break;
    case VK_HOME:
        Caret = 0;
        if (!shift) SelAnchor = Caret;
        break;
    case VK_END:
        Caret = (int)Text.size();
        if (!shift) SelAnchor = Caret;
        break;
    case VK_RETURN:
        if (Submitted) Submitted();
        break;
    default: break;
    }
    blinkT_ = 0.f;
    blinkOn_ = true;
}

void Input::OnClickAt(float x, float) {
    Caret = PosFromX(x);
    if (!(GetKeyState(VK_SHIFT) & 0x8000)) SelAnchor = Caret;
    dragAnchor_ = SelAnchor;
    blinkT_ = 0.f;
    blinkOn_ = true;
}

void Input::OnDrag(float x, float) {
    if (dragAnchor_ < 0) return;
    SelAnchor = dragAnchor_;
    Caret = PosFromX(x);
}

bool Input::OnImeMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    (void)wp;
    HIMC imc = ImmGetContext(hwnd);
    switch (msg) {
    case WM_IME_STARTCOMPOSITION:
        comp_.clear();
        imeActive_ = true;
        if (imc) {
            // 候选窗跟随光标
            float pad = 12.f;
            float x = fr_.left + pad + Tw(Text.substr(0, (size_t)Caret), 14.f, 400);
            float y = fr_.top + (fr_.bottom - fr_.top);
            COMPOSITIONFORM cf{};
            cf.dwStyle = CFS_POINT;
            cf.ptCurrentPos.x = (LONG)x;
            cf.ptCurrentPos.y = (LONG)(fr_.top + 6.f);
            ImmSetCompositionWindow(imc, &cf);
            CANDIDATEFORM cdf{};
            cdf.dwIndex = 0;
            cdf.dwStyle = CFS_EXCLUDE;
            cdf.ptCurrentPos.x = (LONG)x;
            cdf.ptCurrentPos.y = (LONG)y;
            cdf.rcArea.left = (LONG)(fr_.left + pad);
            cdf.rcArea.top = (LONG)fr_.top;
            cdf.rcArea.right = (LONG)x;
            cdf.rcArea.bottom = (LONG)y;
            ImmSetCandidateWindow(imc, &cdf);
        }
        ImmReleaseContext(hwnd, imc);
        return true;

    case WM_IME_COMPOSITION: {
        if (!imc) return true;
        if (lp & GCS_RESULTSTR) {
            LONG n = ImmGetCompositionStringW(imc, GCS_RESULTSTR, nullptr, 0);
            if (n > 0) {
                std::wstring res((size_t)n / sizeof(wchar_t), L'\0');
                ImmGetCompositionStringW(imc, GCS_RESULTSTR, res.data(), n);
                undo_ = Text;
                DelSelection();
                Text.insert(Caret, res);
                Caret += (int)res.size();
                SelAnchor = Caret;
                if (Changed) Changed(Text);
            }
            comp_.clear();
        }
        if (lp & GCS_COMPSTR) {
            LONG n = ImmGetCompositionStringW(imc, GCS_COMPSTR, nullptr, 0);
            if (n > 0) {
                comp_.assign((size_t)n / sizeof(wchar_t), L'\0');
                ImmGetCompositionStringW(imc, GCS_COMPSTR, comp_.data(), n);
            } else {
                comp_.clear();
            }
        }
        ImmReleaseContext(hwnd, imc);
        return true;
    }

    case WM_IME_ENDCOMPOSITION:
        comp_.clear();
        imeActive_ = false;
        ImmReleaseContext(hwnd, imc);
        return true;

    default:
        ImmReleaseContext(hwnd, imc);
        return false;
    }
}

// ---------------- Slider ----------------

D2D1_SIZE_F Slider::Desired(float availW, float availH) {
    (void)availH;
    return D2D1::SizeF(W > 0 ? W : std::max(120.f, availW), H > 0 ? H : 34.f);
}

bool Slider::Update(float dt) {
    bool changed = Widget::Update(dt);
    float target = (Hover || Press) ? 1.f : 0.f;
    if (aHoverScale != target) {
        float k = 1.f - std::exp(-dt / .09f);
        aHoverScale += (target - aHoverScale) * k;
        if (std::abs(target - aHoverScale) < .004f) aHoverScale = target;
        changed = true;
        AnimDirty = true;
    }
    return changed;
}

void Slider::Paint(PaintCtx& ctx) {
    const Tokens& t = ctx.t;
    float cy = (fr_.top + fr_.bottom) * .5f;
    float th = 4.f;
    // 轨道与滑点同域：两端各内缩 pad（约等于滑点半径），
    // 这样 Value=0 时滑点正好压在轨道起点，不会在蓝条前面留一段空灰条。
    float pad = 8.f;
    D2D1_RECT_F tr{ fr_.left + pad, cy - th * .5f, fr_.right - pad, cy + th * .5f };
    if (tr.right <= tr.left) tr = D2D1_RECT_F{ fr_.left, cy - th * .5f, fr_.right, cy + th * .5f };
    ctx.g.RoundRect(tr, th * .5f, Mix(t.wz, t.bj, .82f));

    float span = tr.right - tr.left;
    float x = tr.left + span * Value;

    float kr = 7.f * (1.f + .22f * aHoverScale);
    if (x > tr.left)
        ctx.g.RoundRect(D2D1_RECT_F{ tr.left, cy - th * .5f, x, cy + th * .5f }, th * .5f,
                        t.accent);

    if (Enabled) {
        ctx.g.Circle(x, cy, kr * 1.4f, A(t.accent, .16f * aHoverScale));
        ctx.g.Circle(x, cy, kr, t.accent);
    } else {
        ctx.g.Circle(x, cy, kr, t.text3);
    }
}

void Slider::OnPress() {
    Press = true;
}

void Slider::OnPressAt(float x, float) {
    Press = true;
    if (Enabled) OnDrag(x, 0.f);   // 按下就吸附到该处，再继续拖
}

void Slider::OnDrag(float x, float) {
    float pad = 8.f;
    float span = (fr_.right - fr_.left) - pad * 2.f;
    if (span <= 0.f) return;
    float v = (x - (fr_.left + pad)) / span;
    v = std::clamp(v, 0.f, 1.f);
    if (std::abs(v - Value) < 0.0001f) return;
    Value = v;
    if (Changed) Changed(Value);
}

void Slider::OnKeyDown(int vk) {
    if (!Enabled) return;
    float step = (GetKeyState(VK_SHIFT) & 0x8000) ? .1f : .01f;
    float v = Value;
    switch (vk) {
    case VK_LEFT:
    case VK_DOWN:  v -= step; break;
    case VK_RIGHT:
    case VK_UP:    v += step; break;
    case VK_PRIOR: v -= .1f; break;
    case VK_NEXT:  v += .1f; break;
    case VK_HOME:  v = 0.f; break;
    case VK_END:   v = 1.f; break;
    default: return;
    }
    v = std::clamp(v, 0.f, 1.f);
    if (std::abs(v - Value) < 0.0001f) return;
    Value = v;
    if (Changed) Changed(Value);
}

// ---------------- Progress ----------------

D2D1_SIZE_F Progress::Desired(float availW, float availH) {
    (void)availH;
    return D2D1::SizeF(W > 0 ? W : availW, H > 0 ? H : 6.f);
}

bool Progress::Update(float dt) {
    if (Indeterminate) {
        Widget::Update(dt);
        Phase += dt / 2.f;             // 来回一趟 2s（去 1s + 回 1s），跟 Fluent 的不确定节奏接近
        if (Phase >= 1.f) Phase -= 1.f;
        AnimDirty = true;
        return true;                   // 循环进度是持续动画：让帧循环别睡
    }
    bool changed = Widget::Update(dt);
    if (aVal < 0.f) aVal = Value;
    if (std::abs(aVal - Value) > .0005f) {
        float k = 1.f - std::exp(-dt / .18f);
        aVal += (Value - aVal) * k;
        if (std::abs(Value - aVal) < .0005f) aVal = Value;
        changed = true;
        AnimDirty = true;
    }
    return changed;
}

void Progress::Paint(PaintCtx& ctx) {
    const Tokens& t = ctx.t;
    float rad = 2.f;
    ctx.g.RoundRect(fr_, rad, Mix(t.wz, t.bj, .82f));

    if (Indeterminate) {
        // 一段 accent 在轨道里左右往返，两端各留一小截在轨道内（不会整根消失后再冒出来），
        // 走出轨道的那部分靠裁剪吃掉，所以边缘是干净的直切而不是硬跳。
        float w = fr_.right - fr_.left;
        float segW = std::clamp(w * .30f, 24.f, w);       // 段长约 30%，短条也不会小到看不见
        float eased = .5f - .5f * std::cos(Phase * 6.2831853f);   // 往返一趟，两端自然减速
        float x0 = fr_.left - segW * .65f + (w + segW * .30f) * eased;
        D2D1_RECT_F seg{ x0, fr_.top, x0 + segW, fr_.bottom };
        ctx.g.PushClip(fr_);
        ctx.g.RoundRect(seg, (fr_.bottom - fr_.top) > 4.f ? (fr_.bottom - fr_.top) * .5f : rad, t.accent);
        ctx.g.PopClip();
        return;
    }

    float v = aVal < 0.f ? Value : aVal;
    if (v > 0.f) {
        D2D1_RECT_F f = fr_;
        f.right = fr_.left + (fr_.right - fr_.left) * v;
        if (f.right < f.left + rad * 2.f) f.right = f.left + rad * 2.f;
        ctx.g.RoundRect(f, rad, t.accent);
    }
}

// ---------------- Segmented ----------------

Segmented::Segmented(std::vector<std::wstring> items, int active)
    : Items(std::move(items)), Active(active) {
    H = 32.f;
    Interactive = true;
}

D2D1_SIZE_F Segmented::Desired(float availW, float availH) {
    (void)availW; (void)availH;
    float w = 4.f;
    for (const auto& s : Items) w += Tw(s, 13.f, 400) + 28.f;
    return D2D1::SizeF(W > 0 ? W : w, H > 0 ? H : 32.f);
}

bool Segmented::Update(float dt) {
    bool changed = Widget::Update(dt);
    if (aChip < 0.f) aChip = (float)Active;
    float target = (float)Active;
    if (std::abs(aChip - target) > .002f) {
        float k = 1.f - std::exp(-dt / .13f);
        aChip += (target - aChip) * k;
        if (std::abs(target - aChip) < .002f) aChip = target;
        changed = true;
        AnimDirty = true;
    }
    return changed;
}

std::vector<D2D1_RECT_F> Segmented::SegRects(D2D1_RECT_F rc) const {
    std::vector<D2D1_RECT_F> out;
    if (Items.empty()) return out;
    std::vector<float> widths;
    for (const auto& s : Items) widths.push_back(Tw(s, 13.f, 400) + 28.f);

    float avail = (rc.right - rc.left) - 4.f;
    float total = 0.f;
    for (float w : widths) total += w;

    if (total > avail && total > 0.f) {
        float k = avail / total;
        for (auto& w : widths) w *= k;
    } else if (total < avail) {
        float extra = (avail - total) / float(widths.size());
        for (auto& w : widths) w += extra;
    }

    float x = rc.left + 2.f;
    for (size_t i = 0; i < Items.size(); ++i) {
        out.push_back(D2D1_RECT_F{ x, rc.top + 2.f, x + widths[i], rc.bottom - 2.f });
        x += widths[i];
    }
    return out;
}

void Segmented::Paint(PaintCtx& ctx) {
    float r = ctx.m.radiusInput + 2.f;
    ctx.g.RoundRect(fr_, r, ctx.t.xt);
    ctx.g.StrokeRect(fr_, r, ctx.t.border, 1.f);

    auto rs = SegRects(fr_);
    if (rs.empty()) return;

    // 选中的胶囊在段之间滑动
    float chip = aChip < 0.f ? (float)Active : aChip;
    int i0 = std::clamp((int)std::floor(chip), 0, (int)rs.size() - 1);
    int i1 = std::clamp(i0 + 1, 0, (int)rs.size() - 1);
    float t = chip - float(i0);
    D2D1_RECT_F a = rs[i0], b = rs[i1];
    D2D1_RECT_F chipRc{ a.left + (b.left - a.left) * t, a.top + (b.top - a.top) * t,
                        a.right + (b.right - a.right) * t, a.bottom + (b.bottom - a.bottom) * t };
    ctx.g.RoundRect(chipRc, ctx.m.radiusInput, ctx.t.kp);
    ctx.g.StrokeRect(chipRc, ctx.m.radiusInput, ctx.t.border, 1.f);

    for (size_t i = 0; i < rs.size(); ++i) {
        bool on = int(i) == Active;
        bool hot = int(i) == Hot && !on;
        ctx.g.Text(rs[i], Items[i], 13.f, on ? 600 : 400,
                   on ? ctx.t.wz : (hot ? Mix(ctx.t.text2, ctx.t.wz, .5f) : ctx.t.text2),
                   HAlign::Center, VAlign::Middle);
    }
}

void Segmented::OnHover(bool) {
    Hot = -1;
}

void Segmented::OnClickAt(float x, float) {
    auto rs = SegRects(fr_);
    for (size_t i = 0; i < rs.size(); ++i) {
        if (x >= rs[i].left && x < rs[i].right) {
            if (int(i) == Active) return;
            Active = int(i);
            if (Changed) Changed(Active);
            return;
        }
    }
}

void Segmented::OnKeyDown(int vk) {
    if (!Enabled || Items.empty()) return;
    int n = (int)Items.size();
    int target = Active;
    switch (vk) {
    case VK_LEFT:  target = std::max(0, Active - 1); break;
    case VK_RIGHT: target = std::min(n - 1, Active + 1); break;
    case VK_HOME:  target = 0; break;
    case VK_END:   target = n - 1; break;
    default: return;
    }
    if (target == Active) return;
    Active = target;
    if (Changed) Changed(Active);
}

// ---------------- Badge ----------------

Badge::Badge(std::wstring text, Tone tone) : Text(std::move(text)), ToneKind(tone) {
    H = 20.f;
}

D2D1_SIZE_F Badge::Desired(float availW, float availH) {
    (void)availW; (void)availH;
    return D2D1::SizeF(Tw(Text, 11.f, 600) + 16.f, H > 0 ? H : 20.f);
}

void Badge::Paint(PaintCtx& ctx) {
    D2D1_COLOR_F bg{}, fg{};
    switch (ToneKind) {
    case Accent:   bg = ctx.t.accentSoft; fg = ctx.t.accent; break;
    case Success:  bg = Hex(0x107C10, .14f); fg = ctx.t.dark ? Hex(0x6CCB5F) : Hex(0x0F6B0F); break;
    case Warning:  bg = Hex(0xC77A00, .14f); fg = ctx.t.dark ? Hex(0xF5B54C) : Hex(0x9C6000); break;
    case Danger:   bg = Hex(0xC42B1C, .12f); fg = ctx.t.dark ? Hex(0xFF8A7A) : Hex(0xA81F12); break;
    default:       bg = Overlay(ctx.t, .07f); fg = ctx.t.text2; break;
    }
    ctx.g.RoundRect(fr_, (fr_.bottom - fr_.top) * .5f, bg);
    ctx.g.Text(fr_, Text, 11.f, 600, fg, HAlign::Center, VAlign::Middle);
}

} // namespace vcui
