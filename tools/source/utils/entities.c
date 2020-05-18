// entities.c

#include "light.h"

entity_t	entities[MAX_MAP_ENTITIES];
int			num_entities;

/*
==============================================================================

ENTITY FILE PARSING

If a light has a targetname, generate a unique style in the 32-63 range
==============================================================================
*/

int		numlighttargets;
char	lighttargets[32][64];

int LightStyleForTargetname (char *targetname, qboolean alloc)
{
	int		i;
	
	for (i=0 ; i<numlighttargets ; i++)
		if (!strcmp (lighttargets[i], targetname))
			return 32 + i;
	if (!alloc)
		return -1;
	strcpy (lighttargets[i], targetname);
	numlighttargets++;
	return numlighttargets-1 + 32;
}


/*
==================
MatchTargets
==================
*/
void MatchTargets (void)
{
	int		i,j;
	
	for (i=0 ; i<num_entities ; i++)
	{
		if (!entities[i].target[0])
			continue;
			
		for (j=0 ; j<num_entities ; j++)
			if (!strcmp(entities[j].targetname, entities[i].target))
			{
				entities[i].targetent = &entities[j];
				break;
			}
		if (j==num_entities)
		{
			logprint ("WARNING: entity at (%i,%i,%i) (%s) has unmatched target\n", (int)entities[i].origin[0], (int)entities[i].origin[1], (int)entities[i].origin[2], entities[i].classname);
			continue;
		}
		
// set the style on the source ent for switchable lights
		if (entities[j].style)
		{
			char	s[16];
			
			entities[i].style = entities[j].style;
			sprintf (s,"%i", entities[i].style);
			SetKeyValue (&entities[i], "style", s);
		}
	}	
}

// jkrige
/*void vec_from_mangle(vec3_t v, vec3_t m)
{
    vec3_t tmp;

    VectorScale(m, Q_PI / 180, tmp);
    v[0] = cos(tmp[0]) * cos(tmp[1]);
    v[1] = sin(tmp[0]) * cos(tmp[1]);
    v[2] = sin(tmp[1]);
}*/
// jkrige

