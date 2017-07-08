/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 urShadow
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

#include "urmem.hpp"

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

const char *pattern =
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
"\x85\xC0";             /*test eax,eax*/

const char *mask = "xxxxxxxxxxxxxxxxxxxxxxxxxxxx";
#else
#define THISCALL

const char *pattern =
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
"\xEB\x14";                     /*jmp 0x2e*/

const char *mask = "xxxxxxxxxxxxxxxxxxxxxxxxxx";
#endif

using m = urmem;
using logprintf_t = void(*)(char *format, ...);

logprintf_t logprintf;

extern void *pAMXFunctions;

class Plugin {
public:
	static constexpr char
		*kName = "Pawn.CMD",
		*kVersion = "3.1.2",
		*kPublicVarNameVersion = "_pawncmd_version",
		*kPublicVarNameIsGamemode = "_pawncmd_is_gamemode";

	struct AmxListItem;
	struct AmxQueueItem;
	struct CommandMapItem;

	using CommandMap = std::unordered_map<std::string, CommandMapItem>;
	using AmxList = std::list<AmxListItem>;
	using AmxQueue = std::queue<AmxQueueItem>;
	using CmdArray = std::vector<std::string>;
	using CmdArraySet = std::unordered_set<std::shared_ptr<CmdArray>>;

	struct CommandMapItem {
		int public_id;
		unsigned int flags;
		bool is_alias;
	};

	struct AmxQueueItem {
		AMX *amx;
		bool is_gamemode;
	};

	struct AmxListItem {
		AMX *amx;

		struct {
			bool exists;
			int id;
		}	public_on_player_command_received,
			public_on_player_command_performed,
			public_on_init;

		CommandMap cmd_map;
	};

	static bool Load(void) {
		m::sig_scanner scanner;

		if (scanner.init(reinterpret_cast<void *>(logprintf))) {
			m::address_t addr{};

			if (scanner.find(pattern, mask, addr)) {
				_hook_fs__on_player_command_text = std::make_shared<m::hook>(
					addr,
					m::get_func_addr(&HOOK_CFilterScripts__OnPlayerCommandText));

				logprintf("%s plugin v%s by urShadow loaded", kName, kVersion);

				return true;
			}
		}

		logprintf("[%s] %s: address not found", kName, __FUNCTION__);

		return false;
	}

	static void Unload(void) {
		_hook_fs__on_player_command_text.reset();

		_amx_list.clear();

		while (!_amx_init_queue.empty()) {
			_amx_init_queue.pop();
		}

		_cmd_array_set.clear();

		logprintf("%s plugin v%s by urShadow unloaded", kName, kVersion);
	}

	static void AmxLoad(AMX *amx) {
		static const std::vector<AMX_NATIVE_INFO> native_vec =
		{
			{ "PC_RegAlias", &n_PC_RegAlias },
			{ "PC_SetFlags", &n_PC_SetFlags },
			{ "PC_GetFlags", &n_PC_GetFlags },
			{ "PC_EmulateCommand", &n_PC_EmulateCommand },
			{ "PC_RenameCommand", &n_PC_RenameCommand },
			{ "PC_CommandExists", &n_PC_CommandExists },
			{ "PC_DeleteCommand", &n_PC_DeleteCommand },

			{ "PC_GetCommandArray", &n_PC_GetCommandArray },
			{ "PC_GetAliasArray", &n_PC_GetAliasArray },
			{ "PC_GetArraySize", &n_PC_GetArraySize },
			{ "PC_FreeArray", &n_PC_FreeArray },
			{ "PC_GetCommandName", &n_PC_GetCommandName },
		};

		cell include_version{}, is_gamemode{};

		if (get_public_var(amx, kPublicVarNameVersion, include_version) &&
			get_public_var(amx, kPublicVarNameIsGamemode, is_gamemode)) {
			if (include_version != PAWNCMD_INCLUDE_VERSION) {
				return logprintf("[%s] %s: .inc-file version does not equal the plugin's version", kName, __FUNCTION__);
			}

			_amx_init_queue.emplace(AmxQueueItem{ amx, is_gamemode == 1 });
		}

		amx_Register(amx, native_vec.data(), native_vec.size());
	}

	static void AmxUnload(AMX *amx) {
		_amx_list.remove_if([amx](const AmxList::value_type &item) {
			return item.amx == amx;
		});
	}

