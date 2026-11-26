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

#include "vcui/Gfx.h"
#include <d2d1helper.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace vcui {

using Microsoft::WRL::ComPtr;

static uint32_t ColorKey(D2D1_COLOR_F c) {
    uint32_t r = (uint32_t)(c.r * 255.f + .5f) & 0xFF;
    uint32_t g = (uint32_t)(c.g * 255.f + .5f) & 0xFF;
    uint32_t b = (uint32_t)(c.b * 255.f + .5f) & 0xFF;
    uint32_t a = (uint32_t)(c.a * 255.f + .5f) & 0xFF;
    return (a << 24) | (r << 16) | (g << 8) | b;
}

static uint64_t Mix64(uint64_t h, uint64_t v) {
    h ^= v;
    h *= 1099511628211ULL;
    return h;
}

static int FmtKeyOf(float px, int weight, const wchar_t* family, HAlign ha, VAlign va) {
    int key = int(px * 100) * 1000 + weight;
    key += (int)wcslen(family) * 7;
    key += (int)ha * 13 + (int)va * 29;
    return key;
}

bool Gfx::Init(HWND hwnd) {
    if (!factory) {
        D2D1_FACTORY_OPTIONS fo{};
        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1),
                                     &fo, reinterpret_cast<void**>(factory.GetAddressOf()))))
            return false;
    }
    if (!dwrite) {
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                            reinterpret_cast<IUnknown**>(dwrite.GetAddressOf()));
    }

    RECT rc{};
    GetClientRect(hwnd, &rc);
    UINT w = (UINT)(rc.right > rc.left ? rc.right - rc.left : 1);
    UINT h = (UINT)(rc.bottom > rc.top ? rc.bottom - rc.top : 1);
    dpi_ = (float)GetDpiForWindow(hwnd);
    if (dpi_ <= 0.f) dpi_ = 96.f;

    // D3D11 设备：BGRA 支持是 D2D 互操作的前提；硬件失败退回 WARP（远程桌面/无显卡也能跑）
    if (!d3d) {
        D3D_FEATURE_LEVEL want[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
                                     D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
        D3D_FEATURE_LEVEL got{};
        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                       D3D11_CREATE_DEVICE_BGRA_SUPPORT, want, 4, D3D11_SDK_VERSION,
                                       d3d.GetAddressOf(), &got, d3dCtx.GetAddressOf());
        if (FAILED(hr))
            hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                                   D3D11_CREATE_DEVICE_BGRA_SUPPORT, want, 4, D3D11_SDK_VERSION,
                                   d3d.GetAddressOf(), &got, d3dCtx.GetAddressOf());
        if (FAILED(hr)) {
            d3d.Reset();
            d3dCtx.Reset();
            return false;
        }
    }
    if (!d2dDevice) {
        Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDev;
        if (FAILED(d3d.As(&dxgiDev))) return false;
        if (FAILED(factory->CreateDevice(dxgiDev.Get(), d2dDevice.GetAddressOf()))) return false;
    }

    // 交换链：FLIP 模型，两缓冲
    if (!swap) {
        Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDev;
        if (FAILED(d3d.As(&dxgiDev))) return false;
        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
        if (FAILED(dxgiDev->GetAdapter(adapter.GetAddressOf()))) return false;
        Microsoft::WRL::ComPtr<IDXGIFactory2> fx;
        if (FAILED(adapter->GetParent(IID_PPV_ARGS(&fx)))) return false;
        DXGI_SWAP_CHAIN_DESC1 sd{};
        sd.Width = w;
        sd.Height = h;
        sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        sd.SampleDesc.Count = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 2;
        sd.Scaling = DXGI_SCALING_STRETCH;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        sd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        if (FAILED(fx->CreateSwapChainForHwnd(d3d.Get(), hwnd, &sd, nullptr, nullptr,
                                              swap.GetAddressOf())))
            return false;
    }
    if (!dc) {
        if (FAILED(d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                                                  dc.GetAddressOf())))
            return false;
        rt = dc;
    }

    layer.Reset();
    brushes.clear();
    shadowCache.clear();
    clipDepth = 0;
    alpha = 1.f;
    if (!EnsureSwapTarget(w, h)) return false;
    if (!EnsureLayer(w, h)) return false;
    return true;
}

