
// draw.c -- this is the only file outside the refresh that touches the
// vid buffer

/*
 * $Header: /H2 Mission Pack/gl_draw.c 2     2/26/98 3:09p Jmonroe $
 */

#include "quakedef.h"

// jkrige - scale2d
#ifdef _WIN32
#include "winquake.h"
#endif
// jkrige - scale2d

// jkrige - jpg
#ifdef JPG_SUPPORT
#include "../jpeg-6/jpeglib.h"
byte	*jpeg_rgba;
#endif
// jkrige - jpg

extern int ColorIndex[16];
extern unsigned ColorPercent[16];
extern qboolean	vid_initialized;

#define MAX_DISC 18

cvar_t		gl_nobind = {"gl_nobind", "0"};
cvar_t		gl_max_size = {"gl_max_size", "1024"};

// jkrige - non power of two
//cvar_t		gl_round_down = {"gl_round_down", "0"};
// jkrige - non power of two

cvar_t		gl_picmip = {"gl_picmip", "0"};

// jkrige - luma textures
cvar_t		gl_lumatex_render = {"gl_lumatex_render", "1", true};
// jkrige - luma textures

byte		*draw_chars;				// 8*8 graphic characters
byte		*draw_smallchars;			// Small characters for status bar
byte		*draw_menufont; 			// Big Menu Font
qpic_t		*draw_disc[MAX_DISC] =
{
	NULL  // make the first one null for sure
};
qpic_t		*draw_backtile;

int			translate_texture[NUM_CLASSES];
int			char_texture;
int			char_smalltexture;
int			char_menufonttexture;

byte		conback_buffer[sizeof(qpic_t) + sizeof(glpic_t)];
qpic_t		*conback = (qpic_t *)&conback_buffer;

// jkrige - .lit colored lights
//int		gl_lightmap_format = 4;
// jkrige - .lit colored lights
int		gl_solid_format = 3;
int		gl_alpha_format = 4;

// jkrige - change bilinear texture filtering to trilinear texture filtering
//int		gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
//int		gl_filter_max = GL_LINEAR;
int		gl_filter_min = GL_LINEAR_MIPMAP_LINEAR;
int		gl_filter_max = GL_LINEAR_MIPMAP_LINEAR;
// jkrige - change bilinear texture filtering to trilinear texture filtering

float tex_mode = -1.0f;

int		texels;

qboolean is_3dfx = false;
qboolean is_PowerVR = false;
//qboolean is_3dfx = true;
//qboolean is_PowerVR = true;

// jkrige - moved to glquake.h
/*typedef struct
{
	int		texnum;

	qboolean	tex_luma; // jkrige - luma textures

	char	identifier[64];
	int		width, height;
	qboolean	mipmap;
	int		lhcsum; // jkrige - memleak & texture mismatch
} gltexture_t;*/

//#define MAX_GLTEXTURES	2048
//#define MAX_GLTEXTURES	4096 // jkrige - increased maximum number of opengl textures
// jkrige - moved to glquake.h

gltexture_t	gltextures[MAX_GLTEXTURES];
int			numgltextures;


void GL_Bind (int texnum)
{
	if (gl_nobind.value)
		texnum = char_texture;
	if (currenttexture == texnum)
		return;
	currenttexture = texnum;

	// jkrige - opengl's bind function
	//bindTexFunc (GL_TEXTURE_2D, texnum);
	glBindTexture(GL_TEXTURE_2D, texnum);
	// jkrige - opengl's bind function
}

void GL_Texels_f (void)
{
	Con_Printf ("Current uploaded texels: %i\n", texels);
}

/*
=============================================================================

  scrap allocation

  Allocate all the little status bar obejcts into a single texture
  to crutch up stupid hardware / drivers

=============================================================================
*/

#define MAX_SCRAPS		1
#define BLOCK_WIDTH		256
#define BLOCK_HEIGHT	256

int			scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH];
byte		scrap_texels[MAX_SCRAPS][BLOCK_WIDTH*BLOCK_HEIGHT*4];
qboolean	scrap_dirty;
int			scrap_texnum;

// returns a texture number and the position inside it
int Scrap_AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	int		bestx;
	int		texnum;

	for (texnum=0 ; texnum<MAX_SCRAPS ; texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i=0 ; i<BLOCK_WIDTH-w ; i++)
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (scrap_allocated[texnum][i+j] >= best)
					break;
				if (scrap_allocated[texnum][i+j] > best2)
					best2 = scrap_allocated[texnum][i+j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i=0 ; i<w ; i++)
			scrap_allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	return -1;
//	Sys_Error ("Scrap_AllocBlock: full");
}

int	scrap_uploads;

void Scrap_Upload (void)
{
	scrap_uploads++;
	GL_Bind(scrap_texnum);
	GL_Upload8 (scrap_texels[0], BLOCK_WIDTH, BLOCK_HEIGHT, true, false, true, 0);
	scrap_dirty = false;

	// jkrige - scale2d
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	// jkrige - scale2d
}

//=============================================================================
/* Support Routines */

typedef struct cachepic_s
{
	char		name[MAX_QPATH];
	qpic_t		pic;
	byte		padding[32];	// for appended glpic
} cachepic_t;

//#define	MAX_CACHED_PICS 	128
#define MAX_CACHED_PICS 	256
cachepic_t	menu_cachepics[MAX_CACHED_PICS];
int			menu_numcachepics;

/*
 * Geometry for the player/skin selection screen image.
 */
#define PLAYER_PIC_WIDTH 68
#define PLAYER_PIC_HEIGHT 114
#define PLAYER_DEST_WIDTH 128
#define PLAYER_DEST_HEIGHT 128

byte		menuplyr_pixels[NUM_CLASSES][PLAYER_PIC_WIDTH*PLAYER_PIC_HEIGHT];

int		pic_texels;
int		pic_count;

qpic_t *Draw_PicFromFile (char *name)
{
	qpic_t	*p;
	glpic_t *gl;

	p = (qpic_t	*)COM_LoadHunkFile (name);
	if (!p)
	{
		return NULL;
	}

	gl = (glpic_t *)p->data;

	// load little ones into the scrap
/*	if (p->width < 64 && p->height < 64)
	{
		int		x, y;
		int		i, j, k;
		int		texnum;

		texnum = Scrap_AllocBlock (p->width, p->height, &x, &y);
		if (texnum == -1)
			goto nonscrap;
		scrap_dirty = true;
		k = 0;
		for (i=0 ; i<p->height ; i++)
			for (j=0 ; j<p->width ; j++, k++)
				scrap_texels[texnum][(y+i)*BLOCK_WIDTH + x + j] = p->data[k];
		texnum += scrap_texnum;
		gl->texnum = texnum;
		gl->sl = (x+0.01)/(float)BLOCK_WIDTH;
		gl->sh = (x+p->width-0.01)/(float)BLOCK_WIDTH;
		gl->tl = (y+0.01)/(float)BLOCK_WIDTH;
		gl->th = (y+p->height-0.01)/(float)BLOCK_WIDTH;

		pic_count++;
		pic_texels += p->width*p->height;
	}
	else*/
	{
nonscrap:
		gl->texnum = GL_LoadPicTexture (p);

		// jkrige - scale2d
		//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		// jkrige - scale2d

		gl->sl = 0;
		gl->sh = 1;
		gl->tl = 0;
		gl->th = 1;
	}
	return p;
}

qpic_t *Draw_PicFromWad (char *name)
{
	qpic_t	*p;
	glpic_t *gl;

	p = W_GetLumpName (name);
	gl = (glpic_t *)p->data;

	// load little ones into the scrap
	if (p->width < 64 && p->height < 64)
	{
		int		x, y;
		int		i, j, k;
		int		texnum;

		texnum = Scrap_AllocBlock (p->width, p->height, &x, &y);
		if (texnum == -1)
			goto nonscrap;
		scrap_dirty = true;
		k = 0;
		for (i=0 ; i<p->height ; i++)
			for (j=0 ; j<p->width ; j++, k++)
				scrap_texels[texnum][(y+i)*BLOCK_WIDTH + x + j] = p->data[k];
		texnum += scrap_texnum;
		gl->texnum = texnum;
		gl->sl = (x+0.01)/(float)BLOCK_WIDTH;
		gl->sh = (x+p->width-0.01)/(float)BLOCK_WIDTH;
		gl->tl = (y+0.01)/(float)BLOCK_WIDTH;
		gl->th = (y+p->height-0.01)/(float)BLOCK_WIDTH;

		pic_count++;
		pic_texels += p->width*p->height;
	}
	else
	{
nonscrap:

		gl->texnum = GL_LoadPicTexture (p);

		// jkrige - scale2d
		//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		// jkrige - scale2d

		gl->sl = 0;
		gl->sh = 1;
		gl->tl = 0;
		gl->th = 1;
	}
	return p;
}


