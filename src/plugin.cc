/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2021 katursis
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

#include "main.h"

bool Plugin::OnLoad() {
  ReadConfig();

  InstallHooks();

  RegisterNative<&Script::PC_Init>("PC_Init");

  RegisterNative<&Script::PC_RegAlias, false>("PC_RegAlias");
  RegisterNative<&Script::PC_SetFlags>("PC_SetFlags");
  RegisterNative<&Script::PC_GetFlags>("PC_GetFlags");
  RegisterNative<&Script::PC_RenameCommand>("PC_RenameCommand");
  RegisterNative<&Script::PC_CommandExists>("PC_CommandExists");
  RegisterNative<&Script::PC_DeleteCommand>("PC_DeleteCommand");

  RegisterNative<&Script::PC_GetCommandArray>("PC_GetCommandArray");
  RegisterNative<&Script::PC_GetAliasArray>("PC_GetAliasArray");
  RegisterNative<&Script::PC_GetArraySize>("PC_GetArraySize");
  RegisterNative<&Script::PC_GetCommandName>("PC_GetCommandName");
  RegisterNative<&Script::PC_FreeArray>("PC_FreeArray");

  RegisterNative<&Script::PC_EmulateCommand>("PC_EmulateCommand");

  Log("\n\n"
      "    | %s %s | 2016 - %s"
      "\n"
      "    |--------------------------------"
      "\n"
      "    | Author and maintainer: katursis"
      "\n\n\n"
      "    | Compiled: %s at %s"
      "\n"
      "    |--------------------------------------------------------------"
      "\n"
      "    | Repository: https://github.com/katursis/%s"
      "\n",
      Name(), VersionAsString().c_str(), &__DATE__[7], __DATE__, __TIME__,
      Name());

  return true;
}

void Plugin::OnUnload() {
  SaveConfig();

  Log("plugin unloaded");
}

void Plugin::ReadConfig() {
  std::fstream{config_path_, std::fstream::out | std::fstream::app};

  const auto config = cpptoml::parse_file(config_path_);

  case_insensitivity_ =
      config->get_as<bool>("CaseInsensitivity").value_or(true);
  legacy_opct_support_ =
      config->get_as<bool>("LegacyOpctSupport").value_or(true);
  use_caching_ = config->get_as<bool>("UseCaching").value_or(true);
  locale_ =
      std::locale{config->get_as<std::string>("LocaleName").value_or("C")};
}

void Plugin::SaveConfig() {
  auto config = cpptoml::make_table();

  config->insert("CaseInsensitivity", case_insensitivity_);
  config->insert("LegacyOpctSupport", legacy_opct_support_);
  config->insert("UseCaching", use_caching_);
  config->insert("LocaleName", locale_.name());

  std::fstream{config_path_, std::fstream::out | std::fstream::trunc}
      << (*config);
}

std::string Plugin::ToLower(const std::string &str) {
  try {
    auto result = str;

    std::use_facet<std::ctype<char>>(locale_).tolower(
        &result.front(), &result.front() + result.size());

    return result;
  } catch (const std::exception &e) {
    Log("%s (%s): %s", __func__, str.c_str(), e.what());
  }

  return str;
}

void Plugin::InstallHooks() {
  urmem::address_t addr_opct{};
  urmem::sig_scanner scanner;
  if (!scanner.init(reinterpret_cast<urmem::address_t>(logprintf_)) ||
      !scanner.find(opct_pattern_, opct_mask_, addr_opct)) {
    throw std::runtime_error{"CFilterScripts::OnPlayerCommandText not found"};
  }

  hook_fs__on_player_command_text_ =
      urmem::hook::make(addr_opct, &HOOK_CFilterScripts__OnPlayerCommandText);
}

int THISCALL Plugin::HOOK_CFilterScripts__OnPlayerCommandText(
    void *, cell playerid, const char *cmdtext) {
  ProcessCommand(playerid, cmdtext);

  return 1;
}

void Plugin::ProcessCommand(cell playerid, const char *cmdtext) {
  if (!cmdtext || cmdtext[0] != '/') {
    return;
  }

  std::size_t i = 1, cmd_start{};
  while (cmdtext[i] == ' ') {
    i++;
  }  // skip excess spaces before cmd
  cmd_start = i;

  while (cmdtext[i] && cmdtext[i] != ' ') {
    i++;
  }

  std::string cmd{&cmdtext[cmd_start], &cmdtext[i]};

  if (cmd.empty()) {
    return;
  }

  while (cmdtext[i] == ' ') {
    i++;
  }  // skip excess spaces after cmd

  const char *params = &cmdtext[i];

  cmd = Script::PrepareCommandName(cmd);

  Plugin::EveryScript([=](auto &script) {
    return script->HandleCommand(playerid, cmdtext, cmd, params);
  });
}
