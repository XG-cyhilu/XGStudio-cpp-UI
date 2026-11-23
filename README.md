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

![License](https://img.shields.io/badge/license-Apache%202.0-blue)
![Platform](https://img.shields.io/badge/platform-Windows%2010%201703%2B-lightgrey)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C)
![Backend](https://img.shields.io/badge/Direct2D-1.0-informational)
![Version](https://img.shields.io/badge/version-1.0.0-brightgreen)

C++17 / Direct2D + DirectWrite + Win32 的**全自绘**控件库。窗口里没有任何系统控件——没有 `EDIT`、没有 `COMBOBOX`、没有 `BUTTON`，每一像素都是 `vcui::Gfx` 用 Direct2D 画出来的。设计语言是单一 Fluent 2 底座（度量参数化），一套 `Theme`（Token + Metrics + accent），浅/深两档 × Microsoft / GitHub 两套色板。

浅色（截图里「+」图标按钮处于悬停，「默认按钮」处于按下）：

![浅色](docs/screenshot-light.png)

深色（同一组状态）：

![深色](docs/screenshot-dark.png)

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

## 这套东西特别在哪

- **真的全自绘**：光标、选区、滚动、下拉弹层、滑块手柄、开关滑块、导航指示条、阴影、圆角描边全是 `Gfx` 画的，没有任何 `CreateWindowEx` 子窗口，也没有 Owner-Draw 混着系统控件画。
- **输入法是真接管了，不是假装**：`Input` 处理 `WM_IME_STARTCOMPOSITION / COMPOSITION / ENDCOMPOSITION / SETCURSOR`，用 `ImmSetCompositionWindow` 把组合串摆在光标下、`ImmSetCandidateWindow` 把候选窗摆到光标附近；组合串在控件内带下划线渲染，光标随组合串推进。自绘文本框里这块最容易做残，本库是完整实现的。
- **没有 D2D 1.1 / Effect Graph / Acrylic / Mica 依赖**：只用 Direct2D 1.0 的 `ID2D1BitmapRenderTarget`，阴影是 4 层缩放的圆角位图叠加（带 LRU 缓存），不碰 `ID2D1Effect`，不需要 D3D11 设备，也不需要 `DwmExtendFrameIntoClientArea` 那套材质。
- **单文件分发**：默认静态库，`VCUI_STATIC_RUNTIME=ON` 时连 MSVC 运行时一起静态链接，产物只剩自己的 exe。
- **闲置真的不烧 CPU**：有动画时跟 `DwmFlush` 合成节拍走，没动画就挂起等消息，实测空闲帧增量为 0。

## 系统要求

| 项 | 要求 |
|---|---|
| 操作系统 | **Windows 10 1703（RS2，build 15063）及以上**，含 Windows 11 |
| 编译器 | MSVC 2019 / 2022（C++17），本库在 MSVC 14.44 上验证 |
| 架构 | x64 验证过；x86 / ARM64 未测 |
| 图形 | 任意支持 Direct2D 1.0 的显卡/驱动（含 WARP 软渲染回退） |

下限是被 **Per-Monitor V2 DPI** 钉死的，跟 Direct2D 版本无关：

| 代码里用到的 | 引入的下限 |
|---|---|
| `SetProcessDpiAwarenessContext` + `DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2` | Win10 1703（15063） |
| `GetDpiForSystem` / `GetDpiForWindow` | Win10 1607 |
| `DwmSetWindowAttribute(DWMWA_USE_IMMERSIVE_DARK_MODE)` | Win10 1809（17763）；更早系统上传不生效，标题栏会是亮的，客户区仍是深色 |
| `DwmFlush` | Win8+（Vista 需 Platform Update） |
| `ID2D1BitmapRenderTarget` | Direct2D 1.0，Win7 + Platform Update 即可 |
| `ImmSetCompositionWindow` / `ImmSetCandidateWindow` | 无额外限制 |

**不支持 Win7 / Win8 / Win8.1**。要降到 Win7 得做三处改动：把 DPI 感知换成 `SetProcessDPIAware`、把逐帧节拍从 `DwmFlush` 换成普通定时器、去掉深色标题栏调用（这几行都在 `src/Surface.cpp`）。

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
│  ├─ Controls.h          13 个控件
│  └─ Surface.h           宿主窗口
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

`add_subdirectory` 引入时会自动给消费方加上 `/std:c++17`（公开头里有 `inline` 变量，标准必须传递，否则 MSVC 默认的 `/std:c++14` 会报错）。公开头是 UTF-8 with BOM，MSVC 即使不加 `/utf-8` 也能正确解析。

## 集成到你的项目

### 1) 源码内嵌（推荐）

```cmake
add_subdirectory(path/to/VCUI)
target_link_libraries(YourApp PRIVATE VCUI::vcui)
```

```cpp
#include <vcui/vcui.h>
```

### 2) 安装后用 find_package（已实测）

```powershell
cmake --install .build --config Release --prefix path/to/install
```

```cmake
find_package(VCUI 1.0 REQUIRED)
target_link_libraries(YourApp PRIVATE VCUI::vcui)
```

配置时把前缀带上：`-DCMAKE_PREFIX_PATH=path/to/install`。安装布局是 `include/vcui/*.h` + `lib/vcui.lib` + `lib/cmake/VCUI/{VCUIConfig,VCUIConfigVersion,VCUITargets}.cmake`，`find_package` 能直接拿到 `VCUI::vcui` 目标及其传递依赖。这条路径已用一个独立工程（只写 `find_package` + `target_link_libraries`）实际配置并链接通过。

### 3) vcpkg

仓库根目录带 `vcpkg.json` manifest（`supports: windows`）。尚未发布到官方 vcpkg 仓库，用 overlay 端口接入：

```powershell
vcpkg install vcui --overlay-ports=path/to/port
```

### 4) 手动

把 `include/` 加进头文件搜索路径，链接 `vcui.lib` 和上面那串系统库。

## 核心概念

### Surface

宿主窗口：建窗、Per-Monitor V2 DPI、帧循环、脏矩形、鼠标/键盘/输入法路由、焦点、覆盖层。

| API | 说明 |
|---|---|
| `Create(title, w, h, dark)` | 建窗，客户区 `w × h`（DIP） |
| `SetRoot(widget)` | 设置控件树根 |
| `Run()` | 进入消息循环，退出时返回 |
| `SetMinSize(w, h)` | 窗口最小尺寸，默认 320×200 |
| `SetTheme(pal, dark)` / `SetDark` / `SetAccent` | 主题，切换时同步 DWM 标题栏明暗 |
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

## 主题与 Token

`Theme` 三件套：`pal`（色板）、`dark`（明暗）、`accent`（强调色）；`Refresh()` 后得到 `t`（Tokens）与 `m`（Metrics）。

```cpp
app.SetTheme(Palette::GitHub, /*dark=*/true);
app.SetAccent(Hex(0xFF6A00));   // accentHover/accentPress/accentSoft 全部按比例重算
```

Token 命名沿用拼音缩写（历史原因），对照如下。表中前四列是 `Palette::Microsoft` / `Palette::GitHub` 在浅色与深色下的实际取值：

| Token | 语义 | MS 浅 | MS 深 | GH 浅 | GH 深 |
|---|---|---|---|---|---|
| `kp` | 卡面 / 控件底色（key surface） | `fbfbfb` | `2b2b2b` | `ffffff` | `0c1117` |
| `bj` | 页面背景（background） | `f3f3f3` | `202020` | `f5f8fa` | `000409` |
| `xt` | 悬停填充 / 禁用控件底 | `eaeaea` | `2d2d2d` | `f2f3f5` | `1e242a` |
| `wz` | 主文字（文字） | `1b1b1b` | `f7f7f7` | `1e2328` | `eef6fc` |

派生 Token（随 accent 与明暗自动计算，不需要手填）：

| Token | 语义 | 浅色算法 | 深色算法 |
|---|---|---|---|
| `accent` | 强调色 | 默认 `#0071EA`，可 `SetAccent` 覆盖 | 同左 |
| `accentHover` | 强调色悬停 | `Lighten(accent, 8%)` | 同左 |
| `accentPress` | 强调色按下 | `Darken(accent, 10%)` | 同左 |
| `onAccent` | 强调色上的文字 | 按亮度自动取黑/白 | 同左 |
| `accentSoft` | 强调色淡底（选中导航、徽章） | `accent @ 12%` | 同左 |
| `accentSel` | 强调色选中底（列表行） | `accent @ 18%` | 同左 |
| `text2` | 次级文字 | 主文字混向背景 45% | 同左 |
| `text3` | 三级文字 / 禁用文字 | 主文字混向背景 65% | 同左 |
| `border` | 控件描边 | 主文字混向卡面 84% | 白 `@14%` |
| `divider` | 分隔线 | 主文字混向卡面 90% | 白 `@8%` |
| `pressed` | 按压叠色 | 悬停底混向主文字 7% | 同左，8% |
| `panel` | 卡面与页面之间的过渡面 | 卡面混向背景 14% | 同左 |

`Theme::m` 是 Fluent 2 度量（圆角、控件高、行高、字号、字体族、描边宽度），改这里就能整体换皮：`radiusBtn/radiusInput/radiusCard/radiusPanel`、`controlH/rowH/gap/padCard`、`fLabel/fBody/fSub/fHeading`、`borderW`、`family`。

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

### 输入法（自绘文本框最难的一块）

`Input` 不是把 `EDIT` 贴上去，所以组合串、候选窗、光标位置全部自己管：

- `WM_IME_STARTCOMPOSITION` 记录组合起点，`WM_IME_COMPOSITION` 取 `GCS_COMPSTR/GCS_RESULTSTR`
- 组合串在控件内以带下划线的方式渲染，下方留给系统候选窗
- `ImmSetCompositionWindow`：组合窗口贴着文字光标基线走，随输入推进
- `ImmSetCandidateWindow`：候选窗定位到光标下方，避免跑到窗口角落
- 结束后把结果串并入 `Text`，触发 `Changed`

键盘侧：选区（Shift+方向 / Home / End / 拖拽）、`Ctrl+A/C/X/V/Z`、撤销栈、Backspace/Delete 都已实现。

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

测试机:
| 项 | 配置 |
| --- | --- |
| GPU0(未使用) | NVIDIA NVS 5400M |
| GPU1(iGPU) | Intel HD Graphics 4000 |
| CPU | Intel Core i7-3520M |
| RAM | 12GB DDR3 (1600MHz) |

- 持久离屏层：绘制发生在 `ID2D1BitmapRenderTarget`，每帧只贴一次到窗口
- 帧循环自建：有动画时 `Tick + UpdateWindow + DwmFlush` 跟合成节拍，闲置时挂起（CPU 0）
- 脏矩形：动画只重绘标脏控件区域（含 28px 阴影余量），滚动只刷滚动容器
- 文本布局缓存（`IDWriteTextLayout`）与阴影位图缓存（尺寸取整 4px，上限 64）
- 实测（HD Graphics 4000，x64 Release）：滚动/动画帧 <12ms，同色全量重绘 <12ms，空闲 0% CPU；`VCUIDemo` 常驻约 20MB 工作集

调试开关（环境变量）：

| 变量 | 作用 |
|---|---|
| `VCUI_FPS=1` | 标题栏显示 fps / paint 耗时，无帧时显示 `idle last X.Xms` |
| `VCUI_NOSHADOW=1` | 跳过阴影绘制，用于定位阴影开销 |
| `VCUI_NOTEXT=1` | 跳过文字绘制，用于定位文字布局开销 |

`VCUIDemo` 额外认两个变量，方便截图与无头复现：`VCUI_DEMO_DARK=1` 直接进深色，`VCUI_DEMO_PALETTE=github` 用 GitHub 色板。

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
