// Copyright 2026 XGstudio
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// SPDX-License-Identifier: Apache-2.0
//
// XGStudio-cpp-UI 控件库展示（不是启动器打样）：
//   顶栏导航 + 左侧子导航 + 平坦内容区，内容全部是控件清单与用法说明。
// 用法：VCUIFrameDemo
//   VCUI_DEMO_DARK=1      深色
//   VCUI_NAV_CENTER=1     顶栏导航"最小居中"，否则整栏
//   VCUI_PAGE=0..3        启动页（总览/控件/布局/主题）

#include <windows.h>
#include <functional>
#include <vcui/vcui.h>

using namespace vcui;

// 文字统一走 Fluent type ramp：字号 / 字重 / 行高成套拿
static std::shared_ptr<Label> L(std::wstring s, const TypeStyle& sty) {
    auto l = std::make_shared<Label>(std::move(s), sty.size, sty.weight);
    l->LineHeight = sty.lineHeight;
    return l;
}

static std::shared_ptr<Label> Section(std::wstring title) {
    auto s = L(std::move(title), Type::Subtitle1);
    s->MT = 8.f;
    return s;
}

// 键值行：左标签固定列宽 + 右值
static std::shared_ptr<Widget> FormRow(const std::wstring& label, const std::wstring& value) {
    auto row = HBox(16.f);
    row->H = 48.f;
    row->Cross = Stack::Center;
    auto l = L(label, Type::Body1);
    l->W = 180.f;
    auto v = L(value, Type::Body1Strong);
    v->Grow = 1.f;
    v->Ellipsis = true;
    row->Add(l);
    row->Add(v);
    return row;
}

// 控件名 + 一句用法说明，作为每个展示块的小标题
static std::shared_ptr<Widget> DemoHead(const std::wstring& name, const std::wstring& desc) {
    auto col = VBox(2.f);
    col->Add(L(name, Type::Body1Strong));
    col->Add(L(desc, Type::Caption1));
    return col;
}

