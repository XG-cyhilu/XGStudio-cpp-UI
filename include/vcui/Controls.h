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
    float Size = 14.f;             // 默认 Body 1
    int Weight = 400;
    float LineHeight = 0.f;        // 0 = 按 type ramp 自动取行高（14→20、32→40…）
    D2D1_COLOR_F Color{ 0, 0, 0, 0 };
    HAlign Align = HAlign::Left;
    VAlign VAlignMode = VAlign::Top;
    bool Wrap = false;
    bool Ellipsis = false;    // 单行超宽时尾部截断加「…」

    Label() = default;
    explicit Label(std::wstring text, float size = 14.f, int weight = 400)
        : Text(std::move(text)), Size(size), Weight(weight) {}

    // 直接套 Fluent type ramp 的一档（自带字号/字重/行高）。
    Label& UseType(const TypeStyle& s) {
        Size = s.size; Weight = s.weight; LineHeight = s.lineHeight;
        return *this;
    }

    D2D1_SIZE_F Desired(float availW, float availH) override;
    void Paint(PaintCtx& ctx) override;
};

// 按钮。Primary 为主色实心，其余为描边。
class VCUI_API Button : public Widget {
public:
    std::wstring Text;
    std::wstring Glyph;       // Segoe MDL2 字符，可空
    bool Primary = false;
    bool Danger = false;      // 危险操作：悬停变红底白字
    float MinWidth = 0.f;

    Button() { H = 34.f; Interactive = true; }
    explicit Button(std::wstring text, bool primary = false);

    D2D1_SIZE_F Desired(float availW, float availH) override;
    void Paint(PaintCtx& ctx) override;
    // 空格 / 回车激活（Fluent 按钮的标准键盘行为）
    void OnKeyDown(int vk) override;
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

    // 光标闪烁（由 Surface 的 530ms 定时器驱动，避免为了闪烁一直跑 60fps 帧循环）
    void ToggleCaret() { blinkOn_ = !blinkOn_; AnimDirty = true; }
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
    // 按下即跳到该位置（Fluent 滑块行为），否则只能先按住再拖
    void OnPressAt(float x, float y) override;
    void OnDrag(float x, float y);
    // 方向键步进 1%（按住 Shift 10%），Home / End 到两端，PgUp / PgDn 10%
    void OnKeyDown(int vk) override;
    CursorShape CursorAt(float, float) const override { return CursorShape::Hand; }
    bool Focusable() const override { return true; }
    std::function<void(float)> Changed;
};

class VCUI_API Progress : public Widget {
public:
    float Value = .42f;
    float aVal = -1.f;
    // 不确定进度：一段 accent 在轨道里来回循环扫过，Value 不参与绘制。
    // 打开后控件每帧都标脏，所以所在页面会一直保持 60fps（没有别的动画时也不会睡）。
    bool Indeterminate = false;
    float Phase = 0.f;                  // 循环相位 0..1，Indeterminate 时按 dt 自走
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
    // 左右方向键切换段，Home / End 跳到首尾
    void OnKeyDown(int vk) override;
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
    // 回车 / 空格相当于点击（侧栏导航可以用键盘走）
    void OnKeyDown(int vk) override;
    bool Focusable() const override { return true; }
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
    void OnKeyDown(int vk) override;   // 空格 / 回车激活
};

// 下拉框：按钮部分自绘，菜单通过 Surface 的 Overlay 层显示。
class VCUI_API Dropdown : public Widget {
public:
    std::vector<std::wstring> Items;
    int Selected = 0;
    bool Open = false;
    float aOpen = 0.f;
    float MenuWidth = -1.f;   // < 0 取自身宽度

    Dropdown() { H = 34.f; FocusRing = false; }   // 展开/聚焦已有 accent 描边
    explicit Dropdown(std::vector<std::wstring> items, int selected = 0);

    D2D1_SIZE_F Desired(float availW, float availH) override;
    void Paint(PaintCtx& ctx) override;
    bool Update(float dt) override;
    CursorShape CursorAt(float, float) const override { return CursorShape::Hand; }
    bool Focusable() const override { return true; }
    void OnClickAt(float x, float y) override;
    // 空格 / 回车 / F4 展开或收起；展开时上下键换选中项（立即生效）
    void OnKeyDown(int vk) override;
    void CloseMenu();
    std::function<void(int)> Changed;

private:
    std::shared_ptr<Widget> menu_;
    std::vector<Widget*> menuRows_;   // 菜单项（弱引用式的裸指针，随 menu_ 一起销毁）
    float aLinear = 0.f;              // 线性进度 0..1，驱动 aOpen 的 smoothstep
    void OpenMenu();
    void SyncMenuSelection();
};

