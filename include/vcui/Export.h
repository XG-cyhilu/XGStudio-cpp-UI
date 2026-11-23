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

// 默认静态链接；要构建 DLL 时定义 VCUI_DLL（库本体自动带 VCUI_EXPORTS）。
#if defined(VCUI_DLL) && defined(_WIN32)
#  ifdef VCUI_EXPORTS
#    define VCUI_API __declspec(dllexport)
#  else
#    define VCUI_API __declspec(dllimport)
#  endif
#else
#  define VCUI_API
#endif