/*
==================
LoadEntities
==================
*/
void LoadEntities (void)
{
	char 		*data;
	entity_t	*entity;
	char		key[64];	
	epair_t		*epair;

	// jkrige
	double   vec[3];
    int      i;
    int      num_lights;
	// jkrige
	
	data = dentdata;
//
// start parsing
//
	num_entities = 0;
	num_lights = 0; // jkrige
	
// go through all the entities
	while (1)
	{
	// parse the opening brace	
		data = COM_Parse (data);
		if (!data)
			break;
		if (com_token[0] != '{')
			logerror ("LoadEntities: found %s when expecting {",com_token); // jkrige - was Error()

		if (num_entities == MAX_MAP_ENTITIES)
			logerror ("LoadEntities: MAX_MAP_ENTITIES"); // jkrige - was Error()
		entity = &entities[num_entities];
		num_entities++;

		// jkrige
		// flag to indicate use of mangle key
		entity->use_mangle = false;
		// jkrige
		
	// go through all the keys in this entity
		while (1)
		{
			int		c;

		// parse key
			data = COM_Parse (data);
			if (!data)
				logerror ("LoadEntities: EOF without closing brace"); // jkrige - was Error()
			if (!strcmp(com_token,"}"))
				break;
			strcpy (key, com_token);

		// parse value
			data = COM_Parse (data);
			if (!data)
				logerror ("LoadEntities: EOF without closing brace"); // jkrige - was Error()
			c = com_token[0];
			if (c == '}')
				logerror ("LoadEntities: closing brace without data"); // jkrige - was Error()
			
			epair = malloc (sizeof(epair_t));
			memset (epair, 0, sizeof(epair));
			strcpy (epair->key, key);
			strcpy (epair->value, com_token);
			epair->next = entity->epairs;
			entity->epairs = epair;
			
			if (!strcmp(key, "classname"))
				strcpy (entity->classname, com_token);
			else if (!strcmp(key, "target"))
				strcpy (entity->target, com_token);			
			else if (!strcmp(key, "targetname"))
				strcpy (entity->targetname, com_token);
			else if (!strcmp(key, "origin"))
			{
				// jkrige
				//if (sscanf(com_token, "%lf %lf %lf",
				//		&entity->origin[0],
				//		&entity->origin[1],
				//		&entity->origin[2]) != 3)
				//	Error ("LoadEntities: not 3 values for origin");
				if (sscanf(com_token, "%lf %lf %lf", &vec[0], &vec[1], &vec[2]) != 3)
                    logerror ("LoadEntities: not 3 values for origin"); // jkrige - was Error()
                for (i=0 ; i<3 ; i++)
                    entity->origin[i] = vec[i];
				// jkrige
			}
			else if (!strncmp(key, "light", 5))
			{
				entity->light = atoi(com_token);
			}
			else if (!strcmp(key, "style"))
			{
				entity->style = atoi(com_token);
				if ((unsigned)entity->style > 254)
					logerror ("Bad light style %i (must be 0-254)", entity->style); // jkrige - was Error()
			}
			else if (!strcmp(key, "angle"))
			{
				entity->angle = (float)atof(com_token);
			}
			// jkrige
			else if (!strcmp(key, "wait"))
                entity->atten = (float)atof(com_token);
            else if (!strcmp(key, "delay"))
                entity->formula = atoi(com_token);
            else if (!strcmp(key, "mangle"))
			{
                // scan into doubles, then assign
				// which makes it vec_t size independent
                if (sscanf(com_token, "%lf %lf %lf", &vec[0], &vec[1], &vec[2]) != 3)
                    logerror ("LoadEntities: not 3 values for mangle"); // jkrige - was Error()

                // Precalculate the direction vector
                entity->use_mangle = true;
				//vec_from_mangle(entity->mangle, vec);
                entity->mangle[0] = cos(vec[0]*Q_PI/180)*cos(vec[1]*Q_PI/180);
                entity->mangle[1] = sin(vec[0]*Q_PI/180)*cos(vec[1]*Q_PI/180);
                entity->mangle[2] = sin(vec[1]*Q_PI/180);
			}
#ifdef HX2COLOR
			else if (!strcmp(key, "_color") || !strcmp(key, "color"))
			{
                // scan into doubles, then assign
				// which makes it vec_t size independent
                if (sscanf(com_token, "%lf %lf %lf", &vec[0], &vec[1], &vec[2]) != 3)
                    logerror ("LoadEntities: not 3 values for colour"); // jkrige - was Error()
                for (i=0 ; i<3 ; i++)
                    //entity->lightcolor[i] = (int)vec[i]*255; // jkrige - added (int)
					entity->lightcolor[i] = (int)vec[i]; // jkrige - added (int)
            }
			else if (!strcmp(key, "_minlight_color") || !strcmp(key, "minlight_color"))
			{
                // scan into doubles, then assign
                // which makes it vec_t size independent
                if (sscanf(com_token, "%lf %lf %lf", &vec[0], &vec[1], &vec[2]) != 3)
                    logerror ("LoadEntities: not 3 values for _minlight_color"); // jkrige - was Error()
                for (i=0 ; i<3 ; i++)
                    //minlight_color[i] = vec[i]*255;
					minlight_color[i] = vec[i];
            }
#endif

			/*else if (!strcmp(key, "_sunlight"))
                sunlight = (int)atof(com_token);
            else if (!strcmp(key, "_sun_mangle"))
			{
                // scan into doubles, then assign
                // which makes it vec_t size independent
                if (sscanf(com_token, "%lf %lf %lf", &vec[0], &vec[1], &vec[2]) != 3)
                    Error ("LoadEntities: not 3 values for _sun_mangle");

                // Precalculate sun vector and
                // make it too large to fit into the map
				vec_from_mangle(sunmangle, vec);
                //sunmangle[0] = cos(vec[0]*Q_PI/180)*cos(vec[1]*Q_PI/180);
                //sunmangle[1] = sin(vec[0]*Q_PI/180)*cos(vec[1]*Q_PI/180);
                //sunmangle[2] = sin(vec[1]*Q_PI/180);
                VectorNormalize(sunmangle);
                VectorScale(sunmangle, -16384, sunmangle);
            }
			else if (!strcmp(key, "_sunlight_color"))
			{
                // scan into doubles, then assign
                // which makes it vec_t size independent
                if (sscanf(com_token, "%lf %lf %lf", &vec[0], &vec[1], &vec[2]) != 3)
                    Error ("LoadEntities: not 3 values for _sunlight_color");
                for (i=0 ; i<3 ; i++)
                    sunlight_color[i] = vec[i];
            }*/
			// jkrige
		}

	// all fields have been parsed
		// jkrige
		//if (!strncmp (entity->classname, "light", 5) && !entity->light)
		//	entity->light = DEFAULTLIGHTLEVEL;
		if (!strncmp (entity->classname, "light", 5))
		{
            if (!entity->light)
                entity->light = DEFAULTLIGHTLEVEL;
            if (entity->atten <= 0.0)
                entity->atten = 1.0;
            if ((entity->formula < 0) || (entity->formula > 3))
                entity->formula = 0;
#ifdef HX2COLOR
            if (!entity->lightcolor[0] && !entity->lightcolor[1] && !entity->lightcolor[2])
			{
				entity->lightcolor[0] = 255;
                entity->lightcolor[1] = 255;
                entity->lightcolor[2] = 255;
            }
#endif
            num_lights++;
        }
		// jkrige

		//if (!strncmp (entity->classname, "light", 5))
		if (!strcmp (entity->classname, "light"))  // jkrige
		{
			if (entity->targetname[0] && !entity->style)
			{
				char	s[16];
				
				entity->style = LightStyleForTargetname (entity->targetname, true);
				sprintf (s,"%i", entity->style);
				SetKeyValue (entity, "style", s);
			}
		}

		// jkrige
		if (!strcmp (entity->classname, "worldspawn"))
		{
            if (entity->light > 0 && !worldminlight)
			{
                worldminlight = entity->light;
                logprint("Using minlight value %i from worldspawn.\n", worldminlight);
            } else if (worldminlight)
			{
                logprint("Using minlight value %i from command line.\n", worldminlight);
            }
        }
		// jkrige

	}

	//logprint ("%d entities read\n", num_entities);
	logprint ("%d entities read, %d are lights.\n", num_entities, num_lights); // jkrige

	MatchTargets ();
}

