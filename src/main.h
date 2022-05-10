/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2022 katursis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef PAWNCMD_MAIN_H_
#define PAWNCMD_MAIN_H_

#define _GLIBCXX_USE_CXX11_ABI 1

#include <queue>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <locale>
#include <codecvt>
#include <cstdarg>

#include "sdk.hpp"
#include "Server/Components/Pawn/pawn.hpp"
#include "samp-ptl/ptl.h"
#include "cpptoml/include/cpptoml.h"

#include "Pawn.CMD.inc"

#ifdef THISCALL
#undef THISCALL
#endif

#ifdef _WIN32
#define THISCALL __thiscall
#else
#define THISCALL
#endif

#include "command.h"
#include "script.h"
#include "native_param.h"
#include "plugin.h"

class PluginComponent final : public IComponent,
                              public PawnEventHandler,
                              public PlayerEventHandler {
  PROVIDE_UID(0xa03b47c907a96c29);

  StringView componentName() const override;

  SemanticVersion componentVersion() const override;

  void onLoad(ICore *c) override;

  void onInit(IComponentList *components) override;

  void onAmxLoad(void *amx) override;

  void onAmxUnload(void *amx) override;

  void onFree(IComponent *component) override;

  void reset() override;

  void free() override;

  bool onPlayerCommandText(IPlayer &player, StringView message) override;

  static void PluginLogprintf(const char *fmt, ...);

  static ICore *&getCore();

 private:
  ICore *core_{};
  IPlayerPool *players_{};
  IPawnComponent *pawn_component_{};

  void *plugin_data_[MAX_PLUGIN_DATA]{};
};

#endif  // PAWNCMD_MAIN_H_
