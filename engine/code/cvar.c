// cvar.c -- dynamic variable tracking

#include "quakedef.h"
#include "winquake.h"

cvar_t	*cvar_vars;
char	*cvar_null_string = "";

// jkrige - cvarlist
/*
=========
Cvar_List
=========
*/

void Cvar_List_f (void)
{
	cvar_t		*cvar;
	char 		*partial;
	int		len;
	int		count;

	if (Cmd_Argc() > 1)
	{
		partial = Cmd_Argv (1);
		len = strlen(partial);
	}
	else
	{
		partial = NULL;
		len = 0;
	}

	count=0;

	for (cvar=cvar_vars ; cvar ; cvar=cvar->next)
	{
		if (partial && strncmp (partial,cvar->name, len))
		{
			continue;
		}

		Con_Printf ("\"%s\" is \"%s\"\n", cvar->name, cvar->string);

		count++;
	}

	Con_Printf ("%i cvar(s)", count);

	if (partial)
	{
		Con_Printf (" beginning with \"%s\"", partial);
	}

	Con_Printf ("\n");
}
// jkrige - cvarlist

/*
============
Cvar_FindVar
============
*/
cvar_t *Cvar_FindVar (char *var_name)
{
	cvar_t	*var;
	
	for (var=cvar_vars ; var ; var=var->next)
		if (!strcmp (var_name, var->name))
			return var;

	return NULL;
}

/*
============
Cvar_VariableValue
============
*/
float	Cvar_VariableValue (char *var_name)
{
	cvar_t	*var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return 0;
	return atof (var->string);
}


/*
============
Cvar_VariableString
============
*/
char *Cvar_VariableString (char *var_name)
{
	cvar_t *var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return cvar_null_string;
	return var->string;
}


/*
============
Cvar_CompleteVariable
============
*/
char *Cvar_CompleteVariable (char *partial)
{
	cvar_t		*cvar;
	int			len;
	
	len = strlen(partial);
	
	if (!len)
		return NULL;
		
// check functions
	for (cvar=cvar_vars ; cvar ; cvar=cvar->next)
		if (!strncmp (partial,cvar->name, len))
			return cvar->name;

	return NULL;
}


/*
============
Cvar_Set
============
*/
void Cvar_Set (char *var_name, char *value)
{
	cvar_t	*var;
	qboolean changed;
	
	var = Cvar_FindVar (var_name);
	if (!var)
	{	// there is an error in C code if this happens
		Con_Printf ("Cvar_Set: variable %s not found\n", var_name);
		return;
	}

	changed = strcmp(var->string, value);
	
	Z_Free (var->string);	// free the old value string
	
	var->string = Z_Malloc (strlen(value)+1);
	strcpy (var->string, value);
	var->value = atof (var->string);
	if (var->server && changed)
	{
		if (sv.active)
			SV_BroadcastPrintf ("\"%s\" changed to \"%s\"\n", var->name, var->string);
	}

	// jkrige - deathmatch/coop not at the same time fix
	if ( (var->value != 0) && (!strcmp (var->name, deathmatch.name)) )
		Cvar_Set ("coop", "0");

	if ( (var->value != 0) && (!strcmp (var->name, coop.name)) )
		Cvar_Set ("deathmatch", "0");
	// jkrige - deathmatch/coop not at the same time fix
}

/*
============
Cvar_SetValue
============
*/
void Cvar_SetValue (char *var_name, float value)
{
	char	val[32];
	
	// jkrige - cvar zeros fix
	//sprintf (val, "%f",value);
	if (value == (int)value)
		sprintf (val, "%d", (int)value);
	else
		sprintf (val, "%f",value);
	// jkrige - cvar zeros fix

	Cvar_Set (var_name, val);
}


/*
============
Cvar_RegisterVariable

Adds a freestanding variable to the variable list.
============
*/
void Cvar_RegisterVariable (cvar_t *variable)
{
	char	*oldstr;

//	if (!LegitCopy)
//		return;

// first check to see if it has allready been defined
	if (Cvar_FindVar (variable->name))
	{
		Con_Printf ("Can't register variable %s, allready defined\n", variable->name);
		return;
	}
	
// check for overlap with a command
	if (Cmd_Exists (variable->name))
	{
		Con_Printf ("Cvar_RegisterVariable: %s is a command\n", variable->name);
		return;
	}
		
// copy the value off, because future sets will Z_Free it
	oldstr = variable->string;
	variable->string = Z_Malloc (strlen(variable->string)+1);	
	strcpy (variable->string, oldstr);
	variable->value = atof (variable->string);
	
// link the variable in
	variable->next = cvar_vars;
	cvar_vars = variable;
}

/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
qboolean	Cvar_Command (void)
{
	cvar_t			*v;

// check variables
	v = Cvar_FindVar (Cmd_Argv(0));
	if (!v)
		return false;
		
// perform a variable print or set
	if (Cmd_Argc() == 1)
	{
		Con_Printf ("\"%s\" is \"%s\"\n", v->name, v->string);
		return true;
	}

	Cvar_Set (v->name, Cmd_Argv(1));
	return true;
}


/*
============
Cvar_WriteVariables

Writes lines containing "set variable value" for all variables
with the archive flag set to true.
============
*/
void Cvar_WriteVariables (FILE *f)
{
	cvar_t	*var;
	
	for (var = cvar_vars ; var ; var = var->next)
		if (var->archive)
			fprintf (f, "%s \"%s\"\n", var->name, var->string);
}

