#include "SDK/amx/amx.h"
#include "SDK/plugincommon.h"

#include "urmem.hpp"

#include <unordered_map>
#include <regex>
#include <string>
#include <vector>
#include <list>

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
using logprintf_t = void(*)(char* format, ...);

logprintf_t logprintf;
extern void *pAMXFunctions;

class Plugin
{
public:

	static constexpr char
		*kName = "Pawn.CMD",
		*kVersion = "3.0",
		*kPublicVarName = "_pawncmd_version";

	struct AmxInfo;

	struct CommandInfo
	{
		int public_id;
		unsigned int flags;
		bool is_alias;
	};

	using CommandMap = std::unordered_map<std::string, CommandInfo>;
	using AmxList = std::list<AmxInfo>;

	struct AmxInfo
	{
		AMX *amx;
		bool is_gamemode;

		struct
		{
			bool exists;
			int id;
		}	public_on_player_command_received,
			public_on_player_command_performed;

		CommandMap cmd_map;
	};

	static bool Load(void)
	{
		m::sig_scanner scanner;

		if (scanner.init(reinterpret_cast<void *>(logprintf)))
		{
			m::address_t addr_fs_on_player_command_text{};

			if (scanner.find(pattern, mask, addr_fs_on_player_command_text))
			{
				_hook_fs__on_player_command_text = std::make_shared<m::hook>(
					addr_fs_on_player_command_text,
					m::get_func_addr(&HOOK_CFilterScripts__OnPlayerCommandText));

				logprintf("%s plugin v%s by urShadow loaded", kName, kVersion);

				return true;
			}
		}

		logprintf("[%s] %s: address not found", kName, __FUNCTION__);

		return false;
	}

	static void Unload(void)
	{
		_hook_fs__on_player_command_text.reset();

		_amx_list.clear();

		logprintf("%s plugin v%s by urShadow unloaded", kName, kVersion);
	}

	static void AmxLoad(AMX *amx)
	{
		static const std::vector<AMX_NATIVE_INFO> native_vec =
		{
			{ "PC_Init", &n_PC_Init },
			{ "PC_RegAlias", &n_PC_RegAlias },
			{ "PC_SetFlags", &n_PC_SetFlags },
			{ "PC_GetFlags", &n_PC_GetFlags },
			{ "PC_EmulateCommand", &n_PC_EmulateCommand },
			{ "PC_RenameCommand", &n_PC_RenameCommand },
			{ "PC_DeleteCommand", &n_PC_DeleteCommand },
		};

		cell addr{}, *phys_addr{};
		if (!amx_FindPubVar(amx, kPublicVarName, &addr) &&
			!amx_GetAddr(amx, addr, &phys_addr))
		{
			if (*phys_addr != PAWNCMD_INCLUDE_VERSION)
				return logprintf("[%s] %s: .inc-file version does not equal the plugin version", kName, __FUNCTION__);
		}

		amx_Register(amx, native_vec.data(), native_vec.size());
	}

	static void AmxUnload(AMX *amx)
	{
		_amx_list.remove_if([amx](const AmxInfo &info)
		{
			return info.amx == amx;
		});
	}

private:

	// native PC_Init(bool:is_gamemode);
	static cell AMX_NATIVE_CALL n_PC_Init(AMX *amx, cell *params)
	{
		if (!check_params(__FUNCTION__, 1, params))
			return 0;

		int num_publics{};

		amx_NumPublics(amx, &num_publics);

		if (!num_publics)
			return 0;

		try
		{
			const bool is_gamemode = (params[1] != 0);

			std::vector<int>
				alias_public_ids,
				flags_public_ids;

			AmxInfo info{};

			info.amx = amx;
			info.is_gamemode = is_gamemode;

			for (int i{}; i < num_publics; i++)
			{
				char public_name[32]{};

				amx_GetPublic(amx, i, public_name);

				std::string s = public_name;

				std::smatch m;

				if (std::regex_match(s, m, _regex_public_cmd_name))
				{
					auto cmdname = m[1].str();

					str_to_lower(cmdname);

					info.cmd_map[cmdname].public_id = i;
				}
				else if (std::regex_match(s, _regex_public_cmd_alias))
				{
					alias_public_ids.push_back(i);
				}
				else if (std::regex_match(s, _regex_public_cmd_flags))
				{
					flags_public_ids.push_back(i);
				}
				else if (s == "OnPlayerCommandReceived")
				{
					info.public_on_player_command_received.id = i;
					info.public_on_player_command_received.exists = true;
				}
				else if (s == "OnPlayerCommandPerformed")
				{
					info.public_on_player_command_performed.id = i;
					info.public_on_player_command_performed.exists = true;
				}
			}

			if (is_gamemode)
			{
				_amx_list.push_back(info); // at the end
			}
			else // is filterscript
			{
				if (_amx_list.empty() ||
					(!_amx_list.back().is_gamemode))
				{
					_amx_list.push_back(info);
				}
				else
				{
					_amx_list.insert(std::prev(_amx_list.end()), info);
				}
			}

			for (int id : flags_public_ids)
			{
				amx_Exec(amx, nullptr, id);
			}

			for (int id : alias_public_ids)
			{
				amx_Exec(amx, nullptr, id);
			}

			return 1;
		}
		catch (const std::exception &e)
		{
			logprintf("[%s] %s: %s", kName, __FUNCTION__, e.what());
		}

		return 0;
	}

