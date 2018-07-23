/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2018 urShadow
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "SDK/amx/amx.h"
#include "SDK/plugincommon.h"

#include "urmem/urmem.hpp"

#include <unordered_map>
#include <unordered_set>
#include <regex>
#include <string>
#include <vector>
#include <list>
#include <queue>

#include "Pawn.CMD.inc"

#ifdef THISCALL
#undef THISCALL
#endif

#ifdef _WIN32
#define THISCALL __thiscall
#else
#define THISCALL
#endif

extern void *pAMXFunctions;

class Plugin {
public:
    static constexpr char
        *kName = "Pawn.CMD",
        *kVersion = "3.1.4",
        *kPublicVarNameVersion = "_pawncmd_version",
        *kPublicVarNameIsGamemode = "_pawncmd_is_gamemode",
#ifdef _WIN32
        *kOpctPattern =
        "\x83\xEC\x08"          /*sub esp,0x8*/ \
        "\x53"                  /*push ebx*/ \
        "\x8B\x5C\x24\x14"      /*mov ebx,DWORD PTR [esp+0x14]*/ \
        "\x55"                  /*push ebp*/ \
        "\x8B\x6C\x24\x14"      /*mov ebp,DWORD PTR [esp+0x14]*/ \
        "\x56"                  /*push esi*/ \
        "\x33\xF6"              /*xor esi,esi*/ \
        "\x57"                  /*push edi*/ \
        "\x8B\xF9"              /*mov edi,ecx*/ \
        "\x89\x74\x24\x10"      /*mov DWORD PTR [esp+0x10],esi*/ \
        "\x8B\x04\xB7"          /*mov eax,DWORD PTR [edi+esi*4]*/ \
        "\x85\xC0",             /*test eax,eax*/
        *kOpctMask = "xxxxxxxxxxxxxxxxxxxxxxxxxxxx";
#else
        *kOpctPattern =
        "\x55"                          /*push ebp*/ \
        "\x89\xE5"                      /*mov ebp,esp*/ \
        "\x57"                          /*push edi*/ \
        "\x56"                          /*push esi*/ \
        "\x53"                          /*push ebx*/ \
        "\x83\xEC\x2C"                  /*sub esp,0x2c*/ \
        "\x8B\x75\x08"                  /*mov esi,DWORD PTR [ebp+0x8]*/ \
        "\xC7\x45\xE4\x00\x00\x00\x00"  /*mov DWORD PTR [ebp-0x1c],0x0*/ \
        "\x8B\x7D\x10"                  /*mov edi,DWORD PTR [ebp+0x10]*/ \
        "\x89\xF3"                      /*mov ebx,esi*/ \
        "\xEB\x14",                     /*jmp 0x2e*/
        *kOpctMask = "xxxxxxxxxxxxxxxxxxxxxxxxxx";
#endif

    struct Command;
    struct Script;

    using logprintf_t = void(*)(const char *format, ...);
    using m = urmem;

    using CommandMap = std::unordered_map<std::string, Command>;
    using ScriptList = std::list<Script>;
    using CmdArray = std::vector<std::string>;
    using CmdArraySet = std::unordered_set<std::shared_ptr<CmdArray>>;

    struct Command {
        cell addr;
        unsigned int flags;
        bool is_alias;
    };

    struct Script {
        Command &GetCommand(const std::string &name) {
            const auto iter = cmds.find(name);

            if (iter == cmds.end()) {
                throw std::runtime_error{"command '" + name + "' not found"};
            }

            return iter->second;
        }

        AMX *amx;
        cell opct_addr, opcr_addr, opcp_addr, on_init_addr;
        std::deque<cell> init_flags_and_aliases_addresses;
        CommandMap cmds;
    };

    static bool Load(void **ppData) {
        pAMXFunctions = ppData[PLUGIN_DATA_AMX_EXPORTS];

        _logprintf = reinterpret_cast<logprintf_t>(ppData[PLUGIN_DATA_LOGPRINTF]);

        m::sig_scanner scanner;
        if (scanner.init(reinterpret_cast<void *>(_logprintf))) {
            m::address_t addr{};
            if (scanner.find(kOpctPattern, kOpctMask, addr)) {
                _hook_fs__on_player_command_text = std::make_shared<m::hook>(
                    addr,
                    m::get_func_addr(&HOOK_CFilterScripts__OnPlayerCommandText)
                );

                _logprintf("%s plugin v%s by urShadow has been loaded", kName, kVersion);

                return true;
            }
        }

        _logprintf("[%s] %s: CFilterScripts::OnPlayerCommandText not found", kName, __FUNCTION__);

        return false;
    }

