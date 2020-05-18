// lighting.c

#include "light.h"

/*

NOTES
-----

*/

float		scaledist = 1.0F;
float		scalecos = 0.5F;
float		rangescale = 0.5F;

// jkrige
int    worldminlight  = 0;
#ifdef HX2COLOR
vec3_t minlight_color = { 255, 255, 255 }; // defaults to white light
#endif
//int sunlight = 0;
//vec3_t sunlight_color = { 255, 255, 255 };	// defaults to white light
//vec3_t sunmangle = { 0, 0, 16384 };	// defaults to straight down
// jkrige

byte		*filebase, *file_p, *file_end;

dmodel_t	*bspmodel;
int			bspfileface;	// next surface to dispatch

vec3_t	bsp_origin;

qboolean	extrasamples;

// jkrige
qboolean facecounter;
#ifdef HX2COLOR
qboolean colored;
#endif
qboolean nominlimit;
qboolean noneglight; // jkrige - no negative lights
qboolean nolightface[MAX_MAP_FACES];
vec3_t faceoffset[MAX_MAP_FACES];
// jkrige

//float		minlights[MAX_MAP_FACES];


byte *GetFileSpace (int size)
{
	byte	*buf;
	
	LOCK;
	file_p = (byte *)(((long)file_p + 3)&~3);
	buf = file_p;
	file_p += size;
	UNLOCK;
	if (file_p > file_end)
		logerror ("GetFileSpace: overrun"); // jkrige - was Error()
	return buf;
}


void LightThread (void *junk)
{
	int			i;
	logprint("Thread %d started\n",(int)junk);
	while (1)
	{
		LOCK;
		i = bspfileface++;
		UNLOCK;

		// jkrige - progress
		if (!facecounter)
            printf("Lighting face %i of %i\r", i, numfaces); // jkrige
		fflush (stdout);
		// jkrige - progress

		if (i >= numfaces)
		{
			logprint ("Lighting face %i of %i\n", i, numfaces);
			logprint ("Lighting Completed.\n\n"); // jkrige
			return;
		}
		
		//LightFace (i);
		LightFace (i, nolightface[i], faceoffset[i]); // jkrige
	}
}

// jkrige
void FindFaceOffsets(void)
{
    int i, j;
    entity_t *ent;
    char name[20];
    char *classname;
    vec3_t org;

    memset(nolightface, 0, sizeof(nolightface));

    for (j = dmodels[0].firstface; j < dmodels[0].numfaces; j++)
		nolightface[j] = 0;

    for (i = 1; i < nummodels; i++)
	{
		sprintf(name, "*%d", i);
		ent = FindEntityWithKeyPair("model", name);
		
		if (!ent)
			logerror("%s: Couldn't find entity for model %s.\n", __FUNCTION__, name); // jkrige - was Error()
		
		classname = ValueForKey(ent, "classname");
		if (!strncmp(classname, "rotate_", 7))
		{
			int start;
			int end;
			
			GetVectorForKey(ent, "origin", org);
			
			start = dmodels[i].firstface;
			end = start + dmodels[i].numfaces;
			
			for (j = start; j < end; j++)
			{
				nolightface[j] = 300;
				faceoffset[j][0] = org[0];
				faceoffset[j][1] = org[1];
				faceoffset[j][2] = org[2];
			}
		}
	}
}
// jkrige

/*
=============
LightWorld
=============
*/
void LightWorld (void)
{
	filebase = file_p = dlightdata;
	file_end = filebase + MAX_MAP_LIGHTING;

	RunThreadsOn (LightThread);

	lightdatasize = file_p - filebase;
	
	logprint ("lightdatasize: %i\n", lightdatasize);
}

// jkrige
FILE	*litfile; // used in lightface to write to extended light file
// jkrige

