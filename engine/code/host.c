// host.c -- coordinates spawning and killing of local servers

/*
 * $Header: /H2 Mission Pack/HOST.C 6     3/12/98 6:31p Mgummelt $
 */

#include "quakedef.h"
//#include "r_local.h" // jkrige - removed

/*

A server can allways be started, even if the system started out as a client
to a remote system.

A client can NOT be started if the system started as a dedicated server.

Memory is cleared / released when a server or client begins, not when they end.

*/

void Host_WriteConfiguration (char *fname);

quakeparms_t host_parms;

qboolean	host_initialized;		// true if into command execution

double		host_frametime;
double		host_time;
double		realtime;				// without any filtering or bounding
double		oldrealtime;			// last frame run
int			host_framecount;

int			host_hunklevel;

int			minimum_memory;

client_t	*host_client;			// current client

jmp_buf 	host_abortserver;

byte		*host_basepal;
byte		*host_colormap;


extern cvar_t	sys_quake2;

// jkrige - configurable fps caps
extern cvar_t   sv_fps;
// jkrige - configurable fps caps

cvar_t	host_framerate = {"host_framerate","0"};	// set for slow motion
cvar_t	host_speeds = {"host_speeds","0"};			// set for running times

// jkrige - configurable fps caps
//		defaults to 0 since sv_fps takes care of framerate
cvar_t	sys_ticrate = {"sys_ticrate","0"};
//cvar_t	sys_ticrate = {"sys_ticrate","0.05"};
// jkrige - configurable fps caps

cvar_t	serverprofile = {"serverprofile","0"};

cvar_t	fraglimit = {"fraglimit","0",false,true};
cvar_t	timelimit = {"timelimit","0",false,true};
cvar_t	teamplay = {"teamplay","0",false,true};

cvar_t	samelevel = {"samelevel","0"};
cvar_t	noexit = {"noexit","0",false,true};

#ifdef QUAKE2
cvar_t	developer = {"developer","1", true};	// should be 0 for release!
#else
cvar_t	developer = {"developer","0", true};
#endif

cvar_t	skill = {"skill","1"};						// 0 - 3
cvar_t	deathmatch = {"deathmatch","0"};			// 0, 1, or 2
cvar_t	randomclass = {"randomclass","0"};			// 0, 1, or 2
cvar_t	coop = {"coop","0"};			// 0 or 1

cvar_t	pausable = {"pausable","1"};
cvar_t	sys_adaptive = {"sys_adaptive","1",true};

cvar_t	temp1 = {"temp1","0"};


// jkrige - fps counter
cvar_t	r_fps = {"r_fps","0",true};
int		fps_count;
// jkrige - fps counter


/*
================
Host_EndGame
================
*/
void Host_EndGame (char *message, ...)
{
	va_list		argptr;
	char		string[1024];
	
	va_start (argptr,message);
	vsprintf (string,message,argptr);
	va_end (argptr);
	Con_DPrintf ("Host_EndGame: %s\n",string);
	
	if (sv.active)
		Host_ShutdownServer (false);

	if (cls.state == ca_dedicated)
		Sys_Error ("Host_EndGame: %s\n",string);	// dedicated servers exit
	
	if (cls.demonum != -1)
		CL_NextDemo ();
	else
		CL_Disconnect ();

	longjmp (host_abortserver, 1);
}

/*
================
Host_Error

This shuts down both the client and server
================
*/
void Host_Error (char *error, ...)
{
	va_list		argptr;
	char		string[1024];
	static	qboolean inerror = false;
	
	if (inerror)
		Sys_Error ("Host_Error: recursively entered");
	inerror = true;
	
	SCR_EndLoadingPlaque ();		// reenable screen updates

	va_start (argptr,error);
	vsprintf (string,error,argptr);
	va_end (argptr);
	Con_Printf ("Host_Error: %s\n",string);
	
	if (sv.active)
		Host_ShutdownServer (false);

	if (cls.state == ca_dedicated)
		Sys_Error ("Host_Error: %s\n",string);	// dedicated servers exit

	CL_Disconnect ();
	cls.demonum = -1;

	inerror = false;

	longjmp (host_abortserver, 1);
}

