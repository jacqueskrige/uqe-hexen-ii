
#include "light.h"

/*
============
CastRay

Returns the distance between the points, or -1 if blocked
=============
*/
double CastRay (vec3_t p1, vec3_t p2)
{
	int		i;
	double	t;
	qboolean	trace;
		
	trace = TestLine (p1, p2);
		
	if (!trace)
		return -1;		// ray was blocked
		
	t = 0;
	for (i=0 ; i< 3 ; i++)
		t += (p2[i]-p1[i]) * (p2[i]-p1[i]);
		
	if (t == 0)
		t = 1;		// don't blow up...
	return sqrt(t);
}

/*
===============================================================================

SAMPLE POINT DETERMINATION

void SetupBlock (dface_t *f) Returns with surfpt[] set

This is a little tricky because the lightmap covers more area than the face.
If done in the straightforward fashion, some of the
sample points will be inside walls or on the other side of walls, causing
false shadows and light bleeds.

To solve this, I only consider a sample point valid if a line can be drawn
between it and the exact midpoint of the face.  If invalid, it is adjusted
towards the center until it is valid.

(this doesn't completely work)

===============================================================================
*/

#define	SINGLEMAP	(18*18*4)

typedef struct
{
	double	lightmaps[MAXLIGHTMAPS][SINGLEMAP];
	int		numlightstyles;
	double	*light;
	double	facedist;
	vec3_t	facenormal;

	int		numsurfpt;
	vec3_t	surfpt[SINGLEMAP];

	vec3_t	texorg;
	vec3_t	worldtotex[2];	// s = (world - texorg) . worldtotex[0]
	vec3_t	textoworld[2];	// world = texorg + s * textoworld[0]

	double	exactmins[2], exactmaxs[2];
	
	int		texmins[2], texsize[2];
	int		lightstyles[256];
	int		surfnum;
	dface_t	*face;

#ifdef HX2COLOR
	// jkrige
	vec3_t  lightmapcolors[MAXLIGHTMAPS][SINGLEMAP];
	// jkrige
#endif

} lightinfo_t;


/*
================
CalcFaceVectors

Fills in texorg, worldtotex. and textoworld
================
*/
void CalcFaceVectors (lightinfo_t *l)
{
	texinfo_t	*tex;
	int			i, j;
	vec3_t	texnormal;
	float	distscale;
	double	dist, len;

	tex = &texinfo[l->face->texinfo];
	
// convert from float to double
	for (i=0 ; i<2 ; i++)
		for (j=0 ; j<3 ; j++)
			l->worldtotex[i][j] = tex->vecs[i][j];

// calculate a normal to the texture axis.  points can be moved along this
// without changing their S/T
	texnormal[0] = tex->vecs[1][1]*tex->vecs[0][2]
		- tex->vecs[1][2]*tex->vecs[0][1];
	texnormal[1] = tex->vecs[1][2]*tex->vecs[0][0]
		- tex->vecs[1][0]*tex->vecs[0][2];
	texnormal[2] = tex->vecs[1][0]*tex->vecs[0][1]
		- tex->vecs[1][1]*tex->vecs[0][0];
	VectorNormalize (texnormal);

// flip it towards plane normal
	distscale = (float)DotProduct (texnormal, l->facenormal);
	if (!distscale)
	{
		// jkrige
		//Error ("Texture axis perpendicular to face");
		logerror ("Texture axis perpendicular to face\n"
               "Face point at (%f, %f, %f)\n",
               dvertexes[ dedges[ l->face->firstedge ].v[ 0 ] ].point[ 0 ],
               dvertexes[ dedges[ l->face->firstedge ].v[ 0 ] ].point[ 1 ],
               dvertexes[ dedges[ l->face->firstedge ].v[ 0 ] ].point[ 2 ]); // jkrige - was Error()
		// jkrige
	}
	if (distscale < 0)
	{
		distscale = -distscale;
		VectorSubtract (vec3_origin, texnormal, texnormal);
	}	

// distscale is the ratio of the distance along the texture normal to
// the distance along the plane normal
	distscale = 1/distscale;

	for (i=0 ; i<2 ; i++)
	{
		len = VectorLength (l->worldtotex[i]);
		dist = DotProduct (l->worldtotex[i], l->facenormal);
		dist *= distscale;
		VectorMA (l->worldtotex[i], -dist, texnormal, l->textoworld[i]);
		VectorScale (l->textoworld[i], (1/len)*(1/len), l->textoworld[i]);
	}


// calculate texorg on the texture plane
	for (i=0 ; i<3 ; i++)
		l->texorg[i] = -tex->vecs[0][3]* l->textoworld[0][i] - tex->vecs[1][3] * l->textoworld[1][i];

// project back to the face plane
	dist = DotProduct (l->texorg, l->facenormal) - l->facedist - 1;
	dist *= distscale;
	VectorMA (l->texorg, -dist, texnormal, l->texorg);
	
}

