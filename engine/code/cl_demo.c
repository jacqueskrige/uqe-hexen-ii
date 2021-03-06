
#include "quakedef.h"

// jkrige - fix demo playback across maps
// Pa3PyX: new variable
int stufftext_frame;
// jkrige - fix demo playback across maps

void CL_FinishTimeDemo (void);

/*
==============================================================================

DEMO CODE

When a demo is playing back, all NET_SendMessages are skipped, and
NET_GetMessages are read from the demo file.

Whenever cl.time gets past the last received message, another message is
read from the demo file.
==============================================================================
*/

/*
==============
CL_StopPlayback

Called when a demo file runs out, or the user starts a game
==============
*/
void CL_StopPlayback (void)
{
	if (!cls.demoplayback)
		return;

	if (intro_playing)
		M_ToggleMenu_f();


	intro_playing=false;
	num_intro_msg=0;

	cls.demoplayback = false;
	cls.state = ca_disconnected;


	// jkrige - pk3 file support
	//fclose (cls.demofile);
	//cls.demofile = NULL;

	if(cls.demobuffer)
		free(cls.demobuffer);

	cls.demobuffer = NULL;
	cls.demobufferposition = 0;
	cls.demobufferlength = 0;
	// jkrige - pk3 file support


	if (cls.timedemo)
		CL_FinishTimeDemo ();


	// jkrige - fmod sound system - begin
	//        - stop demo music playback
#ifdef UQE_FMOD
	FMOD_MusicStop();
#else
	CDAudio_Stop();
	MIDI_Stop();
#endif
	// jkrige - fmod sound system - end
}

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length and view angles
====================
*/
void CL_WriteDemoMessage (void)
{
	int		len;
	int		i;
	float	f;

	len = LittleLong (net_message.cursize);
	fwrite (&len, 4, 1, cls.demofile);
//	fwrite (&len, 4, 1, cls.introdemofile);
	for (i=0 ; i<3 ; i++)
	{
		f = LittleFloat (cl.viewangles[i]);
		fwrite (&f, 4, 1, cls.demofile);
//		fwrite (&f, 4, 1, cls.introdemofile);
	}
	fwrite (net_message.data, net_message.cursize, 1, cls.demofile);
	fflush (cls.demofile);
//	fwrite (net_message.data, net_message.cursize, 1, cls.introdemofile);
//	fflush (cls.introdemofile);
}