/*
================
Host_FindMaxClients
================
*/
void	Host_FindMaxClients (void)
{
	int		i;

	svs.maxclients = 1;
		
	i = COM_CheckParm ("-dedicated");
	if (i)
	{
		cls.state = ca_dedicated;
		if (i != (com_argc - 1))
		{
			svs.maxclients = atoi (com_argv[i+1]);
		}
		else
			svs.maxclients = 8;
	}
	else
		cls.state = ca_disconnected;

	i = COM_CheckParm ("-listen");
	if (i)
	{
		if (cls.state == ca_dedicated)
			Sys_Error ("Only one of -dedicated or -listen can be specified");
		if (i != (com_argc - 1))
			svs.maxclients = atoi (com_argv[i+1]);
		else
			svs.maxclients = 8;
	}
	if (svs.maxclients < 1)
		svs.maxclients = 8;
	else if (svs.maxclients > MAX_SCOREBOARD)
		svs.maxclients = MAX_SCOREBOARD;

	svs.maxclientslimit = svs.maxclients;
	if (svs.maxclientslimit < 4)
		svs.maxclientslimit = 4;
	svs.clients = Hunk_AllocName (svs.maxclientslimit*sizeof(client_t), "clients");

	if (svs.maxclients > 1)
		Cvar_SetValue ("deathmatch", 1.0);
	else
		Cvar_SetValue ("deathmatch", 0.0);
}

/*
===============
Host_SaveConfig_f
===============
*/
void Host_SaveConfig_f (void)
{

	if (cmd_source != src_command)
		return;

/*	if (!sv.active)
	{
		Con_Printf ("Not playing a local game.\n");
		return;
	}

	if (cl.intermission)
	{
		Con_Printf ("Can't save in intermission.\n");
		return;
	}
*/

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("saveConfig <savename> : save a config file\n");
		return;
	}

	if (strstr(Cmd_Argv(1), ".."))
	{
		Con_Printf ("Relative pathnames are not allowed.\n");
		return;
	}

	Host_WriteConfiguration (Cmd_Argv(1));
}


/*
=======================
Host_InitLocal
======================
*/
void Host_InitLocal (void)
{
	Cmd_AddCommand ("saveconfig", Host_SaveConfig_f);

	Host_InitCommands ();
	
	Cvar_RegisterVariable (&sys_quake2);

	Cvar_RegisterVariable (&host_framerate);
	Cvar_RegisterVariable (&host_speeds);

	Cvar_RegisterVariable (&sys_ticrate);
	Cvar_RegisterVariable (&serverprofile);

	Cvar_RegisterVariable (&fraglimit);
	Cvar_RegisterVariable (&timelimit);
	Cvar_RegisterVariable (&teamplay);
	Cvar_RegisterVariable (&samelevel);
	Cvar_RegisterVariable (&noexit);
	Cvar_RegisterVariable (&skill);
	Cvar_RegisterVariable (&developer);
	Cvar_RegisterVariable (&deathmatch);
	Cvar_RegisterVariable (&randomclass);
	Cvar_RegisterVariable (&coop);

	Cvar_RegisterVariable (&pausable);

	Cvar_RegisterVariable (&sys_adaptive);

	Cvar_RegisterVariable (&temp1);

	Cvar_RegisterVariable (&r_fps); // jkrige - fps counter

	Host_FindMaxClients ();
	
	host_time = 1.0;		// so a think at time 0 won't get called
}

extern qboolean	LegitCopy;

