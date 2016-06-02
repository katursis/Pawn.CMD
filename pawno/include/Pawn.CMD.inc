// Pawn.CMD plugin v2.0 by urShadow
 
#if defined _pawncmd_included
  #endinput
#endif
#define _pawncmd_included

native PC_RegAlias(const cmd[], const alias[], ...);

forward OnPlayerCommandReceived(playerid, cmdtext[]);
forward OnPlayerCommandPerformed(playerid, cmdtext[], result);

#define cmd:%0(%1) \
	forward cmd_%0(%1); \
	public cmd_%0(%1)
	
#define CMD:%0(%1) \
	cmd:%0(%1)
	
#define COMMAND:%0(%1) \
	cmd:%0(%1)
 
#define alias:%0(%1); \
	forward alias_%0(); \
	public alias_%0() \
		PC_RegAlias(#%0, %1);