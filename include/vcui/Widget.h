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
    virtual void OnRelease(bool /*inside*/) { Press = false; }
    virtual void OnClick();
    virtual void OnClickAt(float /*x*/, float /*y*/) { OnClick(); }
    virtual void OnDrag(float /*x*/, float /*y*/) {}
    virtual void OnChar(wchar_t) {}
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

} // namespace vcui
