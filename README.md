<!--
  Copyright 2026 XGstudio

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  SPDX-License-Identifier: Apache-2.0
-->

# Void Craft UI

C++17 / Direct2D + DirectWrite + Win32 的自绘控件库。窗口里没有系统控件，所有控件都由 `vcui::Gfx` 画出来，
设计语言是单一 Fluent 2 底座（度量参数化），一套 `Theme`（Token + Metrics + accent），保留浅/深两档与
Microsoft / GitHub 两套色板，字体 Microsoft YaHei。

- 命名空间：`vcui`
- 头文件入口：`#include <vcui/vcui.h>`
- 目标：`VCUI::vcui`（默认静态库，可切 DLL）
- 版本：1.0.0

## 目录

```
VCUI/
├─ include/vcui/          公开头文件（只有这里需要给调用方）
│  ├─ vcui.h              单头入口
│  ├─ Color.h             颜色工具 + Tokens / Metrics / Palette
│  ├─ Theme.h             Theme（色板 + 深浅 + 强调色）
│  ├─ Gfx.h               绘制上下文（自定义控件用）
│  ├─ Widget.h            控件基类 / PaintCtx / 动画与命中
│  ├─ Layout.h            Stack(VBox/HBox) / Card / Absolute / Divider / ScrollView
│  ├─ Controls.h          Label / Button / Checkbox / Switch / Input / Slider / Progress /
│  │                      Segmented / Badge / NavItem / ListRow / IconButton / Dropdown
│  ├─ Surface.h           宿主窗口
│  └─ Export.h            VCUI_API（DLL 导出宏）
├─ src/                   实现（不对外）
├─ demo/
│  ├─ main.cpp            全控件展示
│  └─ minimal.cpp         最小用法示例
└─ CMakeLists.txt
```

## 构建

```powershell
cmake -S . -B .build -G "Visual Studio 17 2022" -A x64
cmake --build .build --config Release
```

产物：
- `Release\vcui.lib`
- `Release\VCUIDemo.exe`（全控件展示）
- `Release\VCUIMinimal.exe`（最小示例）

CMake 选项：

| 选项 | 默认 | 说明 |
|---|---|---|
| `VCUI_BUILD_SHARED` | OFF | ON 时构建 `vcui.dll` + 导入库 |
| `VCUI_BUILD_DEMO` | ON | 构建两个示例 |
| `VCUI_STATIC_RUNTIME` | OFF | `/MT`，MSVC 运行时静态链接，便于单文件分发 |

依赖的系统库已由 `VCUI::vcui` 自动传递：`d2d1 dwrite dwmapi uxtheme user32 gdi32 ole32 imm32`。

## 集成到你的项目

### 1) 源码内嵌（推荐）

```cmake
add_subdirectory(path/to/VCUI)
target_link_libraries(YourApp PRIVATE VCUI::vcui)
```

```cpp
#include <vcui/vcui.h>
```

### 2) 安装后用 find_package

```powershell
cmake --install .build --config Release --prefix path/to/install
```

```cmake
find_package(VCUI 1.0 REQUIRED)
target_link_libraries(YourApp PRIVATE VCUI::vcui)
```

### 3) 手动

把 `include/` 加进头文件搜索路径，链接 `vcui.lib` 和上面那串系统库。

## 快速开始

```cpp
#include <windows.h>
#include <vcui/vcui.h>
using namespace vcui;

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    Surface app;
    if (!app.Create(L"Void Craft UI", 520, 340)) return 1;

    auto page = VBox(14.f);
    page->Pad(24.f);
    page->Add(std::make_shared<Label>(L"标题", 20.f, 600));

    auto card = std::make_shared<Card>();
    auto box = VBox(12.f);
    box->Add(std::make_shared<Input>(L"输入点什么…"));
    auto btn = std::make_shared<Button>(L"确定", true);
    btn->Clicked = [] { /* ... */ };
    box->Add(btn);
    card->Add(box);
    page->Add(card);

    app.SetRoot(page);
    return app.Run();
}
```

## 核心概念

### Surface

宿主窗口：建窗、Per-Monitor V2 DPI、帧循环、脏矩形、鼠标/键盘/输入法路由、焦点、覆盖层。

| API | 说明 |
|---|---|
| `Create(title, w, h, dark)` | 建窗，客户区 `w × h`（DIP） |
| `SetRoot(widget)` | 设置控件树根 |
| `Run()` | 进入消息循环，退出时返回 |
| `SetMinSize(w, h)` | 窗口最小尺寸，默认 320×200 |
| `SetTheme(pal, dark)` / `SetDark` / `SetAccent` | 主题 |
| `Th()` | 取当前 Theme（读 Tokens / Metrics） |
| `AddOverlay` / `RemoveOverlay(Deferred)` | 覆盖层（下拉菜单、弹窗） |
| `SetDismiss(fn, owner)` | 点覆盖层外面时的回调 |
| `Invalidate()` / `InvalidateRectD(rc)` | 整窗 / 局部重绘 |
| `KickAnim()` | 唤醒动画帧循环 |

### Widget

一棵树，每个节点自己算尺寸、自己画。布局参数全是公开字段：