/*
====================
CL_GetMessage

Handles recording and playback of demos, on top of NET_ code
====================
*/
// jkrige - pk3 file support
#if 0
int CL_GetMessage (void)
{
	int		r, i;
	float	f;
	
	if	(cls.demoplayback)
	{
	// decide if it is time to grab the next message		
		if (cls.signon == SIGNONS)	// allways grab until fully connected
		{
			// jkrige - fix demo playback across maps
			/* Pa3PyX: always wait for full frame update on stuff
                       messages. If the server stuffs a reconnect,
                       we must wait for the client to re-initialize
                       before accepting further messages. Otherwise
                       demo playback may freeze. */
            if (stufftext_frame == host_framecount) {
                    return 0;
            }
			// jkrige - fix demo playback across maps



			if (cls.timedemo)
			{
				if (host_framecount == cls.td_lastframe)
					return 0;		// allready read this frame's message
				cls.td_lastframe = host_framecount;
			// if this is the second frame, grab the real td_starttime
			// so the bogus time on the first frame doesn't count
				if (host_framecount == cls.td_startframe + 1)
					cls.td_starttime = realtime;
			}
			else if ( /* cl.time > 0 && */ cl.time <= cl.mtime[0])
			{
					return 0;		// don't need another message yet
			}
		}
		
	// get the next message
//		if(intro_playing&&num_intro_msg>0&&num_intro_msg<21)
//			V_DarkFlash_f();//Fade into demo

/*		if(skip_start&&num_intro_msg>3)
		{
			while(num_intro_msg<1110)
			{
				fread (&net_message.cursize, 4, 1, cls.demofile);
				VectorCopy (cl.mviewangles[0], cl.mviewangles[1]);
				for (i=0 ; i<3 ; i++)
				{
					r = fread (&f, 4, 1, cls.demofile);
					cl.mviewangles[0][i] = LittleFloat (f);
				}
				
				net_message.cursize = LittleLong (net_message.cursize);
				num_intro_msg++;
				if (net_message.cursize > MAX_MSGLEN)
					Sys_Error ("Demo message > MAX_MSGLEN");
				r = fread (net_message.data, net_message.cursize, 1, cls.demofile);
				if (r != 1)
				{
					CL_StopPlayback ();
					return 0;
				}
				if(num_intro_msg==174||
					num_intro_msg==178||
					num_intro_msg==428||
					num_intro_msg==553||
					num_intro_msg==1012)
					break;
			}
			if(num_intro_msg==1110)
				skip_start=false;
		}
		else
		{*/
			fread (&net_message.cursize, 4, 1, cls.demofile);
			VectorCopy (cl.mviewangles[0], cl.mviewangles[1]);
			for (i=0 ; i<3 ; i++)
			{
				r = fread (&f, 4, 1, cls.demofile);
				cl.mviewangles[0][i] = LittleFloat (f);
			}
			
			net_message.cursize = LittleLong (net_message.cursize);
			num_intro_msg++;
			if (net_message.cursize > MAX_MSGLEN)
				Sys_Error ("Demo message > MAX_MSGLEN");
			r = fread (net_message.data, net_message.cursize, 1, cls.demofile);
			if (r != 1)
			{
				CL_StopPlayback ();
				return 0;
			}
//		}

//		if (cls.demorecording)
//			CL_WriteDemoMessage ();
	
		return 1;
	}

	while (1)
	{
		r = NET_GetMessage (cls.netcon);
		
		if (r != 1 && r != 2)
			return r;
	
	// discard nop keepalive message
		if (net_message.cursize == 1 && net_message.data[0] == svc_nop)
			Con_Printf ("<-- server to client keepalive\n");
		else
			break;
	}

	if (cls.demorecording)
		CL_WriteDemoMessage ();
	
	return r;
}
#endif
int CL_GetMessage (void)
{
	int		r, i;
	float	f;
	
	if	(cls.demoplayback)
	{
	// decide if it is time to grab the next message		
		if (cls.signon == SIGNONS)	// allways grab until fully connected
		{
			// jkrige - fix demo playback across maps
			/* Pa3PyX: always wait for full frame update on stuff
                       messages. If the server stuffs a reconnect,
                       we must wait for the client to re-initialize
                       before accepting further messages. Otherwise
                       demo playback may freeze. */
            if (stufftext_frame == host_framecount) {
                    return 0;
            }
			// jkrige - fix demo playback across maps



			if (cls.timedemo)
			{
				if (host_framecount == cls.td_lastframe)
					return 0;		// allready read this frame's message
				cls.td_lastframe = host_framecount;
			// if this is the second frame, grab the real td_starttime
			// so the bogus time on the first frame doesn't count
				if (host_framecount == cls.td_startframe + 1)
					cls.td_starttime = realtime;
			}
			else if ( /* cl.time > 0 && */ cl.time <= cl.mtime[0])
			{
					return 0;		// don't need another message yet
			}
		}
		
	// get the next message
//		if(intro_playing&&num_intro_msg>0&&num_intro_msg<21)
//			V_DarkFlash_f();//Fade into demo

/*		if(skip_start&&num_intro_msg>3)
		{
			while(num_intro_msg<1110)
			{
				fread (&net_message.cursize, 4, 1, cls.demofile);
				VectorCopy (cl.mviewangles[0], cl.mviewangles[1]);
				for (i=0 ; i<3 ; i++)
				{
					r = fread (&f, 4, 1, cls.demofile);
					cl.mviewangles[0][i] = LittleFloat (f);
				}
				
				net_message.cursize = LittleLong (net_message.cursize);
				num_intro_msg++;
				if (net_message.cursize > MAX_MSGLEN)
					Sys_Error ("Demo message > MAX_MSGLEN");
				r = fread (net_message.data, net_message.cursize, 1, cls.demofile);
				if (r != 1)
				{
					CL_StopPlayback ();
					return 0;
				}
				if(num_intro_msg==174||
					num_intro_msg==178||
					num_intro_msg==428||
					num_intro_msg==553||
					num_intro_msg==1012)
					break;
			}
			if(num_intro_msg==1110)
				skip_start=false;
		}
		else
		{*/
			//fread (&net_message.cursize, 4, 1, cls.demofile);
		net_message.cursize = ( (cls.demobuffer[cls.demobufferposition + 3] << 24) + (cls.demobuffer[cls.demobufferposition + 2] << 16) + (cls.demobuffer[cls.demobufferposition + 1] << 8) + (cls.demobuffer[cls.demobufferposition + 0]) );
		cls.demobufferposition += 4;
		net_message.cursize = LittleLong (net_message.cursize);

		VectorCopy (cl.mviewangles[0], cl.mviewangles[1]);
		for (i=0 ; i<3 ; i++)
		{
			//r = fread (&f, 4, 1, cls.demofile);
			memcpy(&f, &cls.demobuffer[cls.demobufferposition], 4);
			cls.demobufferposition += 4;
			cl.mviewangles[0][i] = LittleFloat (f);
		}
			
		
		num_intro_msg++;
		if (net_message.cursize > MAX_MSGLEN)
			Sys_Error ("Demo message > MAX_MSGLEN");

		//r = fread (net_message.data, net_message.cursize, 1, cls.demofile);
		//if (r != 1)
		//{
		//	CL_StopPlayback ();
		//	return 0;
		//}

		if(net_message.cursize == 0 || net_message.cursize > (cls.demobufferlength - cls.demobufferposition))
		{
			CL_StopPlayback ();
			return 0;
		}

		for(i = 0; i < net_message.cursize; i++)
		{
			net_message.data[i] = cls.demobuffer[cls.demobufferposition++];
		}


//		}

//		if (cls.demorecording)
//			CL_WriteDemoMessage ();
	
		return 1;
	}

	while (1)
	{
		r = NET_GetMessage (cls.netcon);
		
		if (r != 1 && r != 2)
			return r;
	
	// discard nop keepalive message
		if (net_message.cursize == 1 && net_message.data[0] == svc_nop)
			Con_Printf ("<-- server to client keepalive\n");
		else
			break;
	}

	if (cls.demorecording)
		CL_WriteDemoMessage ();
	
	return r;
}
// jkrige - pk3 file support