// jkrige
/*
 * Functions to aid in calculation of polygon centroid
 */
/*void tri_centroid(dvertex_t *v0, dvertex_t *v1, dvertex_t *v2,
	    vec3_t out)
{
    int i;

    for (i = 0; i < 3; i++)
	out[i] = (v0->point[i] + v1->point[i] + v2->point[i]) / 3.0;
}

vec_t tri_area(dvertex_t *v0, dvertex_t *v1, dvertex_t *v2)
{
    int i;
    vec3_t edge0, edge1, cross;

    for (i =0; i < 3; i++) {
	edge0[i] = v1->point[i] - v0->point[i];
	edge1[i] = v2->point[i] - v0->point[i];
    }
    CrossProduct(edge0, edge1, cross);

    return VectorLength(cross) * 0.5;
}

void face_centroid(dface_t *f, vec3_t out)
{
    int i, e;
    dvertex_t *v0, *v1, *v2;
    vec3_t centroid, poly_centroid;
    vec_t area, poly_area;

    VectorCopy(vec3_origin, poly_centroid);
    poly_area = 0;

    e = dsurfedges[f->firstedge];
    if (e >= 0)
	v0 = dvertexes + dedges[e].v[0];
    else
	v0 = dvertexes + dedges[-e].v[1];

    for (i = 1; i < f->numedges - 1; i++) {
	e = dsurfedges[f->firstedge + i];
	if (e >= 0) {
	    v1 = dvertexes + dedges[e].v[0];
	    v2 = dvertexes + dedges[e].v[1];
	} else {
	    v1 = dvertexes + dedges[-e].v[1];
	    v2 = dvertexes + dedges[-e].v[0];
	}

	area = tri_area(v0, v1, v2);
	poly_area += area;

	tri_centroid(v0, v1, v2, centroid);
	VectorMA(poly_centroid, area, centroid, poly_centroid);
    }

    VectorScale(poly_centroid, 1.0 / poly_area, out);
}*/
// jkrige

/*
================
CalcFaceExtents

Fills in s->texmins[] and s->texsize[]
also sets exactmins[] and exactmaxs[]
================
*/
// jkrige
/*void CalcFaceExtents (lightinfo_t *l)
{
	dface_t *s;
	double	mins[2], maxs[2], val;
	int		i,j, e;
	dvertex_t	*v;
	texinfo_t	*tex;
	
	s = l->face;

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = &texinfo[s->texinfo];
	
	for (i=0 ; i<s->numedges ; i++)
	{
		e = dsurfedges[s->firstedge+i];
		if (e >= 0)
			v = dvertexes + dedges[e].v[0];
		else
			v = dvertexes + dedges[-e].v[1];
		
		for (j=0 ; j<2 ; j++)
		{
			val = v->point[0] * tex->vecs[j][0] + 
				v->point[1] * tex->vecs[j][1] +
				v->point[2] * tex->vecs[j][2] +
				tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{	
		l->exactmins[i] = mins[i];
		l->exactmaxs[i] = maxs[i];
		
		mins[i] = floor(mins[i]/16);
		maxs[i] = ceil(maxs[i]/16);

		l->texmins[i] = (int)floor(mins[i]);
		l->texsize[i] = (int)floor(maxs[i] - mins[i]);
		if (l->texsize[i] > 17)
			Error ("Bad surface extents");
	}
}*/

void CalcFaceExtents (lightinfo_t *l, vec3_t faceoffset)
{
    dface_t   *s;
    vec_t     mins[2], maxs[2], val;
    int       i,j, e;
    dvertex_t *v;
    texinfo_t *tex;

    s = l->face;

    mins[0] = mins[1] = 999999;
    maxs[0] = maxs[1] = -99999;

    tex = &texinfo[s->texinfo];

    for (i=0 ; i<s->numedges ; i++) {
        e = dsurfedges[s->firstedge+i];
        if (e >= 0) v = dvertexes + dedges[e].v[0];
        else        v = dvertexes + dedges[-e].v[1];

        for (j=0 ; j<2 ; j++) {
            val = (v->point[0]+faceoffset[0]) * tex->vecs[j][0]
                + (v->point[1]+faceoffset[1]) * tex->vecs[j][1]
                + (v->point[2]+faceoffset[2]) * tex->vecs[j][2]
                + tex->vecs[j][3];
            if (val < mins[j]) mins[j] = val;
            if (val > maxs[j]) maxs[j] = val;
        }
    }

    for (i=0 ; i<2 ; i++) {
        l->exactmins[i] = mins[i];
        l->exactmaxs[i] = maxs[i];

        mins[i] = floor(mins[i]/16);
        maxs[i] = ceil(maxs[i]/16);

        l->texmins[i] = (int)mins[i];
        l->texsize[i] = (int)(maxs[i] - mins[i]);
        if (l->texsize[i] > 17)
            logerror ("Bad surface extents"); // jkrige - was Error()
    }
}
// jkrige

