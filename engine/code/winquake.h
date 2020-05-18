// winquake.h: Win32-specific Quake header file

/*
 * $Header: /H3/game/WINQUAKE.H 5     7/17/97 2:00p Rjohnson $
 */

#pragma warning( disable : 4229 )  // mgraph gets this

#include <windows.h>

#ifndef SERVERONLY
#include <ddraw.h>
#include <dsound.h>
#ifndef GLQUAKE
#include "..\scitech\include\mgraph.h"
#endif
#endif

extern	HINSTANCE	global_hInstance;
extern	int			global_nCmdShow;

#ifndef SERVERONLY

extern LPDIRECTDRAW		lpDD;
extern qboolean			DDActive;
extern LPDIRECTDRAWSURFACE	lpPrimary;
extern LPDIRECTDRAWSURFACE	lpFrontBuffer;
extern LPDIRECTDRAWSURFACE	lpBackBuffer;
extern LPDIRECTDRAWPALETTE	lpDDPal;
extern LPDIRECTSOUND pDS;
extern LPDIRECTSOUNDBUFFER pDSBuf;

extern DWORD gSndBufSize;
//#define SNDBUFSIZE 65536

void	VID_LockBuffer (void);
void	VID_UnlockBuffer (void);

void Snd_AcquireBuffer (void);
void Snd_ReleaseBuffer (void);

#endif

typedef enum {MS_WINDOWED, MS_FULLSCREEN, MS_FULLDIB, MS_FULLDIRECT, MS_UNINIT} modestate_t;

// jkrige - moved to winquake.h (here)
#define MAX_MODE_LIST	30
#ifdef GLQUAKE
#define BASEWIDTH		640
#define BASEHEIGHT		480
#define BASEFOV			90

//#define SCALEHEIGHT		540.0f
//#define SCALEWIDTH		SCALEHEIGHT / 0.5625f // / 0.75f

#else
#define BASEWIDTH		320
#define BASEHEIGHT		240 // jkrige - was 200

//#define SCALEWIDTH		320
//#define SCALEHEIGHT		240 // jkrige - was 200
#endif
// jkrige - moved to winquake.h (here)

// jkrige - moved to winquake.h (here)
#ifdef GLQUAKE
typedef struct {
	modestate_t	type;
	int			width;
	int			height;
	int			modenum;
	int			dib;
	int			fullscreen;
	int			bpp;
	int			halfscreen;
	// jkrige - increased maximum video mode descriptions
	char        modedesc[33];
	//char		modedesc[17];
	// jkrige - increased maximum video mode descriptions
} vmode_t;
#else
typedef struct {
	modestate_t	type;
	int			width;
	int			height;
	int			modenum;
	int			mode13;
	int			stretched;
	int			dib;
	int			fullscreen;
	int			bpp;
	int			halfscreen;
	char		modedesc[13];
} vmode_t;
#endif
// jkrige - moved to winquake.h (here)

// jkrige - scale2d
#ifdef GLQUAKE
extern qpic_t	*conback;
#endif
extern vmode_t	modelist[MAX_MODE_LIST];
// jkrige - scale2d

extern modestate_t	modestate;

extern HWND			mainwindow;
extern qboolean		ActiveApp, Minimized;

//extern qboolean	Win32AtLeastV4, WinNT; // jkrige - remove windows version check
extern qboolean	LegitCopy;

int VID_ForceUnlockedAndReturnState (void);
void VID_ForceLockState (int lk);

void IN_ShowMouse (void);
void IN_DeactivateMouse (void);
void IN_HideMouse (void);
void IN_ActivateMouse (void);
void IN_RestoreOriginalMouseState (void);
void IN_SetQuakeMouseState (void);
void IN_MouseEvent (int mstate);

extern qboolean	winsock_lib_initialized;

extern cvar_t		_windowed_mouse;

// jkrige - scale2d
extern cvar_t		vid_mode;
// jkrige - scale2d


// jkrige - non power of two
extern cvar_t		gl_texture_non_power_of_two;
// jkrige - non power of two

extern int		window_center_x, window_center_y;
extern RECT		window_rect;

extern qboolean	mouseinitialized;
extern HWND		hwnd_dialog;

extern HANDLE	hinput, houtput;

void IN_UpdateClipCursor (void);
void CenterWindow(HWND hWndCenter, int width, int height, BOOL lefttopjustify);

void S_BlockSound (void);
void S_UnblockSound (void);

void VID_SetDefaultMode (void);

int (PASCAL FAR *pWSAStartup)(WORD wVersionRequired, LPWSADATA lpWSAData);
int (PASCAL FAR *pWSACleanup)(void);
int (PASCAL FAR *pWSAGetLastError)(void);
SOCKET (PASCAL FAR *psocket)(int af, int type, int protocol);
int (PASCAL FAR *pioctlsocket)(SOCKET s, long cmd, u_long FAR *argp);
int (PASCAL FAR *psetsockopt)(SOCKET s, int level, int optname,
							  const char FAR * optval, int optlen);
int (PASCAL FAR *precvfrom)(SOCKET s, char FAR * buf, int len, int flags,
							struct sockaddr FAR *from, int FAR * fromlen);
int (PASCAL FAR *psendto)(SOCKET s, const char FAR * buf, int len, int flags,
						  const struct sockaddr FAR *to, int tolen);
int (PASCAL FAR *pclosesocket)(SOCKET s);
int (PASCAL FAR *pgethostname)(char FAR * name, int namelen);
struct hostent FAR * (PASCAL FAR *pgethostbyname)(const char FAR * name);
struct hostent FAR * (PASCAL FAR *pgethostbyaddr)(const char FAR * addr,
												  int len, int type);
int (PASCAL FAR *pgetsockname)(SOCKET s, struct sockaddr FAR *name,
							   int FAR * namelen);

/*
 * $Log: /H3/game/WINQUAKE.H $
 * 
 * 5     7/17/97 2:00p Rjohnson
 * Added a security means to control the running of the game
 * 
 * 4     3/07/97 2:34p Rjohnson
 * Id Updates
 */