/*
===============
Host_WriteConfiguration

Writes key bindings and archived cvars to config.cfg
===============
*/
void Host_WriteConfiguration (char *fname)
{
	FILE	*f;

	if (!LegitCopy)
		return;

// dedicated servers initialize the host but don't parse and set the
// config.cfg cvars
	if (host_initialized & !isDedicated)
	{
		f = fopen (va("%s/%s",com_gamedir,fname), "w");
		if (!f)
		{
			Con_Printf ("Couldn't write %s.\n",fname);
			return;
		}
		
		Key_WriteBindings (f);
		Cvar_WriteVariables (f);
		
		// jkrige - mlook cvar
		//if (in_mlook.state & 1)		//if mlook was down, keep it that way
		//	fprintf (f, "+mlook\n");
		// jkrige - mlook cvar

		fclose (f);
	}
}


/*
=================
SV_ClientPrintf

Sends text across to be displayed 
FIXME: make this just a stuffed echo?
=================
*/
void SV_ClientPrintf (char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];
	
	va_start (argptr,fmt);
	vsprintf (string, fmt,argptr);
	va_end (argptr);
	
	MSG_WriteByte (&host_client->message, svc_print);
	MSG_WriteString (&host_client->message, string);
}

/*
=================
SV_BroadcastPrintf

Sends text to all active clients
=================
*/
void SV_BroadcastPrintf (char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];
	int			i;
	
	va_start (argptr,fmt);
	vsprintf (string, fmt,argptr);
	va_end (argptr);
	
	for (i=0 ; i<svs.maxclients ; i++)
		if (svs.clients[i].active && svs.clients[i].spawned)
		{
			MSG_WriteByte (&svs.clients[i].message, svc_print);
			MSG_WriteString (&svs.clients[i].message, string);
		}
}

/*
=================
Host_ClientCommands

Send text over to the client to be executed
=================
*/
void Host_ClientCommands (char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];
	
	va_start (argptr,fmt);
	vsprintf (string, fmt,argptr);
	va_end (argptr);
	
	MSG_WriteByte (&host_client->message, svc_stufftext);
	MSG_WriteString (&host_client->message, string);
}

/*
=====================
SV_DropClient

Called when the player is getting totally kicked off the host
if (crash = true), don't bother sending signofs
=====================
*/
void SV_DropClient (qboolean crash)
{
	int		saveSelf;
	int		i;
	client_t *client;

	if (!crash)
	{
		// send any final messages (don't check for errors)
		if (NET_CanSendMessage (host_client->netconnection))
		{
			MSG_WriteByte (&host_client->message, svc_disconnect);
			NET_SendMessage (host_client->netconnection, &host_client->message);
		}
	
		if (host_client->edict && host_client->spawned)
		{

		// call the prog function for removing a client
		// this will set the body to a dead frame, among other things

			// jkrige - portals game
			//saveSelf = pr_global_struct->self;
			//pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
			//PR_ExecuteProgram (pr_global_struct->ClientDisconnect);
			//pr_global_struct->self = saveSelf;
			if (old_progdefs)
			{
				saveSelf = pr_global_struct_v111->self;
				pr_global_struct_v111->self = EDICT_TO_PROG(host_client->edict);
				PR_ExecuteProgram (pr_global_struct_v111->ClientDisconnect);
				pr_global_struct_v111->self = saveSelf;
			}
			else
			{
				saveSelf = pr_global_struct->self;
				pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
				PR_ExecuteProgram (pr_global_struct->ClientDisconnect);
				pr_global_struct->self = saveSelf;
			}
			// jkrige - portals game

		}

		Sys_Printf ("Client %s removed\n",host_client->name);
	}

// break the net connection
	NET_Close (host_client->netconnection);
	host_client->netconnection = NULL;

// free the client (the body stays around)
	host_client->active = false;
	host_client->name[0] = 0;
	host_client->old_frags = -999999;
	memset(&host_client->old_v,0,sizeof(host_client->old_v));
	ED_ClearEdict(host_client->edict);
	host_client->send_all_v = true;
	net_activeconnections--;

// send notification to all clients
	for (i=0, client = svs.clients ; i<svs.maxclients ; i++, client++)
	{
		if (!client->active)
			continue;
		MSG_WriteByte (&client->message, svc_updatename);
		MSG_WriteByte (&client->message, host_client - svs.clients);
		MSG_WriteString (&client->message, "");
		MSG_WriteByte (&client->message, svc_updatefrags);
		MSG_WriteByte (&client->message, host_client - svs.clients);
		MSG_WriteShort (&client->message, 0);
		MSG_WriteByte (&client->message, svc_updatecolors);
		MSG_WriteByte (&client->message, host_client - svs.clients);
		MSG_WriteByte (&client->message, 0);
	}
}

