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

#include "vcui/Theme.h"

namespace vcui {

void Theme::Refresh() {
    t = GetTokens(pal, dark);
    m = GetMetrics();

    t.accent      = accent;
    t.accentHover = Lighten(accent, .08f);
    t.accentPress = Darken(accent, .10f);
    t.onAccent    = OnColor(accent);
    t.accentSoft  = A(accent, .12f);
    t.accentSel   = A(accent, .18f);

    t.text2 = Mix(t.wz, t.bj, .42f);
    t.text3 = Mix(t.wz, t.bj, .62f);
}

void Theme::Set(Palette p, bool isDark) {
    pal = p;
    dark = isDark;
    Refresh();
}

void Theme::SetAccent(D2D1_COLOR_F a) {
    accent = a;
    Refresh();
}

} // namespace vcui
