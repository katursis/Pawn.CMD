/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2020 urShadow
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

#ifndef PAWNCMD_SCRIPT_H_
#define PAWNCMD_SCRIPT_H_

using PublicPtr = std::shared_ptr<ptl::Public>;
using CmdArray = std::vector<std::string>;
using CmdArrayPtr = std::shared_ptr<CmdArray>;

class Command {
 public:
  Command(const PublicPtr &pub, unsigned int flags = 0, bool is_alias = false)
      : public_{pub}, flags_{flags}, is_alias_{is_alias} {}

  inline const PublicPtr &GetPublic() const { return public_; }

  inline unsigned int GetFlags() const { return flags_; }

  inline void SetFlags(unsigned int flags) { flags_ = flags; }

  inline bool IsAlias() const { return is_alias_; }

 private:
  PublicPtr public_;
  unsigned int flags_{};
  bool is_alias_{};
};

using CommandPtr = std::shared_ptr<Command>;

class Script : public ptl::AbstractScript<Script> {
 public:
  const char *VarIsGamemode() { return "_pawncmd_is_gamemode"; }

  const char *VarVersion() { return "_pawncmd_version"; }

  bool OnLoad();

  bool HandleCommand(cell playerid, const char *cmdtext, const std::string &cmd,
                     const char *params);

  void NewCommand(const std::string &name, const PublicPtr &pub,
                  unsigned int flags = 0, bool is_alias = false);

  const CommandPtr &GetCommand(const std::string &name, bool strict = false);

  cell DeleteCommand(const std::string &name);

  cell CommandExists(const std::string &name);

  cell NewCmdArray();

  cell NewAliasArray(const std::string &cmd_name);

  void DeleteArray(cell arr);

  const CmdArrayPtr &GetCmdArray(cell ptr);

  void InitFlagsAndAliases();

  static void PrepareCommandName(std::string &name);

 private:
  const std::regex regex_public_cmd_name_{R"(pc_cmd_(\w+))"};
  const std::regex regex_public_cmd_alias_{R"(pc_alias_\w+)"};
  const std::regex regex_public_cmd_flags_{R"(pc_flags_\w+)"};

  std::unordered_map<std::string, CommandPtr> cmds_;
  PublicPtr opct_public_;     // OnPlayerCommandText
  PublicPtr opcr_public_;     // OnPlayerCommandReceived
  PublicPtr opcp_public_;     // OnPlayerCommandPerformed
  PublicPtr on_init_public_;  // PC_OnInit

  std::unordered_set<std::shared_ptr<CmdArray>> cmd_arrays_;

  std::deque<PublicPtr> init_flags_and_aliases_pubs_;
};

#endif  // PAWNCMD_SCRIPT_H_