    static void Unload() {
        _hook_fs__on_player_command_text.reset();
        _scripts.clear();
        _cmd_array_set.clear();

        _logprintf("%s plugin v%s by urShadow has been unloaded", kName, kVersion);
    }

    static void AmxLoad(AMX *amx) {
        const std::vector<AMX_NATIVE_INFO> natives{
            {"PC_Init", &n_PC_Init},

            {"PC_RegAlias", &n_PC_RegAlias},
            {"PC_SetFlags", &n_PC_SetFlags},
            {"PC_GetFlags", &n_PC_GetFlags},
            {"PC_EmulateCommand", &n_PC_EmulateCommand},
            {"PC_RenameCommand", &n_PC_RenameCommand},
            {"PC_CommandExists", &n_PC_CommandExists},
            {"PC_DeleteCommand", &n_PC_DeleteCommand},

            {"PC_GetCommandArray", &n_PC_GetCommandArray},
            {"PC_GetAliasArray", &n_PC_GetAliasArray},
            {"PC_GetArraySize", &n_PC_GetArraySize},
            {"PC_FreeArray", &n_PC_FreeArray},
            {"PC_GetCommandName", &n_PC_GetCommandName},
        };

        cell include_version{}, is_gamemode{};
        if (
            GetAmxPublicVar(amx, kPublicVarNameVersion, include_version)
            && GetAmxPublicVar(amx, kPublicVarNameIsGamemode, is_gamemode)
        ) {
            if (include_version != PAWNCMD_INCLUDE_VERSION) {
                _logprintf("[%s] %s: mismatch between the plugin and include versions", kName, __FUNCTION__);
            }
        }

        amx_Register(amx, natives.data(), natives.size());

        int num_publics{};
        amx_NumPublics(amx, &num_publics);
        if (!num_publics) {
            return;
        }

        Script script{};
        script.amx = amx;

        for (int index{}; index < num_publics; ++index) {
            std::string public_name = GetAmxPublicName(amx, index);
            std::smatch match;
            if (std::regex_match(public_name, match, _regex_public_cmd_name)) {
                auto cmd_name = match[1].str();
                StrToLower(cmd_name);

                Command command{};
                command.addr = GetAmxPublicAddr(amx, index);

                script.cmds[cmd_name] = command;
            } else if (std::regex_match(public_name, _regex_public_cmd_alias)) {
                script.init_flags_and_aliases_addresses.push_back(GetAmxPublicAddr(amx, index));
            } else if (std::regex_match(public_name, _regex_public_cmd_flags)) {
                script.init_flags_and_aliases_addresses.push_front(GetAmxPublicAddr(amx, index));
            } else if (public_name == "OnPlayerCommandText") {
                script.opct_addr = GetAmxPublicAddr(amx, index);
            } else if (public_name == "OnPlayerCommandReceived") {
                script.opcr_addr = GetAmxPublicAddr(amx, index);
            } else if (public_name == "OnPlayerCommandPerformed") {
                script.opcp_addr = GetAmxPublicAddr(amx, index);
            } else if (public_name == "PC_OnInit") {
                script.on_init_addr = GetAmxPublicAddr(amx, index);
            }
        }

        is_gamemode ? _scripts.push_back(script) : _scripts.push_front(script);
    }

    static void AmxUnload(AMX *amx) {
        _scripts.remove_if([amx](const ScriptList::value_type &script) {
            return script.amx == amx;
        });
    }

private:
    // native PC_Init();
    static cell AMX_NATIVE_CALL n_PC_Init(AMX *amx, cell *params) {
        try {
            const auto &script = GetScript(amx);

            for (auto addr : script.init_flags_and_aliases_addresses) {
                ExecAmxPublic(amx, nullptr, addr);
            }

            if (script.on_init_addr) {
                ExecAmxPublic(amx, nullptr, script.on_init_addr);
            }
        } catch (const std::exception &e) {
            _logprintf("[%s] %s: %s", kName, __FUNCTION__, e.what());

            return 0;
        }

        return 1;
    }

