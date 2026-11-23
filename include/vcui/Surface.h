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
#include "vcui/Controls.h"

namespace vcui {

// 宿主窗口：负责 DPI、绘制循环、鼠标/键盘路由、焦点与覆盖层。
// 业务侧只搭 Widget 树，不直接碰 Win32。
class VCUI_API Surface {
public:
    Surface();
    ~Surface();

    // title 为空时系统拿窗口类名 VCUISurface 当标题
    bool Create(const std::wstring& title, int width, int height, bool dark = false);
    // 窗口最小尺寸（DIP），Create 之前或之后都可调用
    void SetMinSize(int w, int h) { minW_ = w; minH_ = h; }

    // 无边框窗口（自绘标题栏）。必须在 Create 之前调用：
    // 客户区吃掉整个窗口，边框缩放由库自己做命中测试，标题栏用 TitleBar + Caption。
    void SetCustomFrame(bool on) { customFrame_ = on; }
    bool CustomFrame() const { return customFrame_; }
    // 边框可拖拽宽度（DIP），默认 6
    void SetResizeBorder(float dip) { resizeBorder_ = dip < 2.f ? 2.f : dip; }
    int Run();
    void Close();
    void Minimize();
    void ToggleMaximize();
    bool Maximized() const;
    Gfx& G() { return g_; }

    void SetRoot(std::shared_ptr<Widget> root);
    Widget* Root() const { return root_.get(); }

    void Invalidate();
    // 唤醒动画帧循环（平时没有动画时循环是睡着的）。控件入场动画会自动调用。
    void PlayAnim();
    // 只重绘一小块（动画帧用），坐标 DIP
    void InvalidateRectD(D2D1_RECT_F rc);
    HWND Hwnd() const { return hwnd_; }
    float DpiScale() const { return scale_; }
    Theme& Th() { return theme_; }

    void SetTheme(Palette p, bool dark);
    void SetDark(bool dark);
    void SetAccent(D2D1_COLOR_F a);
    bool IsDark() const { return theme_.dark; }

    // 覆盖层（下拉菜单、弹出面板）。坐标为 DIP、相对客户区。
    void AddOverlay(std::shared_ptr<Widget> w);
    void RemoveOverlay(Widget* owner);
    // 事件回调中安全移除（实际清理发生在下一次绘制前）
    void RemoveOverlayDeferred(Widget* owner);
    void ClearOverlays();
    // 点击覆盖层之外时回调（用于收起菜单）。owner 为该弹层的宿主控件，
    // 点它自身不算"外面"，这样才能点一下展开、再点一下收起。
    void SetDismiss(std::function<void()> fn, Widget* owner = nullptr) {
        dismiss_ = std::move(fn);
        dismissOwner_ = owner;
    }
    void ClearDismiss() { dismiss_ = nullptr; dismissOwner_ = nullptr; }
    // 只有当前登记宿主是 owner 时才清，避免旧弹层淡出时把新弹层的 dismiss 抹掉
    void ClearDismissIf(Widget* owner) {
        if (dismissOwner_ == owner) { dismiss_ = nullptr; dismissOwner_ = nullptr; }
    }

    Widget* Focused() const { return focus_; }
    void SetFocus(Widget* w);
    // 唤醒动画帧循环（输入事件后调用；没有动画时会自行停掉）
    void KickAnim();

    // ---- 帧驱动（多窗口共用一条消息循环时用）----
    // 有动画在跑（需要 60fps 推进）时返回 true
    bool Animating() const { return animating_; }
    // 推进一帧（由 App 的共享循环调用；单窗口时 Surface::Run 内部自己调）
    void Pump(float dt) { Tick(dt); }

    // 客户区尺寸（DIP）。弹层定位用，比如对话框铺满、提示条贴右上角。
    D2D1_SIZE_F ClientSize() const {
        return D2D1::SizeF(float(width_) / scale_, float(height_) / scale_);
    }

