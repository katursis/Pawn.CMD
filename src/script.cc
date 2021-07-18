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

bool Script::OnLoad() {
  int num_publics{};
  amx_->NumPublics(&num_publics);

  auto &plugin = Plugin::Instance();

  for (int index{}; index < num_publics; index++) {
    std::string public_name = GetPublicName(index);
    std::smatch match;
    if (std::regex_match(public_name, match, regex_public_cmd_name_)) {
      std::string cmd_name = PrepareCommandName(match[1].str());

      NewCommand(cmd_name, MakePublic(public_name, plugin.UseCaching()));
    } else if (std::regex_match(public_name, regex_public_cmd_alias_)) {
      init_flags_and_aliases_pubs_.push_back(MakePublic(public_name));
    } else if (std::regex_match(public_name, regex_public_cmd_flags_)) {
      init_flags_and_aliases_pubs_.push_front(MakePublic(public_name));
    } else if (plugin.LegacyOpctSupport() &&
               public_name == "OnPlayerCommandText") {
      opct_public_ = MakePublic(public_name, plugin.UseCaching());
    } else if (public_name == "OnPlayerCommandReceived") {
      opcr_public_ = MakePublic(public_name, plugin.UseCaching());
    } else if (public_name == "OnPlayerCommandPerformed") {
      opcp_public_ = MakePublic(public_name, plugin.UseCaching());
    } else if (public_name == "PC_OnInit") {
      on_init_public_ = MakePublic(public_name);
    }
  }

  return true;
}

bool Script::HandleCommand(cell playerid, const char *cmdtext,
                           const std::string &cmd, const char *params) {
  cell retval{};

  try {
    if (opct_public_ && opct_public_->Exists()) {
      retval = opct_public_->Exec(playerid, cmdtext);

      if (retval == 1) {
        return false;  // break the cycle
      }
    }

    const auto &iter_cmd = cmds_.find(cmd);
    bool command_exists = iter_cmd != cmds_.end();
    auto flags = command_exists ? iter_cmd->second->GetFlags() : 0;

    if (opcr_public_ && opcr_public_->Exists()) {
      retval = opcr_public_->Exec(playerid, cmd, params, flags);

      if (retval == 0) {
        return true;  // continue
      }
    }

    if (command_exists) {
      retval = iter_cmd->second->GetPublic()->Exec(playerid, params);
    } else {
      retval = -1;
    }

    if (opcp_public_ && opcp_public_->Exists()) {
      retval = opcp_public_->Exec(playerid, cmd, params, retval, flags);
    }

    if (retval == 1) {
      return false;  // break the cycle
    }
  } catch (const std::exception &e) {
    Plugin::Instance().Log("%s: %s", __func__, e.what());
  }

  return true;
}

void Script::NewCommand(const std::string &name, const PublicPtr &pub,
                        unsigned int flags, bool is_alias) {
  if (CommandExists(name)) {
    throw std::runtime_error{"Command name '" + name + "' is occupied"};
  }

  cmds_[name] = std::make_shared<Command>(pub, flags, is_alias);
}

const CommandPtr &Script::GetCommand(const std::string &name, bool strict) {
  const auto &command = cmds_.at(name);

  if (strict && command->IsAlias()) {
    throw std::runtime_error{"Command '" + name + "' is an alias"};
  }

  return command;
}

cell Script::DeleteCommand(const std::string &name) {
  return cmds_.erase(name);
}

cell Script::CommandExists(const std::string &name) {
  return cmds_.count(name);
}

cell Script::NewCmdArray() {
  auto arr = std::make_shared<CmdArray>();

  for (const auto &item : cmds_) {
    if (item.second->IsAlias()) {
      continue;
    }

    arr->push_back(item.first);
  }

  cmd_arrays_.insert(arr);

  return reinterpret_cast<cell>(arr.get());
}

cell Script::NewAliasArray(const std::string &cmd_name) {
  auto &command = GetCommand(cmd_name, true);

  auto arr = std::make_shared<CmdArray>();

  for (const auto &item : cmds_) {
    if (item.second->GetPublic() != command->GetPublic() ||
        !item.second->IsAlias()) {
      continue;
    }

    arr->push_back(item.first);
  }

  cmd_arrays_.insert(arr);

  return reinterpret_cast<cell>(arr.get());
}

