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
#include "vcui/App.h"
#include "vcui/Controls.h"
#include <dwmapi.h>
#include <windowsx.h>
#include <imm.h>
#include <algorithm>
#include <cmath>

namespace vcui {

namespace {

// 活动窗口注册表：多窗口共用一条消息循环时，帧推进要遍历所有窗口。
std::vector<Surface*> g_surfaces;
int g_liveWindows = 0;

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

namespace detail {
const std::vector<Surface*>& LiveSurfaces() { return g_surfaces; }
} // namespace detail

Surface::Surface() = default;

Surface::~Surface() {
    if (hwnd_ && !destroyed_) {
        DestroyWindow(hwnd_);       // 会回调 Handle(WM_DESTROY)（Quit 计数在那里减）
    }
    hwnd_ = nullptr;
    for (auto it = g_surfaces.begin(); it != g_surfaces.end(); ++it) {
        if (*it == this) { g_surfaces.erase(it); break; }
    }
}

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

    // 注册进活动窗口表：App 的共享消息循环靠它遍历所有窗口推帧
    g_surfaces.push_back(this);
    ++g_liveWindows;

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
        if (!anyRect) {
            // 只是后台泵要跑（下载进度、图标队列），没有可视变化：
            // 停掉 60fps 帧循环，改用 40ms 定时器继续泵，别白烧 CPU
            animating_ = false;
            SetTimer(hwnd_, 4, 40, nullptr);
        } else {
            KillTimer(hwnd_, 4);
            // 动画只重绘标脏区域（含覆盖层）：让淡出这类小区域动画保持满帧，而不是每帧整窗重画。
            // 余量要盖住阴影扩散范围，否则弹层边缘会出现一圈陈旧像素。
            acc.left -= 28; acc.top -= 28; acc.right += 28; acc.bottom += 36;
            InvalidateRectD(acc);
        }
        lastDirtyN_ = nDirty;
        lastDirtyRc_ = acc;
    } else {
        KillTimer(hwnd_, 4);
        animating_ = false;   // 没有动画，帧循环睡觉；光标闪烁由定时器负责
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
    tip_.reset();
    tipOwner_ = nullptr;
    tipPending_ = nullptr;
    bars_.clear();
    if (hwnd_) { KillTimer(hwnd_, 1); KillTimer(hwnd_, 2); }
    Invalidate();
}

void Surface::ArmTooltip(Widget* w) {
    if (hwnd_) KillTimer(hwnd_, 1);
    if (tipPending_ && tipPending_ != w) { /* 换了控件，旧提示不弹 */ }
    tipPending_ = (w && !w->Tooltip.empty() && tipOwner_ != w) ? w : nullptr;
    if (tipPending_ && hwnd_) SetTimer(hwnd_, 1, 600, nullptr);
}

void Surface::ShowTooltipFor(Widget* w) {
    if (!w || w->Tooltip.empty() || !w->Visible) return;
    auto tip = std::make_shared<Tooltip>();
    tip->Text = w->Tooltip;
    tip->GFX = &g_;                 // 先给测量入口，才能算出气泡宽度
    float tw = g_.TextW(tip->Text, 12.f, 400) + 26.f;
    float th = 28.f;
    if (tw < 40.f) tw = 40.f;
    D2D1_RECT_F f = w->Frame();
    D2D1_SIZE_F cs = ClientSize();

    float x = (f.left + f.right) * .5f - tw * .5f;
    float y = f.bottom + 6.f;
    if (y + th > cs.height - 6.f) y = f.top - 6.f - th;   // 下面放不下就翻到上面
    if (y < 4.f) y = 4.f;
    x = std::clamp(x, 6.f, std::max(6.f, cs.width - tw - 6.f));

    HideTooltip();
    tip->X = x; tip->Y = y; tip->W = tw; tip->H = th;
    tip_ = tip;
    tipOwner_ = w;
    AddOverlay(tip);
    Invalidate();
}

void Surface::HideTooltip() {
    if (hwnd_) KillTimer(hwnd_, 1);
    tipPending_ = nullptr;
    if (tip_) {
        RemoveOverlay(tip_.get());
        tip_.reset();
    }
    tipOwner_ = nullptr;
}

void Surface::LayoutInfoBars() {
    float csW = ClientSize().width;
    float y = 56.f;      // 标题栏下方
    for (auto& b : bars_) {
        if (b->Dead) continue;
        if (b->W <= 0.f || b->H <= 0.f) continue;
        b->baseX = csW - b->W - 16.f;
        b->X = b->baseX + 24.f * (1.f - b->aIn);
        b->Y = y;
        y += b->H + 8.f;
    }
}

void Surface::ShowInfoBar(const std::wstring& text, InfoTone tone, float seconds) {
    if (text.empty()) return;
    auto bar = std::make_shared<InfoBar>(text, tone, std::max(600.f, seconds * 1000.f));
    // 统一宽度：所有提示条一样长，超宽文字省略；被截断时悬停看全文
    bar->W = infoBarWidth_;
    AddOverlay(bar);                       // 先挂上，才有 GFX 可以量字宽
    D2D1_SIZE_F d = bar->Desired(infoBarWidth_, 60.f);
    bar->H = d.height;
    float textArea = infoBarWidth_ - 82.f;
    if (g_.Ready() && g_.TextW(text, 14.f, 400) > textArea - 2.f) bar->Tooltip = text;
    bars_.push_back(bar);
    LayoutInfoBars();
    if (hwnd_) SetTimer(hwnd_, 2, 150, nullptr);
    PlayAnim();
    Invalidate();
}

void Surface::CloseInfoBars() {
    for (auto& b : bars_) b->StartExit();
    PlayAnim();
}

void Surface::ForgetWidget(Widget* w) {
    if (!w) return;
    // 这里不回调 OnFocus(false)：对象已经在析构，不能再碰它
    if (focus_ == w) { focus_ = nullptr; if (hwnd_) KillTimer(hwnd_, 3); }
    if (hot_ == w) hot_ = nullptr;
    if (active_ == w) active_ = nullptr;
    if (tipPending_ == w) tipPending_ = nullptr;
    if (tipOwner_ == w) {
        if (tip_) tip_->Dead = true;   // 气泡属于这个控件，一起撤掉
        tip_.reset();
        tipOwner_ = nullptr;
    }
    if (dismissOwner_ == w) {
        dismiss_ = nullptr;
        dismissOwner_ = nullptr;
    }
}

void Surface::SetFocus(Widget* w) {
    if (focus_ == w) { Invalidate(); return; }   // 焦点环可见性可能刚被切过，仍要重绘
    KickAnim();
    if (focus_) {
        focus_->Focus = false;
        focus_->OnFocus(false);
    }
    focus_ = w;
    if (dynamic_cast<Input*>(focus_)) SetTimer(hwnd_, 3, 530, nullptr);
    else KillTimer(hwnd_, 3);
    if (focus_) {
        focus_->Focus = true;
        focus_->OnFocus(true);
        EnsureVisible(focus_);      // Tab 走到屏幕外的控件时自动滚进来
    }
    Invalidate();
}

void Surface::EnsureVisible(Widget* w) {
    if (!w) return;
    // 从自身往上找滚动容器，让它们把这块矩形滚进视口
    D2D1_RECT_F rc = w->Frame();
    for (Widget* p = w->Parent; p; p = p->Parent) {
        if (auto* sv = dynamic_cast<ScrollView*>(p)) {
            sv->ScrollIntoView(rc);
            KickAnim();
        }
    }
}

void Surface::PaintFocusRing(PaintCtx& ctx) {
    Widget* f = focus_;
    // Fluent 的规矩：鼠标点出来的焦点不画环，只有键盘走出来的焦点（focus visible）才画
    if (!f || !f->FocusRing || !windowActive_ || !focusVisible_) return;
    // 祖先里有隐藏的直接不画
    for (Widget* p = f; p; p = p->Parent)
        if (!p->Visible) return;
    D2D1_RECT_F fr = f->Frame();
    if (fr.right <= fr.left || fr.bottom <= fr.top) return;

    // 焦点环会被滚动容器裁掉一部分：按最近一层裁剪矩形夹一次
    D2D1_RECT_F clip{ -1e6f, -1e6f, 1e6f, 1e6f };
    for (Widget* p = f->Parent; p; p = p->Parent) {
        if (!p->Clip) continue;
        D2D1_RECT_F c = p->Content();
        clip.left = std::max(clip.left, c.left);
        clip.top = std::max(clip.top, c.top);
        clip.right = std::min(clip.right, c.right);
        clip.bottom = std::min(clip.bottom, c.bottom);
    }
    if (clip.right <= clip.left || clip.bottom <= clip.top) return;

    // Fluent：控件外扩 2px 的 2px accent 描边（浅色下也够对比度）
    D2D1_RECT_F ring{ std::max(fr.left - 2.f, clip.left), std::max(fr.top - 2.f, clip.top),
                      std::min(fr.right + 2.f, clip.right), std::min(fr.bottom + 2.f, clip.bottom) };
    if (ring.right <= ring.left || ring.bottom <= ring.top) return;
    ctx.g.StrokeRect(ring, theme_.m.radiusBtn + 2.f, theme_.t.accent, 2.f);
}

void Surface::CollectFocusable(Widget* w, std::vector<Widget*>& out) {
    if (!w || !w->Visible || !w->Enabled) return;
    if (w->Focusable()) out.push_back(w);
    for (auto& k : w->Children()) CollectFocusable(k.get(), out);
}

void Surface::CycleFocus(bool back) {
    // Tab 的作用域：有模态（FillHost 覆盖层，比如对话框）时只在最上面那层里转，
    // 否则会跑到被遮罩盖住的页面控件上
    Widget* scope = nullptr;
    for (auto it = overlays_.rbegin(); it != overlays_.rend(); ++it) {
        if ((*it)->FillHost) { scope = it->get(); break; }
    }
    std::vector<Widget*> all;
    CollectFocusable(scope ? scope : root_.get(), all);
    if (all.empty()) return;
    int idx = -1;
    for (size_t i = 0; i < all.size(); ++i)
        if (all[i] == focus_) { idx = int(i); break; }
    int n = int(all.size());
    int next = back ? (idx <= 0 ? n - 1 : idx - 1) : ((idx + 1) % n);
    focusVisible_ = true;      // 键盘走出来的焦点才画环
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
    g_.cullOn = partial;
    g_.cullRect = D2D1_RECT_F{ pendingDirty_.left - 2, pendingDirty_.top - 2,
                               pendingDirty_.right + 2, pendingDirty_.bottom + 2 };
    if (partial) {
        g_.PushClip(pendingDirty_);
        g_.FillRect(pendingDirty_, theme_.t.bj);
    } else {
        g_.Clear(theme_.t.bj);
    }

    g_.rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);   // 位图 RT 上 ClearType 会被逐次模拟，灰度快得多
    PaintCtx ctx{ g_, theme_.t, theme_.m, s };
    ctx.time = float((GetTickCount64() - startTick_) % 1000000) / 1000.f;