bool Gfx::EnsureSwapTarget(UINT w, UINT h) {
    if (!swap || !dc) return false;
    if (swapBmp) {
        D2D1_SIZE_U ps = swapBmp->GetPixelSize();
        if (ps.width == w && ps.height == h) return true;
    }
    swapBmp.Reset();
    Microsoft::WRL::ComPtr<IDXGISurface> surf;
    if (FAILED(swap->GetBuffer(0, IID_PPV_ARGS(&surf)))) return false;
    // 传自定义 properties 会被 D2D 判 E_INVALIDARG，交给 D2D 按 surface 自身格式/alpha 推断
    if (FAILED(dc->CreateBitmapFromDxgiSurface(surf.Get(), nullptr, swapBmp.GetAddressOf()))) return false;
    return true;
}

bool Gfx::EnsureLayer(UINT w, UINT h) {
    if (!dc || !swap || w == 0 || h == 0) return false;
    if (layer) {
        D2D1_SIZE_U ps = layer->GetPixelSize();
        if (ps.width == w && ps.height == h) {
            dc->SetTarget(layer.Get());
            return true;
        }
    }
    layer.Reset();
    brushes.clear();
    shadowCache.clear();
    D2D1_BITMAP_PROPERTIES1 bp{};
    bp.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
    bp.dpiX = bp.dpiY = dpi_;
    bp.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
    if (FAILED(dc->CreateBitmap(D2D1::SizeU(w, h), nullptr, 0, bp, layer.GetAddressOf()))) return false;
    dc->SetTarget(layer.Get());
    dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    dc->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    return true;
}

void Gfx::Present() {
    if (!swap || !layer || !dc || !swapBmp) return;
    dc->SetTarget(swapBmp.Get());
    dc->SetDpi(dpi_, dpi_);
    dc->BeginDraw();
    dc->SetTransform(D2D1::Matrix3x2F::Identity());
    D2D1_SIZE_F sz = swapBmp->GetSize();
    dc->DrawBitmap(layer.Get(), D2D1::RectF(0, 0, sz.width, sz.height), 1.f,
                   D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
    HRESULT hr = dc->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DropResources();
        return;
    }
    swap->Present(1, 0);
    dc->SetDpi(dpi_, dpi_);
    dc->SetTarget(layer.Get());
}

void Gfx::Resize(UINT w, UINT h) {
    if (!swap || !dc || w == 0 || h == 0) return;
    if (layer) {
        D2D1_SIZE_U ps = layer->GetPixelSize();
        if (ps.width == w && ps.height == h) return;
    }
    dc->SetTarget(nullptr);
    swapBmp.Reset();
    layer.Reset();
    if (FAILED(swap->ResizeBuffers(2, w, h, DXGI_FORMAT_B8G8R8A8_UNORM, 0))) {
        DropResources();
        return;
    }
    if (!EnsureSwapTarget(w, h)) return;
    EnsureLayer(w, h);
}

void Gfx::DropResources() {
    brushes.clear();
    shadowCache.clear();
    layouts.clear();
    clipDepth = 0;
    alpha = 1.f;
    if (dc) dc->SetTarget(nullptr);
    layer.Reset();
    swapBmp.Reset();
    rt.Reset();
    dc.Reset();
    swap.Reset();
    d2dDevice.Reset();
    d3dCtx.Reset();
    d3d.Reset();
}
ID2D1SolidColorBrush* Gfx::Solid(D2D1_COLOR_F c) {
    if (!rt) return nullptr;
    if (alpha < .999f) {
        float a = c.a * alpha;
        c.a = a < 0.f ? 0.f : (a > 1.f ? 1.f : a);
    }
    // 颜色按 1/63 量化：缓动插值过程中不会每帧新建画笔（误差 < 4/255，肉眼无感）
    auto q = [](float v) { float x = (float)((int)(v * 63.f + .5f)); return x > 63.f ? 1.f : x / 63.f; };
    c.r = q(c.r); c.g = q(c.g); c.b = q(c.b); c.a = q(c.a);
    uint32_t key = ColorKey(c);
    auto it = brushes.find(key);
    if (it != brushes.end()) return it->second.Get();
    ComPtr<ID2D1SolidColorBrush> br;
    if (FAILED(rt->CreateSolidColorBrush(c, &br))) return nullptr;
    auto ins = brushes.emplace(key, br);
    return ins.first->second.Get();
}