/*
=================
CalcPoints

For each texture aligned grid point, back project onto the plane
to get the world xyz value of the sample point
=================
*/
int c_bad;
void CalcPoints (lightinfo_t *l)
{
	int		i;
	int		s, t, j;
	int		w, h, step;
	double	starts, startt, us, ut;
	double	*surf;
	double	mids, midt;
	vec3_t	facemid, move;

//
// fill in surforg
// the points are biased towards the center of the surface
// to help avoid edge cases just inside walls
//
	surf = l->surfpt[0];
	mids = (l->exactmaxs[0] + l->exactmins[0])/2;
	midt = (l->exactmaxs[1] + l->exactmins[1])/2;

	for (j=0 ; j<3 ; j++)
		facemid[j] = l->texorg[j] + l->textoworld[0][j]*mids + l->textoworld[1][j]*midt;

	if (extrasamples)
	{	// extra filtering
		h = (l->texsize[1]+1)*2;
		w = (l->texsize[0]+1)*2;
		starts = (l->texmins[0]-0.5)*16;
		startt = (l->texmins[1]-0.5)*16;
		step = 8;
	}
	else
	{
		h = l->texsize[1]+1;
		w = l->texsize[0]+1;
		starts = l->texmins[0]*16;
		startt = l->texmins[1]*16;
		step = 16;
	}

	l->numsurfpt = w * h;
	for (t=0 ; t<h ; t++)
	{
		for (s=0 ; s<w ; s++, surf+=3)
		{
			us = starts + s*step;
			ut = startt + t*step;

		// if a line can be traced from surf to facemid, the point is good
			for (i=0 ; i<6 ; i++)
			{
			// calculate texture point
				for (j=0 ; j<3 ; j++)
					surf[j] = l->texorg[j] + l->textoworld[0][j]*us
					+ l->textoworld[1][j]*ut;

				if (CastRay (facemid, surf) != -1)
					break;	// got it
				if (i & 1)
				{
					if (us > mids)
					{
						us -= 8;
						if (us < mids)
							us = mids;
					}
					else
					{
						us += 8;
						if (us > mids)
							us = mids;
					}
				}
				else
				{
					if (ut > midt)
					{
						ut -= 8;
						if (ut < midt)
							ut = midt;
					}
					else
					{
						ut += 8;
						if (ut > midt)
							ut = midt;
					}
				}
				
				// move surf 8 pixels towards the center
				VectorSubtract (facemid, surf, move);
				VectorNormalize (move);
				VectorMA (surf, 8, move, surf);
			}
			if (i == 2)
				c_bad++;
		}
	}
	
}


/*
===============================================================================

FACE LIGHTING

===============================================================================
*/

int		c_culldistplane, c_proper;

// jkrige
/*
 * ==============================================
 * TYRLITE: Attenuation formulae setup functions
 * ==============================================
 */
float scaledDistance(float distance, entity_t *light)
{
    switch (light->formula) {
    case 1:
    case 2:
    case 3:
        /* Return a small distance to prevent culling these lights, since we */
        /* know these formulae won't fade to nothing.                        */
        return (float)((distance<=0) ? -0.25 : 0.25);
    case 0:
        return scaledist * light->atten * distance;
    default:
        return 1;  /* shut up compiler warnings */
    }
}

float scaledLight(float distance, entity_t *light)
{
    float tmp = scaledist * light->atten * distance;
    switch (light->formula)
	{
    case 3: return (float)light->light;
    case 1: return light->light / (tmp / 128);
    case 2: return light->light / ((tmp * tmp) / 16384);
    case 0:
        if (light->light > 0)
            return (light->light - tmp > 0) ? light->light - tmp : 0;
        else
            return (light->light + tmp < 0) ? light->light + tmp : 0;
    default:
        return 1;  /* shut up compiler warnings */
    }
}
// jkrige