/*
====================
CL_Stop_f

stop recording a demo
====================
*/
void CL_Stop_f (void)
{
	if (cmd_source != src_command)
		return;

	if (!cls.demorecording)
	{
		Con_Printf ("Not recording a demo.\n");
		return;
	}

	intro_playing=false;
	num_intro_msg=0;
// write a disconnect message to the demo file
	SZ_Clear (&net_message);
	MSG_WriteByte (&net_message, svc_disconnect);
	CL_WriteDemoMessage ();

// finish up
//	fclose (cls.introdemofile);
//	cls.introdemofile = NULL;
	fclose (cls.demofile);
	cls.demofile = NULL;
	cls.demorecording = false;
	Con_Printf ("Completed demo\n");
}

/*
====================
CL_Record_f

record <demoname> <map> [cd track]
====================
*/
void CL_Record_f (void)
{
	int		c;
	char	name[MAX_OSPATH];
	int		track;

	if (cmd_source != src_command)
		return;

	c = Cmd_Argc();
	if (c != 2 && c != 3 && c != 4)
	{
		Con_Printf ("record <demoname> [<map> [cd track]]\n");
		return;
	}

	if (strstr(Cmd_Argv(1), ".."))
	{
		Con_Printf ("Relative pathnames are not allowed.\n");
		return;
	}

	if (c == 2 && cls.state == ca_connected)
	{
		Con_Printf("Can not record - already connected to server\nClient demo recording must be started before connecting\n");
		return;
	}

// write the forced cd track number, or -1
	if (c == 4)
	{
		track = atoi(Cmd_Argv(3));
		Con_Printf ("Forcing CD track to %i\n", cls.forcetrack);
	}
	else
		track = -1;	

	sprintf (name, "%s/%s", com_gamedir, Cmd_Argv(1));
	
//
// start the map up
//
	if (c > 2)
		Cmd_ExecuteString ( va("map %s", Cmd_Argv(2)), src_command);
	
//
// open the demo file
//
	COM_DefaultExtension (name, ".dem");

	Con_Printf ("recording to %s.\n", name);
	cls.demofile = fopen (name, "wb");
	if (!cls.demofile)
	{
		Con_Printf ("ERROR: couldn't open.\n");
		return;
	}

	cls.forcetrack = track;
	fprintf (cls.demofile, "%i\n", cls.forcetrack);
	
	cls.demorecording = true;
}


/*
====================
CL_PlayDemo_f

play [demoname]
====================
*/

// jkrige - get rid of the menu and/or console
#define	m_none	0	// enumerated menu state from menu.c
extern	int	m_state;
// jkrige - get rid of the menu and/or console

