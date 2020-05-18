
#define DEFAULTLIGHTLEVEL	300

typedef struct epair_s
{
	struct epair_s	*next;
	char	key[MAX_KEY];
	char	value[MAX_VALUE];
} epair_t;

typedef struct entity_s
{
	char	classname[64];
	vec3_t	origin;
	float	angle;

	// jkrige - added fields
	int      formula;
	float    atten;
	vec3_t   mangle;
	qboolean use_mangle;
#ifdef HX2COLOR
	int      lightcolor[3];
#endif
	// jkrige - added fields

	int		light;
	int		style;
	char	target[32];
	char	targetname[32];
	struct epair_s	*epairs;
	struct entity_s	*targetent;
} entity_t;

/* Explanation of values added to struct entity_s
 *
 * formula:
 *    takes a value 0-3 (default 0)
 *    0 - Standard lighting formula like original light
 *    1 - light fades as 1/x
 *    2 - light fades as 1/(x^2)
 *    3 - Light stays same brightness reguardless of distance
 *
 * atten:
 *    Takes a float as a value (default 1.0).
 *    This reflects how fast a light fades with distance.
 *    For example a value of 2 will fade twice as fast, and a value of 0.5
 *      will fade half as fast. (Just like arghlite)
 *
 *  mangle:
 *    If the entity is a light, then point the spotlight in this direction.
 *    If it is the worldspawn, then this is the sunlight mangle
 *
 *  lightcolour:
 *    Stores the RGB values to determine the light colour
 */

extern	entity_t	entities[MAX_MAP_ENTITIES];
extern	int			num_entities;

entity_t *FindEntityWithKeyPair (char *key, char *value); // jkrige
char 	*ValueForKey (entity_t *ent, char *key);
void 	SetKeyValue (entity_t *ent, char *key, char *value);
float	FloatForKey (entity_t *ent, char *key);
void 	GetVectorForKey (entity_t *ent, char *key, vec3_t vec);

void LoadEntities (void);
void WriteEntitiesToString (void);
