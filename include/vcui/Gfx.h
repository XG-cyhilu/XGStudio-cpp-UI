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

#include <windows.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace vcui {

enum class HAlign { Left, Center, Right };
enum class VAlign { Top, Middle, Bottom };

// 绘制上下文：一次绘制周期内的 D2D / DWrite 封装。
// 画笔与文本格式按内容缓存，避免每帧重新分配。
// 自定义控件时在 Paint() 里用它画图，不要直接碰 ID2D1RenderTarget。
struct VCUI_API Gfx {
    Microsoft::WRL::ComPtr<ID2D1Factory> factory;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwrite;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> hwndRt;    // 只负责把层贴到窗口
    Microsoft::WRL::ComPtr<ID2D1BitmapRenderTarget> layer;  // 所有绘制都发生在这一层，跨帧保留
    Microsoft::WRL::ComPtr<ID2D1RenderTarget> rt;           // = layer，绘制入口
    std::unordered_map<int, Microsoft::WRL::ComPtr<IDWriteTextFormat>> fmts;
    std::unordered_map<uint32_t, Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>> brushes;
    // 文本布局缓存：字符串 + 格式 → IDWriteTextLayout，避免每帧重新 shaping
    std::unordered_map<uint64_t, Microsoft::WRL::ComPtr<IDWriteTextLayout>> layouts;
    std::unordered_map<uint64_t, Microsoft::WRL::ComPtr<ID2D1Bitmap>> shadowCache;
    int clipDepth = 0;
    float dpi_ = 96.f;
    float alpha = 1.f;    // 全局不透明度（弹层淡入用），作用在画笔上

    bool Init(HWND hwnd);
    void Resize(UINT w, UINT h);
    bool Ready() const { return rt != nullptr; }
    // 确保离屏层尺寸正确（客户区像素）
    bool EnsureLayer(UINT w, UINT h);
    // 把离屏层整层贴到窗口
    void Present();
    D2D1_SIZE_F Size() const { return hwndRt ? hwndRt->GetSize() : D2D1::SizeF(0, 0); }
    void DropResources();

    IDWriteTextFormat* Fmt(float px, int weight, const wchar_t* family = L"Microsoft YaHei",
                           HAlign ha = HAlign::Left, VAlign va = VAlign::Top, bool wrap = false);
    IDWriteTextLayout* Layout(const std::wstring& s, float px, int weight, const wchar_t* family,
                              HAlign ha, VAlign va, float maxWidth, float maxHeight = 4096.f,
                              bool wrap = false);
    ID2D1SolidColorBrush* Solid(D2D1_COLOR_F c);

    void Clear(D2D1_COLOR_F c);
    void FillRect(D2D1_RECT_F rc, D2D1_COLOR_F c);
    void RoundRect(D2D1_RECT_F rc, float rad, D2D1_COLOR_F c);
    void StrokeRect(D2D1_RECT_F rc, float rad, D2D1_COLOR_F c, float w = 1.f);
    void Line(float x1, float y1, float x2, float y2, D2D1_COLOR_F c, float w = 1.f);
    void Circle(float cx, float cy, float r, D2D1_COLOR_F c);
    void StrokeCircle(float cx, float cy, float r, D2D1_COLOR_F c, float w = 1.f);
    void Shadow(D2D1_RECT_F rc, float rad, float spread, float dy, D2D1_COLOR_F c);

    void PushClip(D2D1_RECT_F rc);
    void PopClip();

    void Text(D2D1_RECT_F rc, const std::wstring& s, float px, int weight, D2D1_COLOR_F c,
              HAlign ha = HAlign::Left, VAlign va = VAlign::Top, bool wrap = false);
    // 单行文本 + 省略号：超出宽度时截断并在尾部补「…」
    void TextTrim(D2D1_RECT_F rc, const std::wstring& s, float px, int weight, D2D1_COLOR_F c,
                  HAlign ha = HAlign::Left, VAlign va = VAlign::Top);
    void Glyph(D2D1_RECT_F rc, const std::wstring& g, float px, D2D1_COLOR_F c,
               HAlign ha = HAlign::Center, VAlign va = VAlign::Middle);
    float TextW(const std::wstring& s, float px, int weight);
};

} // namespace vcui