    if (root_) {
        QueryPerformanceCounter(&q0);
        root_->Arrange(D2D1_RECT_F{ 0, 0, w, h });
        QueryPerformanceCounter(&q1);
        arrangeMsAcc_ += float(q1.QuadPart - q0.QuadPart) * 1000.f / float(qf.QuadPart);
        root_->PaintTree(ctx);
    }
    for (auto& o : overlays_) {
        if (o->FillHost) o->Arrange(D2D1_RECT_F{ 0, 0, w, h });   // 对话框遮罩铺满、随缩放自适应
        else o->Arrange(D2D1_RECT_F{ o->X, o->Y, o->X + o->W, o->Y + o->H });
        o->PaintTree(ctx);
    }

    PaintFocusRing(ctx);   // 焦点环画在最后，压在所有内容之上

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
                swprintf_s(buf, L"%s  |  fps %.0f  paint %.2fms  arrange %.2fms", title_.c_str(), fps, avg, paintFrames_ ? arrangeMsAcc_ / float(paintFrames_) : 0.f);
            else
                swprintf_s(buf, L"%s  |  idle  last %.2fms", title_.c_str(), lastPaintMs_);
            lastPaintMs_ = avg;
            SetWindowTextW(hwnd_, buf);
            statTick_ = now;
            paintFrames_ = 0;
            paintMsAcc_ = 0.f;
            arrangeMsAcc_ = 0.f;
        }
    }
    pendingFull_ = false;
    pendingDirty_ = D2D1_RECT_F{ 0, 0, 0, 0 };
    paintTick_ = GetTickCount64();
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

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd_, &ps);
        (void)hdc;
        // 节流：8ms 内的多次 WM_PAINT 合并，等帧循环下一次 UpdateWindow 再画
        unsigned long long tickNow = GetTickCount64();
        if (!pendingFull_ && paintTick_ && tickNow - paintTick_ < 8) {
            EndPaint(hwnd_, &ps);
            return 0;
        }
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
        bool hotChanged = (w != hot_);
        if (hotChanged) {
            if (hot_) { hot_->Hover = false; hot_->OnHover(false); }
            hot_ = w;
            if (hot_) { hot_->Hover = true; hot_->OnHover(true); }
            ArmTooltip(hot_);
        }
        UpdateCursor(x, y);
        if (hotChanged) {
            KickAnim();    // 只有悬停目标变了才起缓动；单纯移动鼠标不再整窗重画
            Invalidate();
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        float x = (float)GET_X_LPARAM(lp) / scale_;
        float y = (float)GET_Y_LPARAM(lp) / scale_;
        HideTooltip();
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
        // 鼠标点出来的焦点不画焦点环（Fluent：focus 有，focus visible 没有）
        if (focusVisible_) { focusVisible_ = false; Invalidate(); }
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

    case WM_TIMER: {
        if (wp == 1) {                      // 悬停提示到期
            KillTimer(hwnd_, 1);
            Widget* w = tipPending_;
            tipPending_ = nullptr;
            if (w && w == hot_ && !mouseDown_) ShowTooltipFor(w);
            return 0;
        }
        if (wp == 4) {                      // 后台泵节拍：没有可视动画时的低频推进
            unsigned long long now4 = GetTickCount64();
            float dt4 = float(now4 - animLast_) / 1000.f;
            animLast_ = now4;
            Tick(dt4);
            return 0;
        }
        if (wp == 3) {                      // 输入框光标闪烁：静止时靠它，不拉帧循环
            if (auto* in = dynamic_cast<Input*>(focus_)) {
                in->ToggleCaret();
                InvalidateRectD(in->Frame());
            } else {
                KillTimer(hwnd_, 3);
            }
            return 0;
        }
        if (wp == 2) {                      // 提示条计时：静止时没有帧循环，靠这里推时钟
            for (size_t i = 0; i < bars_.size();) {
                auto& b = bars_[i];
                if (b->Dead) { bars_.erase(bars_.begin() + i); continue; }
                b->TickClock();
                if (!b->Exiting && b->holdMs_ >= b->LifeMs) b->StartExit();
                ++i;
            }
            if (bars_.empty()) KillTimer(hwnd_, 2);
            PlayAnim();
            // 继续走通用帧推进，让淡出动画平滑
        }
        unsigned long long now = GetTickCount64();
        float dt = float(now - animLast_) / 1000.f;
        animLast_ = now;
        Tick(dt);
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
        if (wp == VK_TAB) {                       // 焦点环走一遍可聚焦控件
            CycleFocus((GetKeyState(VK_SHIFT) & 0x8000) != 0);
            return 0;
        }
        if (wp == VK_ESCAPE) {
            HideTooltip();
            if (dismiss_) {                       // 先关最上层弹层 / 对话框
                auto fn = dismiss_;
                dismiss_ = nullptr;
                dismissOwner_ = nullptr;
                fn();
                KickAnim();
                Invalidate();
                return 0;
            }
        }
        // 键盘操作过就进入"键盘焦点"模式（NavItem 的方向键换焦点也在这里生效）
        if (!focusVisible_) { focusVisible_ = true; Invalidate(); }
        if (focus_) focus_->OnKeyDown((int)wp);
        Invalidate();
        return 0;
    }

    case WM_SYSKEYDOWN: {
        // Alt+Down / Alt+Up 是下拉框的"展开/收起"标准快捷键，只转发这两键，
        // 其余交给系统（Alt+F4 关闭、Alt+Space 系统菜单）
        if ((GetKeyState(VK_MENU) & 0x8000) && focus_ && (wp == VK_DOWN || wp == VK_UP)) {
            if (!focusVisible_) focusVisible_ = true;
            focus_->OnKeyDown(wp == VK_DOWN ? VK_F4 : VK_ESCAPE);
            Invalidate();
            return 0;
        }
        break;
    }

    case WM_ACTIVATE: {
        bool active = LOWORD(wp) != WA_INACTIVE;
        if (active != windowActive_) {
            windowActive_ = active;
            Invalidate();                         // 失焦时不画焦点环
        }
        break;
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
        KillTimer(hwnd_, 1);
        KillTimer(hwnd_, 2);
        g_.DropResources();
        destroyed_ = true;    // hwnd_ 保留（WndProcThunk 还会用到），但析构时不再重复 DestroyWindow
        // 多窗口：只有最后一个窗口关掉才结束消息循环，否则关一个窗整进程就退了
        if (--g_liveWindows <= 0) {
            g_liveWindows = 0;
            PostQuitMessage(0);
        }
        return 0;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

int Surface::Run() {
    // 单窗口也走 App 的共享循环：同一个进程里再开窗口时只有一条消息循环，
    // 否则第二个窗口的消息没人 Pump，等于卡死。
    return RunAppLoop();
}

} // namespace vcui