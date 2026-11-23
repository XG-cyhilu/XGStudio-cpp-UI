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
#include "vcui/Widget.h"

namespace vcui {

// 主轴方向的一维容器：先按 Basis/内容取值，剩余空间按 Grow 分配，交叉轴默认拉伸。
class VCUI_API Stack : public Widget {
public:
    enum Dir { Row, Col };
    enum Align { Start, Center, End, Stretch };
    Dir Direction = Col;
    Align Cross = Stretch;
    float Gap = 0.f;

    Stack(Dir d = Col, float gap = 0.f) : Direction(d), Gap(gap) {}

    D2D1_SIZE_F Desired(float availW, float availH) override;
    void LayoutChildren() override;

private:
    void LayoutRow(D2D1_RECT_F in);
    void LayoutCol(D2D1_RECT_F in);
};

VCUI_API std::shared_ptr<Stack> VBox(float gap = 0.f);
VCUI_API std::shared_ptr<Stack> HBox(float gap = 0.f);

// 卡片 / 分组容器：底色 + 圆角 + 描边（可选阴影）。
class VCUI_API Card : public Widget {
public:
    bool Shadow = true;
    bool Border = true;
    float Radius = -1.f;
    D2D1_COLOR_F Fill{ 0, 0, 0, 0 };   // a == 0 时用主题 kp

    Card();
    D2D1_SIZE_F Desired(float availW, float availH) override;
    void LayoutChildren() override;
    void Paint(PaintCtx& ctx) override;
};

// 绝对定位容器：子项用 X/Y/W/H 指定位置，坐标相对容器内容区。
class VCUI_API Absolute : public Widget {
public:
    Absolute() = default;
    void LayoutChildren() override;
};

// 纯留白。
VCUI_API std::shared_ptr<Widget> Spacer(float size = 0.f);

// 1px 分隔线。
class VCUI_API Divider : public Widget {
public:
    bool Vertical = false;
    Divider();
    D2D1_SIZE_F Desired(float availW, float availH) override;
    void Paint(PaintCtx& ctx) override;
};

// 纵向滚动容器：单子项，内容高超过视口时可滚（带缓动）。
class VCUI_API ScrollView : public Widget {
public:
    float ScrollY = 0.f;
    float TargetY = 0.f;
    ScrollView();
    D2D1_SIZE_F Desired(float availW, float availH) override;
    void LayoutChildren() override;
    bool Update(float dt) override;
    bool WantsWheel() const override { return true; }
    void OnWheel(float dy, float x, float y) override;
    void Paint(PaintCtx& ctx) override;
};

} // namespace vcui