    // native PC_RegAlias(const cmd[], const alias[], ...);
    static cell AMX_NATIVE_CALL n_PC_RegAlias(AMX *amx, cell *params) {
        try {
            if (params[0] < (2 * sizeof(cell))) {
                throw std::runtime_error{"number of parameters must not be less than 2"};
            }

            auto &script = GetScript(amx);

            Command command{};
            for (size_t i = 1; i <= params[0] / sizeof(cell); ++i) {
                auto cmd_name = GetAmxString(amx, params[i]);
                StrToLower(cmd_name);

                if (i == 1) {
                    command = script.GetCommand(cmd_name);

                    if (command.is_alias) {
                        throw std::runtime_error{"command '" + cmd_name + "' is an alias"};
                    }

                    command.is_alias = true;
                } else {
                    if (script.cmds.count(cmd_name)) {
                        throw std::runtime_error{"alias '" + cmd_name + "' is occupied"};
                    }

                    script.cmds[cmd_name] = command;
                }
            }
        } catch (const std::exception &e) {
            _logprintf("[%s] %s: %s", kName, __FUNCTION__, e.what());

            return 0;
        }

        return 1;
    }

    // native PC_SetFlags(const cmd[], flags);
    static cell AMX_NATIVE_CALL n_PC_SetFlags(AMX *amx, cell *params) {
        try {
            AssertParams(2, params);

            auto &script = GetScript(amx);

            auto cmd_name = GetAmxString(amx, params[1]);
            StrToLower(cmd_name);

            auto &command = script.GetCommand(cmd_name);

            if (command.is_alias) {
                throw std::runtime_error{"command '" + cmd_name + "' is an alias"};
            }

            command.flags = params[2];
        } catch (const std::exception &e) {
            _logprintf("[%s] %s: %s", kName, __FUNCTION__, e.what());

            return 0;
        }

        return 1;
    }

    // native PC_GetFlags(const cmd[]);
    static cell AMX_NATIVE_CALL n_PC_GetFlags(AMX *amx, cell *params) {
        try {
            AssertParams(1, params);

            auto &script = GetScript(amx);

            auto cmd_name = GetAmxString(amx, params[1]);
            StrToLower(cmd_name);

            return script.GetCommand(cmd_name).flags;
        } catch (const std::exception &e) {
            _logprintf("[%s] %s: %s", kName, __FUNCTION__, e.what());
        }

        return 0;
    }

    // native PC_EmulateCommand(playerid, const cmdtext[]);
    static cell AMX_NATIVE_CALL n_PC_EmulateCommand(AMX *amx, cell *params) {
        try {
            AssertParams(2, params);

            ProcessCommand(params[1], GetAmxString(amx, params[2]).c_str());
        } catch (const std::exception &e) {
            _logprintf("[%s] %s: %s", kName, __FUNCTION__, e.what());

            return 0;
        }

        return 1;
    }

    // native PC_RenameCommand(const cmd[], const newname[]);
    static cell AMX_NATIVE_CALL n_PC_RenameCommand(AMX *amx, cell *params) {
        try {
            AssertParams(2, params);

            auto &script = GetScript(amx);

            auto cmd_name = GetAmxString(amx, params[1]);
            StrToLower(cmd_name);

            auto cmd_newname = GetAmxString(amx, params[2]);
            StrToLower(cmd_newname);

            auto &command = script.GetCommand(cmd_name);

            if (script.cmds.count(cmd_newname)) {
                throw std::runtime_error{"name '" + cmd_newname + "' is occupied"};
            }

            script.cmds[cmd_newname] = command;

            script.cmds.erase(cmd_name);
        } catch (const std::exception &e) {
            _logprintf("[%s] %s: %s", kName, __FUNCTION__, e.what());

            return 0;
        }

        return 1;
    }