/*
================
Draw_CachePic
================
*/
qpic_t	*Draw_CachePic (char *path)
{
	cachepic_t	*pic;
	int			i;
	qpic_t		*dat;
	glpic_t 	*gl;

	for (pic=menu_cachepics, i=0 ; i<menu_numcachepics ; pic++, i++)
		if (!strcmp (path, pic->name))
			return &pic->pic;

	if (menu_numcachepics == MAX_CACHED_PICS)
		Sys_Error ("menu_numcachepics == MAX_CACHED_PICS");
	menu_numcachepics++;
	strcpy (pic->name, path);

//
// load the pic from disk
//
	dat = (qpic_t *)COM_LoadTempFile (path);
	if (!dat)
		Sys_Error ("Draw_CachePic: failed to load %s", path);
	SwapPic (dat);

	// HACK HACK HACK --- we need to keep the bytes for
	// the translatable player picture just for the menu
	// configuration dialog
	/* garymct */
	if (!strcmp (path, "gfx/menu/netp1.lmp"))
		memcpy (menuplyr_pixels[0], dat->data, dat->width*dat->height);
	else if (!strcmp (path, "gfx/menu/netp2.lmp"))
		memcpy (menuplyr_pixels[1], dat->data, dat->width*dat->height);
	else if (!strcmp (path, "gfx/menu/netp3.lmp"))
		memcpy (menuplyr_pixels[2], dat->data, dat->width*dat->height);
	else if (!strcmp (path, "gfx/menu/netp4.lmp"))
		memcpy (menuplyr_pixels[3], dat->data, dat->width*dat->height);
	else if (!strcmp (path, "gfx/menu/netp5.lmp"))
		memcpy (menuplyr_pixels[4], dat->data, dat->width*dat->height);

	pic->pic.width = dat->width;
	pic->pic.height = dat->height;

	gl = (glpic_t *)pic->pic.data;
	gl->texnum = GL_LoadPicTexture (dat);

	// point sample status bar
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// jkrige - scale2d
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// jkrige - scale2d

	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return &pic->pic;
}


/*void Draw_CharToConback (int num, byte *dest)
{
	int		row, col;
	byte	*source;
	int		drawline;
	int		x;

	row = num>>5;
	col = num&31;
	source = draw_chars + (row<<11) + (col<<3);

	drawline = 8;

	while (drawline--)
	{
		for (x=0 ; x<8 ; x++)
			if (source[x] != 255)
				dest[x] = 0x60 + source[x];
		source += 256;
		dest += 320;
	}

}*/

typedef struct
{
	char *name;
	int	minimize, maximize;
} mode_t;

mode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

/*
===============
Draw_TextureMode_f
===============
*/
/*void Draw_TextureMode_f (void)
{
	int		i, j;
	gltexture_t	*glt;

	if (Cmd_Argc() == 1)
	{
		for (i=0 ; i< 6 ; i++)
			if (gl_filter_min == modes[i].minimize)
			{
				Con_Printf ("%s\n", modes[i].name);
				return;
			}
		Con_Printf ("current filter is unknown???\n");
		return;
	}

	for (i=0 ; i< 6 ; i++)
	{
		if (!Q_strcasecmp (modes[i].name, Cmd_Argv(1) ) )
			break;
	}
	if (i == 6)
	{
		Con_Printf ("bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for (j=0, glt=gltextures ; j<numgltextures ; j++, glt++)
	{
		if (glt->mipmap)
		{
			GL_Bind (glt->texnum);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

			// jkrige - anisotropic filtering
			if(i == 5 && anisotropic_ext == true)
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maximumAnisotrophy);
			// jkrige - anisotropic filtering
		}
	}
}*/

// jkrige - texture mode
extern int	solidskytexture;
extern int	alphaskytexture;