	static void ProcessTick(void) {
		while (!_amx_init_queue.empty()) {
			const auto &queue_item = _amx_init_queue.front();

			const auto amx = queue_item.amx;

			const bool is_gamemode = queue_item.is_gamemode;

			int num_publics{};

			amx_NumPublics(amx, &num_publics);

			if (!num_publics) {
				continue;
			}

			std::deque<int> alias_and_flags_public_ids;

			AmxListItem list_item{ amx };

			for (int i{}; i < num_publics; i++) {
				char public_name[32]{};

				amx_GetPublic(amx, i, public_name);

				std::string s = public_name;

				std::smatch m;

				if (std::regex_match(s, m, _regex_public_cmd_name)) {
					auto cmd_name = m[1].str();

					str_to_lower(cmd_name);

					list_item.cmd_map.emplace(std::move(cmd_name), CommandMapItem{ i, 0, false });
				} else if (std::regex_match(s, _regex_public_cmd_alias)) {
					alias_and_flags_public_ids.push_back(i);
				} else if (std::regex_match(s, _regex_public_cmd_flags)) {
					alias_and_flags_public_ids.push_front(i);
				} else if (s == "OnPlayerCommandReceived") {
					list_item.public_on_player_command_received = { true, i };
				} else if (s == "OnPlayerCommandPerformed") {
					list_item.public_on_player_command_performed = { true, i };
				} else if (s == "PC_OnInit") {
					list_item.public_on_init = { true, i };
				}
			}

			if (is_gamemode) {
				_amx_list.push_back(list_item);
			} else {
				_amx_list.push_front(list_item);
			}

			for (const int id : alias_and_flags_public_ids) {
				amx_Exec(amx, nullptr, id);
			}

			if (list_item.public_on_init.exists) {
				amx_Exec(amx, nullptr, list_item.public_on_init.id);
			}

			_amx_init_queue.pop();
		}
	}

private:

	// native PC_RegAlias(const cmd[], const alias[], ...);
	static cell AMX_NATIVE_CALL n_PC_RegAlias(AMX *amx, cell *params) {
		if (params[0] < (2 * sizeof(cell))) {
			return 0;
		}

		const auto iter_amx = std::find_if(_amx_list.begin(), _amx_list.end(), [amx](const AmxList::value_type &item) {
			return item.amx == amx;
		});

		if (iter_amx == _amx_list.end()) {
			logprintf("[%s] %s: amx not found", kName, __FUNCTION__);

			return 0;
		}

		CommandMap::iterator iter_command{};

		int original_id{};
		unsigned int original_flags{};
		std::unique_ptr<char[]> cmdstr;

		for (size_t i = 1; i <= params[0] / sizeof(cell); i++) {
			cmdstr.reset(get_string(amx, params[i]));

			if (!cmdstr) {
				logprintf("[%s] %s: invalid str", kName, __FUNCTION__);

				continue;
			}

			std::string s(cmdstr.get());

			str_to_lower(s);

			if (i == 1) {
				if ((iter_command = iter_amx->cmd_map.find(s)) == iter_amx->cmd_map.end()) {
					logprintf("[%s] %s: command '%s' not found", kName, __FUNCTION__, s.c_str());

					return 0;
				}

				if (iter_command->second.is_alias) {
					logprintf("[%s] %s: command '%s' is an alias", kName, __FUNCTION__, s.c_str());

					return 0;
				}

				original_id = iter_command->second.public_id;
				original_flags = iter_command->second.flags;
			} else {
				if (iter_amx->cmd_map.find(s) != iter_amx->cmd_map.end()) {
					logprintf("[%s] %s: alias '%s' is occupied", kName, __FUNCTION__, s.c_str());

					continue;
				}

				iter_amx->cmd_map.emplace(std::move(s), CommandMapItem{ original_id, original_flags, true });
			}
		}

		return 1;
	}

