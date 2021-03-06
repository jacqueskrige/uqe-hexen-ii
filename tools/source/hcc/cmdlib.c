
//**************************************************************************
//**
//** misc.c
//**
//** $Header: /H2 Mission Pack/Tools/source5.0/hcc/CMDLIB.C 1     1/26/98 12:25p Jmonroe $
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <sys/types.h>
#include <sys/stat.h>
#ifdef WIN32
#include <direct.h>
#endif
#include "hcc.h"

// MACROS ------------------------------------------------------------------

#define PATHSEPERATOR '/'

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int myargc;
char **myargv;
char ms_Token[1024];
qboolean com_eof;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// MS_Error
//
// For abnormal program terminations.
//
//==========================================================================
// jkrige - logerror
/*
void MS_Error(char *error, ...)
{
	va_list argptr;

	logprint("************ ERROR ************\n");
	va_start(argptr, error);
	vprintf(error, argptr);
	va_end(argptr);
	logprint("\n");
	exit(1);
}
*/
// jkrige - logerror
//==========================================================================
//
// MS_ParseError
//
//==========================================================================

void MS_ParseError(char *error, ...)
{
	va_list argptr;
	char string[1024];

	va_start(argptr, error);
	vsprintf(string, error, argptr);
	va_end(argptr);
	logprint("%s(%d) : %s\n", strings+s_file, lx_SourceLine, string);
	longjmp(pr_parse_abort, 1);
}

//==========================================================================
//
// MS_ParseWarning
//
//==========================================================================

void MS_ParseWarning(char *error, ...)
{
	va_list argptr;
	char string[1024];

	if(hcc_WarningsActive == false)
	{
		return;
	}
	va_start(argptr, error);
	vsprintf(string, error, argptr);
	va_end(argptr);
	logprint("%s(%d) : warning : %s\n", strings+s_file, lx_SourceLine, string);
}

//==========================================================================
//
// MS_GetTime
//
//==========================================================================

double MS_GetTime(void)
{
	time_t	t;

	time(&t);
	return t;
}

//==========================================================================
//
// MS_Parse
//
//==========================================================================

char *MS_Parse(char *data)
{
	int c;
	int len;
	qboolean done;

	len = 0;
	ms_Token[0] = 0;

	if(!data)
	{
		return NULL;
	}

	done = false;
	do
	{
		// Skip whitespace
		while((c = *data) <= ' ')
		{
			if(c == 0)
			{ // EOF
				com_eof = true;
				return NULL;
			}
			data++;
		}

		if(c == '/' && data[1] == '*')
		{ // Skip comment
			data += 2;
			while(!(*data == '*' && data[1] == '/'))
			{
				if(*data == 0)
				{ // EOF
					com_eof = true;
					return NULL;
				}
				data++;
			}
			data += 2;
		}
		else if(c == '/' && data[1] == '/')
		{ // Skip CPP comment
			while(*data && *data != '\n')
			{
				data++;
			}
		}
		else
		{
			done = true;
		}
	} while(done == false);

	if(c == '\"')
	{ // Parse quoted string
		data++;
		do
		{
			c = *data++;
			if (c=='\"')
			{
				ms_Token[len] = 0;
				return data;
			}
			ms_Token[len] = c;
			len++;
		} while (1);
	}

	if(c == '{' || c == '}'|| c == '('|| c == ')'
		|| c == '\'' || c == ':')
	{ // Parse special character
		ms_Token[len] = c;
		len++;
		ms_Token[len] = 0;
		return data+1;
	}

	// Parse regular word
	do
	{
		ms_Token[len] = c;
		data++;
		len++;
		c = *data;
		if(c == '{' || c == '}' || c == '(' || c == ')'
			|| c == '\'' || c == ':')
		{
			break;
		}
	} while(c > 32);

	ms_Token[len] = 0;
	return data;
}

//==========================================================================
//
// MS_StringCompare
//
// Compare two strings without regard to case.
//
//==========================================================================

int MS_StringCompare(char *s1, char *s2)
{
	for(; tolower(*s1) == tolower(*s2); s1++, s2++)
	{
		if(*s1 == '\0')
		{
			return 0;
		}
	}
	return tolower(*s1)-tolower(*s2);
}