    // native PC_CommandExists(const cmd[]);
    static cell AMX_NATIVE_CALL n_PC_CommandExists(AMX *amx, cell *params) {
        try {
            AssertParams(1, params);

            const auto &script = GetScript(amx);

            auto cmd_name = GetAmxString(amx, params[1]);
            StrToLower(cmd_name);

            return script.cmds.count(cmd_name);
        } catch (const std::exception &e) {
            _logprintf("[%s] %s: %s", kName, __FUNCTION__, e.what());
        }

        return 0;
    }

    // native PC_DeleteCommand(const cmd[]);
    static cell AMX_NATIVE_CALL n_PC_DeleteCommand(AMX *amx, cell *params) {
        try {
            AssertParams(1, params);

            auto &script = GetScript(amx);

            auto cmd_name = GetAmxString(amx, params[1]);
            StrToLower(cmd_name);

            script.GetCommand(cmd_name);

            return script.cmds.erase(cmd_name);
        } catch (const std::exception &e) {
            _logprintf("[%s] %s: %s", kName, __FUNCTION__, e.what());
        }

        return 0;
    }

    // native CmdArray:PC_GetCommandArray();
    static cell AMX_NATIVE_CALL n_PC_GetCommandArray(AMX *amx, cell *params) {
        try {
            const auto &script = GetScript(amx);

            const auto cmd_array = std::make_shared<CmdArray>();

            for (const auto &cmd : script.cmds) {
                if (cmd.second.is_alias) {
                    continue;
                }

                cmd_array->push_back(cmd.first);
            }

            _cmd_array_set.insert(cmd_array);

            return reinterpret_cast<cell>(cmd_array.get());
        } catch (const std::exception &e) {
            _logprintf("[%s] %s: %s", kName, __FUNCTION__, e.what());
        }

        return 0;
    }

    // native CmdArray:PC_GetAliasArray(const cmd[]);
    static cell AMX_NATIVE_CALL n_PC_GetAliasArray(AMX *amx, cell *params) {
        try {
            AssertParams(1, params);

            auto &script = GetScript(amx);

            auto cmd_name = GetAmxString(amx, params[1]);
            StrToLower(cmd_name);

            auto &command = script.GetCommand(cmd_name);

            if (command.is_alias) {
                throw std::runtime_error{"command '" + cmd_name + "' is an alias"};
            }

            const auto cmd_array = std::make_shared<CmdArray>();

            for (const auto &alias : script.cmds) {
                if (
                    alias.second.addr != command.addr
                    || !alias.second.is_alias
                ) {
                    continue;
                }

                cmd_array->push_back(alias.first);
            }

            _cmd_array_set.insert(cmd_array);

            return reinterpret_cast<cell>(cmd_array.get());
        } catch (const std::exception &e) {
            _logprintf("[%s] %s: %s", kName, __FUNCTION__, e.what());
        }

        return 0;
    }

    // native PC_GetArraySize(CmdArray:arr);
    static cell AMX_NATIVE_CALL n_PC_GetArraySize(AMX *amx, cell *params) {
        try {
            AssertParams(1, params);

            return GetCmdArray(params[1])->size();
        } catch (const std::exception &e) {
            _logprintf("[%s] %s: %s", kName, __FUNCTION__, e.what());
        }

        return 0;
    }

    // native PC_FreeArray(&CmdArray:arr);
    static cell AMX_NATIVE_CALL n_PC_FreeArray(AMX *amx, cell *params) {
        try {
            AssertParams(1, params);

            cell *cptr{};

            if (amx_GetAddr(amx, params[1], &cptr) != AMX_ERR_NONE) {
                throw std::runtime_error{"invalid param reference"};
            }

            _cmd_array_set.erase(GetCmdArray(*cptr));

            *cptr = 0;

            return 1;
        } catch (const std::exception &e) {
            _logprintf("[%s] %s: %s", kName, __FUNCTION__, e.what());
        }

        return 0;
    }

    // native PC_GetCommandName(CmdArray:arr, index, name[], size = sizeof name);
    static cell AMX_NATIVE_CALL n_PC_GetCommandName(AMX *amx, cell *params) {
        try {
            AssertParams(4, params);

            const auto cmd_array = GetCmdArray(params[1]);

            const auto index = static_cast<size_t>(params[2]);

            SetAmxString(amx, params[3], cmd_array->at(index).c_str(), params[4]);

            return 1;
        } catch (const std::exception &e) {
            _logprintf("[%s] %s: %s", kName, __FUNCTION__, e.what());
        }

        return 0;
    }

