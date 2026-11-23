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

#pragma once
#include "vcui/Layout.h"

namespace vcui {

// 文本。Color 的 alpha 为 0 时使用主题文字色。
class VCUI_API Label : public Widget {
public:
    std::wstring Text;
    float Size = 13.f;
    int Weight = 400;
    D2D1_COLOR_F Color{ 0, 0, 0, 0 };
    HAlign Align = HAlign::Left;
    VAlign VAlignMode = VAlign::Top;
    bool Wrap = false;

    Label() = default;
    explicit Label(std::wstring text, float size = 13.f, int weight = 400)
        : Text(std::move(text)), Size(size), Weight(weight) {}

    D2D1_SIZE_F Desired(float availW, float availH) override;
    void Paint(PaintCtx& ctx) override;
};

// 按钮。Primary 为主色实心，其余为描边。
class VCUI_API Button : public Widget {
public:
    std::wstring Text;
    std::wstring Glyph;       // Segoe MDL2 字符，可空
    bool Primary = false;
    float MinWidth = 0.f;

    Button() { H = 34.f; Interactive = true; }
    explicit Button(std::wstring text, bool primary = false);

    D2D1_SIZE_F Desired(float availW, float availH) override;
    void Paint(PaintCtx& ctx) override;
    CursorShape CursorAt(float, float) const override { return CursorShape::Hand; }
    bool Focusable() const override { return true; }
};

class VCUI_API Checkbox : public Widget {
public:
    std::wstring Text;
    bool On = false;
    float aOn = -1.f;
    Checkbox() { H = 26.f; Interactive = true; }
    explicit Checkbox(std::wstring text, bool on = false);

    D2D1_SIZE_F Desired(float availW, float availH) override;
    void Paint(PaintCtx& ctx) override;
    bool Update(float dt) override;
    void OnClick() override;
    void OnKeyDown(int vk) override;
    CursorShape CursorAt(float, float) const override { return CursorShape::Hand; }
    bool Focusable() const override { return true; }
    std::function<void(bool)> Changed;
};

class VCUI_API Switch : public Widget {
public:
    bool On = false;
    float aKnob = -1.f;   // 动画位置，< 0 表示未初始化
    Switch() { W = 40.f; H = 26.f; Interactive = true; }
    explicit Switch(bool on) : On(on) { W = 40.f; H = 26.f; Interactive = true; }

    D2D1_SIZE_F Desired(float, float) override;
    void Paint(PaintCtx& ctx) override;
    bool Update(float dt) override;
    void OnClick() override;
    void OnKeyDown(int vk) override;
    CursorShape CursorAt(float, float) const override { return CursorShape::Hand; }
    bool Focusable() const override { return true; }
    std::function<void(bool)> Changed;
};

// 单行输入。支持选区、剪贴板快捷键、撤销、输入法组合串与候选窗定位。
class VCUI_API Input : public Widget {
public:
    std::wstring Text;
    std::wstring Placeholder;
    int Caret = 0;
    int SelAnchor = 0;        // 与 Caret 一起构成选区（相等即无选区）
    float MaxWidth = 1e6f;

    Input();
    explicit Input(std::wstring placeholder);

    D2D1_SIZE_F Desired(float availW, float availH) override;
    void Paint(PaintCtx& ctx) override;
    bool Update(float dt) override;
    bool Focusable() const override { return true; }
    CursorShape CursorAt(float, float) const override { return CursorShape::IBeam; }
    void OnFocus(bool on) override;
    void OnChar(wchar_t c) override;
    void OnKeyDown(int vk) override;
    void OnClickAt(float x, float y) override;
    void OnDrag(float x, float y) override;
    bool OnImeMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) override;
    bool ImeComposing() const { return imeActive_; }
    std::function<void(const std::wstring&)> Changed;
    std::function<void()> Submitted;

private:
    std::wstring comp_;       // 输入法组合串
    bool imeActive_ = false;
    std::wstring undo_;       // 单级撤销快照
    float blinkT_ = 0.f;
    bool blinkOn_ = true;
    int dragAnchor_ = -1;

    void ClampCaret();
    int SelLo() const { return Caret < SelAnchor ? Caret : SelAnchor; }
    int SelHi() const { return Caret < SelAnchor ? SelAnchor : Caret; }
    void DelSelection();
    int PosFromX(float x);
    void SyncCaretToIme(bool keepAnchor);
};

// 0..1 滑动条，支持点击与拖拽。
class VCUI_API Slider : public Widget {
public:
    float Value = .42f;
    float aHoverScale = 0.f;
    Slider() { H = 34.f; Interactive = true; }
    explicit Slider(float v) : Value(v) { H = 34.f; Interactive = true; }