/*
================
SingleLightFace
================
*/
//void SingleLightFace (entity_t *light, lightinfo_t *l)
void SingleLightFace (entity_t *light, lightinfo_t *l, vec3_t faceoffset) // jkrige
{
	double	dist;
	vec3_t	incoming;
	double	angle;
	double	add;
	double	*surf;
	qboolean	hit;
	int		mapnum;
	int		size;
	int		c, i;
	vec3_t	rel;
	vec3_t	spotvec;
	double	falloff;
	double	*lightsamp;

#ifdef HX2COLOR
	// jkrige
	vec3_t        *lightcolorsamp;
	// jkrige
#endif
	
	VectorSubtract (light->origin, bsp_origin, rel);
	//dist = scaledist * (DotProduct (rel, l->facenormal) - l->facedist);
	dist = (double)scaledDistance((float)(DotProduct(rel, l->facenormal) - l->facedist), light); // jkrige - added (double) (float)
	
// don't bother with lights behind the surface
	if (dist <= 0)
		return;
		
// don't bother with light too far away
	//if (dist > light->light)
	if (dist > abs(light->light)) // jkrige
	{
		c_culldistplane++;
		return;
	}

	if (light->targetent)
	{
		VectorSubtract (light->targetent->origin, light->origin, spotvec);
		VectorNormalize (spotvec);
		if (!light->angle)
			falloff = -cos(20*Q_PI/180);
		else
			falloff = -cos(light->angle/2*Q_PI/180);
	}
	else if (light->use_mangle) // jkrige
	{
        VectorCopy (light->mangle, spotvec);
        if (!light->angle)
            falloff = -cos(20*Q_PI/180);
        else
            falloff = -cos(light->angle/2*Q_PI/180);
    }
	else
		falloff = 0;	// shut up compiler warnings
	
	mapnum = 0;
	for (mapnum=0 ; mapnum<l->numlightstyles ; mapnum++)
		if (l->lightstyles[mapnum] == light->style)
			break;

	lightsamp = l->lightmaps[mapnum];

	// jkrige
#ifdef HX2COLOR
	lightcolorsamp = l->lightmapcolors[mapnum];
#endif

    /* TYR: This way of doing things will generate "Too many light
     *      styles on a face" errors that aren't necessarily true.
     *      This is especially likely to happen if you make styled
     *      lights with delay != 0 as they are seen to be within
     *      lighting distance, even though they may be on totally
     *      opposite sides of the map (or separated by solid wall(s))
     *
     *      We should only generate the error when if find out that the
     *      face actually gets hit by the light
     *
     *      I think maybe MAXLIGHTMAPS is actually supposed to be 3.
     *      Have to make some more test maps (or check the engine source)
     *      for this.
     */
	// jkrige

	if (mapnum == l->numlightstyles)
	{	// init a new light map

		// jkrige
		//if (mapnum == MAXLIGHTMAPS)
		//{
		//	logprint ("WARNING: Too many light styles on a face\n");
		//	return;
		//}
		// jkrige
		size = (l->texsize[1]+1)*(l->texsize[0]+1);
		// jkrige
		//for (i=0 ; i<size ; i++)
		//	lightsamp[i] = 0;
		for (i=0 ; i<size ; i++)
		{
#ifdef HX2COLOR
			// jkrige
            if (colored)
			{
                lightcolorsamp[i][0] = 0;
                lightcolorsamp[i][1] = 0;
                lightcolorsamp[i][2] = 0;
            }
			// jkrige
#endif

			lightsamp[i] = 0;
		}
		// jkrige
	}

//
// check it for real
//
	hit = false;
	c_proper++;
	
	surf = l->surfpt[0];
	for (c=0 ; c<l->numsurfpt ; c++, surf+=3)
	{
		//dist = CastRay(light->origin, surf)*scaledist;
		dist = (double)scaledDistance((float)CastRay(light->origin, surf), light); // jkrige - added (double), (float) ?
		if (dist < 0)
			continue;	// light doesn't reach

		VectorSubtract (light->origin, surf, incoming);
		VectorNormalize (incoming);
		angle = DotProduct (incoming, l->facenormal);
		//if (light->targetent)
		if (light->targetent || light->use_mangle) // jkrige
		{	// spotlight cutoff
			if (DotProduct (spotvec, incoming) > falloff)
				continue;
		}

		angle = (1.0-scalecos) + scalecos*angle;
		//add = light->light - dist;
		add = (double)scaledLight((float)CastRay(light->origin, surf), light); // jkrige - added (double), (float) ?
		add *= angle;

		// jkrige - nominlimit
		//if (add < 0)
		//	continue;
		// jkrige - nominlimit

		lightsamp[c] += add;

#ifdef HX2COLOR
		// jkrige
		if (colored) {
            // tQER<1>: Calculate add and keep in CPU register for faster
            // processing. x2.24 faster in profiler:
            add /= 255.0f;
            lightcolorsamp[c][0] += (float)add * light->lightcolor[0];
            lightcolorsamp[c][1] += (float)add * light->lightcolor[1];
            lightcolorsamp[c][2] += (float)add * light->lightcolor[2];
        }
		// jkrige
#endif

		// jkrige
		//if (lightsamp[c] > 1)		// ignore real tiny lights
		//	hit = true;
		if (fabs(lightsamp[c]) > 1)  // ignore really tiny lights // jkrige - changed abs() to fabs()
            hit = true;
		// jkrige
	}
		
	if (mapnum == l->numlightstyles && hit)
	{
		// jkrige
		if (mapnum == MAXLIGHTMAPS-1)
		{
            logprint ("WARNING: Too many light styles on a face\n");
            logprint ("   lightmap point near (%0.0f, %0.0f, %0.0f)\n", l->surfpt[0][0], l->surfpt[0][1], l->surfpt[0][2]);
            logprint ("   light->origin (%0.0f, %0.0f, %0.0f)\n", light->origin[0], light->origin[1], light->origin[2]);
            return;
        }
		// jkrige

		l->lightstyles[mapnum] = light->style;
		l->numlightstyles++;	// the style has some real data now
	}
}