void Script::DeleteArray(cell arr) { cmd_arrays_.erase(GetCmdArray(arr)); }

const CmdArrayPtr &Script::GetCmdArray(cell ptr) {
  const auto iter = std::find_if(
      cmd_arrays_.begin(), cmd_arrays_.end(),
      [ptr](const auto &p) { return reinterpret_cast<cell>(p.get()) == ptr; });

  if (iter == cmd_arrays_.end()) {
    throw std::runtime_error{"Invalid array handle"};
  }

  return *iter;
}

void Script::InitFlagsAndAliases() {
  for (const auto &pub : init_flags_and_aliases_pubs_) {
    if (pub && pub->Exists()) {
      pub->Exec();
    }
  }

  if (on_init_public_ && on_init_public_->Exists()) {
    on_init_public_->Exec();
  }
}

std::string Script::PrepareCommandName(const std::string &name) {
  auto &plugin = Plugin::Instance();

  if (plugin.CaseInsensitivity()) {
    return plugin.ToLower(name);
  }

  return name;
}

// native PC_Init();
cell Script::PC_Init() {
  InitFlagsAndAliases();

  return 1;
}

// native PC_RegAlias(const cmd[], const alias[], ...);
cell Script::PC_RegAlias(cell *params) {
  AssertMinParams(2, params);

  CommandPtr command{};
  for (std::size_t i = 1; i <= params[0] / sizeof(cell); i++) {
    auto cmd_name = PrepareCommandName(GetString(params[i]));

    if (i == 1) {
      command = GetCommand(cmd_name, true);
    } else {
      NewCommand(cmd_name, command->GetPublic(), command->GetFlags(), true);
    }
  }

  return 1;
}

// native PC_SetFlags(const cmd[], flags);
cell Script::PC_SetFlags(std::string cmd_name, cell flags) {
  cmd_name = PrepareCommandName(cmd_name);

  GetCommand(cmd_name, true)->SetFlags(flags);

  return 1;
}

// native PC_GetFlags(const cmd[]);
cell Script::PC_GetFlags(std::string cmd_name) {
  cmd_name = PrepareCommandName(cmd_name);

  return GetCommand(cmd_name)->GetFlags();
}

// native PC_RenameCommand(const cmd[], const newname[]);
cell Script::PC_RenameCommand(std::string cmd_name, std::string cmd_newname) {
  cmd_name = PrepareCommandName(cmd_name);
  cmd_newname = PrepareCommandName(cmd_newname);

  const auto &command = GetCommand(cmd_name);

  NewCommand(cmd_newname, command->GetPublic(), command->GetFlags(),
             command->IsAlias());

  DeleteCommand(cmd_name);

  return 1;
}

// native PC_CommandExists(const cmd[]);
cell Script::PC_CommandExists(std::string cmd_name) {
  cmd_name = PrepareCommandName(cmd_name);

  return CommandExists(cmd_name);
}

// native PC_DeleteCommand(const cmd[]);
cell Script::PC_DeleteCommand(std::string cmd_name) {
  cmd_name = PrepareCommandName(cmd_name);

  GetCommand(cmd_name);

  return DeleteCommand(cmd_name);
}

// native CmdArray:PC_GetCommandArray();
cell Script::PC_GetCommandArray() { return NewCmdArray(); }

// native CmdArray:PC_GetAliasArray(const cmd[]);
cell Script::PC_GetAliasArray(std::string cmd_name) {
  cmd_name = PrepareCommandName(cmd_name);

  return NewAliasArray(cmd_name);
}

// native PC_GetArraySize(CmdArray:arr);
cell Script::PC_GetArraySize(CmdArrayPtr arr) { return arr->size(); }

// native PC_GetCommandName(CmdArray:arr, index, dest[], size = sizeof dest);
cell Script::PC_GetCommandName(CmdArrayPtr arr, cell index, cell *dest,
                               cell size) {
  SetString(dest, arr->at(index), size);

  return 1;
}

// native PC_FreeArray(&CmdArray:arr);
cell Script::PC_FreeArray(cell *arr) {
  DeleteArray(*arr);

  *arr = 0;

  return 1;
}

// native PC_EmulateCommand(playerid, const cmdtext[]);
cell Script::PC_EmulateCommand(cell playerid, std::string cmdtext) {
  Plugin::ProcessCommand(playerid, cmdtext.c_str());

  return 1;
}