	// native PC_RegAlias(const cmd[], const alias[], ...);
	static cell AMX_NATIVE_CALL n_PC_RegAlias(AMX *amx, cell *params)
	{
		if (params[0] < (2 * sizeof(cell)))
			return 0;

		const auto iter_amx = std::find_if(_amx_list.begin(), _amx_list.end(), [amx](const AmxInfo &info)
		{
			return info.amx == amx;
		});

		if (iter_amx == _amx_list.end())
			return logprintf("[%s] %s: amx not found", kName, __FUNCTION__), 0;

		CommandMap::iterator iter_command{};

		int original_id{};

		unsigned int original_flags{};

		for (size_t i = 1; i <= params[0] / sizeof(cell); i++)
		{
			const std::unique_ptr<char[]> cmdstr(get_string(amx, params[i]));

			if (!cmdstr)
			{
				logprintf("[%s] %s: invalid str", kName, __FUNCTION__);

				continue;
			}

			std::string s;

			str_to_lower(s = cmdstr.get());

			if (i == 1)
			{
				if ((iter_command = iter_amx->cmd_map.find(s)) == iter_amx->cmd_map.end())
					return logprintf("[%s] %s: command '%s' not found", kName, __FUNCTION__, s.c_str()), 0;

				if (iter_command->second.is_alias)
					return logprintf("[%s] %s: command '%s' is an alias", kName, __FUNCTION__, s.c_str()), 0;

				original_id = iter_command->second.public_id;

				original_flags = iter_command->second.flags;
			}
			else
			{
				if (iter_amx->cmd_map.find(s) != iter_amx->cmd_map.end())
				{
					logprintf("[%s] %s: alias '%s' is occupied", kName, __FUNCTION__, s.c_str());

					continue;
				}

				iter_amx->cmd_map.emplace(std::move(s), CommandInfo{ original_id, original_flags, true });
			}
		}

		return 1;
	}

	// native PC_SetFlags(const cmd[], flags);
	static cell AMX_NATIVE_CALL n_PC_SetFlags(AMX *amx, cell *params)
	{
		if (!check_params(__FUNCTION__, 2, params))
			return 0;

		const auto iter_amx = std::find_if(_amx_list.begin(), _amx_list.end(), [amx](const AmxInfo &info)
		{
			return info.amx == amx;
		});

		if (iter_amx == _amx_list.end())
			return logprintf("[%s] %s: amx not found", kName, __FUNCTION__), 0;

		std::unique_ptr<char[]>	cmd(get_string(amx, params[1]));

		if (!cmd)
			return logprintf("[%s] %s: invalid cmd", kName, __FUNCTION__), 0;

		const auto iter_cmd = iter_amx->cmd_map.find(cmd.get());

		if (iter_cmd == iter_amx->cmd_map.end())
			return logprintf("[%s] %s: cmd '%s' not found", kName, __FUNCTION__, cmd.get()), 0;

		iter_cmd->second.flags = static_cast<unsigned int>(params[2]);

		return 1;
	}