    // 右上角提示条（Toast）。堆叠显示，到期自动淡出；返回的可忽略。
    void ShowInfoBar(const std::wstring& text, InfoTone tone = InfoTone::Info, float seconds = 3.f);
    void CloseInfoBars();
    // 提示条统一宽度（DIP）。文字超宽会省略，悬停显示完整提示。
    void SetInfoBarWidth(float dip) { infoBarWidth_ = dip > 140.f ? dip : 140.f; }
    float InfoBarWidth() const { return infoBarWidth_; }

private:
    friend class Widget;   // Widget 析构时要把裸指针从下面这些槽里摘掉
    void ForgetWidget(Widget* w);

    static LRESULT CALLBACK WndProcThunk(HWND, UINT, WPARAM, LPARAM);
    LRESULT Handle(UINT msg, WPARAM wp, LPARAM lp);

    void Paint();
    void Layout();
    void Tick(float dt);
    void UpdateCursor(float x, float y);
    Widget* HitAt(float x, float y, bool* inOverlay = nullptr);
    void CollectFocusable(Widget* w, std::vector<Widget*>& out);
    void CycleFocus(bool back);
    // 键盘焦点移到屏幕外时，让最近的滚动祖先把它滚进来
    void EnsureVisible(Widget* w);
    // 焦点环（Fluent 的 2px accent 外描边），在整棵树画完后叠加
    void PaintFocusRing(PaintCtx& ctx);
    // 悬停提示：WM_MOUSEMOVE 在 hot 变化时排定，600ms 后由 WM_TIMER 弹出
    void ArmTooltip(Widget* w);
    void ShowTooltipFor(Widget* w);
    void HideTooltip();
    void LayoutInfoBars();

    HWND hwnd_ = nullptr;
    Gfx g_;
    Theme theme_;
    std::shared_ptr<Widget> root_;
    std::vector<std::shared_ptr<Widget>> overlays_;
    Widget* hot_ = nullptr;
    Widget* active_ = nullptr;
    Widget* focus_ = nullptr;
    std::function<void()> dismiss_;
    Widget* dismissOwner_ = nullptr;
    float scale_ = 1.f;
    int minW_ = 0, minH_ = 0;
    bool customFrame_ = false;
    float resizeBorder_ = 6.f;
    bool windowActive_ = true;            // 窗口在前台时才画焦点环
    bool focusVisible_ = false;           // 焦点环只在"键盘焦点"时显示：鼠标点出来的焦点不画
    bool destroyed_ = false;              // 已收到 WM_DESTROY（析构时别再 DestroyWindow）
    bool mouseDown_ = false;
    float downX_ = 0.f, downY_ = 0.f;
    bool dragging_ = false;
    std::shared_ptr<Widget> tip_;          // 当前提示气泡（覆盖层）
    Widget* tipOwner_ = nullptr;           // 触发提示的控件
    Widget* tipPending_ = nullptr;         // 已排定、尚未到时间的控件
    std::vector<std::shared_ptr<InfoBar>> bars_;
    float infoBarWidth_ = 360.f;          // 提示条统一宽度
    int width_ = 0, height_ = 0;
    bool animating_ = false;
    unsigned long long animLast_ = 0;
    unsigned long long startTick_ = 0;
    bool pendingFull_ = true;
    D2D1_RECT_F pendingDirty_{ 0, 0, 0, 0 };
    std::wstring title_;
    unsigned long long statTick_ = 0;
    int paintFrames_ = 0;
    float paintMsAcc_ = 0.f;
    float lastPaintMs_ = 0.f;
    int lastDirtyN_ = 0;
    D2D1_RECT_F lastDirtyRc_{ 0, 0, 0, 0 };
    bool showStats_ = false;
};

} // namespace vcui

namespace vcui {
namespace detail {

// 内部：活动 Surface 注册表。多窗口共用一条消息循环时，App 靠它遍历所有窗口
// 推帧。库外不要调用（生命周期由 Create / 析构自动维护）。
VCUI_API const std::vector<Surface*>& LiveSurfaces();

} // namespace detail
} // namespace vcui
