
//**************************************************************************
//**
//** hcc.h
//**
//** $Header: /H2 Mission Pack/Tools/hcc/hcc.H 2     1/28/98 12:01p Jmonroe $
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <setjmp.h>
#include "pr_comp.h"
#include "tyrlog.h" // jkrige

// jkrige
#ifdef _WIN32
#include <windows.h>
#endif
// jkrige

// MACROS ------------------------------------------------------------------

// jkrige - capability increase : hc
/*#define MAX_STRINGS 500000
#define MAX_GLOBALS 16384
#define MAX_FIELDS 1024
#define MAX_STATEMENTS 65536*2    // new limit is 2147483647
#define MAX_FUNCTIONS 8192
#define MAX_SOUNDS 1024
#define MAX_MODELS 1024
#define MAX_FILES 1024
#define MAX_DATA_PATH 64
#define MAX_ERRORS 10
#define MAX_NAME 64
#define MAX_REGS 65536
#define MS_HASH_TABLE_SIZE 9973*/
#define MAX_STRINGS 500000
#define MAX_GLOBALS 16384*2
#define MAX_FIELDS 1024*2
#define MAX_STATEMENTS 65536*2    // new limit is 2147483647
#define MAX_FUNCTIONS 8192*2
#define MAX_SOUNDS 1024*2
#define MAX_MODELS 1024*2
#define MAX_FILES 1024*2
#define MAX_DATA_PATH 64
#define MAX_ERRORS 10
#define MAX_NAME 64
#define MAX_REGS 65536*2
#define MS_HASH_TABLE_SIZE 9973*2
// jkrige - capability increase : hc
#define TK_CHECK(t) (pr_tokenclass==t?(LX_Fetch(),true):(false))
#define TK_TEST(t) (pr_tokenclass==t)
#define G_FLOAT(o) (pr_globals[o])
#define G_INT(o) (*(int *)&pr_globals[o])
#define G_VECTOR(o) (&pr_globals[o])
#define G_STRING(o) (strings + *(string_t *)&pr_globals[o])
#define G_FUNCTION(o) (*(func_t *)&pr_globals[o])
#define TOP_PRIORITY 6
#define NOT_PRIORITY 4

// TYPES -------------------------------------------------------------------

typedef int	gofs_t;

#ifndef __BYTEBOOL__
#define __BYTEBOOL__
typedef enum { false, true } qboolean;
typedef unsigned char byte;
#endif

typedef struct type_s
{
	etype_t type;
	struct def_s *def; // a def that points to this type
	struct type_s *next;

	struct type_s *aux_type; // Return type or field type
	int num_parms; // -1 = variable args
	struct type_s *parm_types[MAX_PARMS]; // only [num_parms] allocated
} type_t;

typedef struct def_s
{
	type_t *type;
	char *name;
	struct def_s *next;
	gofs_t ofs;
	struct def_s *scope; // function the var was defined in, or NULL
	int initialized; // 1 when a declaration included "= immediate"
	int referenceCount;
	struct def_s *parentVector;
} def_t;

typedef union eval_s
{
	string_t string;
	float _float;
	float vector[3];
	func_t function;
	int _int;
	union eval_s *ptr;
} eval_t;

typedef struct
{
	type_t *types;
	def_t def_head;
	def_t *def_tail;
	int size_fields;
} pr_info_t;

typedef struct
{
	int					builtin;	// if non 0, call an internal function
	int					code;		// first statement
//	char				*file;		// source file with definition
//	int					file_line;
	struct def_s		*def;
	int					parm_ofs[MAX_PARMS];
} function_t;

typedef struct
{
	char *name;
	char *opname;
	//float priority;
	int priority;
	qboolean right_associative;
	def_t *type_a, *type_b, *type_c;
	int tag;
} opcode_t;

typedef enum
{
	tt_eof,
	tt_name,
	tt_punct,
	tt_immediate
} token_type_t;

enum
{
	TK_NONE,
	TK_SEMICOLON,
	TK_LPAREN,
	TK_RPAREN,
	TK_COMMA,
	TK_PLUS,
	TK_ASTERISK,
	TK_SLASH,
	TK_LBRACE,
	TK_RBRACE,
	TK_LBRACKET,
	TK_RBRACKET,
	TK_NUMBERSIGN,
	TK_EQ,
	TK_NE,
	TK_LT,
	TK_GT,
	TK_LE,
	TK_GE,
	TK_ASSIGN,
	TK_AND,
	TK_BITAND,
	TK_OR,
	TK_BITOR,
	TK_NOT,
	TK_PERIOD,
	TK_ELLIPSIS,
	TK_MINUS,
	TK_INC,
	TK_DEC,
	TK_MULASSIGN,
	TK_DIVASSIGN,
	TK_ADDASSIGN,
	TK_SUBASSIGN,
	TK_COLON,
	TK_RANGE,
	TK_BITSET,
	TK_BITCLR
};

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// hcc.c