// jkrige
/*
void SkyLightFace(lightinfo_t * l) //, const vec3_t faceoffset)
{
    int i, j;
    vec_t *surf;
    vec3_t incoming;
    vec_t angle;

    // Don't bother if surface facing away from sun
    //if (DotProduct (sunmangle, l->facenormal) <= 0)
    if (DotProduct(sunmangle, l->facenormal) < -ANGLE_EPSILON)
	return;

    // if sunlight is set, use a style 0 light map
    for (i = 0; i < l->numlightstyles; i++)
	if (l->lightstyles[i] == 0)
	    break;
    if (i == l->numlightstyles) {
	if (l->numlightstyles == MAXLIGHTMAPS)
	    return;		// oh well, too many lightmaps...
	l->lightstyles[i] = 0;
	l->numlightstyles++;
    }

    // Check each point...
    VectorCopy(sunmangle, incoming);
    VectorNormalize(incoming);
    angle = DotProduct(incoming, l->facenormal);
    angle = (1.0 - scalecos) + scalecos * angle;

#if 0
    // Experimental - lighting of faces parallel to sunlight
    {
	int a, b, c, k;
	vec_t oldangle, offset;
	vec3_t sun_vectors[5];

	// Try to hit parallel surfaces?
	oldangle = DotProduct(incoming, l->facenormal);
	if (oldangle < ANGLE_EPSILON) {
	    printf("real small angle! (%f)\n", oldangle);
	    angle = (1.0 - scalecos) + scalecos * ANGLE_EPSILON;
	}

	a = fabs(sunmangle[0]) > fabs(sunmangle[1]) ?
	    (fabs(sunmangle[0]) > fabs(sunmangle[2]) ? 0 : 2) :
	    (fabs(sunmangle[1]) > fabs(sunmangle[2]) ? 1 : 2);
	b = (a + 1) % 3;
	c = (a + 2) % 3;

	offset = sunmangle[a] * ANGLE_EPSILON * 2.0;	// approx...
	for (j = 0; j < 5; ++j)
	    VectorCopy(sunmangle, sun_vectors[j]);
	sun_vectors[1][b] += offset;
	sun_vectors[2][b] -= offset;
	sun_vectors[3][c] += offset;
	sun_vectors[4][c] -= offset;

	surf = l->surfpt[0];
	for (j = 0; j < l->numsurfpt; j++, surf += 3) {
	    for (k = 0; k < 1 || (oldangle < ANGLE_EPSILON && k < 5); ++k) {
		if (TestSky(surf, sun_vectors[k])) {
		    l->lightmaps[i][j] += (angle * sunlight);
		    if (colored)
			VectorMA(l->lightmapcolors[i][j], angle * sunlight /
				 (vec_t)255, sunlight_color,
				 l->lightmapcolors[i][j]);
		    break;
		}
	    }
	}
    }
#else
    surf = l->surfpt[0];
    for (j = 0; j < l->numsurfpt; j++, surf += 3)
	{
		if (TestSky(surf, sunmangle))
		{
			l->lightmaps[i][j] += (angle * sunlight);
			if (colored)
				VectorMA(l->lightmapcolors[i][j], angle * sunlight / (vec_t)255, sunlight_color, l->lightmapcolors[i][j]);
		}
    }
#endif
}*/
// jkrige