// jkrige - pk3 file support
/*void CL_PlayDemo_f (void)
{
	char	name[256];

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("playdemo <demoname> : plays a demo\n");
		return;
	}

//
// disconnect from server
//
	CL_Disconnect ();
	
//
// open the demo file
//
	strcpy (name, Cmd_Argv(1));
	if(!stricmp(name,"t9"))
	{
		intro_playing=true;
//		skip_start=true;
	}
	else
		intro_playing=false;
	COM_DefaultExtension (name, ".dem");

	Con_Printf ("Playing demo from %s.\n", name);
//	if(intro_playing)
//	{
//		cls.demorecording = true;
//		cls.introdemofile=fopen("t9.dem","wb");
//	}
	COM_FOpenFile (name, &cls.demofile, false);
	if (!cls.demofile)
	{
		Con_Printf ("ERROR: couldn't open.\n");
		cls.demonum = -1;		// stop demo loop
		return;
	}

	cls.demoplayback = true;
	cls.state = ca_connected;
	fscanf (cls.demofile, "%i\n", &cls.forcetrack);

	// jkrige - get rid of the menu and/or console
	if (key_dest == key_console || key_dest == key_menu)
	{
		key_dest = key_game;
		m_state = m_none;
	}
	// jkrige - get rid of the menu and/or console

	// jkrige - moved to CL_PlayDemo_f
	// Get a new message on playback start.
	cls.td_lastframe = -1;
	// jkrige - moved to CL_PlayDemo_f
}*/
void CL_PlayDemo_f (void)
{
	char	name[256];
	int c;

	qboolean neg = false;

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("playdemo <demoname> : plays a demo\n");
		return;
	}

//
// disconnect from server
//
	CL_Disconnect ();
	
//
// open the demo file
//
	strcpy (name, Cmd_Argv(1));
	if(!stricmp(name,"t9"))
	{
		intro_playing=true;
//		skip_start=true;
	}
	else
		intro_playing=false;
	COM_DefaultExtension (name, ".dem");

	Con_Printf ("Playing demo from %s.\n", name);
/*	if(intro_playing)
	{
		cls.demorecording = true;
		cls.introdemofile=fopen("t9.dem","wb");
	}
*/
	cls.demobufferlength = COM_FOpenFile (name, &cls.demofile, false);
	cls.demobufferposition = 0;

	if (/*!cls.demofile |*/ cls.demobufferlength < 1)
	{
		Con_Printf ("ERROR: couldn't open.\n");
		cls.demonum = -1;		// stop demo loop
		return;
	}

	cls.demobuffer = COM_FReadFile(cls.demofile, cls.demobufferlength);
	if (!cls.demobuffer)
		return;

	cls.demoplayback = true;
	cls.state = ca_connected;
	cls.forcetrack = 0;

	while ((c = cls.demobuffer[cls.demobufferposition++]) != '\n')
	{
		if (c == '-')
			neg = true;
		else
			cls.forcetrack = cls.forcetrack * 10 + (c - '0');
	}

	if (neg)
		cls.forcetrack = -cls.forcetrack;


	//fscanf (cls.demofile, "%i\n", &cls.forcetrack);


	// jkrige - get rid of the menu and/or console
	if (key_dest == key_console || key_dest == key_menu)
	{
		key_dest = key_game;
		m_state = m_none;
	}
	// jkrige - get rid of the menu and/or console

	// jkrige - moved to CL_PlayDemo_f
	// Get a new message on playback start.
	cls.td_lastframe = -1;
	// jkrige - moved to CL_PlayDemo_f
}
// jkrige - pk3 file support

/*
====================
CL_FinishTimeDemo

====================
*/
void CL_FinishTimeDemo (void)
{
	int		frames;
	float	time;
	
	cls.timedemo = false;
	
// the first frame didn't count
	frames = (host_framecount - cls.td_startframe) - 1;
	time = realtime - cls.td_starttime;
	if (!time)
		time = 1;
	Con_Printf ("%i frames %5.1f seconds %5.1f fps\n", frames, time, frames/time);
}

/*
====================
CL_TimeDemo_f

timedemo [demoname]
====================
*/
void CL_TimeDemo_f (void)
{
	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("timedemo <demoname> : gets demo speeds\n");
		return;
	}

	// jkrige - get rid of the menu and/or console
	//if (key_dest == key_console || key_dest == key_menu)
	//{
	//	key_dest = key_game;
	//	m_state = m_none;
	//}
	// jkrige - get rid of the menu and/or console

	CL_PlayDemo_f ();
	
// cls.td_starttime will be grabbed at the second frame of the demo, so
// all the loading time doesn't get counted
	
	cls.timedemo = true;
	cls.td_startframe = host_framecount;
	// jkrige - moved to CL_PlayDemo_f()
	//cls.td_lastframe = -1;		// get a new message this frame
	// jkrige - moved to CL_PlayDemo_f()
}