void Draw_TextureMode_f (void)
{
	int		i, j;
	gltexture_t	*glt;
	int AnisotropyModes = 0;

	if(tex_mode < 0.0f)
		tex_mode = gl_texturemode.value;

	if(tex_mode == gl_texturemode.value)
		return;

	if(floor(gl_texturemode.value) < 0.0f | gl_texturemode.value != floor(gl_texturemode.value))
	{
		Con_Printf ("unknown texture mode\n");
		Cvar_Set ("gl_texturemode", "2");

		return;
	}

	i = 0;

	if(gl_texturemode.value == 1.0f)
		i = 3;

	if(gl_texturemode.value >= 2.0f)
		i = 5;

	if(anisotropic_ext == true)
		AnisotropyModes = (int)floor((log(maximumAnisotrophy)/log(2.0f)) + 0.5f);
	else
		AnisotropyModes = 0;

	if(anisotropic_ext == false)
	{
		if (gl_texturemode.value > 2.0f)
		{
			Con_Printf ("unknown texture mode\n");
			Cvar_Set ("gl_texturemode", "2");
			
			return;
		}
	}

	if(anisotropic_ext == true)
	{
		if (gl_texturemode.value > (2.0f + (float)AnisotropyModes))
		{
			Con_Printf ("unknown texture mode\n");
			Cvar_SetValue ("gl_texturemode", (2.0f + (float)AnisotropyModes));
			
			return;
		}
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for (j=0, glt=gltextures ; j<numgltextures ; j++, glt++)
	{
		if (glt->mipmap)
		{
			GL_Bind (glt->texnum);

			if(gl_texturemode.value <= 2.0f)
			{
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
				if(anisotropic_ext == true)
					glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
			}

			// jkrige - anisotropic filtering
			if(gl_texturemode.value > 2.0f && anisotropic_ext == true)
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, pow(2.0f, gl_texturemode.value - 2.0f));
			// jkrige - anisotropic filtering


			// jkrige - luma textures
			if(glt->tex_luma == true)
			{
				GL_Bind (JK_LUMA_TEX + glt->texnum);

				if(gl_texturemode.value <= 2.0f)
				{
					glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
					glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
					if(anisotropic_ext == true)
						glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
				}

				// jkrige - anisotropic filtering
				if(gl_texturemode.value > 2.0f && anisotropic_ext == true)
					glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, pow(2.0f, gl_texturemode.value - 2.0f));
				// jkrige - anisotropic filtering
			}
			// jkrige - luma textures
		}
	}


	// sky textures
	if(gl_texturemode.value == 0.0f)
	{
		GL_Bind (solidskytexture);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		GL_Bind (alphaskytexture);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	else
	{
		GL_Bind (solidskytexture);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		GL_Bind (alphaskytexture);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	// sky textures


	tex_mode = gl_texturemode.value;
}
// jkrige - texture mode

/*
===============
Draw_Init
===============
*/
void Draw_Init (void)
{
	int		i;
	qpic_t	*cb,*mf;
	byte	*dest;
	int		x, y;
	char	ver[40];
	glpic_t *gl;
	char temp[MAX_QPATH];

	Cvar_RegisterVariable (&gl_nobind);
	Cvar_RegisterVariable (&gl_max_size);

	// jkrige - non power of two
	//Cvar_RegisterVariable (&gl_round_down);
	// jkrige - non power of two

	Cvar_RegisterVariable (&gl_picmip);

	// jkrige - luma textures
	Cvar_RegisterVariable (&gl_lumatex_render);
	// jkrige - luma textures

	// 3dfx can only handle 256 wide textures
	if (is_3dfx || is_PowerVR)
	{
		Cvar_Set ("gl_max_size", "256");
	}

	//Cmd_AddCommand ("gl_texturemode", &Draw_TextureMode_f); // jkrige - texture mode
	Cmd_AddCommand ("gl_texels", &GL_Texels_f);


	// load the console background and the charset
	// by hand, because we need to write the version
	// string into the background before turning
	// it into a texture
	//draw_chars = W_GetLumpName ("conchars");
	draw_chars = COM_LoadHunkFile ("gfx/menu/conchars.lmp");
	for (i=0 ; i<256*128 ; i++)
		if (draw_chars[i] == 0)
			draw_chars[i] = 255;	// proper transparent color

	char_texture = GL_LoadTexture ("charset", "lump", 256, 128, draw_chars, false, true, 0, 1);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	draw_smallchars = W_GetLumpName("tinyfont");
	for (i=0 ; i<128*32 ; i++)
		if (draw_smallchars[i] == 0)
			draw_smallchars[i] = 255;	// proper transparent color

	// now turn them into textures
	char_smalltexture = GL_LoadTexture ("smallcharset", "lump", 128, 32, draw_smallchars, false, true, 0, 1);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	mf = (qpic_t *)COM_LoadTempFile("gfx/menu/bigfont2.lmp");
	for (i=0 ; i<160*80 ; i++)
		if (mf->data[i] == 0)
			mf->data[i] = 255;	// proper transparent color


	char_menufonttexture = GL_LoadTexture ("menufont", "lump", 160, 80, mf->data, false, true, 0, 1);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// jkrige - scale2d
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// jkrige - scale2d

	cb = (qpic_t *)COM_LoadTempFile ("gfx/menu/conback.lmp");
	if (!cb)
		Sys_Error ("Couldn't load gfx/menu/conback.lmp");
	SwapPic (cb);

	// jkrige - print version info to the console
	// hack the version number directly into the pic
	//dest = cb->data + 320 - 43 + 320*186;
	//sprintf (ver, "%4.2f", HEXEN2_VERSION);

//	sprintf (ver, "(gl %4.2f) %4.2f", (float)GLQUAKE_VERSION, (float)VERSION);  // old quake
//	dest = cb->data + 320*186 + 320 - 11 - 8*strlen(ver);						// old quake
	//y = strlen(ver);
	//for (x=0 ; x<y ; x++)
	//	Draw_CharToConback (ver[x], dest+(x<<3));
	// jkrige - print version info to the console

	gl = (glpic_t *)conback->data;
	gl->texnum = GL_LoadTexture ("conback", "lump", cb->width, cb->height, cb->data, false, false, 0, 1);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;
	conback->width = vid.conwidth;
	conback->height = vid.conheight;

	// save a texture slot for translated picture
	translate_texture[0] = texture_extension_number++;
	translate_texture[1] = texture_extension_number++;
	translate_texture[2] = texture_extension_number++;
	translate_texture[3] = texture_extension_number++;
	translate_texture[4] = texture_extension_number++;

	// save slots for scraps
	scrap_texnum = texture_extension_number;
	texture_extension_number += MAX_SCRAPS;

	//
	// get the other pics we need
	//
	for(i=MAX_DISC-1;i>=0;i--)
	{
		sprintf(temp,"gfx/menu/skull%d.lmp",i);
		draw_disc[i] = Draw_PicFromFile (temp);
	}

//	draw_disc = Draw_PicFromWad ("disc");
//	draw_backtile = Draw_PicFromWad ("backtile");
	draw_backtile = Draw_PicFromFile ("gfx/menu/backtile.lmp");
}



/*
================
Draw_Character

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void Draw_Character (int x, int y, unsigned int num)
{
	byte			*dest;
	byte			*source;
	unsigned short	*pusdest;
	int				drawline;
	int				row, col;
	float			frow, fcol, xsize,ysize;

	if (num == 32)
		return; 	// space

	num &= 511;

	if (y <= -8)
		return; 		// totally off screen

	row = num>>5;
	col = num&31;

	xsize = 0.03125;
	ysize = 0.0625;
	fcol = col*xsize;
	frow = row*ysize;

	GL_Bind (char_texture);

	glBegin (GL_QUADS);
	glTexCoord2f (fcol, frow);
	glVertex2f (x, y);
	glTexCoord2f (fcol + xsize, frow);
	glVertex2f (x+8, y);
	glTexCoord2f (fcol + xsize, frow + ysize);
	glVertex2f (x+8, y+8);
	glTexCoord2f (fcol, frow + ysize);
	glVertex2f (x, y+8);
	glEnd ();
}

/*
================
Draw_String
================
*/
void Draw_String (int x, int y, char *str)
{
	while (*str)
	{
		Draw_Character (x, y, *str);
		str++;
		x += 8;
	}
}


//==========================================================================
//
// Draw_SmallCharacter
//
// Draws a small character that is clipped at the bottom edge of the
// screen.
//
//==========================================================================
void Draw_SmallCharacter (int x, int y, int num)
{
	byte			*dest;
	byte			*source;
	unsigned short	*pusdest;
	int				drawline;
	int				row, col;
	float			frow, fcol, xsize,ysize;

	if(num < 32)
	{
		num = 0;
	}
	else if(num >= 'a' && num <= 'z')
	{
		num -= 64;
	}
	else if(num > '_')
	{
		num = 0;
	}
	else
	{
		num -= 32;
	}

	if (num == 0) return;

	if (y <= -8)
		return; 		// totally off screen

	if(y >= vid.height)
	{ // Totally off screen
		return;
	}

	row = num>>4;
	col = num&15;

	xsize = 0.0625;
	ysize = 0.25;
	fcol = col*xsize;
	frow = row*ysize;

	GL_Bind (char_smalltexture);

	glBegin (GL_QUADS);
	glTexCoord2f (fcol, frow);
	glVertex2f (x, y);
	glTexCoord2f (fcol + xsize, frow);
	glVertex2f (x+8, y);
	glTexCoord2f (fcol + xsize, frow + ysize);
	glVertex2f (x+8, y+8);
	glTexCoord2f (fcol, frow + ysize);
	glVertex2f (x, y+8);
	glEnd ();
}

//==========================================================================
//
// Draw_SmallString
//
//==========================================================================
void Draw_SmallString(int x, int y, char *str)
{
	while (*str)
	{
		Draw_SmallCharacter (x, y, *str);
		str++;
		x += 6;
	}
}

/*
================
Draw_DebugChar

Draws a single character directly to the upper right corner of the screen.
This is for debugging lockups by drawing different chars in different parts
of the code.
================
*/
void Draw_DebugChar (char num)
{
}

/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, qpic_t *pic)
{
//	byte			*dest, *source;
//	unsigned short	*pusdest;
//	int				v, u;
	glpic_t 		*gl;

	if (scrap_dirty)
		Scrap_Upload ();
	gl = (glpic_t *)pic->data;
	glColor4f (1,1,1,1);
	GL_Bind (gl->texnum);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glBegin (GL_QUADS);
	glTexCoord2f (gl->sl, gl->tl);
	glVertex2f (x, y);
	glTexCoord2f (gl->sh, gl->tl);
	glVertex2f (x+pic->width, y);
	glTexCoord2f (gl->sh, gl->th);
	glVertex2f (x+pic->width, y+pic->height);
	glTexCoord2f (gl->sl, gl->th);
	glVertex2f (x, y+pic->height);
	glEnd ();

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void Draw_PicCropped(int x, int y, qpic_t *pic)
{
	int height;
	glpic_t 		*gl;
	float th,tl;

	if((x < 0) || (x+pic->width > vid.width))
	{
		Sys_Error("Draw_PicCropped: bad coordinates");
	}

	if (y >= (int)vid.height || y+pic->height < 0)
	{ // Totally off screen
		return;
	}

	if (scrap_dirty)
		Scrap_Upload ();
	gl = (glpic_t *)pic->data;

	// rjr tl/th need to be computed based upon pic->tl and pic->th
	//     cuz the piece may come from the scrap
	if(y+pic->height > vid.height)
	{
		height = vid.height-y;
		tl = 0;
		th = (height-0.01)/pic->height;
	}
	else if (y < 0)
	{
		height = pic->height+y;
		y = -y;
		tl = (y-0.01)/pic->height;
		th = (pic->height-0.01)/pic->height;
		y = 0;
	}
	else
	{
		height = pic->height;
		tl = gl->tl;
		th = gl->th;//(height-0.01)/pic->height;
	}


	glColor4f (1,1,1,1);
	GL_Bind (gl->texnum);
	glBegin (GL_QUADS);
	glTexCoord2f (gl->sl, tl);
	glVertex2f (x, y);
	glTexCoord2f (gl->sh, tl);
	glVertex2f (x+pic->width, y);
	glTexCoord2f (gl->sh, th);
	glVertex2f (x+pic->width, y+height);
	glTexCoord2f (gl->sl, th);
	glVertex2f (x, y+height);
	glEnd ();
}

/*
=============
Draw_TransPic
=============
*/
void Draw_TransPic (int x, int y, qpic_t *pic)
{
//	byte	*dest, *source, tbyte;
//	unsigned short	*pusdest;
//	int				v, u;

	if (x < 0 || (unsigned)(x + pic->width) > vid.width || y < 0 ||
		 (unsigned)(y + pic->height) > vid.height)
	{
		Sys_Error ("Draw_TransPic: bad coordinates");
	}

	Draw_Pic (x, y, pic);
}

void Draw_TransPicCropped(int x, int y, qpic_t *pic)
{
	Draw_PicCropped (x, y, pic);
}

/*
=============
Draw_TransPicTranslate

Only used for the player color selection menu
=============
*/
void Draw_TransPicTranslate (int x, int y, qpic_t *pic, byte *translation)
{
	int				v, u, c;
	unsigned		trans[PLAYER_DEST_WIDTH * PLAYER_DEST_HEIGHT], *dest;
	byte			*src;
	int				p;

	extern int setup_class;

	GL_Bind (translate_texture[setup_class-1]);

	c = pic->width * pic->height;

	dest = trans;
	for (v=0 ; v<64 ; v++, dest += 64)
	{
		src = &menuplyr_pixels[setup_class-1][ ((v*pic->height)>>6) *pic->width];
		for (u=0 ; u<64 ; u++)
		{
			p = src[(u*pic->width)>>6];
			if (p == 255)
				dest[u] = p;
			else
				dest[u] =  d_8to24table[translation[p]];
		}
	}

#if 0
	{
		int i;
		
		for( i = 0; i < 64 * 64; i++ )
		{
			trans[i] = d_8to24table[translation[menuplyr_pixels[i]]];
		}
	}
#endif

	{
		int x, y;
		
		for( x = 0; x < PLAYER_PIC_WIDTH; x++ )
			for( y = 0; y < PLAYER_PIC_HEIGHT; y++ )
			{
				trans[y * PLAYER_DEST_WIDTH + x] = d_8to24table[translation[menuplyr_pixels[setup_class-1][y * PLAYER_PIC_WIDTH + x]]];
			}
	}

	glTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, PLAYER_DEST_WIDTH, PLAYER_DEST_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	//	  glTexImage2D (GL_TEXTURE_2D, 0, 1, 64, 64, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, menuplyr_pixels);

	// jkrige - use "point sampled" texture mode
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	// jkrige - use "point sampled" texture mode

	glColor3f (1,1,1);
	glBegin (GL_QUADS);
	glTexCoord2f (0, 0);
	glVertex2f (x, y);
	glTexCoord2f (( float )PLAYER_PIC_WIDTH / PLAYER_DEST_WIDTH, 0);
	glVertex2f (x+pic->width, y);
	glTexCoord2f (( float )PLAYER_PIC_WIDTH / PLAYER_DEST_WIDTH, ( float )PLAYER_PIC_HEIGHT / PLAYER_DEST_HEIGHT);
	glVertex2f (x+pic->width, y+pic->height);
	glTexCoord2f (0, ( float )PLAYER_PIC_HEIGHT / PLAYER_DEST_HEIGHT);
	glVertex2f (x, y+pic->height);
	glEnd ();
}


int M_DrawBigCharacter (int x, int y, int num, int numNext)
{
	byte			*dest;
	byte			*source;
	unsigned short	*pusdest;
	int				drawline;
	int				row, col;
	float			frow, fcol, xsize,ysize;
	int				add;

	if (num == ' ') return 32;

	if (num == '/') num = 26;
	else num -= 65;

	if (num < 0 || num >= 27)  // only a-z and /
		return 0;

	if (numNext == '/') numNext = 26;
	else numNext -= 65;

	row = num/8;
	col = num%8;

	xsize = 0.125;
	ysize = 0.25;
	fcol = col*xsize;
	frow = row*ysize;

	GL_Bind (char_menufonttexture);

	glBegin (GL_QUADS);
	glTexCoord2f (fcol, frow);
	glVertex2f (x, y);
	glTexCoord2f (fcol + xsize, frow);
	glVertex2f (x+20, y);
	glTexCoord2f (fcol + xsize, frow + ysize);
	glVertex2f (x+20, y+20);
	glTexCoord2f (fcol, frow + ysize);
	glVertex2f (x, y+20);
	glEnd ();

	if (numNext < 0 || numNext >= 27) return 0;

	add = 0;
	if (num == (int)'C'-65 && numNext == (int)'P'-65)
		add = 3;

	return BigCharWidth[num][numNext] + add;
}

// jkrige - print version info to the console
void Console_Print (int cx, int cy, char *str)
{
	while (*str)
	{
		Draw_Character (cx, cy, ((unsigned char)(*str))+256);
		str++;
		cx += 8;
	}
}
// jkrige - print version info to the console

/*
================
Draw_ConsoleBackground

================
*/
void Draw_ConsoleBackground (int lines)
{
	// jkrige - print version info to the console
	int ver_x, ver_y;
	char	engineversion[32];
	sprintf(engineversion, "UQE Hexen II %.2f", HEXEN2_VERSION);
	ver_x = (int)vid.width - (strlen(engineversion)*8) /*- 8*/;
	ver_y = (int)vid.height - 8; //- 16;
	// jkrige - print version info to the console

	Draw_Pic (0, lines - (int)vid.height, conback);

	// jkrige - print version info to the console
	Console_Print(ver_x, lines - (int)vid.height + ver_y, engineversion);
	// jkrige - print version info to the console
}


/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h)
{
	glColor3f (1,1,1);
	GL_Bind (*(int *)draw_backtile->data);
	glBegin (GL_QUADS);
	glTexCoord2f (x/64.0, y/64.0);
	glVertex2f (x, y);
	glTexCoord2f ( (x+w)/64.0, y/64.0);
	glVertex2f (x+w, y);
	glTexCoord2f ( (x+w)/64.0, (y+h)/64.0);
	glVertex2f (x+w, y+h);
	glTexCoord2f ( x/64.0, (y+h)/64.0 );
	glVertex2f (x, y+h);
	glEnd ();
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, int c)
{
	glDisable (GL_TEXTURE_2D);
	glColor3f (host_basepal[c*3]/255.0,
		host_basepal[c*3+1]/255.0,
		host_basepal[c*3+2]/255.0);

	glBegin (GL_QUADS);

	glVertex2f (x,y);
	glVertex2f (x+w, y);
	glVertex2f (x+w, y+h);
	glVertex2f (x, y+h);

	glEnd ();
	glColor3f (1,1,1);
	glEnable (GL_TEXTURE_2D);
}
//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{
	int bx,by,ex,ey;
	int c;
	glAlphaFunc(GL_ALWAYS, 0);

	glEnable (GL_BLEND);
	glDisable (GL_TEXTURE_2D);