/*
==================
Host_ShutdownServer

This only happens at the end of a game, not between levels
==================
*/
void Host_ShutdownServer(qboolean crash)
{
	int		i;
	int		count;
	sizebuf_t	buf;
	char		message[4];
	double	start;

	if (!sv.active)
		return;

	sv.active = false;

// stop all client sounds immediately
	if (cls.state == ca_connected)
		CL_Disconnect ();

// flush any pending messages - like the score!!!
	start = Sys_FloatTime();
	do
	{
		count = 0;
		for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
		{
			if (host_client->active && host_client->message.cursize)
			{
				if (NET_CanSendMessage (host_client->netconnection))
				{
					NET_SendMessage(host_client->netconnection, &host_client->message);
					SZ_Clear (&host_client->message);
				}
				else
				{
					NET_GetMessage(host_client->netconnection);
					count++;
				}
			}
		}
		if ((Sys_FloatTime() - start) > 3.0)
			break;
	}
	while (count);

// make sure all the clients know we're disconnecting
	buf.data = message;
	buf.maxsize = 4;
	buf.cursize = 0;
	MSG_WriteByte(&buf, svc_disconnect);
	count = NET_SendToAll(&buf, 5);
	if (count)
		Con_Printf("Host_ShutdownServer: NET_SendToAll failed for %u clients\n", count);

	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
		if (host_client->active)
			SV_DropClient(crash);

//
// clear structures
//
	memset (&sv, 0, sizeof(sv));
	memset (svs.clients, 0, svs.maxclientslimit*sizeof(client_t));
}


/*
================
Host_ClearMemory

This clears all the memory used by both the client and server, but does
not reinitialize anything.
================
*/
void Host_ClearMemory (void)
{
	Con_DPrintf ("Clearing memory\n");
	D_FlushCaches ();
	Mod_ClearAll ();
	if (host_hunklevel)
		Hunk_FreeToLowMark (host_hunklevel);

	cls.signon = 0;
	memset (&sv, 0, sizeof(sv));
	memset (&cl, 0, sizeof(cl));
}


//============================================================================


/*
===================
Host_FilterTime

Returns false if the time is too short to run a frame
===================
*/
qboolean Host_FilterTime (float time)
{
	// jkrige - configurable fps caps
	static double min_frametime;
	// jkrige - configurable fps caps

	realtime += time;

	// jkrige - configurable fps caps
	// added configurable fps caps, different for dedicated and
    // listen servers. Listen servers run at 72 fps by default,
    // dedicated ones at 20.
    if (cls.state != ca_dedicated)
	{
		min_frametime = 1.0 / cl_maxfps.value;
    }
    else
	{
		min_frametime = 1.0 / sv_fps.value;
    }

    // Avoid infinite loop
    if (min_frametime > 1)
	{
		min_frametime = 1.0;
    }
    if ((realtime - oldrealtime < min_frametime) && !cls.timedemo)
	{
		// Keep the CPU cool for the remainder of time, rather than run
		// a loop delay. And always keep a slack of 15% of frametime for
		// ouselves, so that we don't "oversleep" our next frame.
		if (realtime - oldrealtime < 0.80 * min_frametime)
		{
			Sys_LongSleep((0.85 * min_frametime - (realtime - oldrealtime)) * 1000.0);
		}
		return false;
    }

	//if (!cls.timedemo && realtime - oldrealtime < 1.0/72.0)
	//	return false;		// framerate is too high
    // jkrige - configurable fps caps

	host_frametime = realtime - oldrealtime;
	oldrealtime = realtime;

	if (host_framerate.value > 0)
		host_frametime = host_framerate.value;
	else
	{	// don't allow really long or short frames
		if (host_frametime > 0.05 && !sys_adaptive.value)
			host_frametime = 0.05;

		// jkrige - configurable fps caps
		//		short frames should better be allowed now; it seems
        //		that 1000 fps is in fact attainable nowadays, and we
        //		got rid of framerate cap
		//if (host_frametime < 0.001)
		//	host_frametime = 0.001;
		// jkrige - configurable fps caps
	}
	
	return true;
}