	// native PC_SetFlags(const cmd[], flags);
	static cell AMX_NATIVE_CALL n_PC_SetFlags(AMX *amx, cell *params) {
		if (!check_params(__FUNCTION__, 2, params)) {
			return 0;
		}

		const auto iter_amx = std::find_if(_amx_list.begin(), _amx_list.end(), [amx](const AmxList::value_type &item) {
			return item.amx == amx;
		});

		if (iter_amx == _amx_list.end()) {
			logprintf("[%s] %s: amx not found", kName, __FUNCTION__);

			return 0;
		}

		std::unique_ptr<char[]>	cmd(get_string(amx, params[1]));

		if (!cmd) {
			logprintf("[%s] %s: invalid cmd", kName, __FUNCTION__);

			return 0;
		}

		std::string s(cmd.get());

		str_to_lower(s);

		const auto iter_cmd = iter_amx->cmd_map.find(s);

		if (iter_cmd == iter_amx->cmd_map.end()) {
			logprintf("[%s] %s: cmd '%s' not found", kName, __FUNCTION__, s.c_str());

			return 0;
		}

		iter_cmd->second.flags = static_cast<unsigned int>(params[2]);

		return 1;
	}

	// native PC_GetFlags(const cmd[]);
	static cell AMX_NATIVE_CALL n_PC_GetFlags(AMX *amx, cell *params) {
		if (!check_params(__FUNCTION__, 1, params)) {
			return 0;
		}

		const auto iter_amx = std::find_if(_amx_list.begin(), _amx_list.end(), [amx](const AmxList::value_type &item) {
			return item.amx == amx;
		});

		if (iter_amx == _amx_list.end()) {
			logprintf("[%s] %s: amx not found", kName, __FUNCTION__);

			return 0;
		}

		const std::unique_ptr<char[]> cmd(get_string(amx, params[1]));

		if (!cmd) {
			logprintf("[%s] %s: invalid cmd", kName, __FUNCTION__);

			return 0;
		}

		std::string s(cmd.get());

		str_to_lower(s);

		const auto iter_cmd = iter_amx->cmd_map.find(s);

		if (iter_cmd == iter_amx->cmd_map.end()) {
			logprintf("[%s] %s: cmd '%s' not found", kName, __FUNCTION__, s.c_str());

			return 0;
		}

		return static_cast<cell>(iter_cmd->second.flags);
	}

	// native PC_EmulateCommand(playerid, const cmdtext[]);
	static cell AMX_NATIVE_CALL n_PC_EmulateCommand(AMX *amx, cell *params) {
		if (!check_params(__FUNCTION__, 2, params)) {
			return 0;
		}

		const std::unique_ptr<char[]> cmdtext(get_string(amx, params[2]));

		if (!cmdtext) {
			logprintf("[%s] %s: invalid str", kName, __FUNCTION__);

			return 0;
		}

		ProcessCommand(params[1], cmdtext.get());

		return 1;
	}

	// native PC_RenameCommand(const cmd[], const newname[]);
	static cell AMX_NATIVE_CALL n_PC_RenameCommand(AMX *amx, cell *params) {
		if (!check_params(__FUNCTION__, 2, params)) {
			return 0;
		}

		const auto iter_amx = std::find_if(_amx_list.begin(), _amx_list.end(), [amx](const AmxList::value_type &item) {
			return item.amx == amx;
		});

		if (iter_amx == _amx_list.end()) {
			logprintf("[%s] %s: amx not found", kName, __FUNCTION__);

			return 0;
		}

		const std::unique_ptr<char[]>
			cmd(get_string(amx, params[1])),
			newname(get_string(amx, params[2]));

		if ((!cmd) || (!newname)) {
			logprintf("[%s] %s: invalid name or newname", kName, __FUNCTION__);

			return 0;
		}

		std::string
			s_cmd(cmd.get()),
			s_newname(newname.get());

		str_to_lower(s_cmd);

		str_to_lower(s_newname);

		const auto iter_cmd = iter_amx->cmd_map.find(s_cmd);

		if (iter_cmd == iter_amx->cmd_map.end()) {
			logprintf("[%s] %s: cmd '%s' not found", kName, __FUNCTION__, s_cmd.c_str());

			return 0;
		}

		if (iter_amx->cmd_map.find(s_newname) != iter_amx->cmd_map.end()) {
			logprintf("[%s] %s: name '%s' is occupied", kName, __FUNCTION__, s_newname.c_str());

			return 0;
		}

		const auto command_info = iter_cmd->second;

		iter_amx->cmd_map.erase(iter_cmd);

		iter_amx->cmd_map.emplace(std::move(s_newname), command_info);

		return 1;
	}