    D2D1_SIZE_F Desired(float availW, float availH) override;
    void Paint(PaintCtx& ctx) override;
    bool Update(float dt) override;
    void OnPress() override;
    void OnDrag(float x, float y);
    CursorShape CursorAt(float, float) const override { return CursorShape::Hand; }
    bool Focusable() const override { return true; }
    std::function<void(float)> Changed;
};

class VCUI_API Progress : public Widget {
public:
    float Value = .42f;
    float aVal = -1.f;
    Progress() { H = 6.f; }
    explicit Progress(float v) : Value(v) { H = 6.f; }

    D2D1_SIZE_F Desired(float availW, float availH) override;
    void Paint(PaintCtx& ctx) override;
    bool Update(float dt) override;
};

// 分段的单选条。
class VCUI_API Segmented : public Widget {
public:
    std::vector<std::wstring> Items;
    int Active = 0;
    int Hot = -1;
    float aChip = -1.f;
    Segmented() { H = 32.f; Interactive = true; }
    explicit Segmented(std::vector<std::wstring> items, int active = 0);

    D2D1_SIZE_F Desired(float availW, float availH) override;
    void Paint(PaintCtx& ctx) override;
    bool Update(float dt) override;
    void OnClickAt(float x, float y) override;
    void OnHover(bool on) override;
    CursorShape CursorAt(float, float) const override { return CursorShape::Hand; }
    bool Focusable() const override { return true; }
    std::function<void(int)> Changed;

private:
    std::vector<D2D1_RECT_F> SegRects(D2D1_RECT_F rc) const;
};

class VCUI_API Badge : public Widget {
public:
    std::wstring Text;
    enum Tone { Neutral, Accent, Success, Warning, Danger } ToneKind = Neutral;
    Badge() { H = 20.f; }
    explicit Badge(std::wstring text, Tone tone = Neutral);

    D2D1_SIZE_F Desired(float availW, float availH) override;
    void Paint(PaintCtx& ctx) override;
};

// 侧栏导航项（图标 + 文字 + 选中指示条）。
class VCUI_API NavItem : public Widget {
public:
    std::wstring Text;
    std::wstring Glyph;
    bool Active = false;
    NavItem() { H = 38.f; PL = 12.f; PR = 12.f; }
    explicit NavItem(std::wstring text, std::wstring glyph = L"");

    D2D1_SIZE_F Desired(float availW, float availH) override;
    void Paint(PaintCtx& ctx) override;
    CursorShape CursorAt(float, float) const override { return CursorShape::Hand; }
    void OnClick() override;
};

// 列表行：标题 + 副标题 + 右侧文字。
class VCUI_API ListRow : public Widget {
public:
    std::wstring Title, Sub, Right;
    bool Selected = false;
    ListRow() { H = 56.f; PL = 14.f; PR = 14.f; Interactive = true; }
    ListRow(std::wstring title, std::wstring sub, std::wstring right);

    D2D1_SIZE_F Desired(float availW, float availH) override;
    void Paint(PaintCtx& ctx) override;
    CursorShape CursorAt(float, float) const override { return CursorShape::Hand; }
};

// 图标按钮（Segoe MDL2 字形，后续换 SVG 只需替换绘制）。
class VCUI_API IconButton : public Widget {
public:
    std::wstring Glyph;
    float GlyphSize = 16.f;
    bool Ghost = true;
    IconButton() { W = 32.f; H = 32.f; Interactive = true; }
    explicit IconButton(std::wstring glyph, bool ghost = true);

    D2D1_SIZE_F Desired(float, float) override;
    void Paint(PaintCtx& ctx) override;
    CursorShape CursorAt(float, float) const override { return CursorShape::Hand; }
    bool Focusable() const override { return true; }
};

// 下拉框：按钮部分自绘，菜单通过 Surface 的 Overlay 层显示。
class VCUI_API Dropdown : public Widget {
public:
    std::vector<std::wstring> Items;
    int Selected = 0;
    bool Open = false;
    float aOpen = 0.f;
    float MenuWidth = -1.f;   // < 0 取自身宽度

    Dropdown() { H = 34.f; }
    explicit Dropdown(std::vector<std::wstring> items, int selected = 0);

    D2D1_SIZE_F Desired(float availW, float availH) override;
    void Paint(PaintCtx& ctx) override;
    bool Update(float dt) override;
    CursorShape CursorAt(float, float) const override { return CursorShape::Hand; }
    bool Focusable() const override { return true; }
    void OnClickAt(float x, float y) override;
    void CloseMenu();
    std::function<void(int)> Changed;

private:
    std::shared_ptr<Widget> menu_;
    float aLinear = 0.f;   // 线性进度 0..1，驱动 aOpen 的 smoothstep
};

} // namespace vcui
