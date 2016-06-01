# Pawn.CMD
The fastest and most functional command processor (SA:MP)

## Example
```pawn
#include <Pawn.CMD> 

cmd:menu(playerid, params[]) // also possible to use CMD and COMMAND 
{ 
    // code here 
} 
alias:menu("settings", "help", "info"); // aliases (not case sensitive) 

public OnPlayerReceivedCommand(playerid, cmd[], params[], bool:exists) 
{ 
    if (!exists) 
    { 
        SendClientMessage(playerid, 0xFFFFFFFF, "SERVER: Unknown command."); 
         
        return 0; 
    } 
     
    return 1; 
}  
```
## Comparison
![alt tag](http://i.imgur.com/EyjYENV.png)