/*
===================
Host_GetConsoleCommands

Add them exactly as if they had been typed at the console
===================
*/
void Host_GetConsoleCommands (void)
{
	char	*cmd;

	while (1)
	{
		cmd = Sys_ConsoleInput ();
		if (!cmd)
			break;
		Cbuf_AddText (cmd);
	}
}


//#define FPS_20

void R_UpdateParticles (void);
void CL_UpdateEffects (void);

/*
==================
Host_ServerFrame

==================
*/
#ifdef FPS_20

void _Host_ServerFrame (void)
{
// run the world state
	// jkrige - portals game
	//pr_global_struct->frametime = host_frametime;
	if (old_progdefs)
		pr_global_struct_v111->frametime = host_frametime;
	else
		pr_global_struct->frametime = host_frametime;
	// jkrige - portals game

// read client messages
	SV_RunClients ();
	
// move things around and think
// always pause in single player if in console or menus
	if (!sv.paused && (svs.maxclients > 1 || key_dest == key_game) )
	{
		SV_Physics ();

		R_UpdateParticles ();
		CL_UpdateEffects ();
	}
}

void Host_ServerFrame (void)
{
	float	save_host_frametime;
	float	temp_host_frametime;

// run the world state
	// jkrige - portals game
	//pr_global_struct->frametime = host_frametime;
	if (old_progdefs)
		pr_global_struct_v111->frametime = host_frametime;
	else
		pr_global_struct->frametime = host_frametime;
	// jkrige - portals game

// set the time and clear the general datagram
	SV_ClearDatagram ();
	
// check for new clients
	SV_CheckForNewClients ();

	temp_host_frametime = save_host_frametime = host_frametime;
	while(temp_host_frametime > (1.0/72.0))
	{
		if (temp_host_frametime > 0.05)
			host_frametime = 0.05;
		else
			host_frametime = temp_host_frametime;
		temp_host_frametime -= host_frametime;
		_Host_ServerFrame ();
	}
	host_frametime = save_host_frametime;

// send all messages to the clients
	SV_SendClientMessages ();
}

#else

void Host_ServerFrame (void)
{
// run the world state
	// jkrige - portals game
	//pr_global_struct->frametime = host_frametime;
	if (old_progdefs)
		pr_global_struct_v111->frametime = host_frametime;
	else
		pr_global_struct->frametime = host_frametime;
	// jkrige - portals game

// set the time and clear the general datagram
	SV_ClearDatagram ();
	
// check for new clients
	SV_CheckForNewClients ();

// read client messages
	SV_RunClients ();
	
// move things around and think
// always pause in single player if in console or menus
	if (!sv.paused && (svs.maxclients > 1 || key_dest == key_game) )
		SV_Physics ();

// send all messages to the clients
	SV_SendClientMessages ();
}

#endif