//	glColor4f (248.0/255.0, 220.0/255.0, 120.0/255.0, 0.2);
	glColor4f (208.0/255.0, 180.0/255.0, 80.0/255.0, 0.2);
	glBegin (GL_QUADS);
	glVertex2f (0,0);
	glVertex2f (vid.width, 0);
	glVertex2f (vid.width, vid.height);
	glVertex2f (0, vid.height);
	glEnd ();

	glColor4f (208.0/255.0, 180.0/255.0, 80.0/255.0, 0.035);
	for(c=0;c<40;c++)
	{
		bx = rand() % vid.width-20;
		by = rand() % vid.height-20;
		ex = bx + (rand() % 40) + 20;
		ey = by + (rand() % 40) + 20;
		if (bx < 0) bx = 0;
		if (by < 0) by = 0;
		if (ex > vid.width) ex = vid.width;
		if (ey > vid.height) ey = vid.height;

		glBegin (GL_QUADS);
		glVertex2f (bx,by);
		glVertex2f (ex, by);
		glVertex2f (ex, ey);
		glVertex2f (bx, ey);
		glEnd ();
	}

	glColor4f (1,1,1,1);
	glEnable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);

	// Pa3PyX: to avoid clipping of smaller fonts/graphics
    glAlphaFunc(GL_GREATER, 0.632);
	//glAlphaFunc(GL_GREATER, 0.666);

	SB_Changed();
}

//=============================================================================

/*
================
Draw_BeginDisc

Draws the little blue disc in the corner of the screen.
Call before beginning any disc IO.
================
*/
void Draw_BeginDisc (void)
{
	static int index = 0;

	if (!draw_disc[index] || loading_stage) return;

	index++;
	if (index >= MAX_DISC) index = 0;

	glDrawBuffer  (GL_FRONT);

	Draw_Pic (vid.width - 28, 0, draw_disc[index]);

	glDrawBuffer  (GL_BACK);
}

/*
================
Draw_EndDisc

Erases the disc icon.
Call after completing any disc IO
================
*/
void Draw_EndDisc (void)
{
}

/*
================
GL_Set2D

Setup as if the screen was 320*200
================
*/
void GL_Set2D (void)
{
	glViewport (glx, gly, glwidth, glheight);

	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
//	glOrtho  (0, 320, 200, 0, -99999, 99999);
	glOrtho  (0, vid.width, vid.height, 0, -99999, 99999);

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
	glDisable (GL_BLEND);
	glEnable (GL_ALPHA_TEST);
//	glDisable (GL_ALPHA_TEST);

	glColor4f (1,1,1,1);
}

//====================================================================

/*
================
GL_FindTexture
================
*/
int GL_FindTexture (char *identifier)
{
	int		i;
	gltexture_t	*glt;

	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
	{
		if (!strcmp (identifier, glt->identifier))
			return gltextures[i].texnum;
	}

	return -1;
}

/*
================
GL_ResampleTexture
================
*/
void GL_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out,  int outwidth, int outheight)
{
	int		i, j;
	unsigned	*inrow, *inrow2;
	unsigned	frac, fracstep;
	unsigned	p1[1024], p2[1024];
	byte		*pix1, *pix2, *pix3, *pix4;

	fracstep = inwidth*0x10000/outwidth;

	frac = fracstep>>2;
	for (i=0 ; i<outwidth ; i++)
	{
		p1[i] = 4*(frac>>16);
		frac += fracstep;
	}
	frac = 3*(fracstep>>2);
	for (i=0 ; i<outwidth ; i++)
	{
		p2[i] = 4*(frac>>16);
		frac += fracstep;
	}

	for (i=0 ; i<outheight ; i++, out += outwidth)
	{
		inrow = in + inwidth*(int)((i+0.25)*inheight/outheight);
		inrow2 = in + inwidth*(int)((i+0.75)*inheight/outheight);

		frac = fracstep >> 1;
		for (j=0 ; j<outwidth ; j++)
		{
			pix1 = (byte *)inrow + p1[j];
			pix2 = (byte *)inrow + p2[j];
			pix3 = (byte *)inrow2 + p1[j];
			pix4 = (byte *)inrow2 + p2[j];
			((byte *)(out+j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0])>>2;
			((byte *)(out+j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1])>>2;
			((byte *)(out+j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2])>>2;
			((byte *)(out+j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3])>>2;
		}
	}
}

/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
void GL_MipMap (byte *in, int width, int height)
{
	int		i, j;
	byte	*out;

	width <<=2;
	height >>= 1;
	out = in;
	for (i=0 ; i<height ; i++, in+=width)
	{
		for (j=0 ; j<width ; j+=8, out+=4, in+=8)
		{
			out[0] = (in[0] + in[4] + in[width+0] + in[width+4])>>2;
			out[1] = (in[1] + in[5] + in[width+1] + in[width+5])>>2;
			out[2] = (in[2] + in[6] + in[width+2] + in[width+6])>>2;
			out[3] = (in[3] + in[7] + in[width+3] + in[width+7])>>2;
		}
	}
}