// demo 内的推进器：每隔几秒给两条定值进度条换一次目标值，让缓动过程看得见。
// Visible=false 所以不参与排布也不绘制，但仍会被 Tick 到（Widget::Update 不按 Visible 过滤）。
class SweepTicker : public vcui::Widget {
public:
    std::shared_ptr<vcui::Progress> a, b;
    float t = 0.f;
    bool fwd = true;
    bool Update(float dt) override {
        t += dt;
        if (t < 2.4f) return false;
        t = 0.f;
        fwd = !fwd;
        if (a) a->Value = fwd ? .86f : .18f;
        if (b) b->Value = fwd ? .30f : .72f;
        return true;
    }
};

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    bool dark = GetEnvironmentVariableW(L"VCUI_DEMO_DARK", nullptr, 0) > 0;
    bool navCenter = GetEnvironmentVariableW(L"VCUI_NAV_CENTER", nullptr, 0) > 0;
    wchar_t pageBuf[8] = {};
    int startPage = GetEnvironmentVariableW(L"VCUI_PAGE", pageBuf, 8) > 0 ? _wtoi(pageBuf) : 0;

    Surface app;
    app.SetMinSize(960, 620);
    app.SetCustomFrame(true);
    if (!app.Create(L"XGStudio-cpp-UI · 控件展示", 1180, 740, dark)) return 1;

    auto root = VBox(0.f);

    auto bar = std::make_shared<TitleBar>();
    bar->Title = L"XGStudio-cpp-UI";
    bar->Fill = true;
    bar->H = 48.f;
    bar->NavItems = { L"总览", L"控件", L"布局", L"主题" };
    bar->NavCentered = navCenter;
    root->Add(bar);

    auto body = HBox(0.f);
    body->Grow = 1.f;

    auto sub = VBox(4.f);
    sub->W = 220.f;
    sub->Pad(12.f);

    auto content = VBox(0.f);
    content->Grow = 1.f;
    content->Pad(28.f);

    body->Add(sub);
    body->Add(content);
    root->Add(body);

    // ---------------- 页 0：总览（关于本库） ----------------
    auto BuildOverview = [&]() {
        sub->Visible = false;

        auto head = HBox(16.f);
        head->H = 56.f;
        head->Cross = Stack::Center;
        auto title = L(L"XGStudio-cpp-UI", Type::Title1);
        title->Grow = 1.f;
        head->Add(title);
        head->Add(std::make_shared<Badge>(L"v0.1.0", Badge::Accent));
        content->Add(head);

        auto lead = L(L"Windows 上全自绘的 Fluent 2 控件库：没有一个系统控件，每个像素都由 Direct2D 自己画。", Type::Body1);
        lead->MT = 2.f;
        content->Add(lead);

        auto div = std::make_shared<Divider>();
        div->MT = 20.f;
        content->Add(div);

        content->Add(Section(L"构成"));
        content->Add(FormRow(L"绘制", L"Direct2D 1.0 + DirectWrite（离屏层，不依赖 D2D1.1 / Effect）"));
        content->Add(std::make_shared<Divider>());
        content->Add(FormRow(L"窗口", L"Win32 自绘边框，客户区吃掉整个窗口"));
        content->Add(std::make_shared<Divider>());
        content->Add(FormRow(L"语言 / 编译器", L"C++17 · MSVC 2019/2022 x64"));
        content->Add(std::make_shared<Divider>());
        content->Add(FormRow(L"系统要求", L"Windows 10 1703+（Per-Monitor V2）"));
        content->Add(std::make_shared<Divider>());
        content->Add(FormRow(L"许可证", L"Apache-2.0"));
        content->Add(std::make_shared<Divider>());
        content->Add(FormRow(L"仓库", L"github.com/XG-cyhilu/XGStudio-cpp-UI"));

        content->Add(Section(L"这一版有什么"));
        auto stats = HBox(12.f);
        stats->H = 44.f;
        stats->Cross = Stack::Center;
        stats->Add(std::make_shared<Badge>(L"17 个控件", Badge::Neutral));
        stats->Add(std::make_shared<Badge>(L"7 种容器", Badge::Neutral));
        stats->Add(std::make_shared<Badge>(L"14 档字级", Badge::Accent));
        stats->Add(std::make_shared<Badge>(L"浅/深 × 2 色板", Badge::Success));
        content->Add(stats);

        auto note = L(L"换皮不用改控件代码：颜色走 Token，尺寸走 Metrics，字号走 Type ramp。", Type::Caption1);
        note->MT = 12.f;
        content->Add(note);
        content->StaggerIn();
    };

    // ---------------- 页 1：控件 ----------------
    int ctlTab = 0;
    std::function<void(int)> ShowControls;
    ShowControls = [&](int tab) {
        ctlTab = tab;
        sub->Visible = true;
        sub->Clear();
        const wchar_t* tabs[3] = { L"输入类", L"选择类", L"展示类" };
        const wchar_t* glyphs[3] = { L"\xE8D2", L"\xE73A", L"\xE8B6" };
        for (int i = 0; i < 3; ++i) {
            auto n = std::make_shared<NavItem>(tabs[i], glyphs[i]);
            n->Active = (i == tab);
            n->Clicked = [&, i]() { if (ctlTab != i) { ShowControls(i); app.Invalidate(); } };
            sub->Add(n);
        }

        content->Clear();
        content->Add(L(tabs[tab], Type::Subtitle1));
        auto note0 = L(L"每个控件都能单独拿出来用；悬停、按下、焦点都是控件自己画的。", Type::Caption1);
        note0->MT = 4.f;
        content->Add(note0);
        auto div = std::make_shared<Divider>();
        div->MT = 14.f;
        content->Add(div);

        auto scroll = std::make_shared<ScrollView>();
        scroll->Grow = 1.f;
        auto col = VBox(22.f);
        col->MT = 18.f;

        if (tab == 0) {
            auto b1 = VBox(10.f);
            b1->Add(DemoHead(L"Input", L"单行输入：选区、剪贴板、撤销、IME 组合串与候选窗定位"));
            auto in1 = std::make_shared<Input>(L"输入点什么…");
            in1->W = 360.f;
            b1->Add(in1);
            col->Add(b1);

            auto b2 = VBox(10.f);
            b2->Add(DemoHead(L"Slider", L"0..1 连续值，点击和拖拽都会回调 Changed"));
            auto sl = std::make_shared<Slider>(.35f);
            sl->W = 320.f;
            auto slVal = L(L"35%", Type::Caption1);
            auto row = HBox(12.f);
            row->Cross = Stack::Center;
            row->Add(sl);
            row->Add(slVal);
            b2->Add(row);
            sl->Changed = [&app, slVal](float v) {
                wchar_t b[32];
                swprintf_s(b, L"%d%%", (int)(v * 100.f + .5f));
                slVal->Text = b;
                app.Invalidate();
            };
            col->Add(b2);

            auto b3 = VBox(10.f);
            b3->Add(DemoHead(L"Dropdown", L"下拉框：菜单挂在 Overlay 层，可超出窗口边界"));
            auto dd = std::make_shared<Dropdown>(std::vector<std::wstring>{ L"跟随系统", L"浅色", L"深色" }, 0);
            dd->W = 220.f;
            b3->Add(dd);
            col->Add(b3);

            auto b4 = VBox(10.f);
            b4->Add(DemoHead(L"Segmented", L"分段单选，适合 2–4 个同层级选项"));
            auto seg = std::make_shared<Segmented>(std::vector<std::wstring>{ L"天", L"周", L"月" }, 1);
            seg->W = 260.f;
            b4->Add(seg);
            col->Add(b4);

            auto b5 = VBox(10.f);
            b5->Add(DemoHead(L"Checkbox", L"多选：点文字也能勾，键盘 Space 可切"));
            auto c1 = std::make_shared<Checkbox>(L"记住这次选择", true);
            auto c2 = std::make_shared<Checkbox>(L"启动时自动检查更新", false);
            auto c3 = std::make_shared<Checkbox>(L"禁用态示例", false);
            c3->Enabled = false;
            b5->Add(c1);
            b5->Add(c2);
            b5->Add(c3);
            col->Add(b5);
        } else if (tab == 1) {
            auto b1 = VBox(10.f);
            b1->Add(DemoHead(L"Switch", L"开关：滑块位置带缓动，切的是即时状态"));
            auto srow = HBox(16.f);
            srow->Cross = Stack::Center;
            srow->Add(std::make_shared<Switch>(true));
            srow->Add(L(L"开启状态", Type::Caption1));
            auto srow2 = HBox(16.f);
            srow2->Cross = Stack::Center;
            srow2->Add(std::make_shared<Switch>(false));
            srow2->Add(L(L"关闭状态", Type::Caption1));
            b1->Add(srow);
            b1->Add(srow2);
            col->Add(b1);

            auto b2 = VBox(10.f);
            b2->Add(DemoHead(L"ListRow", L"列表行：标题 + 副标题 + 右侧文字，超长尾部省略"));
            auto r1 = std::make_shared<ListRow>(L"这一条标题很长很长很长很长很长很长，用来验证尾部省略",
                                                L"副标题写辅助信息", L"刚刚");
            r1->Selected = true;
            auto r2 = std::make_shared<ListRow>(L"第二条", L"未选中状态", L"2 天前");
            auto r3 = std::make_shared<ListRow>(L"第三条", L"右侧文字可以用更弱的色阶", L"更早");
            b2->Add(r1);
            b2->Add(r2);
            b2->Add(r3);
            col->Add(b2);
        } else {
            auto b1 = VBox(10.f);
            b1->Add(DemoHead(L"Button", L"主按钮 / 普通 / 禁用 / 带字形"));
            auto brow = HBox(12.f);
            brow->Cross = Stack::Center;
            auto bt1 = std::make_shared<Button>(L"主按钮", true);
            auto bt2 = std::make_shared<Button>(L"普通按钮");
            auto bt3 = std::make_shared<Button>(L"禁用", false);
            bt3->Enabled = false;
            auto bt4 = std::make_shared<Button>(L"带图标");
            bt4->Glyph = L"\xE768";
            bt4->MinWidth = 120.f;
            brow->Add(bt1);
            brow->Add(bt2);
            brow->Add(bt3);
            brow->Add(bt4);
            b1->Add(brow);
            col->Add(b1);

            auto b2 = VBox(10.f);
            b2->Add(DemoHead(L"IconButton", L"Segoe MDL2 字形图标按钮，Ghost 模式无底色"));
            auto irow = HBox(8.f);
            irow->Cross = Stack::Center;
            irow->Add(std::make_shared<IconButton>(L"\xE713"));
            irow->Add(std::make_shared<IconButton>(L"\xE896"));
            irow->Add(std::make_shared<IconButton>(L"\xE8B6"));
            irow->Add(std::make_shared<IconButton>(L"\xE74D"));
            b2->Add(irow);
            col->Add(b2);

            auto b3 = VBox(10.f);
            b3->Add(DemoHead(L"Badge", L"五种色调，用于状态标记"));
            auto badrow = HBox(8.f);
            badrow->Cross = Stack::Center;
            badrow->Add(std::make_shared<Badge>(L"中性", Badge::Neutral));
            badrow->Add(std::make_shared<Badge>(L"强调", Badge::Accent));
            badrow->Add(std::make_shared<Badge>(L"成功", Badge::Success));
            badrow->Add(std::make_shared<Badge>(L"警告", Badge::Warning));
            badrow->Add(std::make_shared<Badge>(L"危险", Badge::Danger));
            b3->Add(badrow);
            col->Add(b3);

            auto b4 = VBox(10.f);
            b4->Add(DemoHead(L"Progress",
                             L"前两条定值：每 2.4s 换一次目标值，看的是指数缓动而不是硬跳；第三条是不确定态，accent 在轨道里循环"));
            auto p1 = std::make_shared<Progress>(.30f);
            p1->W = 360.f;
            auto p2 = std::make_shared<Progress>(.72f);
            p2->W = 360.f;
            auto p3 = std::make_shared<Progress>();
            p3->Indeterminate = true;      // 循环态：Phase 自己走，帧循环不会睡
            p3->W = 360.f;
            auto sweep = std::make_shared<SweepTicker>();
            sweep->a = p1;
            sweep->b = p2;
            sweep->Visible = false;        // 不占位、不绘制，只借 Tick 推进
            b4->Add(p1);
            b4->Add(p2);
            b4->Add(p3);
            b4->Add(sweep);
            col->Add(b4);

            auto b5 = VBox(10.f);
            b5->Add(DemoHead(L"Label", L"文本：字号走 Type ramp，可设省略与对齐"));
            auto ell = L(L"这一行很长很长很长很长很长很长，会被截断成省略号", Type::Body1);
            ell->Ellipsis = true;
            ell->W = 420.f;
            b5->Add(ell);
            col->Add(b5);

            // 反馈类：悬停提示 / 提示条 / 对话框
            auto b6 = VBox(10.f);
            b6->Add(DemoHead(L"Tooltip / InfoBar / Dialog",
                             L"提示气泡走 Widget::Tooltip；提示条右上角堆叠并自动消失；对话框带遮罩与按钮回调"));
            auto frow = HBox(10.f);
            frow->Cross = Stack::Center;

            auto tbtn = std::make_shared<Button>(L"悬停看 Tooltip");
            tbtn->Tooltip = L"600ms 后出现在控件下方，下面是 Tooltip 控件画的";
            frow->Add(tbtn);

            auto ibtn = std::make_shared<Button>(L"弹一条提示");
            ibtn->Clicked = [&app]() {
                static int n = 0;
                ++n;
                app.ShowInfoBar(L"第 " + std::to_wstring(n) + L" 条提示：已保存到本地", InfoTone::Success, 3.f);
            };
            frow->Add(ibtn);

            auto wbtn = std::make_shared<Button>(L"警告提示");
            wbtn->Clicked = [&app]() {
                app.ShowInfoBar(L"网络不可达，已切换到离线模式", InfoTone::Warning, 4.f);
            };
            frow->Add(wbtn);

            auto lbtn = std::make_shared<Button>(L"长文本提示");
            lbtn->Clicked = [&app]() {
                app.ShowInfoBar(L"实例 meta 校验失败：第 3 行 readingSpeed 不是数字，已跳过该条目并继续扫描",
                               InfoTone::Danger, 5.f);
            };
            frow->Add(lbtn);

            auto dbtn = std::make_shared<Button>(L"打开对话框", true);
            dbtn->Clicked = [&app]() {
                Dialog::Open(&app, L"删除这个实例？",
                             L"实例目录、版本 JSON 与存档都会被移除，且无法从启动器还原。",
                             { DialogAction{ L"取消", false, false, nullptr },
                               DialogAction{ L"删除", false, true, []() {} } });
            };
            frow->Add(dbtn);
            b6->Add(frow);

            auto hint = L(L"对话框：点遮罩或按 Esc 关闭，按钮回调先执行再关闭。", Type::Caption1);
            b6->Add(hint);
            col->Add(b6);
        }

        scroll->Add(col);
        content->Add(scroll);
        content->StaggerIn();
    };

    // ---------------- 页 2：布局 ----------------
    int layTab = 0;
    std::function<void(int)> ShowLayout;
    ShowLayout = [&](int tab) {
        layTab = tab;
        sub->Visible = true;
        sub->Clear();
        const wchar_t* tabs[3] = { L"容器", L"栅格", L"滚动" };
        const wchar_t* glyphs[3] = { L"\xE8A9", L"\xE80A", L"\xE8B5" };
        for (int i = 0; i < 3; ++i) {
            auto n = std::make_shared<NavItem>(tabs[i], glyphs[i]);
            n->Active = (i == tab);
            n->Clicked = [&, i]() { if (layTab != i) { ShowLayout(i); app.Invalidate(); } };
            sub->Add(n);
        }

        content->Clear();
        content->Add(L(tabs[tab], Type::Subtitle1));
        auto div = std::make_shared<Divider>();
        div->MT = 14.f;
        content->Add(div);

        if (tab == 0) {
            content->Add(Section(L"VBox / HBox"));
            content->Add(FormRow(L"一维容器", L"主轴按 Grow 分剩余空间，交叉轴可 Start/Center/End/Stretch"));
            content->Add(std::make_shared<Divider>());
            content->Add(FormRow(L"间距", L"Gap 控制兄弟间距，Margin 与 Pad 分开算"));
            content->Add(std::make_shared<Divider>());

            content->Add(Section(L"Card"));
            auto card = std::make_shared<Card>();
            card->H = 116.f;
            auto inbox = VBox(6.f);
            inbox->Add(L(L"卡片：底色 + 圆角 + 描边 + 可选阴影", Type::Body1Strong));
            inbox->Add(L(L"子项按普通 VBox 布局，Pad 不会撑破圆角", Type::Caption1));
            card->Add(inbox);
            content->Add(card);

            auto note = L(L"Divider 在容器里是一根 1px 线，Vertical 时可竖着分栏。", Type::Caption1);
            note->MT = 12.f;
            content->Add(note);
        } else if (tab == 1) {
            content->Add(Section(L"Grid(3, 12)"));
            auto grid = std::make_shared<Grid>(3, 12.f);
            for (int i = 0; i < 6; ++i) {
                auto cell = std::make_shared<Card>();
                cell->H = 88.f;
                auto c = VBox(4.f);
                wchar_t b[32];
                swprintf_s(b, L"格 %d", i + 1);
                c->Add(L(b, Type::Body1Strong));
                c->Add(L(L"等宽，行高取该行最高子项", Type::Caption1));
                cell->Add(c);
                grid->Add(cell);
            }
            content->Add(grid);

            auto note = L(L"Grid 的 Desired 返回真实内容高度，父容器的剩余空间才不会算错。", Type::Caption1);
            note->MT = 12.f;
            content->Add(note);
        } else {
            content->Add(Section(L"ScrollView"));
            auto scroll = std::make_shared<ScrollView>();
            scroll->Grow = 1.f;
            scroll->MT = 12.f;
            auto rows = VBox(0.f);
            for (int i = 0; i < 26; ++i) {
                auto r = std::make_shared<ListRow>(L"可滚动内容", L"滚轮 / 拖滚动条 / 点轨道都能滚",
                                                  L"行 " + std::to_wstring(i + 1));
                r->H = 48.f;
                rows->Add(r);
                rows->Add(std::make_shared<Divider>());
            }
            scroll->Add(rows);
            content->Add(scroll);
            auto note = L(L"滚动条静止时是 2px 细线，悬停变粗；内容高度由子项自己算。", Type::Caption1);
            note->MT = 8.f;
            content->Add(note);
        }
        content->StaggerIn();
    };

    // ---------------- 页 3：主题 ----------------
    int thmTab = 0;
    std::function<void(int)> ShowTheme;
    ShowTheme = [&](int tab) {
        thmTab = tab;
        sub->Visible = true;
        sub->Clear();
        const wchar_t* tabs[3] = { L"明暗", L"色板", L"动效" };
        const wchar_t* glyphs[3] = { L"\xE706", L"\xE790", L"\xE804" };
        for (int i = 0; i < 3; ++i) {
            auto n = std::make_shared<NavItem>(tabs[i], glyphs[i]);
            n->Active = (i == tab);
            n->Clicked = [&, i]() { if (thmTab != i) { ShowTheme(i); app.Invalidate(); } };
            sub->Add(n);
        }

        content->Clear();
        content->Add(L(tabs[tab], Type::Subtitle1));
        auto div = std::make_shared<Divider>();
        div->MT = 14.f;
        content->Add(div);

        if (tab == 0) {
            content->Add(Section(L"当前 Token"));
            auto swrow = HBox(16.f);
            swrow->H = 84.f;
            auto cell = [&](const std::wstring& name, D2D1_COLOR_F c) {
                auto col = VBox(6.f);
                auto box = std::make_shared<Card>();
                box->W = 44.f;
                box->H = 40.f;
                box->Fill = c;
                box->Shadow = false;
                box->Radius = 4.f;
                col->Add(box);
                col->Add(L(name, Type::Caption1));
                return col;
            };
            const Tokens& tk = app.Th().t;
            swrow->Add(cell(L"kp 卡片面", tk.kp));
            swrow->Add(cell(L"bj 页面底", tk.bj));
            swrow->Add(cell(L"xt 控件面", tk.xt));
            swrow->Add(cell(L"accent", tk.accent));
            swrow->Add(cell(L"text2 次要字", tk.text2));
            swrow->Add(cell(L"divider", tk.divider));
            content->Add(swrow);

            content->Add(Section(L"切换"));
            auto row = HBox(16.f);
            row->H = 48.f;
            row->Cross = Stack::Center;
            row->Add(L(L"深色模式", Type::Body1));
            auto sw = std::make_shared<Switch>(app.IsDark());
            sw->Changed = [&app](bool on) { app.SetDark(on); app.Invalidate(); };
            row->Add(sw);
            content->Add(row);

            auto note = L(L"SetDark 只换 Token，不重建控件树；已挂着的控件下一帧就按新色画。切完再点「重播入场」看效果。", Type::Caption1);
            note->MT = 8.f;
            content->Add(note);
        } else if (tab == 1) {
            content->Add(Section(L"色板切换"));
            auto seg = std::make_shared<Segmented>(std::vector<std::wstring>{ L"Microsoft", L"GitHub" }, 0);
            seg->W = 260.f;
            seg->Changed = [&app, seg](int i) {
                app.SetTheme(i == 1 ? Palette::GitHub : Palette::Microsoft, app.IsDark());
                seg->Active = i;
                app.Invalidate();
            };
            content->Add(seg);

            content->Add(Section(L"强调色"));
            auto swrow = HBox(12.f);
            swrow->H = 40.f;
            swrow->Cross = Stack::Center;
            for (unsigned rgb : { 0x0F6CBDu, 0x0E7A6Bu, 0x6B4FD6u, 0xC13B7Eu, 0xD9772Au }) {
                auto sw = std::make_shared<Widget>();
                sw->W = 28.f;
                sw->H = 28.f;
                sw->Bg = Hex(rgb);
                sw->Interactive = true;
                D2D1_COLOR_F c = Hex(rgb);
                sw->Clicked = [&app, c]() { app.SetAccent(c); app.Invalidate(); };
                swrow->Add(sw);
            }
            content->Add(swrow);

            auto note = L(L"SetAccent 会把 accent / accentSoft / accentSel 一起派生，控件不用改。", Type::Caption1);
            note->MT = 12.f;
            content->Add(note);
        } else {
            content->Add(Section(L"入场动画"));
            auto replay = std::make_shared<Button>(L"重播入场", true);
            replay->W = 140.f;
            replay->Clicked = [&]() { content->StaggerIn(); app.Invalidate(); };
            content->Add(replay);

            content->Add(Section(L"参数"));
            content->Add(FormRow(L"单项时长", L"252ms（Fluent 常用入场值）"));
            content->Add(std::make_shared<Divider>());
            content->Add(FormRow(L"错峰步长", L"40ms，单页封顶 240ms"));
            content->Add(std::make_shared<Divider>());
            content->Add(FormRow(L"缓动", L"cubic-bezier(0.1, 0.9, 0.2, 1)，起步快、收尾慢"));
            content->Add(std::make_shared<Divider>());
            content->Add(FormRow(L"开销", L"每帧只重绘自己那块脏区，静止后帧循环挂起"));

            auto note = L(L"StaggerIn 遇 ScrollView 会下钻一层，所以列表是逐行出现的。", Type::Caption1);
            note->MT = 12.f;
            content->Add(note);
        }
        content->StaggerIn();
    };

    int curPage = -1;
    auto ShowPage = [&](int idx) {
        if (idx == curPage) return;
        curPage = idx;
        content->Clear();
        switch (idx) {
        case 0: BuildOverview(); break;
        case 1: ShowControls(ctlTab); break;
        case 2: ShowLayout(layTab); break;
        default: ShowTheme(thmTab); break;
        }
        app.Invalidate();
    };
    bar->NavChanged = [&](int i) { ShowPage(i); };

    app.SetRoot(root);
    ShowPage(startPage);
    bar->NavIndex = startPage;
    return app.Run();
}