//==========================================================================
//
// MS_CheckParm
//
// Checks for the given parameter in the program's command line arguments.
// Returns the argument number (1 to argc-1) or 0 if not present.
//
//==========================================================================

int MS_CheckParm(char *check)
{
	int i;

	for(i = 1; i < myargc; i++)
	{
		if(!MS_StringCompare(check, myargv[i]))
		{
			return i;
		}
	}
	return 0;
}

//==========================================================================
//
// MS_FileLength
//
//==========================================================================

int MS_FileLength(FILE *f)
{
	int pos;
	int end;

	pos = ftell(f);
	fseek(f, 0, SEEK_END);
	end = ftell (f);
	fseek(f, pos, SEEK_SET);
	return end;
}

//==========================================================================
//
// MS_SafeOpenWrite
//
//==========================================================================

FILE *MS_SafeOpenWrite(char *filename)
{
	FILE *f;

	f = fopen(filename, "wb");
	if(!f)
	{
		logerror ("Error opening %s: %s", filename, strerror(errno)); // jkrige - was MS_Error()
	}
	return f;
}

//==========================================================================
//
// MS_SafeOpenRead
//
//==========================================================================

FILE *MS_SafeOpenRead(char *filename)
{
	FILE *f;

	f = fopen(filename, "rb");
	if(!f)
	{
		logerror ("Error opening %s: %s", filename, strerror(errno)); // jkrige - was MS_Error()
	}
	return f;
}

//==========================================================================
//
// MS_SafeRead
//
//==========================================================================

void MS_SafeRead(FILE *f, void *buffer, int count)
{
	if(fread(buffer, 1, count, f) != (size_t)count)
	{
		logerror ("File read failure"); // jkrige - was MS_Error()
	}
}

//==========================================================================
//
// MS_SafeWrite
//
//==========================================================================

void MS_SafeWrite(FILE *f, void *buffer, int count)
{
	if(fwrite(buffer, 1, count, f) != (size_t)count)
	{
		logerror ("File read failure"); // jkrige - was MS_Error()
	}
}

//==========================================================================
//
// MS_LoadFile
//
//==========================================================================

int MS_LoadFile(char *filename, void **bufferptr)
{
	FILE *f;
	int length;
	void *buffer;

	f = MS_SafeOpenRead(filename);
	length = MS_FileLength(f);
	buffer = malloc(length+1);
	((char *)buffer)[length] = 0;
	MS_SafeRead(f, buffer, length);
	fclose (f);
	*bufferptr = buffer;
	return length;
}

//==========================================================================
//
// CRC Functions
//
// This is a 16 bit, non-reflected CRC using the polynomial 0x1021 and the
// initial and final xor values shown below...  in other words, the CCITT
// standard CRC used by XMODEM.
//
//==========================================================================

#define CRC_INIT_VALUE 0xffff
#define CRC_XOR_VALUE 0x0000

