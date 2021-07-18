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

// native PC_Init();
cell natives::PC_Init(Script *script) {
  script->InitFlagsAndAliases();

  return 1;
}

// native PC_RegAlias(const cmd[], const alias[], ...);
cell natives::PC_RegAlias(Script *script, cell *params) {
  script->AssertMinParams(2, params);

  CommandPtr command{};
  for (std::size_t i = 1; i <= params[0] / sizeof(cell); i++) {
    auto cmd_name = script->PrepareCommandName(script->GetString(params[i]));

    if (i == 1) {
      command = script->GetCommand(cmd_name, true);
    } else {
      script->NewCommand(cmd_name, command->GetPublic(), command->GetFlags(),
                         true);
    }
  }

  return 1;
}

// native PC_SetFlags(const cmd[], flags);
cell natives::PC_SetFlags(Script *script, std::string cmd_name, cell flags) {
  cmd_name = script->PrepareCommandName(cmd_name);

  script->GetCommand(cmd_name, true)->SetFlags(flags);

  return 1;
}

// native PC_GetFlags(const cmd[]);
cell natives::PC_GetFlags(Script *script, std::string cmd_name) {
  cmd_name = script->PrepareCommandName(cmd_name);

  return script->GetCommand(cmd_name)->GetFlags();
}

// native PC_RenameCommand(const cmd[], const newname[]);
cell natives::PC_RenameCommand(Script *script, std::string cmd_name,
                               std::string cmd_newname) {
  cmd_name = script->PrepareCommandName(cmd_name);
  cmd_newname = script->PrepareCommandName(cmd_newname);

  const auto &command = script->GetCommand(cmd_name);

  script->NewCommand(cmd_newname, command->GetPublic(), command->GetFlags(),
                     command->IsAlias());

  script->DeleteCommand(cmd_name);

  return 1;
}

// native PC_CommandExists(const cmd[]);
cell natives::PC_CommandExists(Script *script, std::string cmd_name) {
  cmd_name = script->PrepareCommandName(cmd_name);

  return script->CommandExists(cmd_name);
}

// native PC_DeleteCommand(const cmd[]);
cell natives::PC_DeleteCommand(Script *script, std::string cmd_name) {
  cmd_name = script->PrepareCommandName(cmd_name);

  script->GetCommand(cmd_name);

  return script->DeleteCommand(cmd_name);
}

// native CmdArray:PC_GetCommandArray();
cell natives::PC_GetCommandArray(Script *script) {
  return script->NewCmdArray();
}

// native CmdArray:PC_GetAliasArray(const cmd[]);
cell natives::PC_GetAliasArray(Script *script, std::string cmd_name) {
  cmd_name = script->PrepareCommandName(cmd_name);

  return script->NewAliasArray(cmd_name);
}

// native PC_GetArraySize(CmdArray:arr);
cell natives::PC_GetArraySize(Script *script, CmdArrayPtr arr) {
  return arr->size();
}

// native PC_GetCommandName(CmdArray:arr, index, dest[], size = sizeof dest);
cell natives::PC_GetCommandName(Script *script, CmdArrayPtr arr, cell index,
                                cell *dest, cell size) {
  script->SetString(dest, arr->at(index), size);

  return 1;
}

// native PC_FreeArray(&CmdArray:arr);
cell natives::PC_FreeArray(Script *script, cell *arr) {
  script->DeleteArray(*arr);

  *arr = 0;

  return 1;
}

// native PC_EmulateCommand(playerid, const cmdtext[]);
cell natives::PC_EmulateCommand(Script *script, cell playerid,
                                std::string cmdtext) {
  Plugin::ProcessCommand(playerid, cmdtext.c_str());

  return 1;
}