- 尺寸：`W/H`（`<0` 表示按内容/由容器决定）、`Grow`、`Basis`
- 间距：`ML/MT/MR/MB`（外边距）、`PL/PT/PR/PB`（内边距），`Pad(v)` / `Margin(v)`
- 行为：`Visible`、`Enabled`、`Interactive`（命中目标）、`Clip`、`Opacity`、`Caption`（窗口拖动区）
- 动画读数：`HoverA()`、`PressA()`、`AnimDirty`
- 事件：`Clicked`，以及可重写的 `OnClick/OnClickAt/OnDrag/OnChar/OnKeyDown/OnFocus/OnWheel/OnImeMessage`

### 容器

| 类型 | 说明 |
|---|---|
| `VBox(gap)` / `HBox(gap)` | 一维栈；`Cross` 控制交叉轴 `Start/Center/End/Stretch`（默认 Stretch） |
| `Card` | 底色 + 圆角 + 描边 + 阴影，`Shadow/Border/Radius/Fill` 可关 |
| `Absolute` | 子项用 `X/Y/W/H` 绝对定位 |
| `ScrollView` | 单子项纵向滚动，滚轮带缓动（`ScrollY/TargetY`） |
| `Spacer(size)` / `Divider()` | 留白 / 1px 分隔线 |

### 主题

```cpp
app.SetTheme(Palette::GitHub, /*dark=*/true);
app.SetAccent(Hex(0xFF6A00));
```

`Theme::m` 是 Fluent 2 度量（圆角、控件高、行高、字号、字体），改这里就能整体换皮；
`Theme::t` 是解析后的 Token（`kp/bj/xt/jh/wz`、`accent*`、`text2/3`、`border`、`divider`、`panel`、`pressed`）。

## 控件

| 控件 | 关键字段 / 回调 |
|---|---|
| `Label` | `Text`、`Size`、`Weight`、`Color`、`Align`、`VAlignMode`、`Wrap` |
| `Button` | `Text`、`Glyph`(Segoe MDL2)、`Primary`、`MinWidth`、`Clicked` |
| `Checkbox` | `On`、`Text`、`Changed(bool)` |
| `Switch` | `On`、`Changed(bool)` |
| `Input` | `Text`、`Placeholder`、`Caret`、`SelAnchor`、`Changed(str)`、`Submitted` |
| `Slider` | `Value`(0..1)、`Changed(float)` |
| `Progress` | `Value`(0..1) |
| `Segmented` | `Items`、`Active`、`Changed(int)` |
| `Badge` | `Text`、`ToneKind`（Neutral/Accent/Success/Warning/Danger） |
| `NavItem` | `Text`、`Glyph`、`Active`、`Clicked` |
| `ListRow` | `Title`、`Sub`、`Right`、`Selected`、`Clicked` |
| `IconButton` | `Glyph`、`GlyphSize`、`Ghost`、`Clicked` |
| `Dropdown` | `Items`、`Selected`、`MenuWidth`、`Changed(int)` |

`Input` 已实现：选区（Shift+方向/Home/End、拖拽）、`Ctrl+A/C/X/V/Z`、撤销、
输入法组合串与 `ImmSetCompositionWindow`/`ImmSetCandidateWindow` 候选窗定位。

## 自定义控件

```cpp
class MyChip : public vcui::Widget {
public:
    MyChip() { H = 28.f; Interactive = true; }
    D2D1_SIZE_F Desired(float, float) override { return D2D1::SizeF(120.f, 28.f); }
    void Paint(vcui::PaintCtx& ctx) override {
        ctx.g.RoundRect(Frame(), 4.f, ctx.t.accentSoft);
        ctx.g.Text(Frame(), L"chip", 12.f, 600, ctx.t.accent,
                   vcui::HAlign::Center, vcui::VAlign::Middle);
    }
};
```

`PaintCtx` 提供 `g`（绘制）、`t`（Token）、`m`（度量）、`scale`（DPI）、`time`（秒）。
需要逐帧动画就重写 `Update(dt)` 并在视觉有变化时置 `AnimDirty = true`；`Surface` 会据此只重绘该控件矩形。

## 性能

- 持久离屏层：绘制发生在 `ID2D1BitmapRenderTarget`，每帧只贴一次到窗口
- 帧循环自建：有动画时 `Tick + UpdateWindow + DwmFlush` 跟合成节拍，闲置时挂起（CPU 0）
- 脏矩形：动画只重绘标脏控件区域（含 28px 阴影余量），滚动只刷滚动容器
- 文本布局缓存（`IDWriteTextLayout`）与阴影位图缓存（尺寸取整 4px，上限 64）
- 实测（HD Graphics 4000，x64 Release）：滚动/动画帧 <12ms，同色全量重绘 <12ms，空闲 0% CPU

调试开关（环境变量）：`VCLKIT_FPS=1` 标题栏显示 fps/paint，`VCLKIT_NOSHADOW=1` 跳阴影，`VCLKIT_NOTEXT=1` 跳文字。

## 已知未做

- 滚动无惯性余速、无滚动条
- 下拉只向下展开，不自动翻转
- 滑块无键盘步进
- 阴影是 4 层圆角位图叠加，不是真高斯
- Segmented 悬停段（`Hot`）尚未按鼠标位置计算

## 许可

Copyright 2026 XGstudio，Apache License 2.0（`LICENSE` 为协议全文）。

每个源文件头部都带 Apache-2.0 的完整声明块，末行是 `SPDX-License-Identifier: Apache-2.0`。
引入第三方代码时请在其文件内单独标注原始许可，不要混用。
