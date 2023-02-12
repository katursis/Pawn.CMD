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

#ifndef PAWNCMD_PLUGIN_H_
#define PAWNCMD_PLUGIN_H_

class Plugin : public ptl::AbstractPlugin<Plugin, Script, NativeParam> {
 public:
  const char *Name() { return "Pawn.CMD"; }

  int Version() { return PAWNCMD_VERSION; }

  bool OnLoad();

  bool LogAmxErrors();

  void OnUnload();

  void ReadConfig();

  void SaveConfig();

  bool CaseInsensitivity() const { return case_insensitivity_; }

  bool LegacyOpctSupport() const { return legacy_opct_support_; }

  bool UseCaching() const { return use_caching_; }

  std::string ToLower(const std::string &str);

  void InstallHooks();

  static int THISCALL HOOK_CFilterScripts__OnPlayerCommandText(
      void *, cell playerid, const char *cmdtext);

  static void ProcessCommand(cell playerid, const char *cmdtext);

 private:
#ifdef _WIN32
  const char *opct_pattern_ =
      "\x83\xEC\x08\x53\x8B\x5C\x24\x14\x55\x8B\x6C\x24\x14\x56\x33\xF6\x57"
      "\x8B\xF9\x89\x74\x24\x10\x8B\x04\xB7\x85\xC0";
  const char *opct_mask_ = "xxxxxxxxxxxxxxxxxxxxxxxxxxxx";
#else
  const char *opct_pattern_ =
      "\x55\x89\xE5\x57\x56\x53\x83\xEC\x2C\x8B\x75\x08\xC7\x45\xE4\x00\x00"
      "\x00\x00\x8B\x7D\x10\x89\xF3\xEB\x14";
  const char *opct_mask_ = "xxxxxxxxxxxxxxxxxxxxxxxxxx";
#endif

  const std::string config_path_ = "plugins/pawncmd.cfg";

  bool case_insensitivity_{};
  bool legacy_opct_support_{};
  bool use_caching_{};
  bool log_amx_errors_{};
  std::locale locale_;

  std::shared_ptr<urmem::hook> hook_fs__on_player_command_text_;
};

#endif  // PAWNCMD_PLUGIN_H_
