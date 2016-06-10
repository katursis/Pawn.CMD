#include "SDK/amx/amx.h"
#include "SDK/plugincommon.h"

#include "urmem.hpp"

#include <unordered_map>
#include <regex>
#include <string>
#include <vector>
#include <list>

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

class PawnCMD
{
public:

	struct AmxInfo;

	using cmd_map_t = std::unordered_map<std::string, int>;
	using amx_list_t = std::list<std::shared_ptr<AmxInfo>>;

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

		cmd_map_t cmd_map;
	};

	static bool Init(void)
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

				logprintf("Pawn.CMD plugin v2.1 by urShadow was loaded");

				return true;
			}
		}

		logprintf("[Pawn.CMD] Addresses were not found");

		return false;
	}

	static void UnInit(void)
	{
		_hook_fs__on_player_command_text.reset();

		_amx_list.clear();

		logprintf("[Pawn.CMD] Was unloaded");
	}

	static void AmxLoad(AMX *amx)
	{
		static std::vector<AMX_NATIVE_INFO> native_vec =
		{
			{ "PC_Init", &n_PC_Init },
			{ "PC_RegAlias", &n_PC_RegAlias }
		};

		amx_Register(amx, native_vec.data(), native_vec.size());
	}

	static void AmxUnload(AMX *amx)
	{
		_amx_list.remove_if([amx](const std::shared_ptr<AmxInfo> &info)
		{
			if (!info)
				return false;

			return info->amx == amx;
		});
	}