	// native PC_GetFlags(const cmd[], &flags);
	static cell AMX_NATIVE_CALL n_PC_GetFlags(AMX *amx, cell *params)
	{
		if (!check_params(__FUNCTION__, 2, params))
			return 0;

		const auto iter_amx = std::find_if(_amx_list.begin(), _amx_list.end(), [amx](const AmxInfo &info)
		{
			return info.amx == amx;
		});

		if (iter_amx == _amx_list.end())
			return logprintf("[%s] %s: amx not found", kName, __FUNCTION__), 0;

		const std::unique_ptr<char[]> cmd(get_string(amx, params[1]));

		if (!cmd)
			return logprintf("[%s] %s: invalid cmd", kName, __FUNCTION__), 0;

		const auto iter_cmd = iter_amx->cmd_map.find(cmd.get());

		if (iter_cmd == iter_amx->cmd_map.end())
			return logprintf("[%s] %s: cmd '%s' not found", kName, __FUNCTION__, cmd.get()), 0;

		cell *cptr{};

		if (amx_GetAddr(amx, params[2], &cptr) != AMX_ERR_NONE)
			return logprintf("[%s] %s: invalid flags reference", kName, __FUNCTION__), 0;

		*cptr = static_cast<cell>(iter_cmd->second.flags);

		return 1;
	}

	// native PC_EmulateCommand(playerid, const cmdtext[]);
	static cell AMX_NATIVE_CALL n_PC_EmulateCommand(AMX *amx, cell *params)
	{
		if (!check_params(__FUNCTION__, 2, params))
			return 0;

		const std::unique_ptr<char[]> cmdtext(get_string(amx, params[2]));

		if (!cmdtext)
			return logprintf("[%s] %s: invalid str", kName, __FUNCTION__), 0;

		ProcessCommand(params[1], cmdtext.get());

		return 1;
	}

	// native PC_RenameCommand(const name[], const newname[]);
	static cell AMX_NATIVE_CALL n_PC_RenameCommand(AMX *amx, cell *params)
	{
		if (!check_params(__FUNCTION__, 2, params))
			return 0;

		const auto iter_amx = std::find_if(_amx_list.begin(), _amx_list.end(), [amx](const AmxInfo &info)
		{
			return info.amx == amx;
		});

		if (iter_amx == _amx_list.end())
			return logprintf("[%s] %s: amx not found", kName, __FUNCTION__), 0;

		const std::unique_ptr<char[]>
			name(get_string(amx, params[1])),
			newname(get_string(amx, params[2]));

		if ((!name) || (!newname))
			return logprintf("[%s] %s: invalid name or newname", kName, __FUNCTION__), 0;

		const auto iter_cmd = iter_amx->cmd_map.find(name.get());

		if (iter_cmd == iter_amx->cmd_map.end())
			return logprintf("[%s] %s: cmd '%s' not found", kName, __FUNCTION__, name.get()), 0;

		if (iter_amx->cmd_map.find(newname.get()) != iter_amx->cmd_map.end())
			return logprintf("[%s] %s: name '%s' is occupied", kName, __FUNCTION__, newname.get()), 0;

		const int public_id = iter_cmd->second.public_id;

		iter_amx->cmd_map.erase(iter_cmd);

		iter_amx->cmd_map[newname.get()].public_id = public_id;

		return 1;
	}

	// native PC_DeleteCommand(const name[]);
	static cell AMX_NATIVE_CALL n_PC_DeleteCommand(AMX *amx, cell *params)
	{
		if (!check_params(__FUNCTION__, 1, params))
			return 0;

		const auto iter_amx = std::find_if(_amx_list.begin(), _amx_list.end(), [amx](const AmxInfo &info)
		{
			return info.amx == amx;
		});

		if (iter_amx == _amx_list.end())
			return logprintf("[%s] %s: amx not found", kName, __FUNCTION__), 0;

		const std::unique_ptr<char[]> name(get_string(amx, params[1]));

		if (!name)
			return logprintf("[%s] %s: invalid name", kName, __FUNCTION__), 0;

		const auto iter_cmd = iter_amx->cmd_map.find(name.get());

		if (iter_cmd == iter_amx->cmd_map.end())
			return logprintf("[%s] %s: cmd '%s' not found", kName, __FUNCTION__, name.get()), 0;

		iter_amx->cmd_map.erase(iter_cmd);

		return 1;
	}

	static int THISCALL HOOK_CFilterScripts__OnPlayerCommandText(void *_this, cell playerid, const char *szCommandText)
	{
		ProcessCommand(playerid, szCommandText);

		return 1;
	}