// Acts the same as glTexImage2D, except that it maps color into the
// current palette and uses paletteized textures.
static void fxPalTexImage2D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	static unsigned char dest_image[256*256];
	long i;
	long mip_width, mip_height;

	mip_width = width;
	mip_height = height;

	if( internalformat != 3 )
		Sys_Error( "fxPalTexImage2D: internalformat != 3" );
	for( i = 0; i < mip_width * mip_height; i++ )
	{
		int r, g, b, index;
		r = ( ( unsigned char * )pixels )[i * 4];
		g = ( ( unsigned char * )pixels )[i * 4+1];
		b = ( ( unsigned char * )pixels )[i * 4+2];
		r >>= 8 - INVERSE_PAL_R_BITS;
		g >>= 8 - INVERSE_PAL_G_BITS;
		b >>= 8 - INVERSE_PAL_B_BITS;
		index = ( r << ( INVERSE_PAL_G_BITS + INVERSE_PAL_B_BITS ) ) |
			( g << INVERSE_PAL_B_BITS ) |
			b;
		dest_image[i] = inverse_pal[index];
//		dest_image[i] = ( ( unsigned char * )pixels )[i*4];
	}
//	glTexImage2D( target, level, 1, width, height, border, GL_LUMINANCE, GL_UNSIGNED_BYTE, dest_image );
	glTexImage2D( target, level, 1, width, height, border, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, dest_image );
/*	if( fxMarkPalTextureExtension )
		fxMarkPalTextureExtension();*/
}


/*
===============
GL_Upload32
===============
*/
void GL_Upload32 (unsigned *data, int width, int height, qboolean mipmap, qboolean alpha)
{
	int			samples;

	// jkrige - doubled textures
	//static	unsigned	scaled[1024*512];	// [512*256];
	static	unsigned	scaled[2048*2048];
	// jkrige - doubled textures

	int			scaled_width, scaled_height;

	// jkrige - anisotropic filtering
	int			i = 3;
	int AnisotropyModes = 0;
	// jkrige - anisotropic filtering


	// jkrige - non power of two
	//for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1);
	//if (gl_round_down.value && scaled_width > width)
	//	scaled_width >>= 1;

	//for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1);
	//if (gl_round_down.value && scaled_height > height)
	//	scaled_height >>= 1;
	if(npow2_ext == true && gl_texture_non_power_of_two.value == 1.0f /*&& !mipmap*/)
	{
		scaled_width = width;
		scaled_height = height;
	}
	else
	{
		for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1);
		for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1);
	}
	// jkrige - non power of two


	if ((scaled_width >> (int)gl_picmip.value) && (scaled_height >> (int)gl_picmip.value))
	{
		scaled_width >>= (int)gl_picmip.value;
		scaled_height >>= (int)gl_picmip.value;
	}

	if (scaled_width > gl_max_size.value)
		scaled_width = gl_max_size.value;

	if (scaled_height > gl_max_size.value)
		scaled_height = gl_max_size.value;

	if (scaled_width * scaled_height > sizeof(scaled)/4)
		Sys_Error ("GL_LoadTexture: too big");

	// 3dfx has some aspect ratio constraints. . . can't go beyond 8 to 1 or below 1 to 8.
	if( is_3dfx )
	{
		if( scaled_width * 8 < scaled_height )
		{
			scaled_width = scaled_height >> 3;
		}
		else if( scaled_height * 8 < scaled_width )
		{
			scaled_height = scaled_width >> 3;
		}
	}

	samples = alpha ? gl_alpha_format : gl_solid_format;

#if 0
	if (mipmap)
		gluBuild2DMipmaps (GL_TEXTURE_2D, samples, width, height, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	else if (scaled_width == width && scaled_height == height)
		glTexImage2D (GL_TEXTURE_2D, 0, samples, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	else
	{
		gluScaleImage (GL_RGBA, width, height, GL_UNSIGNED_BYTE, trans,
			scaled_width, scaled_height, GL_UNSIGNED_BYTE, scaled);
		glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	}
#else

	if (scaled_width == width && scaled_height == height)
	{
		if (!mipmap)
		{
			// jkrige - alpha channel fix
			glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			//if(samples == 3)
			//	glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			//if(samples == 4)
			//	glTexImage2D (GL_TEXTURE_2D, 0, samples/*GL_RGBA8*/, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			// jkrige - alpha channel fix
			goto done;
		}
		memcpy (scaled, data, width*height*4);
	}
	else
		GL_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);

	texels += scaled_width * scaled_height;

	// If you are on a 3Dfx card and your texture has no alpha, then download it
	// as a palettized texture to save memory.
	if( fxSetPaletteExtension && ( samples == 3 ) )
	{
		fxPalTexImage2D( GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled );
	}
	else
	{
		// jkrige - alpha channel fix
		glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
		//if(samples == 3)
		//	glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
		//if(samples == 4)
		//	glTexImage2D (GL_TEXTURE_2D, 0, samples/*GL_RGBA8*/, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
		// jkrige - alpha channel fix
	}

	if (mipmap)
	{
		int		miplevel;

		miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1)
		{
			GL_MipMap ((byte *)scaled, scaled_width, scaled_height);


			// jkrige - non power of two
			scaled_width >>= 1;
			scaled_height >>= 1;
			/*if(npow2_ext == true && gl_texture_non_power_of_two.value == 1.0f && !mipmap)
			{
				scaled_width = (int)floor (width / pow(2, miplevel+1));
				scaled_height = (int)floor (height / pow (2, miplevel+1));
			}
			else
			{
				scaled_width >>= 1;
				scaled_height >>= 1;
			}*/
			// jkrige - non power of two



			if (scaled_width < 1)
				scaled_width = 1;
			if (scaled_height < 1)
				scaled_height = 1;
			miplevel++;
			texels += scaled_width * scaled_height;

			// If you are on a 3Dfx card and your texture has no alpha, then download it
			// as a palettized texture to save memory.
			if( fxSetPaletteExtension && ( samples == 3) )
			{
				fxPalTexImage2D (GL_TEXTURE_2D, miplevel, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
			}
			else
			{
				// jkrige - alpha channel fix
				glTexImage2D (GL_TEXTURE_2D, miplevel, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
				//if(samples == 3)
				//	glTexImage2D (GL_TEXTURE_2D, miplevel, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
				//if(samples == 4)
				//	glTexImage2D (GL_TEXTURE_2D, miplevel, samples/*GL_RGBA8*/, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
				// jkrige - alpha channel fix
			}
		}
	}
done: ;
#endif


	  if (mipmap)
	  {
		  // jkrige - texture modes
		  if(floor(gl_texturemode.value) < 0.0f | gl_texturemode.value != floor(gl_texturemode.value))
		  {
			  Con_Printf ("unknown texture mode\n");
			  Cvar_Set ("gl_texturemode", "2");
		  }

		  if(gl_texturemode.value == 0.0f)
			  i = 0;

		  if(gl_texturemode.value == 1.0f)
			  i = 3;

		  if(gl_texturemode.value >= 2.0f)
			  i = 5;

		  if(anisotropic_ext == true)
			  AnisotropyModes = (int)floor((log(maximumAnisotrophy)/log(2.0f)) + 0.5f);
		  else
			  AnisotropyModes = 0;

		  if(anisotropic_ext == false)
		  {
			  if (gl_texturemode.value > 2.0f)
			  {
				  Con_Printf ("unknown texture mode\n");
				  Cvar_Set ("gl_texturemode", "2");
				  
				  return;
			  }
		  }
		  
		  if(anisotropic_ext == true)
		  {
			  if (gl_texturemode.value > (2.0f + (float)AnisotropyModes))
			  {
				  Con_Printf ("unknown texture mode\n");
				  Cvar_SetValue ("gl_texturemode", (2.0f + (float)AnisotropyModes));
				  
				  return;
			  }
		  }

		  gl_filter_min = modes[i].minimize;
		  gl_filter_max = modes[i].maximize;

		  if(gl_texturemode.value <= 2.0f)
		  {
			  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
			  if(anisotropic_ext == true)
				  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
		  }
		  
		  // jkrige - anisotropic filtering
		  if(gl_texturemode.value > 1.0f && anisotropic_ext == true)
			  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, pow(2.0f, gl_texturemode.value - 2.0f));
		  // jkrige - anisotropic filtering

		  // jkrige - texture modes


		  /*
		  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

		  // jkrige - anisotropic filtering
		  for (i = 0; i < 6; i++)
		  {
			  if (gl_filter_min == modes[i].minimize)
				  break;
		  }
		  
		  if(i == 5 && anisotropic_ext == true)
			  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maximumAnisotrophy);
		  // jkrige - anisotropic filtering
		  */
	  }
	  else
	  {
		  // jkrige - scale2d
		  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		  // jkrige - scale2d

		  //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR /*gl_filter_max*/); // jkrige - change bilinear texture filtering to trilinear texture filtering
		  //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR /*gl_filter_max*/); // jkrige - change bilinear texture filtering to trilinear texture filtering
	  }
}

// jkrige - doubled textures
//static	unsigned	trans[640 * 480]; 	// FIXME, temporary
static	unsigned	trans[2048 * 2048];
// jkrige - doubled textures