static unsigned short crctable[256] =
{
	0x0000,	0x1021,	0x2042,	0x3063,	0x4084,	0x50a5,	0x60c6,	0x70e7,
	0x8108,	0x9129,	0xa14a,	0xb16b,	0xc18c,	0xd1ad,	0xe1ce,	0xf1ef,
	0x1231,	0x0210,	0x3273,	0x2252,	0x52b5,	0x4294,	0x72f7,	0x62d6,
	0x9339,	0x8318,	0xb37b,	0xa35a,	0xd3bd,	0xc39c,	0xf3ff,	0xe3de,
	0x2462,	0x3443,	0x0420,	0x1401,	0x64e6,	0x74c7,	0x44a4,	0x5485,
	0xa56a,	0xb54b,	0x8528,	0x9509,	0xe5ee,	0xf5cf,	0xc5ac,	0xd58d,
	0x3653,	0x2672,	0x1611,	0x0630,	0x76d7,	0x66f6,	0x5695,	0x46b4,
	0xb75b,	0xa77a,	0x9719,	0x8738,	0xf7df,	0xe7fe,	0xd79d,	0xc7bc,
	0x48c4,	0x58e5,	0x6886,	0x78a7,	0x0840,	0x1861,	0x2802,	0x3823,
	0xc9cc,	0xd9ed,	0xe98e,	0xf9af,	0x8948,	0x9969,	0xa90a,	0xb92b,
	0x5af5,	0x4ad4,	0x7ab7,	0x6a96,	0x1a71,	0x0a50,	0x3a33,	0x2a12,
	0xdbfd,	0xcbdc,	0xfbbf,	0xeb9e,	0x9b79,	0x8b58,	0xbb3b,	0xab1a,
	0x6ca6,	0x7c87,	0x4ce4,	0x5cc5,	0x2c22,	0x3c03,	0x0c60,	0x1c41,
	0xedae,	0xfd8f,	0xcdec,	0xddcd,	0xad2a,	0xbd0b,	0x8d68,	0x9d49,
	0x7e97,	0x6eb6,	0x5ed5,	0x4ef4,	0x3e13,	0x2e32,	0x1e51,	0x0e70,
	0xff9f,	0xefbe,	0xdfdd,	0xcffc,	0xbf1b,	0xaf3a,	0x9f59,	0x8f78,
	0x9188,	0x81a9,	0xb1ca,	0xa1eb,	0xd10c,	0xc12d,	0xf14e,	0xe16f,
	0x1080,	0x00a1,	0x30c2,	0x20e3,	0x5004,	0x4025,	0x7046,	0x6067,
	0x83b9,	0x9398,	0xa3fb,	0xb3da,	0xc33d,	0xd31c,	0xe37f,	0xf35e,
	0x02b1,	0x1290,	0x22f3,	0x32d2,	0x4235,	0x5214,	0x6277,	0x7256,
	0xb5ea,	0xa5cb,	0x95a8,	0x8589,	0xf56e,	0xe54f,	0xd52c,	0xc50d,
	0x34e2,	0x24c3,	0x14a0,	0x0481,	0x7466,	0x6447,	0x5424,	0x4405,
	0xa7db,	0xb7fa,	0x8799,	0x97b8,	0xe75f,	0xf77e,	0xc71d,	0xd73c,
	0x26d3,	0x36f2,	0x0691,	0x16b0,	0x6657,	0x7676,	0x4615,	0x5634,
	0xd94c,	0xc96d,	0xf90e,	0xe92f,	0x99c8,	0x89e9,	0xb98a,	0xa9ab,
	0x5844,	0x4865,	0x7806,	0x6827,	0x18c0,	0x08e1,	0x3882,	0x28a3,
	0xcb7d,	0xdb5c,	0xeb3f,	0xfb1e,	0x8bf9,	0x9bd8,	0xabbb,	0xbb9a,
	0x4a75,	0x5a54,	0x6a37,	0x7a16,	0x0af1,	0x1ad0,	0x2ab3,	0x3a92,
	0xfd2e,	0xed0f,	0xdd6c,	0xcd4d,	0xbdaa,	0xad8b,	0x9de8,	0x8dc9,
	0x7c26,	0x6c07,	0x5c64,	0x4c45,	0x3ca2,	0x2c83,	0x1ce0,	0x0cc1,
	0xef1f,	0xff3e,	0xcf5d,	0xdf7c,	0xaf9b,	0xbfba,	0x8fd9,	0x9ff8,
	0x6e17,	0x7e36,	0x4e55,	0x5e74,	0x2e93,	0x3eb2,	0x0ed1,	0x1ef0
};

void MS_CRCInit(unsigned short *crcvalue)
{
	*crcvalue = CRC_INIT_VALUE;
}

void MS_CRCProcessByte(unsigned short *crcvalue, byte data)
{
	*crcvalue = (*crcvalue << 8) ^ crctable[(*crcvalue >> 8) ^ data];
}

unsigned short MS_CRCValue(unsigned short crcvalue)
{
	return crcvalue ^ CRC_XOR_VALUE;
}

//==========================================================================
//
// MS_Hash
//
//==========================================================================

int MS_Hash(char *key)
{
	int i;
	int length;
	char *keyBack;
	unsigned short hash;

	length = strlen(key);
	keyBack = key+length-1;
	hash = CRC_INIT_VALUE;
	if(length > 20)
	{
		length = 20;
	}
	for(i = 0; i < length; i++)
	{
		hash = (hash<<8)^crctable[(hash>>8)^*key++];
		if(++i >= length)
		{
			break;
		}
		hash = (hash<<8)^crctable[(hash>>8)^*keyBack--];
	}
	return hash%MS_HASH_TABLE_SIZE;
}