IDWriteTextFormat* Gfx::Fmt(float px, int weight, const wchar_t* family, HAlign ha, VAlign va,
                            bool wrap) {
    int base = FmtKeyOf(px, weight, family, ha, va);
    int key = base * 2 + (wrap ? 1 : 0);   // 换行/不换行各缓存一份
    auto it = fmts.find(key);
    if (it != fmts.end()) return it->second.Get();

    ComPtr<IDWriteTextFormat> f;
    if (FAILED(dwrite->CreateTextFormat(family, nullptr, (DWRITE_FONT_WEIGHT)weight,
                                        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                        px, L"zh-CN", &f)))
        return nullptr;
    f->SetWordWrapping(wrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
    f->SetTextAlignment(ha == HAlign::Left ? DWRITE_TEXT_ALIGNMENT_LEADING
                         : ha == HAlign::Center ? DWRITE_TEXT_ALIGNMENT_CENTER
                         : DWRITE_TEXT_ALIGNMENT_TRAILING);
    f->SetParagraphAlignment(va == VAlign::Top ? DWRITE_PARAGRAPH_ALIGNMENT_NEAR
                            : va == VAlign::Middle ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER
                            : DWRITE_PARAGRAPH_ALIGNMENT_FAR);
    fmts[key] = f;
    return f.Get();
}

IDWriteTextLayout* Gfx::Layout(const std::wstring& s, float px, int weight, const wchar_t* family,
                               HAlign ha, VAlign va, float maxWidth, float maxHeight, bool wrap) {
    if (!dwrite || s.empty()) return nullptr;
    IDWriteTextFormat* f = Fmt(px, weight, family, ha, va, wrap);
    if (!f) return nullptr;
    if (maxWidth < 1.f) maxWidth = 1.f;
    if (maxHeight < 1.f) maxHeight = 1.f;
    uint64_t h = 1469598103934665603ULL;
    for (wchar_t c : s) h = Mix64(h, (uint64_t)c);
    h = Mix64(h, (uint64_t)FmtKeyOf(px, weight, family, ha, va));
    h = Mix64(h, (uint64_t)(wrap ? 1 : 0));
    h = Mix64(h, (uint64_t)(int)maxWidth);
    h = Mix64(h, (uint64_t)(int)maxHeight);

    auto it = layouts.find(h);
    if (it != layouts.end()) return it->second.Get();

    ComPtr<IDWriteTextLayout> lay;
    if (FAILED(dwrite->CreateTextLayout(s.c_str(), (UINT32)s.size(), f, maxWidth, maxHeight, &lay)))
        return nullptr;
    if (layouts.size() > 3000) layouts.clear();
    layouts[h] = lay;
    return lay.Get();
}

void Gfx::Clear(D2D1_COLOR_F c) {
    if (rt) rt->Clear(c);
}

void Gfx::FillRect(D2D1_RECT_F rc, D2D1_COLOR_F c) {
    if (auto br = Solid(c)) rt->FillRectangle(rc, br);
}

void Gfx::RoundRect(D2D1_RECT_F rc, float rad, D2D1_COLOR_F c) {
    if (auto br = Solid(c)) rt->FillRoundedRectangle(D2D1::RoundedRect(rc, rad, rad), br);
}

void Gfx::FillLinear(D2D1_RECT_F rc, const std::vector<std::pair<float, D2D1_COLOR_F>>& stops,
                     bool horizontal) {
    if (!rt || stops.empty()) return;
    std::vector<D2D1_GRADIENT_STOP> gs;
    gs.reserve(stops.size());
    for (const auto& s : stops) {
        D2D1_COLOR_F c = s.second;
        if (alpha < .999f) c.a = (std::min)(1.f, (std::max)(0.f, c.a * alpha));
        float pos = (std::min)(1.f, (std::max)(0.f, s.first));
        gs.push_back(D2D1_GRADIENT_STOP{ pos, c });
    }
    ComPtr<ID2D1GradientStopCollection> col;
    if (FAILED(rt->CreateGradientStopCollection(gs.data(), (UINT)gs.size(), &col))) return;
    auto p0 = D2D1::Point2F(rc.left, rc.top);
    auto p1 = horizontal ? D2D1::Point2F(rc.right, rc.top) : D2D1::Point2F(rc.left, rc.bottom);
    ComPtr<ID2D1LinearGradientBrush> br;
    if (FAILED(rt->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(p0, p1), col.Get(), &br)))
        return;
    rt->FillRectangle(rc, br.Get());
}

void Gfx::StrokeRect(D2D1_RECT_F rc, float rad, D2D1_COLOR_F c, float w) {
    if (auto br = Solid(c)) rt->DrawRoundedRectangle(D2D1::RoundedRect(rc, rad, rad), br, w);
}

