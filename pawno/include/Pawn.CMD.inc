// Pawn.CMD plugin v2.1 by urShadow
 
#if defined _pawncmd_included
  #endinput
#endif
#define _pawncmd_included

native PC_Init(bool:is_gamemode);
native PC_RegAlias(const cmd[], const alias[], ...);

#if defined OnPlayerCommandReceived
	forward OnPlayerCommandReceived(playerid, cmdtext[]);
#endif

#if defined OnPlayerCommandPerformed
	forward OnPlayerCommandPerformed(playerid, cmdtext[], result);
#endif

#define cmd:%0(%1) \
	forward cmd_%0(%1); \
	public cmd_%0(%1)
	
#define alias:%0(%1); \
	forward pc_alias_%0(); \
	public pc_alias_%0() \
		PC_RegAlias(#%0, %1);
	
#define CMD cmd	
#define COMMAND cmd 

#if defined FILTERSCRIPT
	public OnFilterScriptInit()
	{		
		PC_Init(false);	
		
		#if defined pawncmd_OnFilterScriptInit
			pawncmd_OnFilterScriptInit();
		#endif
		
		return 1;
	}
	 
	#if defined _ALS_OnFilterScriptInit
		#undef OnFilterScriptInit
	#else
		#define _ALS_OnFilterScriptInit
	#endif

	#define OnFilterScriptInit pawncmd_OnFilterScriptInit

	#if defined pawncmd_OnFilterScriptInit
		forward pawncmd_OnFilterScriptInit();
	#endif
#else
	public OnGameModeInit()
	{	
		PC_Init(true);
		
		#if defined pawncmd_OnGameModeInit
			pawncmd_OnGameModeInit();
		#endif
		
		return 1;
	}
	 
	#if defined _ALS_OnGameModeInit
		#undef OnGameModeInit
	#else
		#define _ALS_OnGameModeInit
	#endif

	#define OnGameModeInit pawncmd_OnGameModeInit

	#if defined pawncmd_OnGameModeInit
		forward pawncmd_OnGameModeInit();
	#endif
#endif