    static int THISCALL HOOK_CFilterScripts__OnPlayerCommandText(void *_this, cell playerid, const char *cmdtext) {
        ProcessCommand(playerid, cmdtext);

        return 1;
    }

    static void ProcessCommand(cell playerid, const char *cmdtext) {
        if (
            !cmdtext
            || cmdtext[0] != '/'
        ) {
            return;
        }

        char cmd[32]{};
        const char *params{};

        int i = 1;
        while (cmdtext[i] == ' ') {
            i++;
        } // remove extra spaces before cmd name
        params = &cmdtext[i];

        i = 0;

        char symbol{};
        while (
            (symbol = params[i])
            && symbol != ' '
        ) {
            if (i >= sizeof(cmd)) {
                return;
            }

            cmd[i++] = std::tolower(symbol, _locale);
        }

        while (params[i] == ' ') {
            i++;
        } // remove extra spaces before params

        params = &params[i];

        CommandMap::const_iterator iter_cmd{};
        cell addr_cmdtext{}, addr_cmd_name{}, addr_params{}, retval{}, flags{};
        bool command_exists{};
        for (const auto &script : _scripts) {
            if (script.opct_addr) {
                amx_PushString(script.amx, &addr_cmdtext, nullptr, cmdtext, 0, 0);
                amx_Push(script.amx, playerid);
                ExecAmxPublic(script.amx, &retval, script.opct_addr);
                amx_Release(script.amx, addr_cmdtext);

                if (retval == 1) {
                    break;
                }
            }

            if (command_exists = ((iter_cmd = script.cmds.find(cmd)) != script.cmds.end())) {
                flags = iter_cmd->second.flags;
            }

            if (script.opcr_addr) {
                amx_Push(script.amx, flags);
                amx_PushString(script.amx, &addr_params, nullptr, params, 0, 0);
                amx_PushString(script.amx, &addr_cmd_name, nullptr, cmd, 0, 0);
                amx_Push(script.amx, playerid);
                ExecAmxPublic(script.amx, &retval, script.opcr_addr);
                amx_Release(script.amx, addr_cmd_name);
                amx_Release(script.amx, addr_params);

                if (!retval) {
                    continue;
                }
            }

            if (command_exists) {
                amx_PushString(script.amx, &addr_params, nullptr, params, 0, 0);
                amx_Push(script.amx, playerid);
                ExecAmxPublic(script.amx, &retval, iter_cmd->second.addr);
                amx_Release(script.amx, addr_params);
            } else {
                retval = -1;
            }

            if (script.opcp_addr) {
                amx_Push(script.amx, flags);
                amx_Push(script.amx, retval);
                amx_PushString(script.amx, &addr_params, nullptr, params, 0, 0);
                amx_PushString(script.amx, &addr_cmd_name, nullptr, cmd, 0, 0);
                amx_Push(script.amx, playerid);
                ExecAmxPublic(script.amx, &retval, script.opcp_addr);
                amx_Release(script.amx, addr_cmd_name);
                amx_Release(script.amx, addr_params);
            }

            if (retval == 1) {
                break;
            }
        }
    }

    static inline CmdArraySet::value_type GetCmdArray(cell ptr) {
        const auto iter = std::find_if(_cmd_array_set.begin(), _cmd_array_set.end(), [ptr](const CmdArraySet::value_type &p) {
            return reinterpret_cast<cell>(p.get()) == ptr;
        });

        if (iter == _cmd_array_set.end()) {
            throw std::runtime_error{"invalid array handle"};
        }

        return *iter;
    }

    static inline ScriptList::value_type &GetScript(AMX *amx) {
        const auto iter = std::find_if(_scripts.begin(), _scripts.end(), [amx](const ScriptList::value_type &script) {
            return script.amx == amx;
        });

        if (iter == _scripts.end()) {
            throw std::runtime_error{"amx not found"};
        }

        return *iter;
    }

    static inline void StrToLower(std::string &str) {
        for (auto &c : str) {
            c = std::tolower(c, _locale);
        }
    }