void Gfx::Line(float x1, float y1, float x2, float y2, D2D1_COLOR_F c, float w) {
    if (auto br = Solid(c)) rt->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), br, w);
}

void Gfx::Circle(float cx, float cy, float r, D2D1_COLOR_F c) {
    if (auto br = Solid(c)) rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), br);
}

void Gfx::StrokeCircle(float cx, float cy, float r, D2D1_COLOR_F c, float w) {
    if (auto br = Solid(c)) rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), br, w);
}

void Gfx::Shadow(D2D1_RECT_F rc, float rad, float spread, float dy, D2D1_COLOR_F c) {
    static int off = -1;
    if (off < 0) off = GetEnvironmentVariableW(L"VCUI_NOSHADOW", nullptr, 0) > 0 ? 1 : 0;
    if (off) return;
    if (!rt || c.a <= 0.f) return;
    float w = rc.right - rc.left, h = rc.bottom - rc.top;
    if (w <= 0.f || h <= 0.f) return;

    float E = std::max(spread, std::fabs(dy)) + 2.f;
    // 尺寸取整到 4px，避免布局微抖动导致缓存爆炸
    float bw = float(((int)((w + 2.f * E) / 4.f + .5f)) * 4);
    float bh = float(((int)((h + 2.f * E) / 4.f + .5f)) * 4);
    uint64_t key = ColorKey(c);
    key = Mix64(key, (uint64_t)(int)bw);
    key = Mix64(key, (uint64_t)(int)bh);
    key = Mix64(key, (uint64_t)(int)(rad * 10.f));
    key = Mix64(key, (uint64_t)(int)(E * 10.f));

    ComPtr<ID2D1Bitmap>* cached = nullptr;
    auto it = shadowCache.find(key);
    if (it != shadowCache.end()) cached = &it->second;

    if (!cached) {
        // 阴影本来就是柔的：按半像素密度渲染再放大，成本降到 1/4
        ComPtr<ID2D1Bitmap1> tmp;
        D2D1_SIZE_U px = D2D1::SizeU((UINT)(bw * .5f + .5f), (UINT)(bh * .5f + .5f));
        D2D1_BITMAP_PROPERTIES1 bp{};
        bp.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
        bp.dpiX = bp.dpiY = dpi_ * 2.f;
        bp.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
        if (FAILED(dc->CreateBitmap(px, nullptr, 0, bp, tmp.GetAddressOf()))) return;

        auto* prevTarget = layer.Get();
        dc->SetTarget(tmp.Get());
        dc->SetDpi(dpi_ * 2.f, dpi_ * 2.f);
        dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        dc->BeginDraw();
        dc->Clear(D2D1::ColorF(0, 0, 0, 0));
        ComPtr<ID2D1SolidColorBrush> br;
        dc->CreateSolidColorBrush(c, br.GetAddressOf());
        const int N = 4;
        float ex = (bw - w) * .5f;           // 水平居中
        float ey = (bh - h) * .5f + dy * .5f; // 预留上下余量
        for (int i = N; i >= 1; --i) {
            float e = spread * float(i) / N;
            D2D1_RECT_F r2{ fmaxf(ex - e, 0.f), fmaxf(ey - e, 0.f),
                            fminf(ex + w + e, bw), fminf(ey + h + e, bh) };
            D2D1_COLOR_F cc = c;
            cc.a = c.a / float(N);
            br->SetColor(cc);
            dc->FillRoundedRectangle(D2D1::RoundedRect(r2, rad + e, rad + e), br.Get());
        }
        dc->EndDraw();

        dc->SetTarget(prevTarget);
        dc->SetDpi(dpi_, dpi_);
        if (shadowCache.size() > 64) shadowCache.clear();
        auto ins = shadowCache.emplace(key, tmp);
        cached = &ins.first->second;
    }
    if (cached && *cached) {
        D2D1_SIZE_F sz = (*cached)->GetSize();
        rt->DrawBitmap((*cached).Get(),
                       D2D1::RectF(rc.left - sz.width * .5f + w * .5f,
                                   rc.top - sz.height * .5f + h * .5f, 
                                   rc.left - sz.width * .5f + w * .5f + sz.width,
                                   rc.top - sz.height * .5f + h * .5f + sz.height),
                       alpha, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);   // 跟随 g.alpha，淡出时阴影一起淡
    }
}

