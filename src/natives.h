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

#ifndef PAWNCMD_NATIVES_H_
#define PAWNCMD_NATIVES_H_

namespace natives {
// native PC_Init();
cell PC_Init(Script *script);

// native PC_RegAlias(const cmd[], const alias[], ...);
cell PC_RegAlias(Script *script, cell *params);

// native PC_SetFlags(const cmd[], flags);
cell PC_SetFlags(Script *script, std::string cmd_name, cell flags);

// native PC_GetFlags(const cmd[]);
cell PC_GetFlags(Script *script, std::string cmd_name);

// native PC_RenameCommand(const cmd[], const newname[]);
cell PC_RenameCommand(Script *script, std::string cmd_name,
                      std::string cmd_newname);

// native PC_CommandExists(const cmd[]);
cell PC_CommandExists(Script *script, std::string cmd_name);

// native PC_DeleteCommand(const cmd[]);
cell PC_DeleteCommand(Script *script, std::string cmd_name);

// native CmdArray:PC_GetCommandArray();
cell PC_GetCommandArray(Script *script);

// native CmdArray:PC_GetAliasArray(const cmd[]);
cell PC_GetAliasArray(Script *script, std::string cmd_name);

// native PC_GetArraySize(CmdArray:arr);
cell PC_GetArraySize(Script *script, CmdArrayPtr arr);

// native PC_GetCommandName(CmdArray:arr, index, dest[], size = sizeof dest);
cell PC_GetCommandName(Script *script, CmdArrayPtr arr, cell index, cell *dest,
                       cell size);

// native PC_FreeArray(&CmdArray:arr);
cell PC_FreeArray(Script *script, cell *arr);

// native PC_EmulateCommand(playerid, const cmdtext[]);
cell PC_EmulateCommand(Script *script, cell playerid, std::string cmdtext);
}  // namespace natives

#endif  // PAWNCMD_NATIVES_H_