    static inline int SetAmxString(AMX *amx, cell amx_addr, const char *source, int max) {
        cell *dest = reinterpret_cast<cell *>(amx->base + static_cast<int>(reinterpret_cast<AMX_HEADER *>(amx->base)->dat + amx_addr));
        cell *start = dest;

        while (max-- && *source) {
            *dest++ = static_cast<cell>(*source++);
        }
        *dest = 0;

        return dest - start;
    }

    static inline std::string GetAmxString(AMX *amx, cell amx_addr) {
        int len{};
        cell *addr{};

        if (
            amx_GetAddr(amx, amx_addr, &addr) == AMX_ERR_NONE
            && amx_StrLen(addr, &len) == AMX_ERR_NONE
            && len
        ) {
            len++;

            std::unique_ptr<char[]> buf{new char[len]{}};

            if (
                buf
                && amx_GetString(buf.get(), addr, 0, len) == AMX_ERR_NONE
            ) {
                return buf.get();
            }
        }

        return {};
    }

    static inline bool GetAmxPublicVar(AMX *amx, const char *name, cell &out) {
        cell addr{}, *phys_addr{};

        if (
            amx_FindPubVar(amx, name, &addr) == AMX_ERR_NONE
            && amx_GetAddr(amx, addr, &phys_addr) == AMX_ERR_NONE
        ) {
            out = *phys_addr;

            return true;
        }

        return false;
    }

    static inline int ExecAmxPublic(AMX *amx, cell *retval, cell addr) {
        const auto hdr = reinterpret_cast<AMX_HEADER *>(amx->base);
        const auto cip = hdr->cip;

        hdr->cip = addr;
        int result = amx_Exec(amx, retval, AMX_EXEC_MAIN);
        hdr->cip = cip;

        return result;
    }

    static inline cell GetAmxPublicAddr(AMX *amx, int index) {
        const auto hdr = reinterpret_cast<AMX_HEADER *>(amx->base);
        const auto func = reinterpret_cast<AMX_FUNCSTUB *>(
            reinterpret_cast<unsigned char *>(hdr)
            + static_cast<size_t>(hdr->publics)
            + static_cast<size_t>(index) * hdr->defsize
        );

        return func->address;
    }

    static inline std::string GetAmxPublicName(AMX *amx, int index) {
        char public_name[32]{};

        amx_GetPublic(amx, index, public_name);

        return public_name;
    }

    static inline void AssertParams(int count, cell *params) {
        if (params[0] != (count * sizeof(cell))) {
            throw std::runtime_error{"number of parameters must be equal to " + std::to_string(count)};
        }
    }

    static logprintf_t _logprintf;
    static ScriptList _scripts;
    static CmdArraySet _cmd_array_set;
    static std::shared_ptr<m::hook> _hook_fs__on_player_command_text;
    static std::locale _locale;
    static std::regex
        _regex_public_cmd_name,
        _regex_public_cmd_alias,
        _regex_public_cmd_flags;
};

Plugin::logprintf_t Plugin::_logprintf;
Plugin::ScriptList Plugin::_scripts;
Plugin::CmdArraySet Plugin::_cmd_array_set;
std::shared_ptr<Plugin::m::hook> Plugin::_hook_fs__on_player_command_text;
std::locale Plugin::_locale;
std::regex
    Plugin::_regex_public_cmd_name{R"(pc_cmd_(\w+))"},
    Plugin::_regex_public_cmd_alias{R"(pc_alias_\w+)"},
    Plugin::_regex_public_cmd_flags{R"(pc_flags_\w+)"};

PLUGIN_EXPORT unsigned int PLUGIN_CALL Supports() {
    return SUPPORTS_VERSION | SUPPORTS_AMX_NATIVES;
}

PLUGIN_EXPORT bool PLUGIN_CALL Load(void **ppData) {
    return Plugin::Load(ppData);
}

PLUGIN_EXPORT void PLUGIN_CALL Unload() {
    Plugin::Unload();
}

PLUGIN_EXPORT void PLUGIN_CALL AmxLoad(AMX *amx) {
    Plugin::AmxLoad(amx);
}

PLUGIN_EXPORT void PLUGIN_CALL AmxUnload(AMX *amx) {
    Plugin::AmxUnload(amx);
}
