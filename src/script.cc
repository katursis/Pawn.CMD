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

const CommandPtr &Script::GetCommand(const std::string &name,
                                     bool strict) {
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
