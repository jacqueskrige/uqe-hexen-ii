// view.h

// jkrige - gamma
#ifdef GLQUAKE
extern	cvar_t		gl_gamma;
extern	cvar_t		gl_overbrightbits;
#else
extern	cvar_t		v_gamma;
#endif
// jkrige - gamma

extern	byte		gammatable[256];	// palette is sent through this
extern	byte		ramps[3][256];
extern float v_blend[4];

extern cvar_t lcd_x;


void V_Init (void);
void V_RenderView (void);
float V_CalcRoll (vec3_t angles, vec3_t velocity);
void V_UpdatePalette (void);

