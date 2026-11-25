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
#include "vcui/Layout.h"

namespace vcui {

// 虚拟列表：把 N 条数据按固定行高映射成偏移，只让视口内的那几行真实存在。
// 八万条搜索结果也只建十几个行控件；滚动时优先用 Bind 改写旧行的内容（回收），
// 没有 Bind 就退化成重新 Build，功能一致但每帧都会新建控件。
//
// 前提：行等高。行高不定（文本折行、图片高度不一）时不要用它，用 ScrollView + VBox。
class VCUI_API VirtualList : public Widget {
public:
    int ItemCount = 0;        // 总条数，改完下一帧自动重排
    float RowHeight = 56.f;   // 行高（DIP），虚拟化靠它算偏移
    float Gap = 0.f;          // 行间距
    bool ShowBar = true;      // 自绘滚动条（静默细线，悬停/滚动时变粗）
    float BarW = 6.f;
    int Overscan = 2;         // 视口上下各多备几行，滚动时不至于露白

    // 造行：index -> 行控件。宿主拿来的是没有父节点的裸控件，内部会负责挂树。
    std::function<std::shared_ptr<Widget>(int index)> Build;
    // 复用：把一个已经造好的行改写成第 index 条的内容。
    // 返回 false 表示这条数据的行结构变了、复用不了，内部会丢掉旧行重新 Build。
    std::function<bool(Widget& row, int index)> Bind;

    VirtualList();

    D2D1_SIZE_F Desired(float availW, float availH) override;
    void LayoutChildren() override;
    bool Update(float dt) override;
    bool WantsWheel() const override { return true; }
    void OnWheel(float dy, float x, float y) override;
    void OnPressAt(float x, float y) override;
    void OnDrag(float x, float y) override;
    void OnRelease(bool inside) override;
    bool Focusable() const override { return MaxScroll() > .5f; }
    void OnKeyDown(int vk) override;
    CursorShape CursorAt(float x, float y) const override;
    void Paint(PaintCtx& ctx) override;

    // ---- 滚动 ----
    float ScrollY = 0.f;      // 当前偏移（缓动中）
    float TargetY = 0.f;      // 目标偏移
    float ContentHeight() const;
    float ViewHeight() const;
    float MaxScroll() const;
    void ScrollToIndex(int index, bool center = false);
    void EnsureVisible(int index);

    // ---- 状态 ----
    int FirstVisible() const { return first_; }   // 含 Overscan 的实际首行
    int LastVisible() const { return last_; }
    int LiveRows() const;                         // 当前真实存在的行控件数
    int RecycledCount() const { return recycled_; }

    // 数据换了：丢掉所有行，下次布局重建（不重建时改 ItemCount 也会触发，但换数据集用这个更明确）
    void Reset();
    // 让当前可见行整体错峰入场：数据异步回来填完 ItemCount 后调一次。
    // 只在视口内的行上有动画，行数再多也只动这十几行。
    void AnimateIn(float dur = .20f, float dy = 8.f, float step = .025f, float maxDelay = .16f);

private:
    struct Slot {
        std::shared_ptr<Widget> w;
        int index = -1;       // -1 = 空闲槽
    };
    std::vector<Slot> slots_;
    int laidOutCount_ = -1;   // 上次布局看到的总条数
    int first_ = -1, last_ = -1;
    int recycled_ = 0;

    bool enterPending_ = false;
    float enterDur_ = 0.f, enterDy_ = 0.f, enterStep_ = 0.f, enterMaxDelay_ = 0.f;

    bool draggingBar_ = false;
    float grab_ = 0.f;
    float aBar = 0.f;
    float barHold_ = 0.f;

    float Pitch() const;      // 行距 = RowHeight + Gap
    void ParkAll();
    int AcquireSlot();
    D2D1_RECT_F ThumbRect() const;
    D2D1_RECT_F TrackRect() const;
};

} // namespace vcui