/*
===============
GL_Upload8
===============
*/
/*
* mode:
* 0 - standard
* 1 - color 0 transparent, odd - translucent, even - full value
* 2 - color 0 transparent
* 3 - special (particle translucency table)
*/
void GL_Upload8 (byte *data, int width, int height, qboolean upload32, qboolean mipmap, qboolean alpha, int mode)
{
	int			i, s;
	qboolean	noalpha;
	int			p;


	// jkrige - doubled textures
	byte *dtdata;
	int x, y;

	if (mipmap)
	{
		width *= 2;
		height *= 2;

		s = width * height;
		dtdata = malloc(s);

		for (y = 0; y < height / 2; y++)
		{
			for (x = 0; x < width / 2; x++)
			{
				dtdata[(y * (width * 2)) + (x * 2) + 0] = data[(y * (width / 2)) + x];
				dtdata[(y * (width * 2)) + (x * 2) + 1] = data[(y * (width / 2)) + x];

				dtdata[(y * (width * 2)) + (x * 2) + 0 + width] = data[(y * (width / 2)) + x];
				dtdata[(y * (width * 2)) + (x * 2) + 1 + width] = data[(y * (width / 2)) + x];
			}
		}
	}
	else
	{
		s = width * height;
		dtdata = data;
	}
	//s = width * height;
	// jkrige - doubled textures


	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if ((alpha || mode != 0) && mode != 10 )
	{
		noalpha = true;
		for (i=0 ; i<s ; i++)
		{
			p = dtdata[i];
			if (p == 255)
				noalpha = false;
			trans[i] = d_8to24table[p];
		}

		for (i=0 ; i<s ; i++)
		{
			int n;
			int r = 0, g = 0, b = 0;

			p = dtdata[i];
			if (p == 255)
			{
				unsigned long neighbors[9];
				int num_neighbors_valid = 0;
				int neighbor_u, neighbor_v;

				int u, v;
				u = s % width;
				v = s / width;

				for( neighbor_u = u - 1; neighbor_u <= u + 1; neighbor_u++ )
				{
					for( neighbor_v = v - 1; neighbor_v <= v + 1; neighbor_v++ )
					{
						if( neighbor_u == neighbor_v )
							continue;
						// Make sure  that we are accessing a texel in the image, not out of range.
						if( neighbor_u < 0 || neighbor_u > width || neighbor_v < 0 || neighbor_v > height )
							continue;
						if( dtdata[neighbor_u + neighbor_v * width] == 255 )
							continue;
						neighbors[num_neighbors_valid++] = trans[neighbor_u + neighbor_v * width];
					}
				}

				if( num_neighbors_valid == 0 )
					continue;

				for( n = 0; n < num_neighbors_valid; n++ )
				{
					r += neighbors[n] & 0xff;
					g += ( neighbors[n] & 0xff00 ) >> 8;
					b += ( neighbors[n] & 0xff0000 ) >> 16;
				}

				r /= num_neighbors_valid;
				g /= num_neighbors_valid;
				b /= num_neighbors_valid;

				if( r > 255 )
					r = 255;
				if( g > 255 )
					g = 255;
				if( b > 255 )
					b = 255;

				trans[i] = ( b << 16  ) | ( g << 8 ) | r;
//				trans[i] = 0;
			}
		}

		if (alpha && noalpha)
			alpha = false;

		switch( mode )
		{
		case 1:
			alpha = true;
			for (i=0 ; i<s ; i++)
			{
				p = dtdata[i];
				if (p == 0)
					trans[i] &= 0x00ffffff;
				else if( p & 1 )
				{
					trans[i] &= 0x00ffffff;
					trans[i] |= ( ( int )( 255 * r_wateralpha.value ) ) << 24;
				}
				else
				{
					trans[i] |= 0xff000000;
				}
			}
			break;
		case 2:
			alpha = true;
			for (i=0 ; i<s ; i++)
			{
				p = dtdata[i];
				if (p == 0)
					trans[i] &= 0x00ffffff;
			}
			break;
		case 3:
			alpha = true;
			for (i=0 ; i<s ; i++)
			{
				p = dtdata[i];
				trans[i] = d_8to24table[ColorIndex[p>>4]] & 0x00ffffff;
				trans[i] |= ( int )ColorPercent[p&15] << 24;
				//trans[i] = 0x7fff0000;
			}
			break;
		}
	}
	else
	{
		if (s&3)
			Sys_Error ("GL_Upload8: s&3");
		for (i=0 ; i<s ; i+=4)
		{
			trans[i] = d_8to24table[dtdata[i]];
			trans[i+1] = d_8to24table[dtdata[i+1]];
			trans[i+2] = d_8to24table[dtdata[i+2]];
			trans[i+3] = d_8to24table[dtdata[i+3]];
		}
	}

	// jkrige - doubled textures
	if (mipmap && dtdata)
		free(dtdata);
	// jkrige - doubled textures


	//if(image_bits != 32)
	if(upload32)
		GL_Upload32 (trans, width, height, mipmap, alpha);
	//else
	//	GL_Upload32 (trans, width, height, mipmap, false);
}

/*
================
GL_LoadTexture
================
*/
// jkrige - memleak & texture mismatch
/*int GL_LoadTexture (char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha, int mode)
{
	qboolean	noalpha;
	int			i, p, s;
	gltexture_t	*glt;
	char search[64];

	if (!vid_initialized)
		return -1;

	sprintf (search, "%s%d%d",identifier,width,height);

	// see if the texture is allready present
	if (identifier[0])
	{
		for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
		{
			if (!strcmp (search, glt->identifier))
			{
				if (width != glt->width || height != glt->height)
					Sys_Error ("GL_LoadTexture: cache mismatch");
				return gltextures[i].texnum;
			}
		}
	}
	else
	{
		glt = &gltextures[numgltextures];
	}
	numgltextures++;

	strcpy(glt->identifier, search);
	glt->texnum = texture_extension_number;
	glt->width = width;
	glt->height = height;
	glt->mipmap = mipmap;

	GL_Bind(texture_extension_number );

	GL_Upload8 (data, width, height, mipmap, alpha, mode);

	texture_extension_number++;

	return texture_extension_number-1;
}*/


int lhcsumtable[256];
int GL_LoadTexture (char *identifier, char *textype, int width, int height, byte *data, qboolean mipmap, qboolean alpha, int mode, int bytesperpixel)
{
	//qboolean	noalpha;
	int			i, /*p,*/ s, lhcsum;
	gltexture_t	*glt;


	// occurances. well this isn't exactly a checksum, it's better than that but
	// not following any standards.
	lhcsum = 0;
	s = width*height*bytesperpixel;
	//s = width*height;
	for (i = 0;i < 256;i++) lhcsumtable[i] = i + 1;
	for (i = 0;i < s;i++) lhcsum += (lhcsumtable[data[i] & 255]++);

	// see if the texture is allready present
	if (identifier[0])
	{
		for (i=0, glt=gltextures ; i < numgltextures ; i++, glt++)
		{
			if (!strcmp (identifier, glt->identifier))
			{
				if (lhcsum != glt->lhcsum || width != glt->width || height != glt->height)
				{
					Con_DPrintf("GL_LoadTexture: cache mismatch\n");
					goto GL_LoadTexture_setup;
				}
				return glt->texnum;
			}
		}
	}
	// whoever at id or threewave must've been half asleep...
	glt = &gltextures[numgltextures];
	numgltextures++;

	strcpy (glt->identifier, identifier);
	glt->texnum = texture_extension_number;
	texture_extension_number++;

	GL_LoadTexture_setup:
	glt->lhcsum = lhcsum;
	glt->width = width;
	glt->height = height;
	glt->mipmap = mipmap;

	// jkrige - luma textures (reset)
	glt->tex_luma = false;
	// jkrige - luma textures (reset)

	if (!isDedicated)
	{
		GL_Bind(glt->texnum);

		//if (strcmp (textype, "bloom"))
		{
			if (bytesperpixel == 1)
			{
				GL_Upload8 (data, width, height, true, mipmap, alpha, mode);
			}
			else if (bytesperpixel == 3 | bytesperpixel == 4)
			{
				if(image_alpha == 3)
					GL_Upload32 ((void *)data, width, height, mipmap, false);
				else
					GL_Upload32 ((void *)data, width, height, mipmap, true);
			}
			else
				Sys_Error("GL_LoadTexture: unknown bytes per pixel\n");
		}

		// jkrige - reset external image
		image_width = 0;
		image_height = 0;
		image_bits = 8;
		image_alpha = 3;
		// jkrige - reset external image


		// jkrige - luma textures
		if (!strcmp (textype, "texture"))
		{
			char *ch_dot = strrchr(identifier, '.');

			if (ch_dot != NULL)
			{
				char *ident;
				char ch_name[MAX_PATH];
				byte *lumadata;

				i = 0;
				ident = identifier;

				while (*identifier && *identifier != '.')
					ch_name[i++] = *identifier++;
				ch_name[i++] = 0;

				strcat(ch_name, "_luma");
				strcat(ch_name, ch_dot);
				identifier = ident;

				if ((lumadata = LoadImagePixels (ch_name, false)))
				{
					GL_Bind(JK_LUMA_TEX + glt->texnum);

					GL_Upload32((unsigned int *)lumadata, width, height, mipmap, true);
					free(lumadata);

					glt->tex_luma = true;
				}
			}
		}
		// jkrige - luma textures


		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}

	return glt->texnum;
}
// jkrige - memleak & texture mismatch

