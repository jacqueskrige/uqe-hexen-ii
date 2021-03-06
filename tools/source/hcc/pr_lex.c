
//**************************************************************************
//**
//** lexi.c
//**
//** $Header: /H2 Mission Pack/Tools/hcc/PR_LEX.C 3     3/03/98 10:57p Jmonroe $
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "hcc.h"

// MACROS ------------------------------------------------------------------

#define MAX_FRAME_DEFS 2048
#define MAX_FRAME_INDEX 256
#define MAX_BOOKMARKS 26

// TYPES -------------------------------------------------------------------

typedef enum
{
	CHR_EOF,
	CHR_LETTER,
	CHR_NUMBER,
	CHR_SQUOTE,
	CHR_DQUOTE,
	CHR_DOLLARSIGN,
	CHR_SPECIAL
} chr_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static float LexNumber(void);
static int DigitValue(int digit, int radix);
static float LexFraction(void);
static void LexWhitespace(void);
static void LexString(void);
static void NewLine(void);
static void LexVector(void);
static void LexPunctuation(void);
static void LexName(void);
static void LexWhiteSpace(void);
static void ClearFrameMacros(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int lx_SourceLine;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char FrameMacroNames[MAX_FRAME_DEFS][24];
static int FrameMacroValues[MAX_FRAME_DEFS];
static int FrameMacroBookmarks[MAX_BOOKMARKS];
static int FrameMacroCount;
static int FrameMacroIndex;

static int ASCIIToChrCode[256];

char *pr_file_p;
char *pr_line_start; // start of current source line

int pr_bracelevel;

int pr_tokenclass;
char		pr_token[2048];
token_type_t	pr_token_type;
type_t		*pr_immediate_type;
eval_t		pr_immediate;

char	pr_immediate_string[2048];

int		pr_error_count;

// simple types.  function types are dynamically allocated
type_t type_void = {ev_void, &def_void};
type_t type_string = {ev_string, &def_string};
type_t type_float = {ev_float, &def_float};
type_t type_vector = {ev_vector, &def_vector};
type_t type_entity = {ev_entity, &def_entity};
type_t type_field = {ev_field, &def_field};
type_t type_function = {ev_function, &def_function,NULL,&type_void};
// type_function is a void() function used for state defs
type_t type_pointer = {ev_pointer, &def_pointer};
type_t type_union = {ev_void, &def_void};

type_t	type_floatfield = {ev_field, &def_field, NULL, &type_float};

int		type_size[8] = {1,1,1,3,1,1,1,1};

def_t	def_void = {&type_void, "tmp"};
def_t	def_string = {&type_string, "tmp"};
def_t	def_float = {&type_float, "tmp"};
def_t	def_vector = {&type_vector, "tmp"};
def_t	def_entity = {&type_entity, "tmp"};
def_t	def_field = {&type_field, "tmp"};
def_t	def_function = {&type_function, "tmp"};
def_t	def_pointer = {&type_pointer, "tmp"};

def_t	def_ret, def_parms[MAX_PARMS];

def_t *def_for_type[8] =
{
	&def_void, &def_string, &def_float, &def_vector,
	&def_entity, &def_field, &def_function, &def_pointer
};

// CODE --------------------------------------------------------------------

//==========================================================================
//
// LX_Init
//
//==========================================================================

void LX_Init(void)
{
	int i;

	for(i = 0; i < 256; i++)
	{
		ASCIIToChrCode[i] = CHR_SPECIAL;
	}
	for(i = '0'; i <= '9'; i++)
	{
		ASCIIToChrCode[i] = CHR_NUMBER;
	}
	for(i = 'A'; i <= 'Z'; i++)
	{
		ASCIIToChrCode[i] = CHR_LETTER;
	}
	for(i = 'a'; i <= 'z'; i++)
	{
		ASCIIToChrCode[i] = CHR_LETTER;
	}
	ASCIIToChrCode['_'] = CHR_LETTER;
	ASCIIToChrCode['\"'] = CHR_DQUOTE;
	ASCIIToChrCode['\''] = CHR_SQUOTE;
	ASCIIToChrCode['$'] = CHR_DOLLARSIGN;
	ASCIIToChrCode[0] = CHR_EOF;
}

//==========================================================================
//
// LX_NewSourceFile
//
//==========================================================================

void LX_NewSourceFile(char *fileText)
{
	pr_file_p = fileText;
	ClearFrameMacros();
	lx_SourceLine = 0;
	NewLine();
	LX_Fetch();
}

//==========================================================================
//
// NewLine
//
//==========================================================================

static void NewLine(void)
{
	qboolean m;

	m = false;
	if(*pr_file_p == '\n')
	{
		pr_file_p++;
		m = true;
	}
	lx_SourceLine++;
	pr_line_start = pr_file_p;
	if(m)
	{
		pr_file_p--;
	}
}

//==========================================================================
//
// LexString
//
//==========================================================================

static void LexString(void)
{
	int c;
	int len;

	len = 0;
	pr_file_p++;
	do
	{
		c = *pr_file_p++;
		if (!c)
			MS_ParseError("EOF inside quote");
		if (c=='\n')
			MS_ParseError("newline inside quote");
		if (c=='\\')
		{ // Escape char
			c = *pr_file_p++;
			if (!c)
				MS_ParseError("EOF inside quote");
			if (c == 'n')
				c = '\n';
			else if (c == '"')
				c = '"';
			else
				MS_ParseError("unknown escape char");
		}
		else if (c=='\"')
		{
			pr_token[len] = 0;
			pr_token_type = tt_immediate;
			pr_immediate_type = &type_string;
			strcpy(pr_immediate_string, pr_token);
			return;
		}
		pr_token[len] = c;
		len++;
	} while(1);
}

//==========================================================================
//
// LexNumber
//
//==========================================================================

static float LexNumber(void)
{
	int c;
	int c2;
	int intNumber;
	char *buffer;
	qboolean neg;
	int digitVal;
	int radix;

	if(*pr_file_p == '.')
	{
		return LexFraction();
	}
	buffer = pr_token;
	c = *pr_file_p++;
	c2 = *pr_file_p;
	if(c == '0' && (c2 == 'x' || c2 =='X'))
	{
		*buffer++ = c;
		*buffer++ = *pr_file_p++;
		intNumber = 0;
		c = *pr_file_p;
		while((digitVal = DigitValue(c, 16)) != -1)
		{
			*buffer++ = c;
			intNumber = (intNumber<<4)+digitVal;
			c = *++pr_file_p;
		}
		*buffer = 0;
		return (float)intNumber;
	}
	if(c == '-')
	{
		intNumber = 0;
		neg = true;
	}
	else
	{
		intNumber = c-'0';
		neg = false;
	}
	*buffer++ = c;
	while(ASCIIToChrCode[c2] == CHR_NUMBER)
	{
		*buffer++ = c2;
		intNumber = 10*intNumber+(c2-'0');
		c2 = *++pr_file_p;
	}
	if(c2 == '_')
	{
		*buffer++ = *pr_file_p++;
		radix = intNumber;
		if(radix < 2 || radix > 36)
		{
			MS_ParseError("bad radix in integer constant (%d)",
				radix);
		}
		intNumber = 0;
		c = *pr_file_p;
		while((digitVal = DigitValue(c, radix)) != -1)
		{
			*buffer++ = c;
			intNumber = radix*intNumber+digitVal;
			c = *++pr_file_p;
		}
		*buffer = 0;
		return neg ? (float)-intNumber : (float)intNumber;
	}
	if(c2 == '.' && pr_file_p[1] != '.')
	{
		*buffer++ = c2;
		c2 = *++pr_file_p;
		while(ASCIIToChrCode[c2] == CHR_NUMBER)
		{
			*buffer++ = c2;
			c2 = *++pr_file_p;
		}
		*buffer = 0;
		return (float)atof(pr_token);
	}
	*buffer = 0;
	return neg ? (float)-intNumber : (float)intNumber;
}

//==========================================================================
//
// DigitValue
//
// Returns -1 if the digit is not allowed in the specified radix.
//
//==========================================================================

static int DigitValue(int digit, int radix)
{
	digit = toupper(digit);
	if(digit < '0' || (digit > '9' && digit < 'A') || digit > 'Z')
	{
		return -1;
	}
	if(digit > '9')
	{
		digit = (10+digit-'A');
	}
	else
	{
		digit -= '0';
	}
	if(digit >= radix)
	{
		return -1;
	}
	return digit;
}

//==========================================================================
//
// LexFraction
//
// Called with pr_file_p pointing to the decimal point.
//
//==========================================================================

static float LexFraction(void)
{
	int c;
	char *buffer;

	buffer = pr_token;
	*buffer++ = '0';
	*buffer++ = '.';
	c = *++pr_file_p;
	while(ASCIIToChrCode[c] == CHR_NUMBER)
	{
		*buffer++ = c;
		c = *++pr_file_p;
	}
	*buffer = 0;
	return (float)atof(pr_token);
}

//==========================================================================
//
// LexVector
//
//==========================================================================

static void LexVector(void)
{
	int i;

	pr_file_p++;
	pr_token_type = tt_immediate;
	pr_immediate_type = &type_vector;
	for(i = 0; i < 3; i++)
	{
		LexWhitespace();
		pr_immediate.vector[i] = LexNumber();
	}
	LexWhitespace();
	if(*pr_file_p != '\'')
	{
		MS_ParseError("bad vector");
	}
	pr_file_p++;
}

//==========================================================================
//
// LexName
//
//==========================================================================

static void LexName(void)
{
	int c;
	int len;

	len = 0;
	c = *pr_file_p;
	do
	{
		pr_token[len] = c;
		len++;
		pr_file_p++;
		c = *pr_file_p;
	} while(ASCIIToChrCode[c] == CHR_LETTER
		|| ASCIIToChrCode[c] == CHR_NUMBER);
	pr_token[len] = 0;
	pr_token_type = tt_name;
}

//==========================================================================
//
// LexPunctuation
//
//==========================================================================

static void LexPunctuation(void)
{
	pr_token_type = tt_punct;
	*(int *)pr_token = 0;
	*pr_token = *pr_file_p;
	switch(*pr_file_p++)
	{
	case ';':
		pr_tokenclass = TK_SEMICOLON;
		return;
	case '(':
		if(*pr_file_p == '+' && pr_file_p[1] == ')')
		{
			pr_token[1] = *pr_file_p++;
			pr_token[2] = *pr_file_p++;
			pr_tokenclass = TK_BITSET;
			return;
		}
		if(*pr_file_p == '-' && pr_file_p[1] == ')')
		{
			pr_token[1] = *pr_file_p++;
			pr_token[2] = *pr_file_p++;
			pr_tokenclass = TK_BITCLR;
			return;
		}
		pr_tokenclass = TK_LPAREN;
		return;
	case ')':
		pr_tokenclass = TK_RPAREN;
		return;
	case ',':
		pr_tokenclass = TK_COMMA;
		return;
	case '+':
		if(*pr_file_p == '=')
		{
			pr_token[1] = *pr_file_p++;
			pr_tokenclass = TK_ADDASSIGN;
			return;
		}
		if(*pr_file_p == '+')
		{
			pr_token[1] = *pr_file_p++;
			pr_tokenclass = TK_INC;
			return;
		}
		pr_tokenclass = TK_PLUS;
		return;
	case '*':
		if(*pr_file_p == '=')
		{
			pr_token[1] = *pr_file_p++;
			pr_tokenclass = TK_MULASSIGN;
			return;
		}
		pr_tokenclass = TK_ASTERISK;
		return;
	case '/':
		if(*pr_file_p == '=')
		{
			pr_token[1] = *pr_file_p++;
			pr_tokenclass = TK_DIVASSIGN;
			return;
		}
		pr_tokenclass = TK_SLASH;
		return;
	case '{':
		pr_bracelevel++;
		pr_tokenclass = TK_LBRACE;
		return;
	case '}':
		pr_bracelevel--;
		pr_tokenclass = TK_RBRACE;
		return;
	case '[':
		pr_tokenclass = TK_LBRACKET;
		return;
	case ']':
		pr_tokenclass = TK_RBRACKET;
		return;
	case ':':
		pr_tokenclass = TK_COLON;
		return;
	case '#':
		pr_tokenclass = TK_NUMBERSIGN;
		return;
	case '=':
		if(*pr_file_p == '=')
		{
			pr_token[1] = *pr_file_p++;
			pr_tokenclass = TK_EQ;
			return;
		}
		pr_tokenclass = TK_ASSIGN;
		return;
	case '&':
		if(*pr_file_p == '&')
		{
			pr_token[1] = *pr_file_p++;
			pr_tokenclass = TK_AND;
			return;
		}
		pr_tokenclass = TK_BITAND;
		return;
	case '|':
		if(*pr_file_p == '|')
		{
			pr_token[1] = *pr_file_p++;
			pr_tokenclass = TK_OR;
			return;
		}
		pr_tokenclass = TK_BITOR;
		return;
	case '!':
		if(*pr_file_p == '=')
		{
			pr_token[1] = *pr_file_p++;
			pr_tokenclass = TK_NE;
			return;
		}
		pr_tokenclass = TK_NOT;
		return;
	case '>':
		if(*pr_file_p == '=')
		{
			pr_token[1] = *pr_file_p++;
			pr_tokenclass = TK_GE;
			return;
		}
		pr_tokenclass = TK_GT;
		return;
	case '<':
		if(*pr_file_p == '=')
		{
			pr_token[1] = *pr_file_p++;
			pr_tokenclass = TK_LE;
			return;
		}
		pr_tokenclass = TK_LT;
		return;
	case '.':
		if(ASCIIToChrCode[*pr_file_p] == CHR_NUMBER)
		{
			pr_file_p--;
			pr_token_type = tt_immediate;
			pr_immediate_type = &type_float;
			pr_immediate._float = LexNumber();
			return;
		}
		if(*pr_file_p == '.')
		{
			if(pr_file_p[1] == '.')
			{
				pr_token[1] = *pr_file_p++;
				pr_token[2] = *pr_file_p++;
				pr_tokenclass = TK_ELLIPSIS;
				return;
			}
			else
			{
				pr_token[1] = *pr_file_p++;
				pr_tokenclass = TK_RANGE;
				return;
			}
		}
		pr_tokenclass = TK_PERIOD;
		return;
	case '-':
		if(*pr_file_p == '=')
		{
			pr_token[1] = *pr_file_p++;
			pr_tokenclass = TK_SUBASSIGN;
			return;
		}
		if(*pr_file_p == '-')
		{
			pr_token[1] = *pr_file_p++;
			pr_tokenclass = TK_DEC;
			return;
		}
		if(ASCIIToChrCode[*pr_file_p] == CHR_NUMBER)
		{
			pr_file_p--;
			pr_token_type = tt_immediate;
			pr_immediate_type = &type_float;
			pr_immediate._float = LexNumber();
			return;
		}
		pr_tokenclass = TK_MINUS;
		return;
	default:
		MS_ParseError("unknown punctuation");
	}
}

//==========================================================================
//
// LexWhitespace
//
//==========================================================================

static void LexWhitespace(void)
{
	int c;

	while(1)
	{
		while((c = *pr_file_p) <= ' ')
		{ // Skip whitespace
			if(c == '\n')
			{
				NewLine();
			}
			if(c == 0)
			{ // EOF
				return;
			}
			pr_file_p++;
		}

		if(c == '/' && pr_file_p[1] == '/')
		{ // Skip // comments
			while(*pr_file_p && *pr_file_p != '\n')
			{
				pr_file_p++;
			}
			NewLine();
			pr_file_p++;
			continue;
		}

		if(c == '/' && pr_file_p[1] == '*')
		{ // Skip /* */ comments
			do
			{
				pr_file_p++;
				if(pr_file_p[0] == '\n')
				{
					NewLine();
				}
				if(pr_file_p[1] == 0)
				{ // EOF
					return;
				}
			} while(pr_file_p[-1] != '*' || pr_file_p[0] != '/');
			pr_file_p++;
			continue;
		}
		
		// A character has been found
		break;
	}
}

//==========================================================================
//
// ClearFrameMacros
//
//==========================================================================

static void ClearFrameMacros(void)
{
	int i;

	FrameMacroCount = 0;
	FrameMacroIndex = 0;
	for(i = 0; i < MAX_BOOKMARKS; i++)
	{
		FrameMacroBookmarks[i] = -1;
	}
}

//==========================================================================
//
// FindFrameMacro
//
//==========================================================================

static void FindFrameMacro(void)
{
	int i;

	for(i = 0; i < FrameMacroCount; i++)
	{
		if(!strcmp(pr_token, FrameMacroNames[i]))
		{
			sprintf(pr_token, "%d", i);
			pr_token_type = tt_immediate;
			pr_immediate_type = &type_float;
			pr_immediate._float = (float)FrameMacroValues[i];
			return;
		}
	}
	MS_ParseError("unknown frame macro $%s", pr_token);
}

//==========================================================================
//
// SimpleGetToken
//
//==========================================================================

static qboolean SimpleGetToken(void)
{
	int c;
	int i;

	while((c = *pr_file_p) <= ' ')
	{
		if(c=='\n' || c == 0)
		{
			return false;
		}
		pr_file_p++;
	}

	i = 0;
	while((c = *pr_file_p) > ' '
		&& c != ',' && c != ';' && c != '.'
		&& c != '(' && c != ')'
		&& c != '[' && c != ']')
	{
		pr_token[i] = tolower(c);
		i++;
		pr_file_p++;
	}
	pr_token[i] = 0;
	return true;
}

//==========================================================================
//
// LexGrab
//
// Handles all $ commands.
//
//==========================================================================

static void LexGrab(void)
{
	int mark;

	pr_file_p++;
	if(!SimpleGetToken())
	{
		MS_ParseError("hanging $");
	}

	if(!strcmp(pr_token, "frame"))
	{
		while(SimpleGetToken())
		{
			if(FrameMacroIndex < 0 || FrameMacroIndex >= MAX_FRAME_INDEX)
			{
				MS_ParseError("bad frame value, %s = %d", pr_token,
					FrameMacroIndex);
			}
			strcpy(FrameMacroNames[FrameMacroCount], pr_token);
			FrameMacroValues[FrameMacroCount] = FrameMacroIndex;
			FrameMacroIndex++;
			FrameMacroCount++;
		}
		LX_Fetch();
	}
	else if(!strcmp(pr_token, "framevalue"))
	{
		LX_Fetch();
		if(pr_token_type != tt_immediate
			|| pr_immediate_type != &type_float
			|| pr_immediate._float != (int)pr_immediate._float)
		{
			MS_ParseError("$framevalue : bad frame immediate");
		}
		FrameMacroIndex = (int)pr_immediate._float;
		LX_Fetch();
	}
	else if(!strcmp(pr_token, "framesave"))
	{
		if(SimpleGetToken() == false)
		{
			MS_ParseError("$framesave : no bookmark");
		}
		if(pr_token[1] || *pr_token < 'a' || *pr_token > 'z')
		{
			MS_ParseError("$framesave : bad bookmark");
		}
		mark = *pr_token-'a';
		FrameMacroBookmarks[mark] = FrameMacroIndex;
		LX_Fetch();
	}
	else if(!strcmp(pr_token, "framerestore"))
	{
		if(SimpleGetToken() == false)
		{
			MS_ParseError("$framerestore : no bookmark");
		}
		if(pr_token[1] || *pr_token < 'a' || *pr_token > 'z')
		{
			MS_ParseError("$framerestore : bad bookmark");
		}
		mark = *pr_token-'a';
		if(FrameMacroBookmarks[mark] == -1)
		{
			MS_ParseError("$framerestore : uninitialized bookmark");
		}
		FrameMacroIndex = FrameMacroBookmarks[mark];
		LX_Fetch();
	}
	else if(!strcmp (pr_token, "cd")
		|| !strcmp (pr_token, "origin")
		|| !strcmp (pr_token, "base")
		|| !strcmp (pr_token, "flags")
		|| !strcmp (pr_token, "scale")
		|| !strcmp (pr_token, "skin") )
	{ // Ignore known $commands
		// skip to end of line
		while(SimpleGetToken())
		;
		LX_Fetch();
	}
	else
	{ // Look for a frame name macro
		FindFrameMacro();
	}
}

//==========================================================================
//
// LX_Fetch
//
// Sets pr_token, pr_token_type, and possibly pr_immediate and
// pr_immediate_type.
//
//==========================================================================

void LX_Fetch(void)
{
	int c;

	pr_tokenclass = TK_NONE;

	pr_token[0] = 0;

	if(!pr_file_p)
	{
		pr_token_type = tt_eof;
		return;
	}

	LexWhitespace();

	c = *pr_file_p;

	switch(ASCIIToChrCode[c])
	{
	case CHR_LETTER:
		LexName();
		return;
	case CHR_NUMBER:
		pr_token_type = tt_immediate;
		pr_immediate_type = &type_float;
		pr_immediate._float = LexNumber();
		return;
	case CHR_DQUOTE:
		LexString();
		return;
	case CHR_SQUOTE:
		LexVector();
		return;
	case CHR_DOLLARSIGN:
		LexGrab();
		return;
	case CHR_EOF:
		pr_token_type = tt_eof;
		return;
	case CHR_SPECIAL:
	default:
		LexPunctuation();
		return;
	}
}

//==========================================================================
//
// LX_Require
//
// Issues an error if the current token isn't equal to string.
// Gets the next token.
//
//==========================================================================

void LX_Require(char *string)
{
	if(strcmp(string, pr_token))
	{
		MS_ParseError("expected %s, found %s", string, pr_token);
	}
	LX_Fetch();
}

//==========================================================================
//
// LX_CheckFetch
//
// Returns true and gets the next token if the current token equals string.
// Returns false and does nothing otherwise.
//
//==========================================================================

qboolean LX_CheckFetch(char *string)
{
	if((*string != *pr_token) || strcmp(string, pr_token))
	{
		return false;
	}
	LX_Fetch();
	return true;
}

//==========================================================================
//
// LX_Check
//
//==========================================================================

qboolean LX_Check(char *string)
{
	if(strcmp(string, pr_token))
	{
		return false;
	}
	return true;
}

//==========================================================================
//
// PR_ParseName
//
//==========================================================================

char *PR_ParseName(void)
{
	static char	ident[MAX_NAME];

	if(pr_token_type != tt_name)
	{
		MS_ParseError ("not a name");
	}
	if(strlen(pr_token) >= MAX_NAME-1)
	{
		MS_ParseError ("name too long");
	}
	strcpy(ident, pr_token);
	LX_Fetch();

	return ident;
}

//==========================================================================
//
// PR_FindType
//
// Returns a preexisting complex type that matches the parm, or allocates
// a new one and copies it out.
//
//==========================================================================

type_t *PR_FindType(type_t *type)
{
	int i;
	def_t *def;
	type_t *check;

	for(check = pr.types; check; check = check->next)
	{
		if(check->type != type->type
			|| check->aux_type != type->aux_type
			|| check->num_parms != type->num_parms)
		{
			continue;
		}
		for(i = 0; i < type->num_parms; i++)
		{
			if(check->parm_types[i] != type->parm_types[i])
			{
				break;
			}
		}
		if(i == type->num_parms)
		{
			return check;
		}
	}

	// Allocate a new one
	check = malloc(sizeof(*check));
	*check = *type;
	check->next = pr.types;
	pr.types = check;

	// Allocate a generic def for the type, so fields can reference it
	def = malloc(sizeof(def_t));
	memset(def, 0, sizeof(*def));
	def->name = "COMPLEX TYPE";
	def->type = check;
	check->def = def;
	return check;
}

/*
============
PR_ParseType

Parses a variable type, including field and functions types
============
*/

char pr_parm_names[MAX_PARMS][MAX_NAME];

type_t *PR_ParseType(void)
{
	type_t	new;
	type_t	*type;
	char	*name;

	if(TK_CHECK(TK_PERIOD))
	{
		if(LX_CheckFetch("union"))
		{
			return &type_union;
		}
		memset(&new, 0, sizeof(new));
		new.type = ev_field;
		new.aux_type = PR_ParseType();
		return PR_FindType(&new);
	}

	if (!strcmp (pr_token, "float") )
		type = &type_float;
	else if (!strcmp (pr_token, "vector") )
		type = &type_vector;
//	else if (!strcmp (pr_token, "float") )
//		type = &type_float;
	else if (!strcmp (pr_token, "entity") )
		type = &type_entity;
	else if (!strcmp (pr_token, "string") )
		type = &type_string;
	else if (!strcmp (pr_token, "void") )
		type = &type_void;
	else
	{
		MS_ParseError ("\"%s\" is not a type", pr_token);
		type = &type_float;	// shut up compiler warning
	}
	LX_Fetch();

	if(!TK_CHECK(TK_LPAREN))
	{
		return type;
	}

	// Function type
	memset(&new, 0, sizeof(new));
	new.type = ev_function;
	new.aux_type = type; // Return type
	new.num_parms = 0;
	if(!TK_CHECK(TK_RPAREN))
	{
		if(TK_CHECK(TK_ELLIPSIS))
		{
			new.num_parms = -1;	// variable args
		}
		else
		{
			do
			{
				type = PR_ParseType();
				name = PR_ParseName();
				strcpy (pr_parm_names[new.num_parms], name);
				new.parm_types[new.num_parms] = type;
				new.num_parms++;
			} while(TK_CHECK(TK_COMMA));
		}
		LX_Require(")");
	}
	return PR_FindType(&new);
}

//==========================================================================
//
// LX_ErrorRecovery
//
//==========================================================================

void LX_ErrorRecovery(void)
{
	do
	{
		if(!pr_bracelevel)
		{
			if(TK_CHECK(TK_SEMICOLON))
			{
				return;
			}
			if(TK_CHECK(TK_RBRACE))
			{
				TK_CHECK(TK_SEMICOLON);
				return;
			}
		}
		LX_Fetch();
	} while(pr_token_type != tt_eof);
}
