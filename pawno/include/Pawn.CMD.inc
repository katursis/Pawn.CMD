// Pawn.CMD plugin v1.0 by urShadow
 
#if defined _pawncmd_included
  #endinput
#endif
#define _pawncmd_included

native PC_RegAlias(const cmd[], const alias[], ...);
forward OnPlayerReceivedCommand(playerid, cmd[], params[], bool:exists);

#define cmd:%0(%1) \
	forward cmd_%0(%1); \
	public cmd_%0(%1)
	
#define CMD:%1(%2) \
	cmd:%1(%2)
	
#define COMMAND:%1(%2) \
	cmd:%1(%2)
 
#define alias:%0(%1); \
	forward alias_%0(); \
	public alias_%0() \
		PC_RegAlias(#%0, %1);