	static void ProcessCommand(cell playerid, const char *cmdtext)
	{
		if (!cmdtext)
			return;

		if (cmdtext[0] != '/')
			return;

		char cmd[32]{};
		const char *params{};

		int i = 1;
		while (cmdtext[i] == ' ') { i++; } // remove extra spaces before cmd name
		params = &cmdtext[i];

		char c{};

		i = 0;
		while (
			(c = params[i]) &&
			(c != ' ')
			)
		{
			if (i >= sizeof(cmd))
				return;

			cmd[i] = tolower(c, _locale);

			i++;
		}

		while (params[i] == ' ') { i++; } // remove extra spaces before params

		params = &params[i];

		CommandMap::const_iterator iter_cmd{};

		cell addr_cmd{}, addr_params{}, retval = 1, flags{};

		bool command_exists{};

		for (const auto &iter : _amx_list)
		{
			if (command_exists = ((iter_cmd = iter.cmd_map.find(cmd)) != iter.cmd_map.end()))
				flags = iter_cmd->second.flags;

			if (iter.public_on_player_command_received.exists)
			{
				amx_Push(iter.amx, flags);
				amx_PushString(iter.amx, &addr_params, nullptr, params, 0, 0);
				amx_PushString(iter.amx, &addr_cmd, nullptr, cmd, 0, 0);
				amx_Push(iter.amx, playerid);
				amx_Exec(iter.amx, &retval, iter.public_on_player_command_received.id);
				amx_Release(iter.amx, addr_cmd);
				amx_Release(iter.amx, addr_params);

				if (retval == 0)
					break;
			}

			if (command_exists)
			{
				amx_PushString(iter.amx, &addr_params, nullptr, params, 0, 0);
				amx_Push(iter.amx, playerid);
				amx_Exec(iter.amx, &retval, iter_cmd->second.public_id);
				amx_Release(iter.amx, addr_params);
			}
			else retval = -1;

			if (iter.public_on_player_command_performed.exists)
			{
				amx_Push(iter.amx, flags);
				amx_Push(iter.amx, retval);
				amx_PushString(iter.amx, &addr_params, nullptr, params, 0, 0);
				amx_PushString(iter.amx, &addr_cmd, nullptr, cmd, 0, 0);
				amx_Push(iter.amx, playerid);
				amx_Exec(iter.amx, &retval, iter.public_on_player_command_performed.id);
				amx_Release(iter.amx, addr_cmd);
				amx_Release(iter.amx, addr_params);

				if (retval == 0)
					break;
			}
		}
	}

	static inline void str_to_lower(std::string &str)
	{
		for (auto &c : str)	c = tolower(c, _locale);
	}

	static inline char *get_string(AMX *amx, cell amx_addr)
	{
		int	len{};
		cell *addr{};

		if (!amx_GetAddr(amx, amx_addr, &addr) &&
			!amx_StrLen(addr, &len) &&
			len)
		{
			len++;

			char *str = new (std::nothrow) char[len] {};

			if (str &&
				!amx_GetString(str, addr, 0, len))
				return str;
		}

		return nullptr;
	}

	static inline bool check_params(const char *native, int count, cell *params)
	{
		if (params[0] != (count * sizeof(cell)))
			return logprintf("[%s] %s: invalid number of parameters. Should be %d", kName, native, count), false;

		return true;
	}

	static AmxList
		_amx_list;

	static std::shared_ptr<m::hook>
		_hook_fs__on_player_command_text;

	static std::regex
		_regex_public_cmd_name,
		_regex_public_cmd_alias,
		_regex_public_cmd_flags;

	static std::locale
		_locale;
};

Plugin::AmxList
Plugin::_amx_list;

std::shared_ptr<m::hook>
Plugin::_hook_fs__on_player_command_text;

std::regex
Plugin::_regex_public_cmd_name(R"(pc_cmd_(\w+))"),
Plugin::_regex_public_cmd_alias(R"(pc_alias_\w+)"),
Plugin::_regex_public_cmd_flags(R"(pc_flags_\w+)");

std::locale
Plugin::_locale;

PLUGIN_EXPORT unsigned int PLUGIN_CALL Supports()
{
	return SUPPORTS_VERSION | SUPPORTS_AMX_NATIVES;
}

PLUGIN_EXPORT bool PLUGIN_CALL Load(void **ppData)
{
	pAMXFunctions = ppData[PLUGIN_DATA_AMX_EXPORTS];
	logprintf = reinterpret_cast<logprintf_t>(ppData[PLUGIN_DATA_LOGPRINTF]);

	return Plugin::Load();
}

PLUGIN_EXPORT void PLUGIN_CALL Unload()
{
	Plugin::Unload();
}

PLUGIN_EXPORT void PLUGIN_CALL AmxLoad(AMX *amx)
{
	Plugin::AmxLoad(amx);
}

PLUGIN_EXPORT void PLUGIN_CALL AmxUnload(AMX *amx)
{
	Plugin::AmxUnload(amx);
}