/*
============
FixMinlight
============
*/
void FixMinlight (lightinfo_t *l)
{
	int   i, j;
#ifdef HX2COLOR
	int   k;
    vec_t tmp;
#endif
	//float	minlight;
	
	//minlight = minlights[l->surfnum];

// if minlight is set, there must be a style 0 light map
	if (!worldminlight)
		return; // jkrige
	
	for (i=0 ; i< l->numlightstyles ; i++)
	{
		if (l->lightstyles[i] == 0)
			break;
	}
	if (i == l->numlightstyles)
	{
		if (l->numlightstyles == MAXLIGHTMAPS)
			return;		// oh well..
		// jkrige
		for (j=0 ; j<l->numsurfpt ; j++)
			l->lightmaps[i][j] = worldminlight; //minlight;
#ifdef HX2COLOR
		if (colored)
		{
            for (j=0 ; j<l->numsurfpt ; j++)
			{
                l->lightmapcolors[i][j][0] = (worldminlight * minlight_color[0]) / 255;
                l->lightmapcolors[i][j][1] = (worldminlight * minlight_color[1]) / 255;
                l->lightmapcolors[i][j][2] = (worldminlight * minlight_color[2]) / 255;
            }
		}
		// jkrige
#endif
		l->lightstyles[i] = 0;
		l->numlightstyles++;
	}
	else
	{
		// jkrige
		for (j=0 ; j<l->numsurfpt ; j++)
		{
			if ( l->lightmaps[i][j] < worldminlight /*minlight*/)
				l->lightmaps[i][j] = worldminlight; //minlight
#ifdef HX2COLOR
			if (colored)
			{
                for (k=0 ; k<3 ; k++)
				{
                    tmp = (vec_t)(worldminlight * minlight_color[k]) / 255.0f;
                    if (l->lightmapcolors[i][j][k] < tmp )
                        l->lightmapcolors[i][j][k] = tmp;
                }
            }
#endif
		}
		// jkrige
	}
}


// jkrige
/*
 * To (help) make the code below in SingleLightFace more readable, I'll use
 * these macros.
 *
 * light is the light intensity, needed to check if +ve or -ve.
 * src and dest are the source and destination color vectors (vec3_t).
 * dest becomes a copy of src where
 *    MakePosColoredLight removes negative light components.
 *    MakeNegColoredLight removes positive light components.
 */
#ifdef HX2COLOR
#define MakePosColoredLight(light, dest, src)    \
if (light >= 0) {                                \
    for (k=0 ; k<3 ; k++)                        \
        if (src[k] < 0) dest[k] = 0;             \
        else            dest[k] = src[k];        \
} else {                                         \
    for (k=0 ; k<3 ; k++)                        \
        if (src[k] > 0) dest[k] = 0;             \
        else            dest[k] = src[k];        \
}
#define MakeNegColoredLight(light, dest, src)    \
if (light >= 0) {                                \
    for (k=0 ; k<3 ; k++)                        \
        if (src[k] > 0) dest[k] = 0;             \
        else            dest[k] = src[k];        \
} else {                                         \
    for (k=0 ; k<3 ; k++)                        \
        if (src[k] < 0) dest[k] = 0;             \
        else            dest[k] = src[k];        \
}
#endif

extern	FILE		*litfile;
// jkrige

