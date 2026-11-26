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

#include "vcui/Controls.h"

#include <algorithm>
#include <vector>
#include <unordered_map>

using Microsoft::WRL::ComPtr;

namespace vcui {

namespace {

struct Pt {
    float x = 0.f, y = 0.f;
};

struct Cmd {
    int t = 0;          // 0=line 1=cubic 2=quad
    Pt p[3] = {};
};

struct Fig {
    Pt start;
    std::vector<Cmd> cmds;
    bool closed = false;
};

bool IsDigit(wchar_t c) { return c >= L'0' && c <= L'9'; }
bool IsLetter(wchar_t c) { return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z'); }
bool IsSpace(wchar_t c) { return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' || c == L','; }

// 极简 SVG path 解析：M/m L/l H/h V/v C/c Q/q Z/z，每段可重复多组坐标。
// 数字自己扫，不走 wcstod（避免 locale 之类的坑）。
std::vector<Fig> ParsePath(const std::wstring& d) {
    std::vector<Fig> figs;
    Fig cur;
    bool has = false;
    Pt p{};
    wchar_t cmd = 0;
    size_t i = 0;

    auto num = [&](float& v) -> bool {
        while (i < d.size() && IsSpace(d[i])) ++i;
        size_t st = i;
        bool neg = false;
        if (i < d.size() && (d[i] == L'-' || d[i] == L'+')) {
            neg = d[i] == L'-';
            ++i;
        }
        double ip = 0.0, fr = 0.0, sc = 1.0;
        bool any = false;
        while (i < d.size() && IsDigit(d[i])) {
            ip = ip * 10.0 + (double)(d[i] - L'0');
            ++i;
            any = true;
        }
        if (i < d.size() && d[i] == L'.') {
            ++i;
            while (i < d.size() && IsDigit(d[i])) {
                fr = fr * 10.0 + (double)(d[i] - L'0');
                sc *= 10.0;
                ++i;
                any = true;
            }
        }
        if (!any) {
            i = st;
            return false;
        }
        v = (float)((ip + fr / sc) * (neg ? -1.0 : 1.0));
        return true;
    };
    auto flushFig = [&]() {
        if (!has) return;
        if (!cur.cmds.empty()) figs.push_back(cur);
        cur = Fig{};
        has = false;
    };
    auto moveTo = [&](const Pt& q) {
        flushFig();
        cur.start = q;
        has = true;
        p = q;
    };

    while (i < d.size()) {
        while (i < d.size() && IsSpace(d[i])) ++i;
        if (i >= d.size()) break;
        if (IsLetter(d[i])) {
            cmd = d[i++];
            if (cmd == L'Z' || cmd == L'z') {
                if (has) cur.closed = true;
                flushFig();
                cmd = 0;
            }
            continue;
        }
        if (!cmd) {
            ++i;
            continue;
        }
        float a = 0, b = 0, c1 = 0, d1 = 0, e1 = 0, f1 = 0;
        bool rel = cmd >= L'a' && cmd <= L'z';
        wchar_t up = rel ? (wchar_t)(cmd - 32) : cmd;
        switch (up) {
        case L'M':
            if (!num(a) || !num(b)) { ++i; continue; }
            if (rel) { a += p.x; b += p.y; }
            moveTo(Pt{a, b});
            cmd = rel ? L'l' : L'L';      // M 之后的隐式坐标按 L 处理
            break;
        case L'L':
            if (!num(a) || !num(b)) { ++i; continue; }
            if (rel) { a += p.x; b += p.y; }
            if (!has) {
                moveTo(Pt{a, b});
            } else {
                cur.cmds.push_back(Cmd{0, {Pt{a, b}, {}, {}}});
                p = Pt{a, b};
            }
            break;
        case L'H':
            if (!num(a)) { ++i; continue; }
            if (rel) a += p.x;
            if (!has) {
                moveTo(Pt{a, p.y});
            } else {
                cur.cmds.push_back(Cmd{0, {Pt{a, p.y}, {}, {}}});
                p.x = a;
            }
            break;
        case L'V':
            if (!num(a)) { ++i; continue; }
            if (rel) a += p.y;
            if (!has) {
                moveTo(Pt{p.x, a});
            } else {
                cur.cmds.push_back(Cmd{0, {Pt{p.x, a}, {}, {}}});
                p.y = a;
            }
            break;
        case L'C':
            if (!num(c1) || !num(d1) || !num(e1) || !num(f1) || !num(a) || !num(b)) { ++i; continue; }
            if (rel) { c1 += p.x; d1 += p.y; e1 += p.x; f1 += p.y; a += p.x; b += p.y; }
            if (!has) moveTo(Pt{c1, d1});
            cur.cmds.push_back(Cmd{1, {Pt{c1, d1}, Pt{e1, f1}, Pt{a, b}}});
            p = Pt{a, b};
            break;
        case L'Q':
            if (!num(c1) || !num(d1) || !num(a) || !num(b)) { ++i; continue; }
            if (rel) { c1 += p.x; d1 += p.y; a += p.x; b += p.y; }
            if (!has) moveTo(Pt{c1, d1});
            cur.cmds.push_back(Cmd{2, {Pt{c1, d1}, Pt{a, b}, {}}});
            p = Pt{a, b};
            break;
        default:
            ++i;               // 不认识的命令（例如 A 弧）直接跳过
            break;
        }
    }
    flushFig();
    return figs;
}

} // namespace

void Gfx::SvgPath(D2D1_RECT_F box, const std::wstring& d, D2D1_COLOR_F c, float stroke, bool filled) {
    if (!rt || !factory || d.empty()) return;
    auto figs = ParsePath(d);
    if (figs.empty()) return;

    // bbox（连控制点一起量，简单够用）
    float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
    auto eat = [&](const Pt& q) {
        minx = (std::min)(minx, q.x); maxx = (std::max)(maxx, q.x);
        miny = (std::min)(miny, q.y); maxy = (std::max)(maxy, q.y);
    };
    for (auto& f : figs) {
        eat(f.start);
        for (auto& cm : f.cmds) {
            eat(cm.p[0]);
            if (cm.t == 1) { eat(cm.p[1]); eat(cm.p[2]); }
            else if (cm.t == 2) eat(cm.p[1]);
        }
    }
    float sw = maxx - minx, sh = maxy - miny;
    if (sw <= .001f) sw = .001f;
    if (sh <= .001f) sh = .001f;
    float bw = box.right - box.left, bh = box.bottom - box.top;
    float sc = (std::min)(bw / sw, bh / sh);
    float ox = box.left + (bw - sw * sc) * .5f;
    float oy = box.top + (bh - sh * sc) * .5f;
    auto tf = [&](const Pt& q) { return D2D1::Point2F(ox + (q.x - minx) * sc, oy + (q.y - miny) * sc); };

    auto br = Solid(c);
    if (!br) return;

    // 几何按（路径串 + 尺寸 + 填充/描边）缓存，且建在本地坐标系：列表图标不再每帧解析+建几何
    static std::unordered_map<uint64_t, ComPtr<ID2D1PathGeometry>> geoCache;
    uint64_t gh = 1469598103934665603ULL;
    auto mix = [&gh](uint64_t v) { gh ^= v; gh *= 1099511628211ULL; };
    for (wchar_t ch : d) mix((uint64_t)ch);
    mix((uint64_t)(int)(bw * 4.f));
    mix((uint64_t)(int)(bh * 4.f));
    mix(filled ? 1u : 0u);

    ComPtr<ID2D1PathGeometry> geo;
    auto hit = geoCache.find(gh);
    if (hit != geoCache.end()) {
        geo = hit->second;
    } else {
        ComPtr<ID2D1PathGeometry> made;
        if (FAILED(factory->CreatePathGeometry(&made))) return;
        ComPtr<ID2D1GeometrySink> sink;
        if (FAILED(made->Open(&sink))) return;
        auto tl = [&](const Pt& q) { return D2D1::Point2F((q.x - minx) * sc, (q.y - miny) * sc); };
        for (auto& f : figs) {
            D2D1_POINT_2F pc = tl(f.start);
            sink->BeginFigure(pc, D2D1_FIGURE_BEGIN_FILLED);
            for (auto& cm : f.cmds) {
                if (cm.t == 0) {
                    pc = tl(cm.p[0]);
                    sink->AddLine(pc);
                } else if (cm.t == 1) {
                    D2D1_POINT_2F c1 = tl(cm.p[0]), c2 = tl(cm.p[1]), e = tl(cm.p[2]);
                    sink->AddBezier(D2D1::BezierSegment(c1, c2, e));
                    pc = e;
                } else {
                    // 二次转三次：控制点各取 2/3 位置
                    D2D1_POINT_2F q = tl(cm.p[0]), e = tl(cm.p[1]);
                    D2D1_POINT_2F c1 = D2D1::Point2F(pc.x + (q.x - pc.x) * 2.f / 3.f, pc.y + (q.y - pc.y) * 2.f / 3.f);
                    D2D1_POINT_2F c2 = D2D1::Point2F(e.x + (q.x - e.x) * 2.f / 3.f, e.y + (q.y - e.y) * 2.f / 3.f);
                    sink->AddBezier(D2D1::BezierSegment(c1, c2, e));
                    pc = e;
                }
            }
            sink->EndFigure(f.closed ? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN);
        }
        sink->Close();
        geo = made;
        if (geoCache.size() > 600) geoCache.clear();
        geoCache[gh] = geo;
    }

    // 几何在本地坐标，绘制时平移到 box 左上角再叠加当前变换
    D2D1_MATRIX_3X2_F prev{};
    rt->GetTransform(&prev);
    rt->SetTransform(D2D1::Matrix3x2F::Translation(ox, oy) * prev);
    if (filled || stroke <= 0.f) rt->FillGeometry(geo.Get(), br);
    else rt->DrawGeometry(geo.Get(), br, stroke);
    rt->SetTransform(prev);
}

// ---------------- SvgIcon ----------------

D2D1_SIZE_F SvgIcon::Desired(float availW, float availH) {
    (void)availW;
    (void)availH;
    float s = W > 0 ? W : IconSize;
    return D2D1::SizeF(s, H > 0 ? H : IconSize);
}

void SvgIcon::Paint(PaintCtx& ctx) {
    D2D1_RECT_F fr = Frame();
    if (fr.right <= fr.left || fr.bottom <= fr.top) return;
    D2D1_COLOR_F c = Color.a > 0.f ? Color : ctx.t.text2;
    ctx.g.SvgPath(fr, Path, c, Stroke, Filled);
}

} // namespace vcui