// 颜色选取器：饱和度/明度方块 + 色相条 + 十六进制输入 + 可选预设色板。
// 直接在页面上内嵌使用（不弹出），高度约 200；拖动方块或色相条实时回调 Changed。
class VCUI_API ColorPicker : public Widget {
public:
    D2D1_COLOR_F Color = Hex(0x0071EA);
    float SquareH = 150.f;      // 饱和度/明度方块高度
    float HueH = 14.f;          // 色相条高度
    float Spacing = 8.f;        // 各段间距
    bool ShowHex = true;        // 底部一行：当前色块 + #RRGGBB 输入
    std::vector<unsigned> Swatches;   // 预设色板（0xRRGGBB），非空时底部多一行圆点

    ColorPicker();
    explicit ColorPicker(D2D1_COLOR_F c);

    D2D1_SIZE_F Desired(float availW, float availH) override;
    void LayoutChildren() override;
    void Paint(PaintCtx& ctx) override;
    void OnPressAt(float x, float y) override;
    void OnDrag(float x, float y) override;
    void OnRelease(bool inside) override;
    CursorShape CursorAt(float, float) const override { return CursorShape::Hand; }

    // 外部改色（不触发 Changed）；notify 为真时回调
    void SetColor(D2D1_COLOR_F c, bool notify = false);
    std::function<void(D2D1_COLOR_F)> Changed;

private:
    float hue_ = 210.f, sat_ = 1.f, val_ = .92f;
    int drag_ = 0;                        // 0 无 / 1 方块 / 2 色相条
    std::shared_ptr<Input> hex_;
    void Init();
    D2D1_RECT_F SquareRc() const;
    D2D1_RECT_F HueRc() const;
    D2D1_RECT_F HexRc() const;
    D2D1_RECT_F SwatchRc() const;
    void Apply(float h, float s, float v, bool notify);
    void PickAt(float x, float y);
    void SyncHex();
};

// 一个图标：给一段 SVG path，按 Stroke 描边（Filled=true 时改填充）。
class VCUI_API SvgIcon : public Widget {
public:
    std::wstring Path;
    float IconSize = 14.f;       // 正方形边长；W/H>0 时以 W/H 为准
    float Stroke = 1.6f;
    bool Filled = false;
    D2D1_COLOR_F Color{ 0, 0, 0, 0 };   // a>0 时覆盖主题色

    D2D1_SIZE_F Desired(float availW, float availH) override;
    void Paint(PaintCtx& ctx) override;
};
// 加载中的转圈：一圈小点，最亮的一点随时间旋转（不需要路径几何，纯圆点）
class VCUI_API Spinner : public Widget {
public:
    float Size = 16.f;          // 直径
    float Dots = 8.f;           // 点个数
    float DotScale = 1.15f;     // 点半径 = Size/Dots * DotScale
    D2D1_COLOR_F Color{ 0, 0, 0, 0 };   // a == 0 时用主题强调色
    float Speed = 1.15f;        // 每秒圈数

    Spinner() { W = 16.f; H = 16.f; }
    explicit Spinner(float size) : Size(size) { W = size; H = size; }

    D2D1_SIZE_F Desired(float availW, float availH) override;
    bool Update(float dt) override;
    void Paint(PaintCtx& ctx) override;

private:
    float phase_ = 0.f;
};

// 骨架块：一条会呼吸的圆角占位，Delay 用来给多行错峰
class VCUI_API Skeleton : public Widget {
public:
    float Radius = 6.f;
    float Period = 1.4f;        // 一次呼吸的秒数
    float Delay = 0.f;          // 相对相位，0..Period
    D2D1_COLOR_F Base{ 0, 0, 0, 0 };    // a == 0 时按主题取"浅一档的灰"

    Skeleton() { H = 14.f; }
    explicit Skeleton(float h) { H = h; }

    D2D1_SIZE_F Desired(float availW, float availH) override;
    bool Update(float dt) override;
    void Paint(PaintCtx& ctx) override;

private:
    float t_ = 0.f;
};

// ---------------- 反馈类 ----------------

