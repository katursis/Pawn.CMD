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

#ifndef PAWNCMD_SCRIPT_H_
#define PAWNCMD_SCRIPT_H_

using CmdArray = std::vector<std::string>; // actually array of names, sorry for the bad naming
using CmdArrayPtr = std::shared_ptr<CmdArray>;

using CommandPtr = std::shared_ptr<Command>;

class Script : public ptl::AbstractScript<Script> {
 public:
  const char *VarIsGamemode() { return "_pawncmd_is_gamemode"; }

  const char *VarVersion() { return "_pawncmd_version"; }

  // native PC_Init();
  cell PC_Init();

  // native PC_RegAlias(const cmd[], const alias[], ...);
  cell PC_RegAlias(cell *params);

  // native PC_SetFlags(const cmd[], flags);
  cell PC_SetFlags(std::string cmd_name, cell flags);

  // native PC_GetFlags(const cmd[]);
  cell PC_GetFlags(std::string cmd_name);

  // native PC_RenameCommand(const cmd[], const newname[]);
  cell PC_RenameCommand(std::string cmd_name, std::string cmd_newname);

  // native PC_CommandExists(const cmd[]);
  cell PC_CommandExists(std::string cmd_name);

  // native PC_DeleteCommand(const cmd[]);
  cell PC_DeleteCommand(std::string cmd_name);

  // native CmdArray:PC_GetCommandArray();
  cell PC_GetCommandArray();

  // native CmdArray:PC_GetAliasArray(const cmd[]);
  cell PC_GetAliasArray(std::string cmd_name);

  // native PC_GetArraySize(CmdArray:arr);
  cell PC_GetArraySize(CmdArrayPtr arr);

  // native PC_GetCommandName(CmdArray:arr, index, dest[], size = sizeof dest);
  cell PC_GetCommandName(CmdArrayPtr arr, cell index, cell *dest, cell size);

  // native PC_FreeArray(&CmdArray:arr);
  cell PC_FreeArray(cell *arr);

  // native PC_EmulateCommand(playerid, const cmdtext[]);
  cell PC_EmulateCommand(cell playerid, std::string cmdtext);

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

  static std::string PrepareCommandName(const std::string &name);

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