private:

	// native PC_Init(bool:is_gamemode);
	static cell AMX_NATIVE_CALL n_PC_Init(AMX *amx, cell *params)
	{
		if (params[0] != sizeof(cell))
			return 0;

		int numPublics{};
		amx_NumPublics(amx, &numPublics);

		if (numPublics)
		{
			try
			{
				bool is_gamemode = params[1] != 0;
				auto info = std::make_shared<AmxInfo>();
				std::vector<int> alias_public_ids;
				std::string s;

				info->amx = amx; info->is_gamemode = is_gamemode;

				for (int i{}; i < numPublics; i++)
				{
					char publicName[32]{};

					amx_GetPublic(amx, i, publicName);

					str_to_lower(s = publicName);

					std::smatch m;

					if (std::regex_match(s, m, _regex_public_cmd_name))
					{
						info->cmd_map[m[1].str()] = i;

						logprintf("[Pawn.CMD] command '%s' has been registered", m[1].str().c_str());
					}
					else if (std::regex_match(s, _regex_public_cmd_alias))
					{
						alias_public_ids.push_back(i);
					}
					else if (std::string(publicName) == "OnPlayerCommandReceived")
					{
						info->public_on_player_command_received.id = i;
						info->public_on_player_command_received.exists = true;
					}
					else if (std::string(publicName) == "OnPlayerCommandPerformed")
					{
						info->public_on_player_command_performed.id = i;
						info->public_on_player_command_performed.exists = true;
					}
				}

				if (is_gamemode)
				{
					_amx_list.push_back(info); // at the end
				}
				else // is filterscript
				{
					if (_amx_list.empty() ||
						(!_amx_list.back()->is_gamemode))
					{
						_amx_list.push_back(info);
					}
					else
					{
						_amx_list.insert(std::prev(_amx_list.end()), info);
					}
				}

				for (auto id : alias_public_ids)
				{
					amx_Exec(amx, nullptr, id);
				}
			}
			catch (const std::exception &e)
			{
				logprintf("[Pawn.CMD] %s", e.what());
			}
		}

		return 1;
	}

	// native PC_RegAlias(const cmd[], const alias[], ...);
	static cell AMX_NATIVE_CALL n_PC_RegAlias(AMX *amx, cell *params)
	{
		if (params[0] < (2 * sizeof(cell)))
			return 0;

		try
		{
			auto iterAmx = std::find_if(_amx_list.begin(), _amx_list.end(), [amx](const std::shared_ptr<AmxInfo> &info)
			{
				if (!info)
					return false;

				return info->amx == amx;
			});

			if (iterAmx == _amx_list.end())
				return logprintf("[Pawn.CMD] amx %p was not found", amx), 0;

			auto iterPublic = iterAmx->get()->cmd_map.end();
			int originalID{};

			cell *cptr_cmd{};
			int len{};
			std::string str_cmd;

			for (size_t i = 1; i <= params[0] / sizeof(cell); i++)
			{
				char cmd[32]{};

				amx_GetAddr(amx, params[i], &cptr_cmd);
				amx_StrLen(cptr_cmd, &len);

				if (len > 31)
					len = 31;

				amx_GetString(cmd, cptr_cmd, 0, len + 1);

				str_to_lower(str_cmd = cmd);

				if (i == 1)
				{
					iterPublic = iterAmx->get()->cmd_map.find(str_cmd);
					if (iterPublic == iterAmx->get()->cmd_map.end())
					{
						return logprintf("[Pawn.CMD] command '%s' was not found", str_cmd.c_str()), 0;
					}

					originalID = iterPublic->second;
				}
				else
				{
					iterAmx->get()->cmd_map[str_cmd] = originalID;

					logprintf("[Pawn.CMD] alias '%s' of '%s' has been registered", str_cmd.c_str(), iterPublic->first.c_str());
				}
			}
		}
		catch (const std::exception &e)
		{
			logprintf("[Pawn.CMD] %s", e.what());
		}

		return 1;
	}

	static int THISCALL HOOK_CFilterScripts__OnPlayerCommandText(void *_this, cell playerid, const char *szCommandText)
	{
		if (szCommandText[0] != '/')
			return 1;

		char cmd[32]{};
		const char *params{};

		int i = 1;
		while (szCommandText[i] == ' ') { i++; } // remove extra spaces before cmd name
		params = &szCommandText[i];

		char c{};

		i = 0;
		while (
			(c = params[i]) &&
			(c != ' ')
			)
		{
			if (i >= sizeof(cmd))
				return 1;

			cmd[i] = tolower(c, _locale);

			i++;
		}

		while (params[i] == ' ') { i++; } // remove extra spaces before params

		params = &params[i];

		cmd_map_t::iterator publicIter{};
		cell addr{}, retval = 1;
		for (auto &iter : _amx_list)
		{
			auto &info = *iter;

			if (info.public_on_player_command_received.exists)
			{
				amx_PushString(info.amx, &addr, nullptr, szCommandText, 0, 0);
				amx_Push(info.amx, playerid);
				amx_Exec(info.amx, &retval, info.public_on_player_command_received.id);
				amx_Release(info.amx, addr);

				if (retval == 0)
					break;
			}

			if ((publicIter = info.cmd_map.find(cmd)) != info.cmd_map.end())
			{
				amx_PushString(info.amx, &addr, nullptr, params, 0, 0);
				amx_Push(info.amx, playerid);
				amx_Exec(info.amx, &retval, publicIter->second);
				amx_Release(info.amx, addr);
			}
			else retval = -1;

			if (info.public_on_player_command_performed.exists)
			{
				amx_Push(info.amx, retval);
				amx_PushString(info.amx, &addr, nullptr, szCommandText, 0, 0);
				amx_Push(info.amx, playerid);
				amx_Exec(info.amx, &retval, info.public_on_player_command_performed.id);
				amx_Release(info.amx, addr);

				if (retval == 0)
					break;
			}
		}

		return 1;
	}

	static inline void str_to_lower(std::string &str)
	{
		for (auto &c : str)	c = tolower(c, _locale);
	}

	static amx_list_t
		_amx_list;

	static std::shared_ptr<m::hook>
		_hook_fs__on_player_command_text;

	static std::regex
		_regex_public_cmd_name,
		_regex_public_cmd_alias;

	static std::locale
		_locale;
};

PawnCMD::amx_list_t
PawnCMD::_amx_list;

std::shared_ptr<m::hook>
PawnCMD::_hook_fs__on_player_command_text;

std::regex
PawnCMD::_regex_public_cmd_name(R"(^cmd_(\w+)$)"),
PawnCMD::_regex_public_cmd_alias(R"(^pc_alias_\w+$)");

std::locale
PawnCMD::_locale;

PLUGIN_EXPORT unsigned int PLUGIN_CALL Supports()
{
	return SUPPORTS_VERSION | SUPPORTS_AMX_NATIVES;
}

PLUGIN_EXPORT bool PLUGIN_CALL Load(void **ppData)
{
	pAMXFunctions = ppData[PLUGIN_DATA_AMX_EXPORTS];
	logprintf = reinterpret_cast<logprintf_t>(ppData[PLUGIN_DATA_LOGPRINTF]);

	return PawnCMD::Init();
}

PLUGIN_EXPORT void PLUGIN_CALL Unload()
{
	PawnCMD::UnInit();
}

PLUGIN_EXPORT void PLUGIN_CALL AmxLoad(AMX *amx)
{
	PawnCMD::AmxLoad(amx);
}

PLUGIN_EXPORT void PLUGIN_CALL AmxUnload(AMX *amx)
{
	PawnCMD::AmxUnload(amx);
}