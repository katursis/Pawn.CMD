# Pawn.CMD
The fastest and most functional command processor (SA:MP)
## Comparison
![alt tag](http://i.imgur.com/qGk9Axb.png)
## Example command
```pawn
#include <Pawn.CMD>

cmd:help(playerid, params[]) // also possible to use CMD and COMMAND
{
	// code here
	return 1;
}
```
## Registering aliases
You can register aliases for command as many as you want.
```pawn
cmd:help(playerid, params[])
{
	// code here
	return 1;
}
alias:help("commands", "cmds", "menu");
```
## OnPlayerCommandReceived callback
Calls before executing the command function. Return 1 for continue executing, return 0 for prevent executing.
```pawn
public OnPlayerCommandReceived(playerid, cmdtext[])
{
	if(!IsPlayerLogged(playerid)) return 0;
	if(!CheckAntiFlood(playerid)) return 0;
	return 1;
}
```
## OnPlayerCommandPerformed callback
Calls after executing the command function.
result - returned value from command function, -1 for non-existing command
```pawn
public OnPlayerCommandPerformed(playerid, cmdtext[], result)
{
	if(result == -1)
	{
		SendClientMessage(playerid, 0xFFFFFFFF, "SERVER: Unknown command.");
		return 0;
	}
	
	return 1;
}
```