char 	*ValueForKey (entity_t *ent, char *key)
{
	epair_t	*ep;
	
	for (ep=ent->epairs ; ep ; ep=ep->next)
		if (!strcmp (ep->key, key) )
			return ep->value;
	return "";
}

// jkrige
entity_t *FindEntityWithKeyPair( char *key, char *value )
{
    entity_t *ent;
    epair_t  *ep;
    int i;

    for (i=0 ; i<num_entities ; i++) {
        ent = &entities[ i ];
        for (ep=ent->epairs ; ep ; ep=ep->next)
            if (!strcmp (ep->key, key)) {
                if (!strcmp( ep->value, value ))
		    return ent;
                break;
            }
    }
    return NULL;
}
// jkrige

void 	SetKeyValue (entity_t *ent, char *key, char *value)
{
	epair_t	*ep;
	
	for (ep=ent->epairs ; ep ; ep=ep->next)
		if (!strcmp (ep->key, key) )
		{
			strcpy (ep->value, value);
			return;
		}
	ep = malloc (sizeof(*ep));
	ep->next = ent->epairs;
	ent->epairs = ep;
	strcpy (ep->key, key);
	strcpy (ep->value, value);
}

float	FloatForKey (entity_t *ent, char *key)
{
	char	*k;
	
	k = ValueForKey (ent, key);
	return (float)atof(k);
}

void 	GetVectorForKey (entity_t *ent, char *key, vec3_t vec)
{
	char	*k;
	
	k = ValueForKey (ent, key);
	sscanf (k, "%lf %lf %lf", &vec[0], &vec[1], &vec[2]);
}



/*
================
WriteEntitiesToString
================
*/
void WriteEntitiesToString (void)
{
	char	*buf, *end;
	epair_t	*ep;
	char	line[128];
	int		i;
	
	buf = dentdata;
	end = buf;
	*end = 0;
	
	logprint ("%i switchable light styles\n", numlighttargets);
	
	for (i=0 ; i<num_entities ; i++)
	{
		ep = entities[i].epairs;
		if (!ep)
			continue;	// ent got removed
		
		strcat (end,"{\n");
		end += 2;
				
		for (ep = entities[i].epairs ; ep ; ep=ep->next)
		{
			sprintf (line, "\"%s\" \"%s\"\n", ep->key, ep->value);
			strcat (end, line);
			end += strlen(line);
		}
		strcat (end,"}\n");
		end += 2;

		if (end > buf + MAX_MAP_ENTSTRING)
			logerror ("Entity text too long"); // jkrige - was Error()
	}
	entdatasize = end - buf + 1;
}

