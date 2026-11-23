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

#include "vcui/Surface.h"
#include <dwmapi.h>
#include <windowsx.h>
#include <imm.h>
#include <algorithm>
#include <cmath>

namespace vcui {

namespace {

// 深色标题栏。20 是 Win10 1809+ 的属性号，19 是更早的预览版编号；
// 设置后必须让 DWM 重算一次非客户区，否则已创建的窗口不会立刻换色。
void SetCaptionDark(HWND h, bool dark) {
    BOOL d = dark ? TRUE : FALSE;
    if (FAILED(DwmSetWindowAttribute(h, 20, &d, sizeof(d))))
        DwmSetWindowAttribute(h, 19, &d, sizeof(d));
    SetWindowPos(h, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

// 忽略 Interactive 的命中测试：判断某个点是否落在"装饰性"控件上（标题栏拖动区）。
// Widget::Hit 会跳过非交互控件，所以自绘标题栏要单独走一遍。
static Widget* HitAnyDecorative(Widget* w, float x, float y) {
    if (!w || !w->Visible) return nullptr;
    const auto& kids = w->Children();
    for (auto it = kids.rbegin(); it != kids.rend(); ++it)
        if (Widget* h = HitAnyDecorative(it->get(), x, y)) return h;
    if (w->Clip) {
        D2D1_RECT_F c = w->Content();
        if (x < c.left || x >= c.right || y < c.top || y >= c.bottom) return nullptr;
    }
    D2D1_RECT_F f = w->Frame();
    if (x >= f.left && x < f.right && y >= f.top && y < f.bottom) return w;
    return nullptr;
}

} // namespace

Surface::Surface() = default;
Surface::~Surface() = default;

bool Surface::Create(const std::wstring& title, int width, int height, bool dark) {
    HINSTANCE inst = GetModuleHandleW(nullptr);

    static const wchar_t* kClass = L"VCUISurface";
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &Surface::WndProcThunk;
        wc.hInstance = inst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = kClass;
        if (!RegisterClassExW(&wc)) return false;
        registered = true;
    }

    theme_.Set(Palette::Microsoft, dark);
    startTick_ = GetTickCount64();

    // 客户区尺寸按 DPI 换算回物理像素
    uint32_t dpi = GetDpiForSystem();
    scale_ = (float)dpi / 96.f;

    RECT rc{ 0, 0, (LONG)(width * scale_), (LONG)(height * scale_) };
    // 自绘边框：保留 WS_CAPTION + WS_THICKFRAME 让 DWM 继续给投影/吸附/最大化动画，
    // 但用 WM_NCCALCSIZE 把非客户区吃掉，标题栏改由 TitleBar 控件画。
    DWORD style = customFrame_
        ? (WS_POPUP | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
        : WS_OVERLAPPEDWINDOW;
    // 自绘边框时客户区＝整窗，不能再按非客户区把窗口撑大，否则内容比请求的尺寸大一圈
    if (!customFrame_) AdjustWindowRectEx(&rc, style, FALSE, 0);

    hwnd_ = CreateWindowExW(0, kClass, title.c_str(), style, CW_USEDEFAULT, CW_USEDEFAULT,
                            rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, inst, this);
    if (!hwnd_) return false;

    SetCaptionDark(hwnd_, dark);

    if (!g_.Init(hwnd_)) return false;
    ImmAssociateContextEx(hwnd_, nullptr, IACE_DEFAULT);
    title_ = title;
    {
        wchar_t env[8];
        showStats_ = GetEnvironmentVariableW(L"VCUI_FPS", env, 8) > 0;
    }

    RECT cr{};
    GetClientRect(hwnd_, &cr);
    width_ = cr.right;
    height_ = cr.bottom;

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    return true;
}

static void AttachTree(Widget* w, Surface* host, Gfx* gfx) {
    w->Host = host;
    w->GFX = gfx;
    for (auto& k : w->Children()) AttachTree(k.get(), host, gfx);
}

void Surface::SetRoot(std::shared_ptr<Widget> root) {    root_ = std::move(root);
    if (root_) {
        root_->Parent = nullptr;
        AttachTree(root_.get(), this, &g_);
    }
    Layout();
}

void Surface::Layout() {
    if (g_.rt) g_.rt->SetTransform(D2D1::Matrix3x2F::Identity());
}

void Surface::Invalidate() {
    pendingFull_ = true;
    if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
}

void Surface::InvalidateRectD(D2D1_RECT_F rc) {
    if (pendingFull_) return;   // 已经要整窗重绘，不必再裁
    if (pendingDirty_.right <= pendingDirty_.left) {
        pendingDirty_ = rc;
    } else {
        pendingDirty_.left = std::min(pendingDirty_.left, rc.left);
        pendingDirty_.top = std::min(pendingDirty_.top, rc.top);
        pendingDirty_.right = std::max(pendingDirty_.right, rc.right);
        pendingDirty_.bottom = std::max(pendingDirty_.bottom, rc.bottom);
    }
    if (hwnd_) {
        RECT r{ (LONG)(pendingDirty_.left * scale_), (LONG)(pendingDirty_.top * scale_),
                (LONG)(pendingDirty_.right * scale_ + 1), (LONG)(pendingDirty_.bottom * scale_ + 1) };
        InvalidateRect(hwnd_, &r, FALSE);
    }
}

void Surface::PlayAnim() { KickAnim(); }

void Surface::KickAnim() {
    if (!hwnd_) return;
    animating_ = true;
    animLast_ = GetTickCount64();
}

void Surface::Tick(float dt) {
    if (dt > .25f) dt = .25f;
    bool dirty = false;
    D2D1_RECT_F acc{ 0, 0, 0, 0 };
    bool anyRect = false;
    int nDirty = 0;
    std::function<void(Widget*)> walk = [&](Widget* w) {
        if (w->AnimDirty) {
            ++nDirty;
            D2D1_RECT_F f = w->Frame();
            f.left -= 2; f.top -= 2; f.right += 2; f.bottom += 2;
            if (!anyRect) { acc = f; anyRect = true; }
            else {
                acc.left = std::min(acc.left, f.left); acc.top = std::min(acc.top, f.top);
                acc.right = std::max(acc.right, f.right); acc.bottom = std::max(acc.bottom, f.bottom);
            }
            w->AnimDirty = false;
        }
        for (auto& k : w->Children()) walk(k.get());
    };
    if (root_) {
        dirty |= root_->Update(dt);
        walk(root_.get());
    }
    for (auto& o : overlays_) {
        dirty |= o->Update(dt);
        walk(o.get());
    }

    if (dirty) {
        if (!anyRect) Invalidate();
        else {
            // 动画只重绘标脏区域（含覆盖层）：让淡出这类小区域动画保持满帧，而不是每帧整窗重画。
            // 余量要盖住阴影扩散范围，否则弹层边缘会出现一圈陈旧像素。
            acc.left -= 28; acc.top -= 28; acc.right += 28; acc.bottom += 36;
            InvalidateRectD(acc);
        }
        lastDirtyN_ = nDirty;
        lastDirtyRc_ = acc;
    } else if (!(focus_ && focus_->Focusable())) {
        animating_ = false;   // 没有动画也没有光标闪烁，帧循环可以睡了
    }
}

void Surface::Close() {
    if (hwnd_) DestroyWindow(hwnd_);
}

void Surface::Minimize() {
    if (hwnd_) ShowWindow(hwnd_, SW_MINIMIZE);
}

void Surface::ToggleMaximize() {
    if (!hwnd_) return;
    ShowWindow(hwnd_, IsZoomed(hwnd_) ? SW_RESTORE : SW_MAXIMIZE);
}

bool Surface::Maximized() const {
    return hwnd_ && IsZoomed(hwnd_);
}

void Surface::SetTheme(Palette p, bool dark) {
    theme_.Set(p, dark);
    SetCaptionDark(hwnd_, dark);
    Invalidate();
}

void Surface::SetDark(bool dark) {
    SetTheme(theme_.pal, dark);
}

void Surface::SetAccent(D2D1_COLOR_F a) {
    theme_.SetAccent(a);       // 由 accent 派生 hover / press / soft / sel
    Invalidate();
}

void Surface::AddOverlay(std::shared_ptr<Widget> w) {
    if (!w) return;
    AttachTree(w.get(), this, &g_);
    overlays_.push_back(std::move(w));
    Invalidate();
}

void Surface::RemoveOverlay(Widget* owner) {
    for (auto it = overlays_.begin(); it != overlays_.end(); ++it) {
        if (it->get() == owner) { overlays_.erase(it); break; }
    }
    Invalidate();
}

void Surface::RemoveOverlayDeferred(Widget* owner) {
    if (!owner) return;
    owner->Dead = true;     // 在控件自身打标记，不存裸指针，避免地址复用后误删新弹层
    Invalidate();
}

void Surface::ClearOverlays() {
    overlays_.clear();
    hot_ = nullptr;
    active_ = nullptr;
    Invalidate();
}

void Surface::SetFocus(Widget* w) {
    if (focus_ == w) return;
    KickAnim();
    if (focus_) {
        focus_->Focus = false;
        focus_->OnFocus(false);
    }
    focus_ = w;
    if (focus_) {
        focus_->Focus = true;
        focus_->OnFocus(true);
    }
    Invalidate();
}

void Surface::CollectFocusable(Widget* w, std::vector<Widget*>& out) {
    if (!w || !w->Visible || !w->Enabled) return;
    if (w->Focusable()) out.push_back(w);
    for (auto& k : w->Children()) CollectFocusable(k.get(), out);
}

void Surface::CycleFocus(bool back) {
    std::vector<Widget*> all;
    CollectFocusable(root_.get(), all);
    if (all.empty()) return;
    int idx = -1;
    for (size_t i = 0; i < all.size(); ++i)
        if (all[i] == focus_) { idx = int(i); break; }
    int n = int(all.size());
    int next = back ? (idx <= 0 ? n - 1 : idx - 1) : ((idx + 1) % n);
    SetFocus(all[next]);
}

Widget* Surface::HitAt(float x, float y, bool* inOverlay) {
    for (auto it = overlays_.rbegin(); it != overlays_.rend(); ++it) {
        if (Widget* h = (*it)->Hit(x, y)) {
            if (inOverlay) *inOverlay = true;
            return h;
        }
    }
    if (inOverlay) *inOverlay = false;
    return root_ ? root_->Hit(x, y) : nullptr;
}

void Surface::UpdateCursor(float x, float y) {
    Widget* w = HitAt(x, y);
    if (!w) { SetCursor(LoadCursorW(nullptr, IDC_ARROW)); return; }
    switch (w->CursorAt(x, y)) {
    case CursorShape::Hand: SetCursor(LoadCursorW(nullptr, IDC_HAND)); break;
    case CursorShape::IBeam: SetCursor(LoadCursorW(nullptr, IDC_IBEAM)); break;
    default: SetCursor(LoadCursorW(nullptr, IDC_ARROW)); break;
    }
}

void Surface::Paint() {
    LARGE_INTEGER q0, q1, qf;
    QueryPerformanceFrequency(&qf);
    QueryPerformanceCounter(&q0);
    bool removed = false;
    for (auto it = overlays_.begin(); it != overlays_.end();) {
        if (it->get()->Dead) {
            if (hot_ == it->get()) hot_ = nullptr;
            if (active_ == it->get()) active_ = nullptr;
            it = overlays_.erase(it);
            removed = true;
        } else ++it;
    }
    if (removed) pendingFull_ = true;   // 覆盖层消失后它下面的像素必须整窗重绘，否则残留旧帧
    if (!g_.Ready()) {
        if (!g_.Init(hwnd_)) return;
    }
    if (!g_.EnsureLayer((UINT)width_, (UINT)height_)) return;
    float s = scale_;
    float w = float(width_) / s, h = float(height_) / s;

    g_.rt->BeginDraw();
    g_.rt->SetTransform(D2D1::Matrix3x2F::Identity());
    g_.rt->SetTransform(D2D1::Matrix3x2F::Scale(s, s, D2D1::Point2F(0, 0)));

    bool partial = !pendingFull_ && pendingDirty_.right > pendingDirty_.left;
    if (partial) {
        g_.PushClip(pendingDirty_);
        g_.FillRect(pendingDirty_, theme_.t.bj);
    } else {
        g_.Clear(theme_.t.bj);
    }

    PaintCtx ctx{ g_, theme_.t, theme_.m, s };
    ctx.time = float((GetTickCount64() - startTick_) % 1000000) / 1000.f;

    if (root_) {
        root_->Arrange(D2D1_RECT_F{ 0, 0, w, h });
        root_->PaintTree(ctx);
    }
    for (auto& o : overlays_) {
        o->Arrange(D2D1_RECT_F{ o->X, o->Y, o->X + o->W, o->Y + o->H });
        o->PaintTree(ctx);
    }

    if (partial) g_.PopClip();
    HRESULT hr = g_.rt->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        g_.DropResources();
        g_.Init(hwnd_);
        Invalidate();
    } else {
        g_.Present();
    }
    QueryPerformanceCounter(&q1);
    if (showStats_) {   // 帧统计：标题栏显示 fps / 绘制耗时（设 VCUI_FPS=1 打开）
        float ms = float(q1.QuadPart - q0.QuadPart) * 1000.f / float(qf.QuadPart);
        paintMsAcc_ += ms;
        ++paintFrames_;
        unsigned long long now = GetTickCount64();
        if (now - statTick_ >= 1000) {
            float fps = paintFrames_ ? float(paintFrames_) * 1000.f / float(now - statTick_) : 0.f;
            float avg = paintFrames_ ? paintMsAcc_ / float(paintFrames_) : 0.f;
            wchar_t buf[160];
            if (paintFrames_)
                swprintf_s(buf, L"%s  |  fps %.0f  paint %.2fms", title_.c_str(), fps, avg);
            else
                swprintf_s(buf, L"%s  |  idle  last %.2fms", title_.c_str(), lastPaintMs_);
            lastPaintMs_ = avg;
            SetWindowTextW(hwnd_, buf);
            statTick_ = now;
            paintFrames_ = 0;
            paintMsAcc_ = 0.f;
        }
    }
    pendingFull_ = false;
    pendingDirty_ = D2D1_RECT_F{ 0, 0, 0, 0 };
}

LRESULT CALLBACK Surface::WndProcThunk(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    Surface* self = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = reinterpret_cast<Surface*>(cs->lpCreateParams);
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<Surface*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    }
    if (self && self->hwnd_ == nullptr) self->hwnd_ = h;
    if (self) return self->Handle(msg, wp, lp);
    return DefWindowProcW(h, msg, wp, lp);
}

LRESULT Surface::Handle(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE: {
        width_ = LOWORD(lp);
        height_ = HIWORD(lp);
        if (g_.rt) g_.Resize(width_, height_);
        Invalidate();
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;

    case WM_TIMER: {
        unsigned long long now = GetTickCount64();
        float dt = float(now - animLast_) / 1000.f;
        animLast_ = now;
        Tick(dt);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd_, &ps);
        (void)hdc;
        Paint();
        EndPaint(hwnd_, &ps);
        return 0;
    }

    case WM_NCCALCSIZE:
        // 自绘边框：客户区＝整窗。最大化时把客户区对齐到显示器工作区，
        // 否则 WS_THICKFRAME 会把窗口顶出屏幕 8px。
        if (customFrame_ && wp) {
            NCCALCSIZE_PARAMS* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(lp);
            if (IsZoomed(hwnd_)) {
                MONITORINFO mi{};
                mi.cbSize = sizeof(mi);
                if (GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi))
                    p->rgrc[0] = mi.rcWork;
            }
            return 0;
        }
        break;