	// native PC_CommandExists(const cmd[]);
	static cell AMX_NATIVE_CALL n_PC_CommandExists(AMX *amx, cell *params) {
		if (!check_params(__FUNCTION__, 1, params)) {
			return 0;
		}

		const auto iter_amx = std::find_if(_amx_list.begin(), _amx_list.end(), [amx](const AmxList::value_type &item) {
			return item.amx == amx;
		});

		if (iter_amx == _amx_list.end()) {
			logprintf("[%s] %s: amx not found", kName, __FUNCTION__);

			return 0;
		}

		const std::unique_ptr<char[]> cmd(get_string(amx, params[1]));

		if (!cmd) {
			logprintf("[%s] %s: invalid cmd name", kName, __FUNCTION__);

			return 0;
		}


		std::string s(cmd.get());

		str_to_lower(s);

		if (iter_amx->cmd_map.find(s) == iter_amx->cmd_map.end()) {
			return 0;
		}

		return 1;
	}

	// native PC_DeleteCommand(const cmd[]);
	static cell AMX_NATIVE_CALL n_PC_DeleteCommand(AMX *amx, cell *params) {
		if (!check_params(__FUNCTION__, 1, params)) {
			return 0;
		}

		const auto iter_amx = std::find_if(_amx_list.begin(), _amx_list.end(), [amx](const AmxList::value_type &item) {
			return item.amx == amx;
		});

		if (iter_amx == _amx_list.end()) {
			logprintf("[%s] %s: amx not found", kName, __FUNCTION__);

			return 0;
		}

		const std::unique_ptr<char[]> cmd(get_string(amx, params[1]));

		if (!cmd) {
			logprintf("[%s] %s: invalid name", kName, __FUNCTION__);

			return 0;
		}

		std::string s(cmd.get());

		str_to_lower(s);

		const auto iter_cmd = iter_amx->cmd_map.find(s);

		if (iter_cmd == iter_amx->cmd_map.end()) {
			logprintf("[%s] %s: cmd '%s' not found", kName, __FUNCTION__, s.c_str());

			return 0;
		}

		iter_amx->cmd_map.erase(iter_cmd);

		return 1;
	}

	// native CmdArray:PC_GetCommandArray();
	static cell AMX_NATIVE_CALL n_PC_GetCommandArray(AMX *amx, cell *params) {
		if (!check_params(__FUNCTION__, 0, params)) {
			return 0;
		}

		const auto iter_amx = std::find_if(_amx_list.begin(), _amx_list.end(), [amx](const AmxList::value_type &item) {
			return item.amx == amx;
		});

		if (iter_amx == _amx_list.end()) {
			logprintf("[%s] %s: amx not found", kName, __FUNCTION__);

			return 0;
		}

		const auto cmd_array = std::make_shared<CmdArray>();

		for (const auto &cmd : iter_amx->cmd_map) {
			if (cmd.second.is_alias) {
				continue;
			}

			cmd_array->push_back(cmd.first);
		}

		_cmd_array_set.insert(cmd_array);

		return reinterpret_cast<cell>(cmd_array.get());
	}

	// native CmdArray:PC_GetAliasArray(const cmd[]);
	static cell AMX_NATIVE_CALL n_PC_GetAliasArray(AMX *amx, cell *params) {
		if (!check_params(__FUNCTION__, 1, params)) {
			return 0;
		}

		const auto iter_amx = std::find_if(_amx_list.begin(), _amx_list.end(), [amx](const AmxList::value_type &item) {
			return item.amx == amx;
		});

		if (iter_amx == _amx_list.end()) {
			logprintf("[%s] %s: amx not found", kName, __FUNCTION__);

			return 0;
		}

		const std::unique_ptr<char[]> cmd(get_string(amx, params[1]));

		if (!cmd) {
			logprintf("[%s] %s: invalid cmd name", kName, __FUNCTION__);

			return 0;
		}

		std::string s(cmd.get());

		str_to_lower(s);

		const auto iter_cmd = iter_amx->cmd_map.find(s);

		if (iter_cmd == iter_amx->cmd_map.end()) {
			logprintf("[%s] %s: cmd '%s' not found", kName, __FUNCTION__, s.c_str());

			return 0;
		}

		if (iter_cmd->second.is_alias) {
			logprintf("[%s] %s: cmd '%s' is an alias", kName, __FUNCTION__, s.c_str());

			return 0;
		}

		const int public_id = iter_cmd->second.public_id;

		const auto cmd_array = std::make_shared<CmdArray>();

		for (const auto &cmd : iter_amx->cmd_map) {
			if ((cmd.second.public_id != public_id) || !cmd.second.is_alias) {
				continue;
			}

			cmd_array->push_back(cmd.first);
		}

		_cmd_array_set.insert(cmd_array);

		return reinterpret_cast<cell>(cmd_array.get());
	}

