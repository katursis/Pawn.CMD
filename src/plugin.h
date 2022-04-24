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

#ifndef PAWNCMD_PLUGIN_H_
#define PAWNCMD_PLUGIN_H_

class Plugin : public ptl::AbstractPlugin<Plugin, Script, NativeParam> {
 public:
  const char *Name() { return "Pawn.CMD"; }

  int Version() { return PAWNCMD_VERSION; }

  bool OnLoad();

  void OnUnload();

  void ReadConfig();

  void SaveConfig();

  bool CaseInsensitivity() const { return case_insensitivity_; }

  bool LegacyOpctSupport() const { return legacy_opct_support_; }

  bool UseCaching() const { return use_caching_; }

  std::string ToLower(const std::string &str);

  static void ProcessCommand(cell playerid, const char *cmdtext);

 private:
  const std::string config_path_ = "components/pawncmd.cfg";

  bool case_insensitivity_{};
  bool legacy_opct_support_{};
  bool use_caching_{};
  std::locale locale_;
};

#endif  // PAWNCMD_PLUGIN_H_