/*
==================
Host_Frame

Runs all active servers
==================
*/
void _Host_Frame (float time)
{
	static double		time1 = 0;
	static double		time2 = 0;
	static double		time3 = 0;
	int			pass1, pass2, pass3;
#if !defined(FPS_20)
	double	save_host_frametime,total_host_frametime;
#endif

	if (setjmp (host_abortserver) )
		return;			// something bad happened, or the server disconnected

// keep the random time dependent
	rand ();
	
// decide the simulation time
	if (!Host_FilterTime (time))
		return;			// don't run too fast, or packets will flood out
		
// get new key events
	Sys_SendKeyEvents ();

// allow mice or other external controllers to add commands
	IN_Commands ();

// process console commands
	Cbuf_Execute ();

	NET_Poll();

// if running the server locally, make intentions now
	if (sv.active)
		CL_SendCmd ();
	
//-------------------
//
// server operations
//
//-------------------

// check for commands typed to the host
	Host_GetConsoleCommands ();

#ifdef FPS_20
	
	if (sv.active)
		Host_ServerFrame ();

//-------------------
//
// client operations
//
//-------------------

// if running the server remotely, send intentions now after
// the incoming messages have been read
	if (!sv.active)
		CL_SendCmd ();

	host_time += host_frametime;

// fetch results from server
	if (cls.state == ca_connected)
	{
		CL_ReadFromServer ();
	}

#else

	save_host_frametime = total_host_frametime = host_frametime;
	if (sys_adaptive.value)
	{
		if (host_frametime > 0.05) 
		{
			host_frametime = 0.05;
		}
	}

	if (total_host_frametime > 1.0) 
		total_host_frametime = 0.05;

	do
	{
		if (sv.active)
			Host_ServerFrame ();

	//-------------------
	//
	// client operations
	//
	//-------------------

	// if running the server remotely, send intentions now after
	// the incoming messages have been read
		if (!sv.active)
			CL_SendCmd ();

		host_time += host_frametime;

	// fetch results from server
		if (cls.state == ca_connected)
		{
			CL_ReadFromServer ();
		}


		R_UpdateParticles ();
		CL_UpdateEffects ();

		if (!sys_adaptive.value) break;

		total_host_frametime -= 0.05;
		if (total_host_frametime > 0 && total_host_frametime < 0.05) 
		{
			save_host_frametime -= total_host_frametime;
			oldrealtime -= total_host_frametime;
			break;
		}

	} while (total_host_frametime > 0);


	host_frametime = save_host_frametime;

#endif

// update video
	if (host_speeds.value)
		time1 = Sys_FloatTime ();
		
	SCR_UpdateScreen ();

	if (host_speeds.value)
		time2 = Sys_FloatTime ();
		
// update audio
	if (cls.signon == SIGNONS)
	{
		S_Update (r_origin, vpn, vright, vup);
		CL_DecayLights ();
	}
	else
		S_Update (vec3_origin, vec3_origin, vec3_origin, vec3_origin);

	// jkrige - fmod sound system - begin
#ifdef UQE_FMOD
	FMOD_MusicUpdate(NULL);
#else
	CDAudio_Update();
	MIDI_UpdateVolume(); // jkrige - midi volume
#endif
	// jkrige - fmod sound system - end

	if (host_speeds.value)
	{
		pass1 = (time1 - time3)*1000;
		time3 = Sys_FloatTime ();
		pass2 = (time2 - time1)*1000;
		pass3 = (time3 - time2)*1000;
		Con_Printf ("%3i tot %3i server %3i gfx %3i snd\n",
					pass1+pass2+pass3, pass1, pass2, pass3);
	}
	
	host_framecount++;

	fps_count++; // jkrige - fps counter
}

void Host_Frame (float time)
{
	double	time1, time2;
	static double	timetotal;
	static int		timecount;
	int		i, c, m;

	if (!serverprofile.value)
	{
		_Host_Frame (time);
		return;
	}
	
	time1 = Sys_FloatTime ();
	_Host_Frame (time);
	time2 = Sys_FloatTime ();	
	
	timetotal += time2 - time1;
	timecount++;
	
	if (timecount < 1000)
		return;

	m = timetotal*1000/timecount;
	timecount = 0;
	timetotal = 0;
	c = 0;
	for (i=0 ; i<svs.maxclients ; i++)
	{
		if (svs.clients[i].active)
			c++;
	}

	Con_Printf ("serverprofile: %2i clients %2i msec\n",  c,  m);
}