void Gfx::PushClip(D2D1_RECT_F rc) {
    if (!rt) return;
    rt->PushAxisAlignedClip(rc, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    ++clipDepth;
}

void Gfx::PopClip() {
    if (!rt || clipDepth <= 0) return;
    rt->PopAxisAlignedClip();
    --clipDepth;
}

void Gfx::Text(D2D1_RECT_F rc, const std::wstring& s, float px, int weight, D2D1_COLOR_F c,
               HAlign ha, VAlign va, bool wrap) {
    static int off = -1;
    if (off < 0) off = GetEnvironmentVariableW(L"VCUI_NOTEXT", nullptr, 0) > 0 ? 1 : 0;
    if (off) return;
    if (!rt || s.empty()) return;
    float w = rc.right - rc.left;
    float h = rc.bottom - rc.top;
    if (w <= 0.f || h <= 0.f) return;
    IDWriteTextLayout* lay = Layout(s, px, weight, L"Microsoft YaHei", ha, va, w, h, wrap);
    if (!lay) return;
    if (auto br = Solid(c)) rt->DrawTextLayout(D2D1::Point2F(rc.left, rc.top), lay, br);
}

void Gfx::TextTrim(D2D1_RECT_F rc, const std::wstring& s, float px, int weight, D2D1_COLOR_F c,
                   HAlign ha, VAlign va) {
    static int off = -1;
    if (off < 0) off = GetEnvironmentVariableW(L"VCUI_NOTEXT", nullptr, 0) > 0 ? 1 : 0;
    if (off) return;
    if (!rt || s.empty() || !dwrite) return;
    float w = rc.right - rc.left;
    float h = rc.bottom - rc.top;
    if (w <= 0.f || h <= 0.f) return;
    IDWriteTextFormat* f = Fmt(px, weight, L"Microsoft YaHei", ha, va);
    if (!f) return;

    // 与 Gfx::Layout 分开缓存：裁剪版的 layout 带 trimming，
    // 不能跟普通绘制共用同一份（否则普通 Text 也会被截断）。
    uint64_t key = 1469598103934665603ULL;
    for (wchar_t ch : s) key = Mix64(key, (uint64_t)ch);
    key = Mix64(key, (uint64_t)FmtKeyOf(px, weight, L"Microsoft YaHei", ha, va));
    key = Mix64(key, (uint64_t)(int)w);
    key = Mix64(key, (uint64_t)(int)h);
    key ^= 0x9E3779B97F4A7C15ULL;   // trim 标记位

    IDWriteTextLayout* lay = nullptr;
    auto it = layouts.find(key);
    if (it != layouts.end()) {
        lay = it->second.Get();
    } else {
        ComPtr<IDWriteTextLayout> made;
        if (FAILED(dwrite->CreateTextLayout(s.c_str(), (UINT32)s.size(), f, w, h, &made)))
            return;
        ComPtr<IDWriteInlineObject> sign;
        if (SUCCEEDED(dwrite->CreateEllipsisTrimmingSign(f, &sign))) {
            DWRITE_TRIMMING tr{ DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
            made->SetTrimming(&tr, sign.Get());
        }
        if (layouts.size() > 3000) layouts.clear();
        layouts[key] = made;
        lay = made.Get();
    }
    if (auto br = Solid(c)) rt->DrawTextLayout(D2D1::Point2F(rc.left, rc.top), lay, br);
}

void Gfx::Glyph(D2D1_RECT_F rc, const std::wstring& g, float px, D2D1_COLOR_F c,
                HAlign ha, VAlign va) {
    if (!rt || g.empty()) return;
    float w = rc.right - rc.left;
    float h = rc.bottom - rc.top;
    if (w <= 0.f || h <= 0.f) return;
    IDWriteTextLayout* lay = Layout(g, px, 400, L"Segoe MDL2 Assets", ha, va, w, h);
    if (!lay) return;
    if (auto br = Solid(c)) rt->DrawTextLayout(D2D1::Point2F(rc.left, rc.top), lay, br);
}

float Gfx::TextW(const std::wstring& s, float px, int weight) {
    if (!dwrite || s.empty()) return 0.f;
    IDWriteTextLayout* lay = Layout(s, px, weight, L"Microsoft YaHei", HAlign::Left,
                                    VAlign::Top, 10000.f);
    if (!lay) return 0.f;
    DWRITE_TEXT_METRICS tm{};
    lay->GetMetrics(&tm);
    return tm.widthIncludingTrailingWhitespace;
}

} // namespace vcui