/*
========
main

light modelfile
========
*/
int main (int argc, char **argv)
{
	int		i;
	double		start, end;
	char		source[1024];
#ifdef HX2COLOR
	char   litfilename[1024]; // jkrige
#endif

	// jkrige
	SetConsoleTitle("UQE Hexen II LIGHT");

	init_log("uqehx2light.log");
	logprint("UQEHX2LIGHT 1.18 -- based on LIGHT by John Carmack\n");
    logprint("===============================================\n");
	logprint("Modified by Jacques Krige\n");
	logprint("Build: %s, %s\n", __TIME__, __DATE__);
    logprint("http://www.corvinstein.com\n");
	logprint("===============================================\n\n");
	// jkrige

#ifdef __alpha
#ifdef _WIN32
	logprint ("----- LightFaces ----\n");
	logprint("alpha, win32\n");
#else
	logprint ("----- LightFaces ----\n");
	logprint("alpha\n");
#endif
#else
	logprint ("----- LightFaces ----\n");
#endif
	for (i=1 ; i<argc ; i++)
	{
		if (!strcmp(argv[i],"-threads"))
		{
			numthreads = atoi (argv[i+1]);
			i++;
		}
		else if (!strcmp(argv[i],"-extra"))
		{
			extrasamples = true;
			logprint ("extra sampling enabled\n");
		}
		else if (!strcmp(argv[i],"-dist"))
		{
			scaledist = (float)atof (argv[i+1]);
			i++;
		}
		else if (!strcmp(argv[i],"-range"))
		{
			rangescale = (float)atof (argv[i+1]);
			i++;
		}
		else if (!strcmp(argv[i],"-minlight")) // jkrige
		{
            worldminlight = (int)atof (argv[i+1]);
            i++;
		}
		else if (!strcmp(argv[i],"-nocount")) // jkrige
		{
            facecounter = true;
		}
#ifdef HX2COLOR
		else if (!strcmp(argv[i],"-colored")) // jkrige
		{
            colored = true;
			logprint(".lit file output enabled\n");
        }
#endif
		else if (!strcmp(argv[i],"-nominlimit")) // jkrige
		{
            nominlimit = true;
        }
		else if (!strcmp(argv[i],"-noneglight")) // jkrige - no negative lights
		{
            noneglight = true;
        }
		else if (argv[i][0] == '-')
			logerror ("uqehx2light: Unknown option \"%s\"", argv[i]); // jkrige - was Error()
		else
			break;
	}

	// jkrige
	//if (i != argc - 1)
	//	Error ("usage: light [-threads num] [-extra] [-dist ?] [-range ?] bspfile");
	if (i != argc - 1)
        logerror ("usage: uqehx2light [-threads num] [-minlight num] [-extra]\n"
#ifdef HX2COLOR
               "               [-nocount] [-colored] [-nominlimit] bspfile"); // jkrige - was Error()
#else
               "               [-nocount] [-nominlimit] [-noneglight] bspfile"); // jkrige - was Error()
#endif
	// jkrige

	InitThreads ();

	start = I_FloatTime ();

	strcpy (source, argv[i]);
	StripExtension (source);

#ifdef HX2COLOR
	// jkrige
	if (colored)
		strcpy (litfilename, source);
	// jkrige
#endif

	DefaultExtension (source, ".bsp");

#ifdef HX2COLOR
	// jkrige
	if (colored)
	{
		DefaultExtension (litfilename, ".lit");
		litfile = fopen(litfilename, "wb");
		// LordHavoc: write out the HLIT header
		fputc('H', litfile);
		fputc('L', litfile);
		fputc('I', litfile);
		fputc('T', litfile);
		fputc(1, litfile);
		fputc(0, litfile);
		fputc(0, litfile);
		fputc(0, litfile);
	}
	// jkrige
#endif
	
	LoadBSPFile (source);
	LoadEntities ();
		
	MakeTnodes (&dmodels[0]);

	FindFaceOffsets(); // jkrige

	LightWorld ();

	WriteEntitiesToString ();	
	WriteBSPFile (source);

#ifdef HX2COLOR
	// jkrige
	if (colored)
		if (litfile)
			fclose(litfile);
	// jkrige
#endif

	end = I_FloatTime ();
	logprint ("%5.1f seconds elapsed\n", end-start);
	
	return 0;
}