//============================================================================


extern int vcrFile;
#define	VCR_SIGNATURE	0x56435231
// "VCR1"

void Host_InitVCR (quakeparms_t *parms)
{
	int		i, len, n;
	char	*p;
	
	if (COM_CheckParm("-playback"))
	{
		if (com_argc != 2)
			Sys_Error("No other parameters allowed with -playback\n");

		Sys_FileOpenRead("quake.vcr", &vcrFile);
		if (vcrFile == -1)
			Sys_Error("playback file not found\n");

		Sys_FileRead (vcrFile, &i, sizeof(int));
		if (i != VCR_SIGNATURE)
			Sys_Error("Invalid signature in vcr file\n");

		Sys_FileRead (vcrFile, &com_argc, sizeof(int));
		com_argv = malloc(com_argc * sizeof(char *));
		com_argv[0] = parms->argv[0];
		for (i = 0; i < com_argc; i++)
		{
			Sys_FileRead (vcrFile, &len, sizeof(int));
			p = malloc(len);
			Sys_FileRead (vcrFile, p, len);
			com_argv[i+1] = p;
		}
		com_argc++; /* add one for arg[0] */
		parms->argc = com_argc;
		parms->argv = com_argv;
	}

	if ( (n = COM_CheckParm("-record")) != 0)
	{
		vcrFile = Sys_FileOpenWrite("quake.vcr");

		i = VCR_SIGNATURE;
		Sys_FileWrite(vcrFile, &i, sizeof(int));
		i = com_argc - 1;
		Sys_FileWrite(vcrFile, &i, sizeof(int));
		for (i = 1; i < com_argc; i++)
		{
			if (i == n)
			{
				len = 10;
				Sys_FileWrite(vcrFile, &len, sizeof(int));
				Sys_FileWrite(vcrFile, "-playback", len);
				continue;
			}
			len = strlen(com_argv[i]) + 1;
			Sys_FileWrite(vcrFile, &len, sizeof(int));
			Sys_FileWrite(vcrFile, com_argv[i], len);
		}
	}
	
}

/*
====================
Host_Init
====================
*/
void Host_Init (quakeparms_t *parms)
{
//	if (standard_quake)
		minimum_memory = MINIMUM_MEMORY;
//	else
//		minimum_memory = MINIMUM_MEMORY_LEVELPAK;

	if (COM_CheckParm ("-minmemory"))
		parms->memsize = minimum_memory;

	host_parms = *parms;

	if (parms->memsize < minimum_memory)
		Sys_Error ("Only %4.1f megs of memory available, can't execute game", parms->memsize / (float)0x100000);

	com_argc = parms->argc;
	com_argv = parms->argv;

	Memory_Init (parms->membase, parms->memsize);
	Cbuf_Init ();
	Cmd_Init ();	
	V_Init ();

	// jkrige - removed chase
	//Chase_Init ();
	// jkrige - removed chase

	Host_InitVCR (parms);
	COM_Init (parms->basedir);
	Host_InitLocal ();
	W_LoadWadFile ("gfx.wad");
	Key_Init ();
	Con_Init ();	
	M_Init ();	
	PR_Init ();
	Mod_Init ();
	NET_Init ();
	SV_Init ();

	Con_Printf ("Exe: "__TIME__" "__DATE__"\n");
	Con_Printf ("%4.1f megabyte heap\n",parms->memsize/ (1024*1024.0));
	
	R_InitTextures ();		// needed even for dedicated servers
 
	if (cls.state != ca_dedicated)
	{
		host_basepal = (byte *)COM_LoadHunkFile ("gfx/palette.lmp");
		if (!host_basepal)
			Sys_Error ("Couldn't load gfx/palette.lmp");
		host_colormap = (byte *)COM_LoadHunkFile ("gfx/colormap.lmp");
		if (!host_colormap)
			Sys_Error ("Couldn't load gfx/colormap.lmp");

		VID_Init (host_basepal);

		Draw_Init ();
		SCR_Init ();

		R_Init ();

#ifndef	_WIN32
	// on Win32, sound initialization has to come before video initialization, so we
	// can put up a popup if the sound hardware is in use
		S_Init ();
#else

#ifdef	GLQUAKE
	// FIXME: doesn't use the new one-window approach yet
		S_Init ();
#endif

#endif	// _WIN32

		// jkrige - fmod sound system - begin
		//CDAudio_Init();
		//MIDI_Init();
		// jkrige - fmod sound system - end

		SB_Init();
		CL_Init();
		IN_Init();
	}

	Cbuf_InsertText ("exec hexen.rc\n");

	Hunk_AllocName (0, "-HOST_HUNKLEVEL-");
	host_hunklevel = Hunk_LowMark ();

	host_initialized = true;
	
	Sys_Printf("======== Hexen II Initialized =========\n");
}