/*
===============
GL_Upload8
===============
*/
/*
void GL_UploadTrans8 (byte *data, int width, int height,  qboolean mipmap, byte Alpha)
{
	int			i, s;
	int			p;
	unsigned NewAlpha;

	NewAlpha = ((unsigned)Alpha)<<24;

	s = width*height;
	for (i=0 ; i<s ; i++)
	{
		p = data[i];
		trans[i] = d_8to24table[p];
		if (p != 255)
		{
			trans[i] &= 0x00ffffff;
			trans[i] |= NewAlpha;
		}
	}

	GL_Upload32 (trans, width, height, mipmap, true);
}
*/

/*
================
GL_LoadTransTexture
================
*/
/*int GL_LoadTransTexture (char *identifier, int width, int height, byte *data, qboolean mipmap, byte Alpha)
{
	qboolean	noalpha;
	int			i, p, s;
	gltexture_t	*glt;

	// see if the texture is allready present
	if (identifier[0])
	{
		for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
		{
			if (!strcmp (identifier, glt->identifier))
			{
				if (width != glt->width || height != glt->height)
					Sys_Error ("GL_LoadTexture: cache mismatch");
				return gltextures[i].texnum;
			}
		}
	}
	else
		glt = &gltextures[numgltextures];
	numgltextures++;

	strcpy (glt->identifier, identifier);
	glt->texnum = texture_extension_number;
	glt->width = width;
	glt->height = height;
	glt->mipmap = mipmap;

	GL_Bind(texture_extension_number );

	GL_UploadTrans8 (data, width, height, mipmap, Alpha);

	texture_extension_number++;

	return texture_extension_number-1;
}
*/

/*
================
GL_LoadPicTexture
================
*/
int GL_LoadPicTexture (qpic_t *pic)
{
	int i;

	i = GL_LoadTexture ("", "", pic->width, pic->height, pic->data, false, true, 0, 1);

	// jkrige - scale2d
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	// jkrige - scale2d

	return i;
}

// jkrige - external texture loading
int		image_width;
int		image_height;
int		image_bits;
int		image_alpha;

byte Convert24to8(byte *palette, byte Red, byte Green, byte Blue)
{
	int i;

	for(i=0; i<768; i+=3)
	{
		if(palette[i] == Red && palette[i+1] == Green && palette[i+2] == Blue)
			return 255-(i/3);
	}

	return 0;
}