	// native PC_GetArraySize(CmdArray:arr);
	static cell AMX_NATIVE_CALL n_PC_GetArraySize(AMX *amx, cell *params) {
		if (!check_params(__FUNCTION__, 1, params)) {
			return 0;
		}

		const auto cmd_array = get_cmd_array(params[1]);

		if (!cmd_array) {
			logprintf("[%s] %s: invalid array handle", kName, __FUNCTION__);

			return 0;
		}

		return static_cast<cell>(cmd_array->size());
	}

	// native PC_FreeArray(&CmdArray:arr);
	static cell AMX_NATIVE_CALL n_PC_FreeArray(AMX *amx, cell *params) {
		if (!check_params(__FUNCTION__, 1, params)) {
			return 0;
		}

		cell *cptr{};

		if (amx_GetAddr(amx, params[1], &cptr) != AMX_ERR_NONE) {
			logprintf("[%s] %s: invalid param reference", kName, __FUNCTION__);

			return 0;
		}

		const auto cmd_array = get_cmd_array(*cptr);

		if (!cmd_array) {
			logprintf("[%s] %s: invalid array handle", kName, __FUNCTION__);

			return 0;
		}

		_cmd_array_set.erase(cmd_array);

		*cptr = 0;

		return 1;
	}

	// native PC_GetCommandName(CmdArray:arr, index, name[], size = sizeof name);
	static cell AMX_NATIVE_CALL n_PC_GetCommandName(AMX *amx, cell *params) {
		if (!check_params(__FUNCTION__, 4, params)) {
			return 0;
		}

		try {
			const auto cmd_array = get_cmd_array(params[1]);

			if (!cmd_array) {
				logprintf("[%s] %s: invalid array handle", kName, __FUNCTION__);

				return 0;
			}

			const auto index = static_cast<size_t>(params[2]);

			set_amxstring(amx, params[3], cmd_array->at(index).c_str(), params[4]);

			return 1;
		} catch (const std::exception &e) {
			logprintf("[%s] %s: %s", kName, __FUNCTION__, e.what());
		}

		return 0;
	}

	static int THISCALL HOOK_CFilterScripts__OnPlayerCommandText(void *_this, cell playerid, const char *szCommandText) {
		ProcessCommand(playerid, szCommandText);

		return 1;
	}

	static void ProcessCommand(cell playerid, const char *cmdtext) {
		if (!cmdtext || (cmdtext[0] != '/')) {
			return;
		}

		char cmd[32]{};
		const char *params{};

		int i = 1;
		while (cmdtext[i] == ' ') {
			i++;
		} // remove extra spaces before cmd name
		params = &cmdtext[i];

		char c{};

		i = 0;

		while ((c = params[i]) && (c != ' ')) {
			if (i >= sizeof(cmd)) {
				return;
			}

			cmd[i] = tolower(c, _locale);

			i++;
		}

		while (params[i] == ' ') {
			i++;
		} // remove extra spaces before params

		params = &params[i];

		CommandMap::const_iterator iter_cmd{};
		cell addr_cmd{}, addr_params{}, retval{}, flags{};
		bool command_exists{};

		for (const auto &iter : _amx_list) {
			if (command_exists = ((iter_cmd = iter.cmd_map.find(cmd)) != iter.cmd_map.end())) {
				flags = iter_cmd->second.flags;
			}

			if (iter.public_on_player_command_received.exists) {
				amx_Push(iter.amx, flags);
				amx_PushString(iter.amx, &addr_params, nullptr, params, 0, 0);
				amx_PushString(iter.amx, &addr_cmd, nullptr, cmd, 0, 0);
				amx_Push(iter.amx, playerid);
				amx_Exec(iter.amx, &retval, iter.public_on_player_command_received.id);
				amx_Release(iter.amx, addr_cmd);
				amx_Release(iter.amx, addr_params);

				if (!retval) {
					continue;
				}
			}

			if (command_exists) {
				amx_PushString(iter.amx, &addr_params, nullptr, params, 0, 0);
				amx_Push(iter.amx, playerid);
				amx_Exec(iter.amx, &retval, iter_cmd->second.public_id);
				amx_Release(iter.amx, addr_params);
			} else {
				retval = -1;
			}

			if (iter.public_on_player_command_performed.exists) {
				amx_Push(iter.amx, flags);
				amx_Push(iter.amx, retval);
				amx_PushString(iter.amx, &addr_params, nullptr, params, 0, 0);
				amx_PushString(iter.amx, &addr_cmd, nullptr, cmd, 0, 0);
				amx_Push(iter.amx, playerid);
				amx_Exec(iter.amx, &retval, iter.public_on_player_command_performed.id);
				amx_Release(iter.amx, addr_cmd);
				amx_Release(iter.amx, addr_params);
			}

			if (retval == 1) {
				break;
			}
		}
	}