enum class InfoTone { Info, Success, Warning, Danger };

// 悬停提示气泡。一般不要直接 new，给控件设 Widget::Tooltip 即可。
class VCUI_API Tooltip : public Widget {
public:
    std::wstring Text;
    Tooltip() { H = 28.f; }
    D2D1_SIZE_F Desired(float availW, float availH) override;
    void Paint(PaintCtx& ctx) override;
};

// 提示条（Toast / InfoBar）：右上角堆叠、定时自动消失。
// 由 Surface::ShowInfoBar 创建并托管，不要手动挂到控件树。
class VCUI_API InfoBar : public Widget {
public:
    std::wstring Text;
    InfoTone ToneKind = InfoTone::Info;
    float LifeMs = 3000.f;      // 停留时长（悬停时暂停计时）
    bool Exiting = false;

    InfoBar() { Interactive = true; H = 52.f; }
    explicit InfoBar(std::wstring text, InfoTone tone = InfoTone::Info, float lifeMs = 3000.f);

    D2D1_SIZE_F Desired(float availW, float availH) override;
    void Paint(PaintCtx& ctx) override;
    bool Update(float dt) override;
    void OnClickAt(float x, float y) override;
    CursorShape CursorAt(float x, float y) const override;
    void StartExit();                        // 开始淡出，动画结束自我销毁

    std::function<void()> Closed;

private:
    float aIn = 0.f, aOut = 0.f;
    float baseX = 0.f;        // 目标 X，滑入动画在它上面偏移
    unsigned long long lastMs_ = 0;
    float holdMs_ = 0.f;      // 已停留时长（悬停不计）
    void TickClock();         // 由 Surface 的定时器推进计时（静止时也要走）
    friend class Surface;
};

struct DialogAction {
    std::wstring Text;
    bool Primary = false;
    bool Danger = false;
    std::function<void()> OnClick;    // 点击后先执行它，再关闭对话框
};

// 模态对话框：全屏压暗遮罩 + 居中卡片。用 Dialog::Open 打开，
// 点遮罩 / Esc 关闭（DismissOnScrim），按钮回调里可自行决定是否保留。
class VCUI_API Dialog : public Widget {
public:
    std::wstring Title, Message;
    std::vector<DialogAction> Actions;
    bool DismissOnScrim = true;
    float CardWidth = 420.f;
    std::function<void()> Closed;

    Dialog();
    D2D1_SIZE_F Desired(float availW, float availH) override;
    void LayoutChildren() override;
    void Paint(PaintCtx& ctx) override;
    void OnClickAt(float x, float y) override;
    bool Focusable() const override { return true; }
    void Close();

    // 打开即挂到 Surface 覆盖层最上方。actions 为空时给一个「确定」。
    static std::shared_ptr<Dialog> Open(Surface* host, std::wstring title, std::wstring message,
                                        std::vector<DialogAction> actions = {},
                                        bool dismissOnScrim = true, float cardWidth = 420.f);

private:
    std::shared_ptr<Widget> card_;
    float cardH_ = 0.f;
};

// 自绘标题栏：标题文字 + 可选顶栏导航 + 最小化 / 最大化 / 关闭（配合 Surface::SetCustomFrame）。
// 非按钮区域整体作为窗口拖动区（Caption），双击可最大化。
class VCUI_API TitleBar : public Widget {
public:
    std::wstring Title;
    float BtnWidth = 46.f;
    bool ShowMinimize = true;
    bool ShowMaximize = true;
    bool ShowClose = true;
    bool Fill = false;                        // 是否填 kp 底色；默认透明，跟页面背景融合
    std::function<bool()> OnCloseRequested;   // 返回 true 表示已自行处理（不执行默认关闭）

    // 顶栏导航：整栏（跟在标题右侧）或最小居中（默认跟随内容宽度居中）
    std::vector<std::wstring> NavItems;
    int NavIndex = 0;
    bool NavCentered = false;
    float NavGap = 4.f;
    std::function<void(int)> NavChanged;

    TitleBar();

    D2D1_SIZE_F Desired(float availW, float availH) override;
    void LayoutChildren() override;
    void Paint(PaintCtx& ctx) override;

private:
    std::shared_ptr<Widget> min_, max_, close_;
    std::vector<std::shared_ptr<Widget>> nav_;   // 与 NavItems 一一对应
    void SyncNav();
};

} // namespace vcui