/*
============
LightFace
============
*/
//void LightFace (int surfnum)
void LightFace (int surfnum, qboolean nolight, vec3_t faceoffset) // jkrige
{
	dface_t *f;
	lightinfo_t	l;
	int		s, t;
	int		i,j,c;

	// jkrige
    int         x1,x2,x3,x4;
#ifdef HX2COLOR
	vec_t       max;
    
#endif
	//int         k; // for the macros
	// jkrige

	double	total;
	int		size;
	int		lightmapwidth, lightmapsize;
	byte	*out;
	double	*light;
	int		w, h;

	// jkrige
	vec3_t		point;
#ifdef HX2COLOR
    vec3_t      *lightcolor;
    vec3_t      totalcolors;
	static int	litpixelswritten = 0; // LordHavoc: used for padding out the .lit file to match lightofs
	// jkrige
#endif
	
	f = dfaces + surfnum;

//
// some surfaces don't need lightmaps
//
	f->lightofs = -1;
	for (j=0 ; j<MAXLIGHTMAPS ; j++)
		f->styles[j] = 255;

	if ( texinfo[f->texinfo].flags & TEX_SPECIAL)
	{	// non-lit texture
		return;
	}

	memset (&l, 0, sizeof(l));
	l.surfnum = surfnum;
	l.face = f;

//
// rotate plane
//
	// jkrige
	VectorCopy(dplanes[f->planenum].normal, l.facenormal);
    l.facedist = dplanes[f->planenum].dist;
    VectorScale(l.facenormal, l.facedist, point);
    VectorAdd(point, faceoffset, point);
    l.facedist = DotProduct(point, l.facenormal);
	//VectorCopy (dplanes[f->planenum].normal, l.facenormal);
	//l.facedist = dplanes[f->planenum].dist;
	// jkrige
	if (f->side)
	{
		VectorSubtract (vec3_origin, l.facenormal, l.facenormal);
		l.facedist = -l.facedist;
	}

	CalcFaceVectors (&l);
	//CalcFaceExtents (&l);
	CalcFaceExtents (&l, faceoffset); // jkrige
	CalcPoints (&l);

	lightmapwidth = l.texsize[0]+1;

	size = lightmapwidth*(l.texsize[1]+1);
	if (size > SINGLEMAP)
		logerror ("Bad lightmap size"); // jkrige - was Error()

	for (i=0 ; i<MAXLIGHTMAPS ; i++)
		l.lightstyles[i] = 255;

	/* Under normal circumstances, the lighting procedure is:
     * - cast all light entities
     * - cast sky lighting
     * - do minlighting.
     *
     * However, if nominlimit is enabled then we need to do the following:
     * - cast _positive_ lights
     * - cast _positive_ skylight (if any)
     * - do minlighting
     * - cast _negative_ lights
     * - cast _negative_ sky light (if any)
     */
	
//
// cast all lights
//	
	l.numlightstyles = 0;

	if (nominlimit) { // jkrige
        // cast only positive lights
#ifdef HX2COLOR
        entity_t lt;
#endif

//        for (i=0 ; i<num_entities ; i++) {
//            if (entities[i].light) {
//                memcpy(&lt, &entities[i], sizeof(entity_t));
//#ifdef HX2COLOR
//                MakePosColoredLight (entities[i].light, lt.lightcolor, entities[i].lightcolor); // jkrige
//#endif
//                SingleLightFace (&lt, &l, faceoffset); // jkrige
//            }
//        }

		for (i = 0; i < num_entities; i++)
		{
#ifdef HX2COLOR
			if (colored)
			{
				if (entities[i].light)
				{
					memcpy(&lt, &entities[i], sizeof(entity_t));
					MakePosColoredLight(entities[i].light, lt.lightcolor, entities[i].lightcolor);
					SingleLightFace(&lt, &l, faceoffset);
				}
			}
			else if (entities[i].light > 0)
#else
			if (entities[i].light > 0)
#endif
			{
				SingleLightFace(&entities[i], &l, faceoffset);
			}
		}

		// cast positive sky light
		/*if (sunlight)
		{
			if (colored)
			{
				vec3_t suncol_save;
				
				VectorCopy(sunlight_color, suncol_save);
				MakePosColoredLight(sunlight, sunlight_color, suncol_save);
				SkyLightFace(&l); // , faceoffset);
				VectorCopy(suncol_save, sunlight_color);
			}
			else if (sunlight > 0)
			{
				SkyLightFace(&l); //, faceoffset);
			}
		}*/

    } else { // jkrige
		for (i=0 ; i<num_entities ; i++)
		{
			if (entities[i].light)
				SingleLightFace (&entities[i], &l, faceoffset); // jkrige
		}

		/*
		// cast sky light
		if (sunlight)
			SkyLightFace(&l); // , faceoffset);
		*/
	}

	FixMinlight (&l);

	// jkrige
	if (nominlimit && !noneglight)
	{
        // cast only negative lights
#ifdef HX2COLOR
        entity_t lt;
#endif

//        for (i=0 ; i<num_entities ; i++)
//		{
//            if (entities[i].light)
//			{
//                //memcpy(&lt, &entities[i], sizeof(entity_t));
//#ifdef HX2COLOR
//                MakeNegColoredLight (entities[i].light, lt.lightcolor, entities[i].lightcolor); // jkrige
//#endif
//                //SingleLightFace (&lt, &l, faceoffset); // jkrige
//            }
//        }

		for (i = 0; i < num_entities; i++)
		{
#ifdef HX2COLOR
			if (colored)
			{
				if (entities[i].light)
				{
					memcpy(&lt, &entities[i], sizeof(entity_t));
					MakeNegColoredLight(entities[i].light, lt.lightcolor, entities[i].lightcolor);
					SingleLightFace(&lt, &l, faceoffset);
				}
			}
			else if (entities[i].light < 0)
#else
			if (entities[i].light < 0)
#endif
			{
				SingleLightFace(&entities[i], &l, faceoffset);
			}
		}

		// cast negative sky light
		/*if (sunlight)
		{
			if (colored)
			{
			vec3_t suncol_save;

			VectorCopy(sunlight_color, suncol_save);
			MakeNegColoredLight(sunlight, sunlight_color, suncol_save);
			SkyLightFace(&l); //, faceoffset);
			VectorCopy(suncol_save, sunlight_color);
			}
			else if (sunlight < 0)
			{
				SkyLightFace(&l); // , faceoffset);
			}
		}*/


        // Fix any negative values
		//k = worldminlight;
		//worldminlight = 0;
        //FixMinlight (&l);
        //worldminlight = k;

		for (i = 0; i < l.numlightstyles; i++)
		{
			for (j = 0; j < l.numsurfpt; j++)
			{
				if (l.lightmaps[i][j] < 0)
				{
					l.lightmaps[i][j] = 0;
				}
#ifdef HX2COLOR
				if (colored)
				{
					for (k = 0; k < 3; k++)
					{
						if (l.lightmapcolors[i][j][k] < 0)
						{
							l.lightmapcolors[i][j][k] = 0;
						}
					}
				}
#endif
			}
		}

    }
	// jkrige
		
	if (!l.numlightstyles)
	{	// no light hitting it
		return;
	}
	
//
// save out the values
//
	for (i=0 ; i <MAXLIGHTMAPS ; i++)
		f->styles[i] = l.lightstyles[i];

	lightmapsize = size*l.numlightstyles;

	out = GetFileSpace (lightmapsize);
	f->lightofs = out - filebase;

#ifdef HX2COLOR
	// jkrige
	// LordHavoc: pad out litfile to match the lightofs table
	if (!litpixelswritten)
		litpixelswritten = f->lightofs;
	if (colored)
	{
		for (;litpixelswritten < f->lightofs;litpixelswritten++)
		{
			fputc(0, litfile);
			fputc(0, litfile);
			fputc(0, litfile);
		}
	}
	// jkrige
#endif
	
// extra filtering
	h = (l.texsize[1]+1)*2;
	w = (l.texsize[0]+1)*2;

	for (i=0 ; i< l.numlightstyles ; i++)
	{
		if (l.lightstyles[i] == 0xff)
			logerror ("Wrote empty lightmap"); // jkrige - was Error()
		light = l.lightmaps[i];
#ifdef HX2COLOR
		lightcolor = l.lightmapcolors[i]; // jkrige
#endif
		c = 0;
		for (t=0 ; t<=l.texsize[1] ; t++)
			for (s=0 ; s<=l.texsize[0] ; s++, c++)
			{
				if (extrasamples)
				{
					// jkrige
					x1 = t*2*w+s*2;
                    x2 = x1+1;
                    x3 = (t*2+1)*w+s*2;
                    x4 = x3+1;
					// jkrige

					// filtered sample
					// jkrige
					//total = light[t*2*w+s*2] + light[t*2*w+s*2+1] + light[(t*2+1)*w+s*2] + light[(t*2+1)*w+s*2+1];
					total = light[x1] + light[x2] + light[x3] + light[x4];
					// jkrige
					total *= 0.25;

#ifdef HX2COLOR
					// jkrige
					if (colored)
					{
                        totalcolors[0] = lightcolor[x1][0] + lightcolor[x2][0] + lightcolor[x3][0] + lightcolor[x4][0];
                        totalcolors[0] *= 0.25;
                        totalcolors[1] = lightcolor[x1][1] + lightcolor[x2][1] + lightcolor[x3][1] + lightcolor[x4][1];
                        totalcolors[1] *= 0.25;
                        totalcolors[2] = lightcolor[x1][2] + lightcolor[x2][2] + lightcolor[x3][2] + lightcolor[x4][2];
                        totalcolors[2] *= 0.25;
                    }
					// jkrige
#endif
				}
				else
				{
					total = light[c];

#ifdef HX2COLOR
					// jkrige
					if (colored)
                        VectorCopy (lightcolor[c], totalcolors);
					// jkrige
#endif
				}
				total *= rangescale;	// scale before clamping

#ifdef HX2COLOR
				// jkrige
				// CSL - Scale back intensity, instead of capping individual colors
                if (colored) {
                    VectorScale (totalcolors, rangescale, totalcolors);
                    max = 0.0;
                    for (j = 0; j < 3; j++)
					{
                        if (totalcolors[j] > max)
						{
                            max = totalcolors[j];
                        }
						else if (totalcolors[j] < 0.0f)
						{
                            logerror ("color %i < 0", j); // jkrige - was Error()
                        }
					}
                    if (max > 255.0f)
                        VectorScale (totalcolors, 255.0f / max, totalcolors);
                }
				// jkrige
#endif

				if (total > 255)
					total = 255;
				if (total < 0)
					//Error ("light < 0");
					total = 0;

#ifdef HX2COLOR
				// jkrige
				// write out the lightmap in RGBA format
                if (colored) 
				{
					litpixelswritten++;
					fputc((int)totalcolors[0], litfile); // jkrige - added (int)
					fputc((int)totalcolors[1], litfile); // jkrige - added (int)
					fputc((int)totalcolors[2], litfile); // jkrige - added (int)
                }
				// jkrige
#endif

				*out++ = (byte) total;
			}
	}

	
}