// jkrige - jpg
#ifdef JPG_SUPPORT
/*
========
LoadJPG
========
*/
byte *LoadJPG ( const char *filename)
{
  /* This struct contains the JPEG decompression parameters and pointers to
   * working space (which is allocated as needed by the JPEG library).
   */
  struct jpeg_decompress_struct cinfo;
  /* We use our private extension JPEG error handler.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
  /* This struct represents a JPEG error handler.  It is declared separately
   * because applications often want to supply a specialized error handler
   * (see the second half of this file for an example).  But here we just
   * take the easy way out and use the standard error handler, which will
   * print a message on stderr and call exit() if compression fails.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
	struct jpeg_error_mgr jerr;
  /* More stuff */
	JSAMPARRAY buffer;		/* Output row buffer */
	int row_stride;		/* physical row width in output buffer */
	unsigned char *out;
	byte	*fbuffer;
	byte  *bbuf;

  /* In this example we want to open the input file before doing anything else,
   * so that the setjmp() error recovery below can assume the file is open.
   * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
   * requires it in order to read binary files.
   */


	// jkrige - pk3 file support
	int len;
	FILE	*f;
	byte	*pic;

	//
	// load the file
	//
	len = COM_FOpenFile (( char * )filename, &f, false);
	if(len < 1)
		return NULL;

	fbuffer = COM_FReadFile(f, len);
	if (!fbuffer)
		return NULL;
	
	pic = NULL;
	// jkrige - pk3 file support


  /* Step 1: allocate and initialize JPEG decompression object */

  /* We have to set up the error handler first, in case the initialization
   * step fails.  (Unlikely, but it could happen if you are out of memory.)
   * This routine fills in the contents of struct jerr, and returns jerr's
   * address which we place into the link field in cinfo.
   */
	cinfo.err = jpeg_std_error(&jerr);

  /* Now we can initialize the JPEG decompression object. */
	jpeg_create_decompress(&cinfo);

  /* Step 2: specify data source (eg, a file) */

	jpeg_stdio_src(&cinfo, fbuffer);

  /* Step 3: read file parameters with jpeg_read_header() */

	(void) jpeg_read_header(&cinfo, TRUE);
  /* We can ignore the return value from jpeg_read_header since
   *   (a) suspension is not possible with the stdio data source, and
   *   (b) we passed TRUE to reject a tables-only JPEG file as an error.
   * See libjpeg.doc for more info.
   */

  /* Step 4: set parameters for decompression */

  /* In this example, we don't need to change any of the defaults set by
   * jpeg_read_header(), so we do nothing here.
   */

  /* Step 5: Start decompressor */

	(void) jpeg_start_decompress(&cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* We may need to do some setup of our own at this point before reading
   * the data.  After jpeg_start_decompress() we have the correct scaled
   * output image dimensions available, as well as the output colormap
   * if we asked for color quantization.
   * In this example, we need to make an output work buffer of the right size.
   */ 
  /* JSAMPLEs per row in output buffer */
	row_stride = cinfo.output_width * cinfo.output_components;

	out = malloc(cinfo.output_width*cinfo.output_height*cinfo.output_components);

	// jkrige
	// *pic = out;
	// *width = cinfo.output_width;
	// *height = cinfo.output_height;
	pic = out;
	image_width = cinfo.image_width;
	image_height = cinfo.image_height;
	// jkrige

  /* Step 6: while (scan lines remain to be read) */
  /*           jpeg_read_scanlines(...); */

  /* Here we use the library's state variable cinfo.output_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   */
	while (cinfo.output_scanline < cinfo.output_height)
	{
    /* jpeg_read_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could ask for
     * more than one scanline at a time if that's more convenient.
     */
		bbuf = ((out+(row_stride*cinfo.output_scanline)));
		buffer = &bbuf;
		(void) jpeg_read_scanlines(&cinfo, buffer, 1);
	}

  // clear all the alphas to 255
	{
		int	i, j;
		byte	*buf;

		buf = pic;

		j = cinfo.output_width * cinfo.output_height * 4;
		for ( i = 3 ; i < j ; i+=4 )
		{
			buf[i] = 255;
		}
  }

  /* Step 7: Finish decompression */

	(void) jpeg_finish_decompress(&cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* Step 8: Release JPEG decompression object */

  /* This is an important step since it will release a good deal of memory. */
	jpeg_destroy_decompress(&cinfo);

  /* After finish_decompress, we can close the input file.
   * Here we postpone it until after no more JPEG errors are possible,
   * so as to simplify the setjmp error logic above.  (Actually, I don't
   * think that jpeg_destroy can do an error exit, but why assume anything...)
   */
	
	image_bits = 32;
	image_alpha = 3;

	free (fbuffer);

  /* At this point you may want to check to see whether any corrupt-data
   * warnings occurred (test whether jerr.pub.num_warnings is nonzero).
   */
	

  /* And we're done! */
	return pic;
}
#endif
// jkrige - jpg

/*
=============
TARGA LOADING
=============
*/

typedef struct _TargaHeader 
{
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;


TargaHeader		targa_header;

int fgetLittleShort (FILE *f)
{
	byte	b1, b2;

	b1 = fgetc(f);
	b2 = fgetc(f);

	return (short)(b1 + b2*256);
}

int fgetLittleLong (FILE *f)
{
	byte	b1, b2, b3, b4;

	b1 = fgetc(f);
	b2 = fgetc(f);
	b3 = fgetc(f);
	b4 = fgetc(f);

	return b1 + (b2<<8) + (b3<<16) + (b4<<24);
}


/*
========
LoadTGA
========
*/
byte *LoadTGA ( const char *name)
{
	int		columns, rows, numPixels;
	byte	*pixbuf;
	int		row, column;
	byte	*buf_p;
	byte	*buffer;
	TargaHeader	targa_header;
	byte		*targa_rgba;

	// jkrige - bitsperpixel check
	int i;
	// jkrige - bitsperpixel check


	// jkrige - pk3 file support
	int len;
	FILE	*f;
	byte	*pic;
	// jkrige - pk3 file support


	// *pic = NULL; // jkrige
	pic = NULL;
	buffer = NULL;

	//
	// load the file
	//
	len = COM_FOpenFile (( char * )name, &f, false);
	if(len < 1)
		return NULL;

	buffer = COM_FReadFile(f, len);
	

	if (!buffer) {
		return NULL;
	}

	buf_p = buffer;

	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;
	
	targa_header.colormap_index = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.colormap_length = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.colormap_size = *buf_p++;
	targa_header.x_origin = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.y_origin = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.width = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.height = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.pixel_size = *buf_p++;
	targa_header.attributes = *buf_p++;

	if (targa_header.image_type!=2 && targa_header.image_type!=10 && targa_header.image_type != 3 ) 
	{
		Sys_Error ("LoadTGA: Only type 2 (RGB), 3 (gray), and 10 (RGB) TGA images supported\n");
	}

	if ( targa_header.colormap_type != 0 )
	{
		Sys_Error ("LoadTGA: colormaps not supported\n" );
	}

	if ( ( targa_header.pixel_size != 32 && targa_header.pixel_size != 24 ) && targa_header.image_type != 3 )
	{
		Sys_Error ("LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n");
	}

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	// jkrige
	/*if (width)
		*width = columns;
	if (height)
		*height = rows;*/
	image_width = columns;
	image_height = rows;
	// jkrige

	targa_rgba = malloc (numPixels*4);
	// *pic = targa_rgba; // jkrige
	pic = targa_rgba;

	if (targa_header.id_length != 0)
		buf_p += targa_header.id_length;  // skip TARGA image comment
	
	if ( targa_header.image_type == 2 || targa_header.image_type == 3 )
	{ 
		// Uncompressed RGB or gray scale image
		for(row=rows-1; row>=0; row--) 
		{
			pixbuf = targa_rgba + row*columns*4;
			for(column=0; column<columns; column++) 
			{
				unsigned char red,green,blue,alphabyte;
				switch (targa_header.pixel_size) 
				{
					
				case 8:
					blue = *buf_p++;
					green = blue;
					red = blue;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 255;
					break;

				case 24:
					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 255;
					break;
				case 32:
					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					alphabyte = *buf_p++;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = alphabyte;
					break;
				default:
					Sys_Error("LoadTGA: illegal pixel_size '%d' in file '%s'\n", targa_header.pixel_size, name );
					break;
				}
			}
		}
	}
	else if (targa_header.image_type==10)
	{   // Runlength encoded RGB images
		unsigned char red,green,blue,alphabyte,packetHeader,packetSize,j;

		red = 0;
		green = 0;
		blue = 0;
		alphabyte = 0xff;

		for(row=rows-1; row>=0; row--)
		{
			pixbuf = targa_rgba + row*columns*4;
			for(column=0; column<columns; )
			{
				packetHeader= *buf_p++;
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80)
				{        // run-length packet
					switch (targa_header.pixel_size)
					{
						case 24:
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								alphabyte = 255;
								break;
						case 32:
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								alphabyte = *buf_p++;
								break;
						default:
							Sys_Error("LoadTGA: illegal pixel_size '%d' in file '%s'\n", targa_header.pixel_size, name );
							break;
					}
	
					for(j=0;j<packetSize;j++)
					{
						*pixbuf++=red;
						*pixbuf++=green;
						*pixbuf++=blue;
						*pixbuf++=alphabyte;
						column++;
						if (column==columns)
						{ // run spans across rows
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row*columns*4;
						}
					}
				}
				else
				{                            // non run-length packet
					for(j=0;j<packetSize;j++)
					{
						switch (targa_header.pixel_size)
						{
							case 24:
									blue = *buf_p++;
									green = *buf_p++;
									red = *buf_p++;
									*pixbuf++ = red;
									*pixbuf++ = green;
									*pixbuf++ = blue;
									*pixbuf++ = 255;
									break;
							case 32:
									blue = *buf_p++;
									green = *buf_p++;
									red = *buf_p++;
									alphabyte = *buf_p++;
									*pixbuf++ = red;
									*pixbuf++ = green;
									*pixbuf++ = blue;
									*pixbuf++ = alphabyte;
									break;
							default:
								Sys_Error ("LoadTGA: illegal pixel_size '%d' in file '%s'\n", targa_header.pixel_size, name );
								break;
						}
						column++;
						if (column==columns)
						{ // pixel packet run spans across rows
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row*columns*4;
						}						
					}
				}
			}
			breakOut:;
		}
	}

//#if 0 
  // TTimo: this is the chunk of code to ensure a behavior that meets TGA specs 
  // bk0101024 - fix from Leonardo
  // bit 5 set => top-down
  if (targa_header.attributes & 0x20)
  {
    unsigned char *flip = (unsigned char*)malloc (columns*4);
    unsigned char *src, *dst;

    for (row = 0; row < rows/2; row++)
	{
      src = targa_rgba + row * 4 * columns;
      dst = targa_rgba + (rows - row - 1) * 4 * columns;

      memcpy (flip, src, columns*4);
      memcpy (src, dst, columns*4);
      memcpy (dst, flip, columns*4);
    }
    free (flip);
  }
//#endif
  // instead we just print a warning
  //if (targa_header.attributes & 0x20)
  //{
  //  Con_Printf("WARNING: '%s' TGA file header declares top-down image, ignoring\n", name);
  //}
  

  // jkrige - bitsperpixel check
  image_bits = 32;
  image_alpha = gl_solid_format;
  for (i = 0;i < image_width*image_height;i++)
  {
	  if (targa_rgba[i*4+3] < 255)
	  {
		  image_alpha = gl_alpha_format;
		  break;
	  }
  }
  // jkrige - bitsperpixel check
  
  
  free (buffer);

  return pic;
}

byte* LoadImagePixels (char* filename, qboolean complain)
{
	char	basename[128], name[128];
	byte	*data;

	COM_StripExtension(filename, basename); // strip the extension to allow more filetypes

	sprintf (name, "%s.tga", basename);
	data = LoadTGA(name);
	if(data)
		return data;

	// jkrige - jpg
#ifdef JPG_SUPPORT
	sprintf (name, "%s.jpg", basename);
	data = LoadJPG(name);
	if(data)
		return data;
#endif
	// jkrige - jpg

	if (complain)
		Con_Printf ("Couldn't load %s\n", name);

	return NULL;
}

int LoadTextureImage (char* filename, char *textype, int matchwidth, int matchheight, qboolean complain, qboolean mipmap)
{
	int texnum;
	int	i;
	qboolean	alpha;
	byte *data;

	if (!(data = LoadImagePixels (filename, complain)))
		return 0;

	if(image_alpha == 4)
		alpha = true;
	else
		alpha = false;

	if(image_bits != 32)
		texnum = GL_LoadTexture (filename, textype, image_width, image_height, data, mipmap, true, 0, 1);
	else
		texnum = GL_LoadTexture (filename, textype, image_width, image_height, data, mipmap, alpha, 4, image_alpha);

	if (data)
		free(data);

	return texnum;
}
// jkrige - external texture loading

/*
 * $Log: /H2 Mission Pack/gl_draw.c $
 * 
 * 2     2/26/98 3:09p Jmonroe
 * fixed gl for numclasses
 * 
 * 34    9/30/97 6:12p Rlove
 * Updates
 * 
 * 33    9/30/97 4:22p Rjohnson
 * PowerVRUpdates
 * 
 * 32    9/25/97 2:10p Rjohnson
 * Smaller status bar
 * 
 * 31    9/23/97 9:47p Rjohnson
 * Fix for dedicated gl server and color maps for sheeps
 * 
 * 30    9/09/97 10:49a Rjohnson
 * Updates
 * 
 * 29    9/03/97 9:10a Rjohnson
 * Update
 * 
 * 28    9/02/97 12:25a Rjohnson
 * Font Update
 * 
 * 27    8/31/97 9:27p Rjohnson
 * GL Updates
 *
 * 24	 8/20/97 2:05p Rjohnson
 * fix for internationalization
 *
 * 23	 8/20/97 11:40a Rjohnson
 * Character Fixes
 *
 * 22	 8/19/97 10:35p Rjohnson
 * Fix for loading plaque
 *
 * 21	 8/18/97 12:03a Rjohnson
 * Added loading progress
 *
 * 20	 8/15/97 11:27a Rlove
 * Changed MAX_CACHED_PICS to 256
 *
 * 19	 6/17/97 10:03a Rjohnson
 * GL Updates
 *
 * 18	 6/16/97 4:25p Rjohnson
 * Fixed a few minor things
 *
 * 17	 6/16/97 5:28a Rjohnson
 * Minor fixes
 *
 * 16	 6/15/97 7:52p Rjohnson
 * Added new paused and loading graphics
 *
 * 15	 6/10/97 9:09a Rjohnson
 * GL Updates
 *
 * 14	 6/06/97 5:17p Rjohnson
 * New console characters
 *
 * 13	 6/02/97 3:42p Gmctaggart
 * GL Catchup
 *
 * 12	 4/30/97 11:20p Bgokey
 *
 * 11	 4/18/97 11:24a Rjohnson
 * Changed the background of the menus when in the game
 *
 * 10	 4/17/97 3:42p Rjohnson
 * Modifications for the gl version for menus
 *
 * 9	 4/17/97 12:14p Rjohnson
 * Modified the cropped drawing routine
 *
 * 8	 3/22/97 5:19p Rjohnson
 * No longer has static large arrays for texture loading
 *
 * 7	 3/22/97 3:22p Rjohnson
 * Moved the glpic structure to the glquake.h header file
 *
 * 6	 3/13/97 10:53p Rjohnson
 * Support for small font and uploading a texture with a specific alpha
 * value
 *
 * 5	 3/13/97 12:24p Rjohnson
 * Implemented the draw "cropped" commands in the gl version
 *
 * 4	 3/07/97 5:54p Rjohnson
 * Made it so that gl_round_down defaults to 0
 *
 * 3	 3/07/97 1:06p Rjohnson
 * Id Updates
 *
 * 2	 2/20/97 12:13p Rjohnson
 * Code fixes for id update
 */