extern int hcc_OptimizeImmediates;
extern int hcc_OptimizeNameTable;
extern qboolean hcc_WarningsActive;
extern qboolean hcc_ShowUnrefFuncs;

// lexi.c

void LX_Init(void);
void LX_NewSourceFile(char *fileText);
void LX_Fetch(void);
qboolean LX_Check(char *string);
qboolean LX_CheckFetch(char *string);
void LX_Require(char *string);
void LX_NewLine(void);
void LX_ErrorRecovery(void);

// comp.c

void CO_Init(void);
qboolean CO_CompileFile(char *fileText, char *fileName);
def_t *CO_GenCode(opcode_t *op, def_t *var_a, def_t *var_b);
void CO_GenCodeDirect(opcode_t *op, def_t *var_a, def_t *var_b,
	def_t *var_c);
def_t *CO_ParseImmediate(void);
void CO_ParseDefs(void);
def_t *PR_GetDef(type_t *type, char *name, def_t *scope,
	qboolean allocate);

// expr.c

extern int ex_FunctionCallCount;

void EX_Init(void);
def_t *EX_Expression(int priority);

// stmt.c

extern type_t *st_ReturnType;
extern qboolean st_ReturnParsed;

void ST_ParseStatement(void);

// misc.c

int MS_FileLength(FILE *f);
double MS_GetTime(void);
//void MS_Error (char *error, ...); // jkrige - logerror
void MS_ParseError(char *error, ...);
void MS_ParseWarning(char *error, ...);
int MS_CheckParm(char *check);
FILE *MS_SafeOpenWrite(char *filename);
FILE *MS_SafeOpenRead(char *filename);
void MS_SafeRead(FILE *f, void *buffer, int count);
void MS_SafeWrite(FILE *f, void *buffer, int count);
int MS_LoadFile(char *filename, void **bufferptr);
char *MS_Parse(char *data);
void MS_CRCInit(unsigned short *crcvalue);
void MS_CRCProcessByte(unsigned short *crcvalue, byte data);
unsigned short MS_CRCValue(unsigned short crcvalue);
int MS_Hash(char *string);

// PUBLIC DATA DECLARATIONS ------------------------------------------------

extern int myargc;
extern char **myargv;

extern char ms_Token[1024];
extern qboolean com_eof;


extern	int		type_size[8];
extern	def_t	*def_for_type[8];

extern type_t type_void, type_string, type_float, type_vector, type_entity,
	type_field, type_function, type_pointer, type_floatfield, type_union;

extern def_t def_void, def_string, def_float, def_vector, def_entity,
	def_field, def_function, def_pointer;

extern	pr_info_t	pr;

extern	opcode_t	pr_opcodes[128];		// sized by initialization

extern	def_t		*pr_global_defs[MAX_REGS];	// to find def for a global variable

extern int pr_tokenclass;
extern	char		pr_token[2048];
extern	token_type_t	pr_token_type;
extern	type_t		*pr_immediate_type;
extern	eval_t		pr_immediate;

void PR_PrintStatement (dstatement_t *s);

type_t *PR_FindType(type_t *type);
type_t *PR_ParseType(void);
char *PR_ParseName(void);

extern int locals_end; // for tracking local variables vs temps

extern	jmp_buf		pr_parse_abort;		// longjump with this on parse error

extern int lx_SourceLine;

extern	char		*pr_file_p;

extern	def_t	*pr_scope;
extern	int		pr_error_count;

extern	char		pr_parm_names[MAX_PARMS][MAX_NAME];

char *PR_ValueString (etype_t type, void *val);

extern	string_t	s_file;			// filename for function definition

extern	def_t	def_ret, def_parms[MAX_PARMS];

extern	char	strings[MAX_STRINGS];
extern	int		strofs;

extern	dstatement_t	statements[MAX_STATEMENTS];
extern	int			numstatements;
extern	int			statement_linenums[MAX_STATEMENTS];

extern	dfunction_t	functions[MAX_FUNCTIONS];
extern	int			numfunctions;

extern	float		pr_globals[MAX_REGS];
extern	int			numpr_globals;

extern	char	pr_immediate_string[2048];

extern	char		precache_sounds[MAX_SOUNDS][MAX_DATA_PATH];
extern	int			precache_sounds_block[MAX_SOUNDS];
extern	int			numsounds;

extern	char		precache_models[MAX_MODELS][MAX_DATA_PATH];
extern	int			precache_models_block[MAX_SOUNDS];
extern	int			nummodels;

extern	char		precache_files[MAX_FILES][MAX_DATA_PATH];
extern	int			precache_files_block[MAX_SOUNDS];
extern	int			numfiles;

int	CopyString(char *str);