/*
===============
Host_Shutdown

FIXME: this is a callback from Sys_Quit and Sys_Error.  It would be better
to run quit through here before the final handoff to the sys code.
===============
*/
void Host_Shutdown(void)
{
	static qboolean isdown = false;
	
	if (isdown)
	{
		printf ("recursive shutdown\n");
		return;
	}
	isdown = true;

// keep Con_Printf from trying to update the screen
	scr_disabled_for_loading = true;

	Host_WriteConfiguration ("config.cfg"); 

	// jkrige - fmod sound system - begin
	//CDAudio_Shutdown ();
    //MIDI_Cleanup();
	// jkrige - fmod sound system - end

	NET_Shutdown ();

	// jkrige - fmod sound system - begin
#ifdef UQE_FMOD
	FMOD_Shutdown();
#else
	CDAudio_Shutdown();
	MIDI_Cleanup();
#endif
	// jkrige - fmod sound system - end

	S_Shutdown();
	IN_Shutdown ();

	if (cls.state != ca_dedicated)
	{
		VID_Shutdown();
	}
}

/*
 * $Log: /H2 Mission Pack/HOST.C $
 * 
 * 6     3/12/98 6:31p Mgummelt
 * 
 * 5     3/03/98 1:41p Jmonroe
 * removed old mp stuff
 * 
 * 4     3/01/98 8:20p Jmonroe
 * removed the slow "quake" version of common functions
 * 
 * 3     2/23/98 2:53p Jmonroe
 * made config.cfg save the mlook status
 * 
 * 2     2/13/98 4:08p Jmonroe
 * added saveconfig
 * 
 * 22    9/15/97 11:15a Rjohnson
 * Updates
 * 
 * 21    9/04/97 4:44p Rjohnson
 * Updates
 * 
 * 20    8/27/97 12:13p Rjohnson
 * Clear out the entity when client removed
 * 
 * 19    8/20/97 3:01p Rjohnson
 * Name change
 * 
 * 18    5/19/97 2:54p Rjohnson
 * Added new client effects
 * 
 * 17    5/05/97 5:34p Rjohnson
 * Added a quake2 console variable
 * 
 * 16    4/30/97 11:20p Bgokey
 * 
 * 15    4/04/97 3:06p Rjohnson
 * Networking updates and corrections
 * 
 * 14    3/31/97 7:28p Rjohnson
 * Cleared out the class when a client is dropped
 * 
 * 13    3/31/97 3:17p Rlove
 * Removing references to Quake
 * 
 * 12    3/07/97 1:29p Rjohnson
 * Id Updates
 * 
 * 11    3/05/97 11:18a Rjohnson
 * Made the developer console var to be saved
 * 
 * 10    2/27/97 4:11p Rjohnson
 * Added midi init and cleanup, as well as limiting the adaptavite timing
 * to 1 sec catchup
 * 
 * 9     2/24/97 6:01p Rjohnson
 * Removed weird character
 * 
 * 8     2/24/97 1:50p Rjohnson
 * Looked into id's 20 fps
 * 
 * 7     2/18/97 11:42a Rjohnson
 * Added headers
 */
