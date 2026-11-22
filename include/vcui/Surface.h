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

    // title 为空时用 L"Void Craft UI"
    bool Create(const std::wstring& title, int width, int height, bool dark = false);
    // 窗口最小尺寸（DIP），Create 之前或之后都可调用
    void SetMinSize(int w, int h) { minW_ = w; minH_ = h; }
    int Run();
    void Close();
    void Minimize();
    void ToggleMaximize();
    bool Maximized() const;
    Gfx& G() { return g_; }

    void SetRoot(std::shared_ptr<Widget> root);
    Widget* Root() const { return root_.get(); }

    void Invalidate();
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

private:
    static LRESULT CALLBACK WndProcThunk(HWND, UINT, WPARAM, LPARAM);
    LRESULT Handle(UINT msg, WPARAM wp, LPARAM lp);

    void Paint();
    void Layout();
    void Tick(float dt);
    void UpdateCursor(float x, float y);
    Widget* HitAt(float x, float y, bool* inOverlay = nullptr);
    void CollectFocusable(Widget* w, std::vector<Widget*>& out);
    void CycleFocus(bool back);

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
    bool mouseDown_ = false;
    float downX_ = 0.f, downY_ = 0.f;
    bool dragging_ = false;
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
