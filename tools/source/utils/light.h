// structure of the light lump

#include "cmdlib.h"
#include "mathlib.h"
#include "bspfile.h"
#include "entities.h"
#include "threads.h"

#ifdef _WIN32
#include <windows.h>
#endif

#define	ON_EPSILON	0.1
#define ANGLE_EPSILON 0.001 // jkrige

#define	MAXLIGHTS			1024

void LoadNodes (char *file);
qboolean TestLine (vec3_t start, vec3_t stop);
qboolean TestSky (vec3_t start, vec3_t dirn); // jkrige

//void LightFace (int surfnum);
void LightFace (int surfnum, qboolean nolight, vec3_t faceoffset); // jkrige
void LightLeaf (dleaf_t *leaf);

void MakeTnodes (dmodel_t *bm);

extern	float		scaledist;
extern	float		scalecos;
extern	float		rangescale;

// jkrige
extern int    worldminlight;
#ifdef HX2COLOR
extern vec3_t minlight_color;
#endif
//extern int sunlight;
//extern vec3_t sunlight_color;
//extern vec3_t sunmangle;
// jkrige

extern	int		c_culldistplane, c_proper;

byte *GetFileSpace (int size);
extern	byte		*filebase;

extern	vec3_t	bsp_origin;
extern	vec3_t	bsp_xvector;
extern	vec3_t	bsp_yvector;

void TransformSample (vec3_t in, vec3_t out);
void RotateSample (vec3_t in, vec3_t out);

extern	qboolean	extrasamples;
// jkrige
extern qboolean facecounter;
#ifdef HX2COLOR
extern qboolean colored;
#endif
extern qboolean nominlimit;
extern qboolean noneglight; // jkrige - no negative light
// jkrige

extern	float		minlights[MAX_MAP_FACES];
