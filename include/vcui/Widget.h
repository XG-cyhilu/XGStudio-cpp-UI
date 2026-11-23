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
#include "vcui/Export.h"
#include "vcui/Gfx.h"
#include "vcui/Theme.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace vcui {

class Surface;


class Surface;

enum class CursorShape { Arrow, Hand, IBeam };

struct PaintCtx {
    Gfx& g;
    const Tokens& t;
    const Metrics& m;
    float scale = 1.f;
    float time = 0.f;     // 秒，用于 caret 闪烁等周期视觉
};

// 控件基类：布局参数走公开字段，交互状态由 Surface 统一维护。
// 自定义控件只实现 Desired / Paint / 事件，不要在 Paint 里改状态。
class VCUI_API Widget {
public:
    virtual ~Widget();

    // ---- 布局输入 ----
    float W = -1.f;          // < 0：按内容
    float H = -1.f;
    float Grow = 0.f;        // 主轴伸展权重
    float Basis = -1.f;      // 主轴基准尺寸，< 0：按内容
    float X = 0.f, Y = 0.f;  // 仅 Absolute 容器读取
    float ML = 0, MT = 0, MR = 0, MB = 0;
    float PL = 0, PT = 0, PR = 0, PB = 0;
    D2D1_COLOR_F Bg{ 0, 0, 0, 0 };
    bool Visible = true;
    bool Enabled = true;
    bool Interactive = false;  // 命中测试的真正目标（装饰性子节点不接管点击）
    bool Clip = false;         // 为真时子项被裁剪到内容区
    bool Caption = false;      // 标记为窗口拖动区（标题栏用）
    float Opacity = 1.f;       // 子树整体不透明度（弹层淡入用）
    std::wstring Id;
    // 悬停提示：非空时鼠标停在该控件上 600ms 后由 Surface 弹出提示。
    std::wstring Tooltip;
    // 覆盖层专用：为真时每次绘制都铺满客户区（对话框遮罩会随窗口尺寸自适应）。
    bool FillHost = false;
    // 键盘焦点时是否画库统一的焦点环。自带焦点视觉的控件（Input 的 accent 描边、
    // Dropdown、对话框）应置 false，否则会跟自身描边叠成双层。
    bool FocusRing = true;

    // ---- 运行状态（Surface 维护，不要手动改）----
    bool Hover = false;
    bool Press = false;
    bool Dead = false;         // 覆盖层请求销毁标记（下次绘制前移除）
    bool Focus = false;

    // ---- 动画（Surface 每帧推进，绘制时读这里）----
    float aHover = 0.f;        // 0..1 悬停权重
    float aPress = 0.f;        // 0..1 按压权重
    float HoverA() const { return aHover; }
    float PressA() const { return aPress; }
    bool AnimDirty = false;    // 本帧自身动画有变化（Surface 用来算脏矩形）
    // 返回 true 表示这一帧视觉有变化，需要重绘
    virtual bool Update(float dt);

    // ---- 入场动画（切页用）：淡入 + 可选向下位移 ----
    // 单控件从出现到归位 dur 秒（Fluent 常用 252ms），delay 为其单独延后。
    // dy > 0 时从"上方 dy 像素处"落位，MT 每帧被改写，动画结束后恢复原值。
    void FadeIn(float dur = .252f, float dy = 0.f, float delay = 0.f);
    // 错峰入场：给直接子项依次加 step 延迟（滚动列表则作用于里面每一行），
    // 每项自身的入场时长仍是 dur，总时长 = dur + 最后一项的延迟。
    void StaggerIn(float dur = .252f, float dy = 8.f, float step = .04f, float maxDelay = .24f);
    float AnimT = -1.f;        // < 0 表示没有入场动画
    float AnimDur = .252f, AnimDelay = 0.f;
    float AnimBaseOpacity = 1.f, AnimDy = 0.f, AnimBaseMT = 0.f;

    // ---- 树 ----
    Widget* Parent = nullptr;
    Surface* Host = nullptr;
    Gfx* GFX = nullptr;        // Surface 递归注入，供 Desired 精确测量
    const std::vector<std::shared_ptr<Widget>>& Children() const { return kids_; }
    Widget& Add(std::shared_ptr<Widget> child);
    void Clear();
    void Detach();

    // ---- 几何 ----
    D2D1_RECT_F Frame() const { return fr_; }
    D2D1_RECT_F Content() const;
    void Arrange(D2D1_RECT_F fr);
    Widget& Pad(float v) { PL = PT = PR = PB = v; return *this; }
    Widget& Margin(float v) { ML = MT = MR = MB = v; return *this; }

    // ---- 布局 ----
    virtual D2D1_SIZE_F Desired(float availW, float availH);
    virtual void LayoutChildren() {}

    // ---- 交互 ----
    virtual bool HitTest(float x, float y) const;
    Widget* Hit(float x, float y);
    virtual CursorShape CursorAt(float x, float y) const { return CursorShape::Arrow; }
    virtual bool Focusable() const { return false; }
    virtual bool WantsWheel() const { return false; }
    bool Inside(float x, float y) const { return HitTest(x, y); }

    virtual void OnHover(bool) {}
    virtual void OnPress() { Press = true; }
    // 带坐标的按下回调：拖拽类控件（滚动条、滑块）靠它判断"按在哪儿"
    virtual void OnPressAt(float /*x*/, float /*y*/) { OnPress(); }
    virtual void OnRelease(bool /*inside*/) { Press = false; }
    virtual void OnClick();
    virtual void OnClickAt(float /*x*/, float /*y*/) { OnClick(); }
    virtual void OnDrag(float /*x*/, float /*y*/) {}
    virtual void OnChar(wchar_t) {}
    // 键盘：焦点控件收到（Tab / Esc 由 Surface 先处理）。
    // 空格与回车统一用 VK_SPACE / VK_RETURN 判断，控件自己决定语义（激活 / 切换 / 步进）。
    virtual void OnKeyDown(int) {}
    virtual void OnFocus(bool) {}
    virtual void OnWheel(float /*dy*/, float /*x*/, float /*y*/) {}
    // 输入法消息（WM_IME_*），返回 true 表示已处理
    virtual bool OnImeMessage(HWND /*hwnd*/, UINT /*msg*/, WPARAM /*wp*/, LPARAM /*lp*/) { return false; }
    virtual bool ImeComposing() const { return false; }

    // ---- 绘制 ----
    virtual void Paint(PaintCtx& /*ctx*/);
    void PaintTree(PaintCtx& ctx);

    std::function<void()> Clicked;

protected:
    D2D1_RECT_F fr_{ 0, 0, 0, 0 };
    std::vector<std::shared_ptr<Widget>> kids_;

    // Desired 中的文本测量（GFX 缺失时退化为近似）
    float Tw(const std::wstring& s, float px, int weight) const;
};

// 便捷构造
VCUI_API std::shared_ptr<Widget> Sizer(float w, float h = 0.f);

// Fluent 的 decelerate 曲线 cubic-bezier(0.1, 0.9, 0.2, 1)：起步快、收尾慢。
VCUI_API float EaseDecelerate(float p);

} // namespace vcui
