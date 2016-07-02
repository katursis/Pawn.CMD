# Pawn.CMD
The fastest and most functional command processor (SA:MP)
## Comparison
![alt tag](http://i.imgur.com/v43AinV.png)
## Natives
```pawn
native PC_Init(bool:is_gamemode); // internal
	
native PC_RegAlias(const cmd[], const alias[], ...);
native PC_SetFlags(const cmd[], flags);
native PC_GetFlags(const cmd[]);
native PC_EmulateCommand(playerid, const cmdtext[]);
native PC_RenameCommand(const cmd[], const newname[]);
native PC_CommandExists(const cmd[]);	
native PC_DeleteCommand(const cmd[]);
	
native CmdArray:PC_GetCommandArray();
native CmdArray:PC_GetAliasArray(const cmd[]);
native PC_GetArraySize(CmdArray:arr);
native PC_FreeArray(&CmdArray:arr);
native PC_GetCommandName(CmdArray:arr, index, dest[], size = sizeof dest);
```
## Callbacks
```pawn
forward PC_OnInit(); // calls after Pawn.CMD initialization
forward OnPlayerCommandReceived(playerid, cmd[], params[], flags); // calls before a command 
forward OnPlayerCommandPerformed(playerid, cmd[], params[], result, flags); // calls after a command 
```
## How to install
Unzip "pawncmd.zip" in your server's folder. Edit "server.cfg":
- Windows
```
plugins pawncmd
```
- Linux
```
plugins pawncmd.so
```
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
You can register aliases for a command.
```pawn
#include <Pawn.CMD>

cmd:help(playerid, params[]) 
{ 
    // code here 
    return 1; 
} 
alias:help("commands", "cmds", "menu"); // case insensitive  
```
## Using flags
You can set flags for a command.
```pawn
#include <Pawn.CMD>

enum(<<=1)
{
	CMD_ADMIN = 1,
	CMD_MODER,
	CMD_USER
};

flags:ban(CMD_ADMIN);
cmd:ban(playerid, params[])
{
    // code here
    return 1;
}

public OnPlayerCommandReceived(playerid, cmd[], params[], flags)
{
    if ((flags & CMD_ADMIN) && !pAdmin[playerid])
        return 0;

    return 1;
}
```
## Full example
```pawn
#include <Pawn.CMD>

enum(<<=1)
{
	CMD_ADMIN = 1,
	CMD_MODER,
	CMD_USER
};

flags:ban(CMD_ADMIN);
cmd:ban(playerid, params[])
{
    // code here
    return 1;
}
alias:ban("block");

public OnPlayerCommandReceived(playerid, cmd[], params[], flags)
{
    if ((flags & CMD_ADMIN) && !pAdmin[playerid])
        return 0;

    return 1;
}

public OnPlayerCommandPerformed(playerid, cmd[], params[], result, flags)
{
    if(result == -1)
    {
        SendClientMessage(playerid, 0xFFFFFFFF, "SERVER: Unknown command.");
        return 0;
    }

    return 1;
}
```
##### If you want to use Pawn.CMD in a filterscript, put this define before including:
```pawn
#define FILTERSCRIPT 
```
