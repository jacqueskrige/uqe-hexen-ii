
#include "pr_comp.h"			// defs shared with qcc
#include "progdefs.h"			// generated by program cdefs

typedef union eval_s
{
	string_t		string;
	float			_float;
	float			vector[3];
	func_t			function;
	int				_int;
	int				edict;
} eval_t;	

//#if RJNET
//#define MAX_BASELINES 17
//#endif

#define	MAX_ENT_LEAFS	16
typedef struct edict_s
{
	qboolean	free;
	link_t		area;				// linked to a division node or leaf
	
	int			num_leafs;
	short		leafnums[MAX_ENT_LEAFS];

#if RJNET2
	entity_state_t	baseline[MAX_BASELINES];
#else
	entity_state_t	baseline;
#endif
	
	float		freetime;			// sv.time when the object was freed
	float		alloctime;			// sv.time when the object was allocated
	entvars_t	v;					// C exported fields from progs
// other fields from progs come immediately after
} edict_t;
#define	EDICT_FROM_AREA(l) STRUCT_FROM_LINK(l,edict_t,area)

//============================================================================

extern	dprograms_t		*progs;
extern	dfunction_t		*pr_functions;
extern	char			*pr_strings;
extern	ddef_t			*pr_globaldefs;
extern	ddef_t			*pr_fielddefs;
extern	dstatement_t	*pr_statements;
extern	globalvars_t	*pr_global_struct;
// jkrige - portals game
extern	globalvars_v111_t	*pr_global_struct_v111;
extern	qboolean		old_progdefs;	// whether we have a Hexen2-v1.11 globals struct
#define	PR_GLOBAL_STRUCT(parm)	(old_progdefs ? (pr_global_struct_v111->parm) : (pr_global_struct->parm))
// jkrige - portals game
extern	float			*pr_globals;			// same as pr_global_struct

extern	int				pr_edict_size;	// in bytes

extern  int             *pr_string_index;
extern  char			*pr_global_strings;
extern	int				pr_string_count;

// jkrige - portals game
//============================================================================
/*	If USE_MULTIPLE_PROGS is defined as 1, the hexen2 binary will look for
	a file named "maplist.txt" in its searchpath and using the info in it,
	it will load a map-specific prog file. Without this, the rider bosses
	shall not appear in the original Hexen2. The mission pack & HexenWorld
	uses a single prog file, so they don't need this.
	2006-03-31: We now use a single binary for both original hexen2 and
	for the mission pack. So we enable it even for the mission pack, but
	we place a maplist.txt file with a 0 in it in the portals directory.
	If you wish to compile progs.dat and progs2.dat together into a single
	progs.dat for original Hexen2, then keep the definition below intact,
	but put a maplist.txt file with only a 0 in it in the data1 directory.
	This way, if you find an old mod which uses multiple progs.dat files,
	it will continue to work properly.
*/
#define	USE_MULTIPLE_PROGS	1
//============================================================================
// jkrige - portals game

void PR_Init (void);

void PR_ExecuteProgram (func_t fnum);
void PR_LoadProgs (void);
void PR_LoadStrings (void);

void PR_Profile_f (void);

edict_t *ED_Alloc (void);
edict_t *ED_Alloc_Temp (void);
void ED_Free (edict_t *ed);

char	*ED_NewString (char *string);
// returns a copy of the string allocated from the server's string heap

void ED_Print (edict_t *ed);
void ED_Write (FILE *f, edict_t *ed);
char *ED_ParseEdict (char *data, edict_t *ent);

void ED_WriteGlobals (FILE *f);
void ED_ParseGlobals (char *data);

void ED_LoadFromFile (char *data);

//define EDICT_NUM(n) ((edict_t *)(sv.edicts+ (n)*pr_edict_size))
//define NUM_FOR_EDICT(e) (((byte *)(e) - sv.edicts)/pr_edict_size)

edict_t *EDICT_NUM(int n);
int NUM_FOR_EDICT(edict_t *e);

#define	NEXT_EDICT(e) ((edict_t *)( (byte *)e + pr_edict_size))

#define	EDICT_TO_PROG(e) ((byte *)e - (byte *)sv.edicts)
#define PROG_TO_EDICT(e) ((edict_t *)((byte *)sv.edicts + e))

//============================================================================

#define	G_FLOAT(o) (pr_globals[o])
#define	G_INT(o) (*(int *)&pr_globals[o])
#define	G_EDICT(o) ((edict_t *)((byte *)sv.edicts+ *(int *)&pr_globals[o]))
#define G_EDICTNUM(o) NUM_FOR_EDICT(G_EDICT(o))
#define	G_VECTOR(o) (&pr_globals[o])
#define	G_STRING(o) (pr_strings + *(string_t *)&pr_globals[o])
#define	G_FUNCTION(o) (*(func_t *)&pr_globals[o])

#define	E_FLOAT(e,o) (((float*)&e->v)[o])
#define	E_INT(e,o) (*(int *)&((float*)&e->v)[o])
#define	E_VECTOR(e,o) (&((float*)&e->v)[o])
#define	E_STRING(e,o) (pr_strings + *(string_t *)&((float*)&e->v)[o])

extern	int		type_size[8];

typedef void (*builtin_t) (void);
extern	builtin_t *pr_builtins;
extern int pr_numbuiltins;

extern int		pr_argc;

extern	qboolean	pr_trace;
extern	dfunction_t	*pr_xfunction;
extern	int			pr_xstatement;

extern	unsigned short		pr_crc;

void PR_RunError (char *error, ...);

void ED_PrintEdicts (void);
void ED_PrintNum (int ent);

eval_t *GetEdictFieldValue(edict_t *ed, char *field);

extern cvar_t	max_temp_edicts;

extern qboolean ignore_precache;