	static inline CmdArraySet::value_type get_cmd_array(cell ptr) {
		const auto iter = std::find_if(_cmd_array_set.begin(), _cmd_array_set.end(), [ptr](const CmdArraySet::value_type &p) {
			return p.get() == reinterpret_cast<void *>(ptr);
		});

		if (iter != _cmd_array_set.end()) {
			return *iter;
		}

		return nullptr;
	}

	static inline void str_to_lower(std::string &str) {
		for (auto &c : str) {
			c = tolower(c, _locale);
		}
	}

	static inline char *get_string(AMX *amx, cell amx_addr) {
		int	len{};
		cell *addr{};

		if ((amx_GetAddr(amx, amx_addr, &addr) == AMX_ERR_NONE) &&
			(amx_StrLen(addr, &len) == AMX_ERR_NONE) &&
			len) {
			len++;

			char *str = new (std::nothrow) char[len] {};

			if (str && !amx_GetString(str, addr, 0, len)) {
				return str;
			}
		}

		return nullptr;
	}

	static inline bool get_public_var(AMX *amx, const char *name, cell &out) {
		cell addr{},
			*phys_addr{};

		if ((amx_FindPubVar(amx, name, &addr) == AMX_ERR_NONE) &&
			(amx_GetAddr(amx, addr, &phys_addr) == AMX_ERR_NONE)) {
			out = *phys_addr;

			return true;
		}

		return false;
	}

	static inline int set_amxstring(AMX *amx, cell amx_addr, const char *source, int max) {
		cell *dest = reinterpret_cast<cell *>(
			amx->base + static_cast<int>(reinterpret_cast<AMX_HEADER *>(amx->base)->dat + amx_addr)
			);
		cell *start = dest;

		while (max--&&*source) {
			*dest++ = static_cast<cell>(*source++);
		}

		*dest = 0;

		return dest - start;
	}

	static inline bool check_params(const char *native, int count, cell *params) {
		if (params[0] != (count * sizeof(cell))) {
			logprintf("[%s] %s: invalid number of parameters. Should be %d", kName, native, count);

			return false;
		}

		return true;
	}

	static AmxList _amx_list;
	static AmxQueue _amx_init_queue;
	static CmdArraySet _cmd_array_set;
	static std::shared_ptr<m::hook> _hook_fs__on_player_command_text;
	static std::locale _locale;
	static std::regex _regex_public_cmd_name,
		_regex_public_cmd_alias,
		_regex_public_cmd_flags;
};

Plugin::AmxList Plugin::_amx_list;
Plugin::AmxQueue Plugin::_amx_init_queue;
Plugin::CmdArraySet Plugin::_cmd_array_set;
std::shared_ptr<m::hook> Plugin::_hook_fs__on_player_command_text;
std::locale Plugin::_locale;
std::regex Plugin::_regex_public_cmd_name(R"(pc_cmd_(\w+))"),
Plugin::_regex_public_cmd_alias(R"(pc_alias_\w+)"),
Plugin::_regex_public_cmd_flags(R"(pc_flags_\w+)");


PLUGIN_EXPORT unsigned int PLUGIN_CALL Supports() {
	return SUPPORTS_VERSION | SUPPORTS_AMX_NATIVES | SUPPORTS_PROCESS_TICK;
}

PLUGIN_EXPORT bool PLUGIN_CALL Load(void **ppData) {
	pAMXFunctions = ppData[PLUGIN_DATA_AMX_EXPORTS];

	logprintf = reinterpret_cast<logprintf_t>(ppData[PLUGIN_DATA_LOGPRINTF]);

	return Plugin::Load();
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

PLUGIN_EXPORT void PLUGIN_CALL ProcessTick() {
	Plugin::ProcessTick();
}