    case WM_NCPAINT:
        if (customFrame_) return 0;   // 没有非客户区可画
        break;

    case WM_NCACTIVATE:
        if (customFrame_) return TRUE;   // 别让系统重画标题栏
        break;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mi = reinterpret_cast<MINMAXINFO*>(lp);
        uint32_t dpi = GetDpiForWindow(hwnd_);
        float sc = (float)dpi / 96.f;
        // 最小尺寸由 SetMinSize 决定（库默认给一个不会把布局挤坏的小值）
        int mw = minW_ > 0 ? minW_ : 320, mh = minH_ > 0 ? minH_ : 200;
        mi->ptMinTrackSize.x = (LONG)(mw * sc);
        mi->ptMinTrackSize.y = (LONG)(mh * sc);
        return 0;
    }

    case WM_NCHITTEST: {
        POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ScreenToClient(hwnd_, &pt);
        float x = (float)pt.x / scale_, y = (float)pt.y / scale_;

        // 自绘边框：先判边缘缩放（最大化时没有可拖的边）
        if (customFrame_ && !IsZoomed(hwnd_)) {
            // pt 已被 ScreenToClient 转成客户区坐标（物理像素），这里必须用客户区矩形比较，
            // 拿 GetWindowRect 的屏幕坐标去比会几乎命中整窗。
            RECT cr{};
            GetClientRect(hwnd_, &cr);
            int bw = (int)std::ceil(resizeBorder_ * scale_);
            bool L = pt.x < bw, R = pt.x >= cr.right - bw;
            bool T = pt.y < bw, B = pt.y >= cr.bottom - bw;
            if (T && L) return HTTOPLEFT;
            if (T && R) return HTTOPRIGHT;
            if (B && L) return HTBOTTOMLEFT;
            if (B && R) return HTBOTTOMRIGHT;
            if (L) return HTLEFT;
            if (R) return HTRIGHT;
            if (T) return HTTOP;
            if (B) return HTBOTTOM;
        }

        LRESULT r = DefWindowProcW(hwnd_, msg, wp, lp);
        if (r != HTCLIENT) return r;

        // 落在 Caption 子树上的非交互区域 → 交给系统做窗口拖动/双击最大化
        Widget* w = root_ ? root_->Hit(x, y) : nullptr;
        if (!w || !w->Interactive) {
            Widget* any = root_ ? HitAnyDecorative(root_.get(), x, y) : nullptr;
            for (Widget* p = any; p; p = p->Parent)
                if (p->Caption) return HTCAPTION;
        }
        return HTCLIENT;
    }

    case WM_MOUSEMOVE: {
        float x = (float)GET_X_LPARAM(lp) / scale_;
        float y = (float)GET_Y_LPARAM(lp) / scale_;

        if (mouseDown_ && active_) {
            if (!dragging_) {
                float dx = x - downX_, dy = y - downY_;
                if (dx * dx + dy * dy > 9.f) dragging_ = true;
            }
            if (dragging_) active_->OnDrag(x, y);
        }

        Widget* w = HitAt(x, y);
        if (w != hot_) {
            if (hot_) { hot_->Hover = false; hot_->OnHover(false); }
            hot_ = w;
            if (hot_) { hot_->Hover = true; hot_->OnHover(true); }
        }
        UpdateCursor(x, y);
        KickAnim();
        Invalidate();
        return 0;
    }

    case WM_LBUTTONDOWN: {
        float x = (float)GET_X_LPARAM(lp) / scale_;
        float y = (float)GET_Y_LPARAM(lp) / scale_;
        bool inOverlay = false;
        Widget* w = HitAt(x, y, &inOverlay);

        if (!inOverlay && dismiss_) {
            bool onOwner = false;
            for (Widget* q = w; q; q = q->Parent)
                if (q == dismissOwner_) { onOwner = true; break; }
            if (!onOwner) {
                auto fn = dismiss_;
                dismiss_ = nullptr;
                dismissOwner_ = nullptr;
                fn();
            }
        }

        SetCapture(hwnd_);
        mouseDown_ = true;
        downX_ = x; downY_ = y;
        dragging_ = false;
        active_ = w;
        if (w) {
            if (w->Focusable()) SetFocus(w);
            else SetFocus(nullptr);
            w->OnPressAt(x, y);
        } else {
            SetFocus(nullptr);
        }
        KickAnim();
        Invalidate();
        return 0;
    }

    case WM_LBUTTONUP: {
        float x = (float)GET_X_LPARAM(lp) / scale_;
        float y = (float)GET_Y_LPARAM(lp) / scale_;
        ReleaseCapture();
        mouseDown_ = false;
        Widget* w = active_;
        active_ = nullptr;
        dragging_ = false;
        if (w) {
            bool inside = w->HitTest(x, y);
            w->OnRelease(inside);
            if (inside) w->OnClickAt(x, y);
        }
        UpdateCursor(x, y);
        KickAnim();
        Invalidate();
        return 0;
    }

    case WM_MOUSEWHEEL: {
        POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ScreenToClient(hwnd_, &pt);
        float x = (float)pt.x / scale_;
        float y = (float)pt.y / scale_;
        Widget* w = HitAt(x, y);
        float dy = (float)GET_WHEEL_DELTA_WPARAM(wp) / (float)WHEEL_DELTA;
        // 冒泡到最近的可滚动祖先
        while (w) {
            if (w->WantsWheel()) {
                w->OnWheel(dy, x, y);
                InvalidateRectD(w->Frame());   // 滚动只重绘滚动容器，别整窗刷
                KickAnim();
                break;
            }
            w = w->Parent;
        }
        return 0;
    }

    case WM_KEYDOWN: {
        if (wp == VK_TAB) {
            CycleFocus((GetKeyState(VK_SHIFT) & 0x8000) != 0);
            return 0;
        }
        if (wp == VK_ESCAPE) {
            if (dismiss_) { auto fn = dismiss_; dismiss_ = nullptr; fn(); }
            return 0;
        }
        if (focus_) focus_->OnKeyDown((int)wp);
        Invalidate();
        return 0;
    }

    case WM_CHAR: {
        if (wp >= 32 && focus_ && !focus_->ImeComposing()) focus_->OnChar((wchar_t)wp);
        Invalidate();
        return 0;
    }

    case WM_IME_STARTCOMPOSITION:
    case WM_IME_COMPOSITION:
    case WM_IME_ENDCOMPOSITION: {
        if (focus_ && focus_->OnImeMessage(hwnd_, msg, wp, lp)) {
            KickAnim();
            Invalidate();
            return 0;
        }
        break;
    }

    case WM_SETCURSOR: {
        if (LOWORD(lp) == HTCLIENT) {
            POINT pt{};
            GetCursorPos(&pt);
            ScreenToClient(hwnd_, &pt);
            UpdateCursor((float)pt.x / scale_, (float)pt.y / scale_);
            return TRUE;
        }
        break;
    }

    case WM_DESTROY:
        g_.DropResources();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

int Surface::Run() {
    MSG msg{};
    LARGE_INTEGER freq, last;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);
    animLast_ = GetTickCount64();

    for (;;) {
        // 消息优先：把所有已排队的消息处理干净
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return (int)msg.wParam;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (animating_) {
            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);
            float dt = float(now.QuadPart - last.QuadPart) / float(freq.QuadPart);
            last = now;
            if (dt <= 0.f) dt = .008f;
            LARGE_INTEGER t0;
            QueryPerformanceCounter(&t0);
            Tick(dt);
            (void)t0;
            if (hwnd_) UpdateWindow(hwnd_);   // 只处理 Tick 标脏的那块
            // 用 DWM 合成节拍锁帧，保证每帧等长、不抖
            DwmFlush();
        } else {
            MsgWaitForMultipleObjectsEx(0, nullptr, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            last = LARGE_INTEGER{};
            QueryPerformanceCounter(&last);
        }
    }
}

} // namespace vcui