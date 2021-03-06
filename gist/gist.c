/*
 * $Id: gist.c,v 1.1 2005-09-18 22:04:31 dhmunro Exp $
 * Implement non-device specific portion of GIST C interface
 */
/* Copyright (c) 2005, The Regents of the University of California.
 * All rights reserved.
 * This file is part of yorick (http://yorick.sourceforge.net).
 * Read the accompanying LICENSE file for details.
 */

#include "gist2.h"
#include "gist/private.h"
#include "gist/engine.h"
#include "gist/clip.h"
#include "play2.h"
#include "play/std.h"

/* Math functions (for determining lengths) */
extern double fabs(double);
extern double sqrt(double);
/* half 1.4142 13562 37309 50488, precise value is not critical here */
#define SQRT_HALF 0.7071067811865

extern char *strcpy(char *, const char *);

void (*gd_free)(void *)= 0;

void (*gp_stdout)(char *msg) = 0;
void (*gp_on_keyline)(char *msg) = 0;

/* Font for character occasional markers on curves */
#define MARKER_FONT GP_FONT_HELVETICA

/* ------------------------------------------------------------------------ */

ga_attributes_t ga_attributes= {
  { PL_FG, GP_LINE_SOLID, 1.0 },                     /* line attributes */
  { PL_FG, GP_MARKER_ASTERISK, 1.0 },                /* marker attributes */
  { PL_FG, GP_FILL_SOLID },                          /* fill attributes */
  { PL_FG, 0, 0.0156,
    GP_ORIENT_RIGHT, GP_HORIZ_ALIGN_NORMAL, GP_VERT_ALIGN_NORMAL, 0 },          /* text attributes */
  { 0, 0, 0, 0.16, 0.14, 0,
    0.13, 0.11375, 1.0, 1.0 },          /* decorated line attributes */
  { 0, 0.125 },                                 /* vector attributes */
  { PL_FG, GP_LINE_NONE, 1.0 },                      /* edge attributes */
  0                                                      /* rgb flag */
  };

gp_transform_t gp_transform= {   /* default mapping */
  {0.0, 1.0, 0.0, 1.0},
  {0.0, 1.0, 0.0, 1.0}
  };

int gp_clipping= 0;         /* default clip flag */

gp_box_t gp_portrait_box=  { 0.0, 0.798584, 0.0, 1.033461 };  /* 8.5 x 11 inches */
gp_box_t gp_landscape_box= { 0.0, 1.033461, 0.0, 0.798584 };  /* 11 x 8.5 inches */

char gp_error[128]= ""; /* most recent error message */

static void InitializeClip(void);
static void mem_error(void);
static int get_scratch(long n);
static char SwapTextMark(void);
static void SwapMarkText(void);
static void SwapNormMap(gp_real_t *scalx, gp_real_t *offx,
                        gp_real_t *scaly, gp_real_t *offy);
static void SwapMapNorm(void);
static void ClipArrow(gp_real_t x[3], gp_real_t y[3]);
static gp_real_t TrueNorm(gp_real_t dx, gp_real_t dy);
static gp_real_t OctagNorm(gp_real_t dx, gp_real_t dy);
static void DecorateLines(long n, const gp_real_t *px, const gp_real_t *py,
                          int closed);
static int MeshRowF(long iMax, long ijMax, int *ireg, int region,
                    long *ii, long *k);
static int MeshColF(long iMax, long ijMax, int *ireg, int region,
                    gp_real_t *x, gp_real_t *y, long *jj, long *kk);
static int MeshRowR(long iMax, long ijMax, int *ireg, int region,
                    long *ii, long *k);
static int MeshColR(long iMax, long ijMax, int *ireg, int region,
                    gp_real_t *x, gp_real_t *y, long *jj, long *kk);
static int MeshRowB(long iMax, long ijMax, int *ireg, int region,
                    long *ii, long *k);
static int MeshColB(long iMax, long ijMax, int *ireg, int region,
                    gp_real_t *x, gp_real_t *y, long *jj, long *kk);
static int *NewReg(long iMax, long ijMax);
static void FreeTmpReg(void);
static int DoSaddle(long zone, long step, long ij, long inc);
static void DoSmoothing(long *n, const gp_real_t **px, const gp_real_t **py,
                        int closed, int smooth, gp_real_t scalx, gp_real_t offx,
                        gp_real_t scaly, gp_real_t offy);
static int SmoothLines(long n, const gp_real_t *px, const gp_real_t *py,
                       int closed, int smooth, int clip);

/* ------------------------------------------------------------------------ */

/* ga_draw_lines communicates privately with gp_draw_lines via these flags.  */
static int gpCloseNext= 0;   /* (used by ga_draw_vectors also) */
static int gpSmoothNext= 0;
static int gpClipDone= 0;
static int gpClipInit= 0;    /* (used by gp_draw_fill also) */

static void InitializeClip(void)
{
  int already= gpClipInit;
  gpClipInit= 0;
  if (!already && gp_clipping) {
    gp_clip_setup(gp_transform.window.xmin, gp_transform.window.xmax,
              gp_transform.window.ymin, gp_transform.window.ymax);
  }
}

int gp_draw_lines(long n, const gp_real_t *px, const gp_real_t *py)
{
  int value= 0;
  gp_engine_t *engine;
  int closed= gpCloseNext;
  int smooth= gpSmoothNext;
  int clip= gp_clipping && !gpClipDone;
  gpCloseNext= gpSmoothNext=  gpClipDone= 0;

  if (smooth) return SmoothLines(n, px, py, closed, smooth, clip);

  if (clip) InitializeClip();
  else gpClipInit= 0;

  if (!clip || gp_clip_begin(px, py, n, closed)) {
    for (engine=gp_next_active_engine(0) ; engine ; engine=gp_next_active_engine(engine))
      if (!engine->inhibit)
        value|= engine->DrawLines(engine, n, px, py, closed, smooth);
  } else while ((n=gp_clip_more())) {
    for (engine=gp_next_active_engine(0) ; engine ; engine=gp_next_active_engine(engine))
      if (!engine->inhibit)
        value|= engine->DrawLines(engine, n, gp_xclip, gp_yclip, 0, smooth);
  }

  return value;
}

int gp_draw_markers(long n, const gp_real_t *px, const gp_real_t *py)
{
  int value= 0;
  gp_engine_t *engine;

  if (gp_clipping) {
    InitializeClip();
    n= gp_clip_points(px, py, n);
    px= gp_xclip;
    py= gp_yclip;
  }
  gpClipInit= 0;
  if (!n) return value;

  for (engine=gp_next_active_engine(0) ; engine ; engine=gp_next_active_engine(engine)) {
    if (!engine->inhibit) {
      if (ga_attributes.m.type<=32) value|= engine->DrawMarkers(engine, n, px, py);
      else value|= gp_draw_pseudo_mark(engine, n, px, py);
    }
  }

  return value;
}

int gp_draw_text(gp_real_t x0, gp_real_t y0, const char *text)
{
  int value= 0;
  gp_engine_t *engine;

  for (engine=gp_next_active_engine(0) ; engine ; engine=gp_next_active_engine(engine))
    if (!engine->inhibit)
      value|= engine->DrwText(engine, x0, y0, text);

  return value;
}

int gp_draw_fill(long n, const gp_real_t *px, const gp_real_t *py)
{
  int value= 0;
  gp_engine_t *engine;

  if (gp_clipping) {
    InitializeClip();
    n= gp_clip_filled(px, py, n);
    px= gp_xclip;
    py= gp_yclip;
  }
  gpClipInit= 0;
  if (n<2) return 0;

  for (engine=gp_next_active_engine(0) ; engine ; engine=gp_next_active_engine(engine))
    if (!engine->inhibit)
      value|= engine->DrawFill(engine, n, px, py);

  return value;
}

int gp_draw_cells(gp_real_t px, gp_real_t py, gp_real_t qx, gp_real_t qy,
            long width, long height, long nColumns,
            const gp_color_t *colors)
{
  int value= 0;
  gp_engine_t *engine;

  for (engine=gp_next_active_engine(0) ; engine ; engine=gp_next_active_engine(engine))
    if (!engine->inhibit)
      value|= engine->DrawCells(engine, px, py, qx, qy,
                                width, height, nColumns, colors);

  return value;
}

int gp_draw_disjoint(long n, const gp_real_t *px, const gp_real_t *py,
               const gp_real_t *qx, const gp_real_t *qy)
{
  int value= 0;
  gp_engine_t *engine;

  if (gp_clipping) {
    InitializeClip();
    n= gp_clip_disjoint(px, py, qx, qy, n);
    px= gp_xclip;
    py= gp_yclip;
    qx= gp_xclip1;
    qy= gp_yclip1;
  }
  gpClipInit= 0;

  for (engine=gp_next_active_engine(0) ; engine ; engine=gp_next_active_engine(engine))
    if (!engine->inhibit)
      value|= engine->DrawDisjoint(engine, n, px, py, qx, qy);

  return value;
}

/* ------------------------------------------------------------------------ */

/* Scratch space requirements--

     ga_draw_mesh - needs jMax gp_real_t points to gather columns
     ga_init_contour - needs #edges gp_real_t points for contours
                     needs ijMax short array for edge markers
                     may need ijMax short array for triangulation
     ga_draw_ticks - needs a bunch
     gp_draw_lines - needs 3*n real points for smoothing (can't overlap
               with contour scratch space)
 */

/* Scratch spaces maintained by _ga_get_scratch_p(), _ga_get_scratch_s()
   and get_scratch(). */
gp_real_t* _ga_scratch_x = NULL;
gp_real_t* _ga_scratch_y = NULL;
static long n_scratch_p = 0;

short*     _ga_scratch_s = NULL;
static long n_scratch_s = 0;

static gp_real_t* scratch_x;
static gp_real_t* scratch_y;
static long n_scratch = 0;

static void mem_error(void)
{
  strcpy(gp_error, "memory manager failed in gist.c function");
}

int _ga_get_scratch_p(long n)
{
  if (n <= n_scratch_p) {
    return 0;
  }
  if (n_scratch_p > 0) {
    n_scratch_p = 0;
    pl_free(_ga_scratch_x);
    pl_free(_ga_scratch_y);
  }
  _ga_scratch_x = PL_NEW(n, gp_real_t);
  _ga_scratch_y = PL_NEW(n, gp_real_t);
  if (_ga_scratch_x == NULL || _ga_scratch_y == NULL) {
    pl_free(_ga_scratch_x);
    pl_free(_ga_scratch_y);
    mem_error();
    return 1;
  }
  n_scratch_p = n;
  return 0;
}

static int get_scratch(long n)
{
  if (n <= n_scratch) {
    return 0;
  }
  if (n_scratch > 0) {
    n_scratch = 0;
    pl_free(scratch_x);
    pl_free(scratch_y);
  }
  scratch_x = PL_NEW(n, gp_real_t);
  scratch_y = PL_NEW(n, gp_real_t);
  if (scratch_x == NULL || scratch_y == NULL) {
    pl_free(scratch_x);
    pl_free(scratch_y);
    mem_error();
    return 1;
  }
  n_scratch = n;
  return 0;
}

int _ga_get_scratch_s(long n)
{
  if (n <= n_scratch_s) {
    return 0;
  }
  if (n_scratch_s > 0) {
    n_scratch_s = 0;
    pl_free(_ga_scratch_s);
  }
  _ga_scratch_s = PL_NEW(n, short);
  if (_ga_scratch_s == NULL) {
    mem_error();
    return 1;
  }
  n_scratch_s = n;
  return 0;
}

int ga_free_scratch(void)
{
  if (n_scratch_p > 0) {
    n_scratch_p = 0;
    pl_free(_ga_scratch_x);
    pl_free(_ga_scratch_y);
  }
  if (n_scratch_s > 0) {
    n_scratch_s = 0;
    pl_free(_ga_scratch_s);
  }
  if (n_scratch > 0) {
    n_scratch = 0;
    pl_free(scratch_x);
    pl_free(scratch_y);
  }
  return 0;
}

/* ------------------------------------------------------------------------ */

static gp_text_attribs_t textSave;

static char SwapTextMark(void)
{
  /* Remember current text attributes */
  textSave= ga_attributes.t;

  /* Swap in text attributes appropriate for markers */
  ga_attributes.t.color= ga_attributes.m.color;
  ga_attributes.t.font= MARKER_FONT;
  ga_attributes.t.height= ga_attributes.m.size * GP_DEFAULT_MARKER_SIZE;
  ga_attributes.t.orient= GP_ORIENT_RIGHT;
  ga_attributes.t.alignH= GP_HORIZ_ALIGN_CENTER;
  if (ga_attributes.m.type != GP_MARKER_POINT) ga_attributes.t.alignV= GP_VERT_ALIGN_HALF;
  else ga_attributes.t.alignV= GP_VERT_ALIGN_BASE;
  ga_attributes.t.opaque= 0;     /* later curves can cross opaque markers anyway */

  if (ga_attributes.m.type>GP_MARKER_CROSS || ga_attributes.m.type==0) return (char)ga_attributes.m.type;
  else if (ga_attributes.m.type==GP_MARKER_POINT) return '.';
  else if (ga_attributes.m.type==GP_MARKER_PLUS) return '+';
  else if (ga_attributes.m.type==GP_MARKER_ASTERISK) return '*';
  else if (ga_attributes.m.type==GP_MARKER_CIRCLE) return 'O';
  else return 'X';
}

static void SwapMarkText(void)
{
  ga_attributes.t= textSave;
}

static gp_box_t windowSave;

static void SwapNormMap(gp_real_t *scalx, gp_real_t *offx,
                        gp_real_t *scaly, gp_real_t *offy)
{
  windowSave= gp_transform.window;

  *scalx= (gp_transform.viewport.xmax-gp_transform.viewport.xmin)/
    (windowSave.xmax-windowSave.xmin);
  *offx= gp_transform.viewport.xmin - windowSave.xmin*(*scalx);
  *scaly= (gp_transform.viewport.ymax-gp_transform.viewport.ymin)/
    (windowSave.ymax-windowSave.ymin);
  *offy= gp_transform.viewport.ymin - windowSave.ymin*(*scaly);

  gp_transform.window= gp_transform.viewport;
  gp_set_transform(&gp_transform);          /* gp_set_transform checks for this case... */
}

static void SwapMapNorm(void)
{
  gp_transform.window= windowSave;
  gp_set_transform(&gp_transform);
}

static void ClipArrow(gp_real_t x[3], gp_real_t y[3])
{
  gp_real_t xmin= gp_transform.window.xmin, xmax= gp_transform.window.xmax;
  gp_real_t ymin= gp_transform.window.ymin, ymax= gp_transform.window.ymax;

  /* NB- (x[1],y[1]) is guaranteed to be inside the window */

  if (x[0]<xmin) {
    y[0]+= (xmin-x[0])*(y[1]-y[0])/(x[1]-x[0]);
    x[0]= xmin;
  } else if (x[0]>xmax) {
    y[0]+= (xmax-x[0])*(y[1]-y[0])/(x[1]-x[0]);
    x[0]= xmax;
  }
  if (y[0]<ymin) {
    x[0]+= (ymin-y[0])*(x[1]-x[0])/(y[1]-y[0]);
    y[0]= ymin;
  } else if (y[0]>ymax) {
    x[0]+= (ymax-y[0])*(x[1]-x[0])/(y[1]-y[0]);
    y[0]= ymax;
  }

  if (x[2]<xmin) {
    y[2]+= (xmin-x[2])*(y[1]-y[2])/(x[1]-x[2]);
    x[2]= xmin;
  } else if (x[2]>xmax) {
    y[2]+= (xmax-x[2])*(y[1]-y[2])/(x[1]-x[2]);
    x[2]= xmax;
  }
  if (y[2]<ymin) {
    x[2]+= (ymin-y[2])*(x[1]-x[2])/(y[1]-y[2]);
    y[2]= ymin;
  } else if (y[2]>ymax) {
    x[2]+= (ymax-y[2])*(x[1]-x[2])/(y[1]-y[2]);
    y[2]= ymax;
  }
}

static gp_real_t TrueNorm(gp_real_t dx, gp_real_t dy)
{
  double x= fabs((double)dx);
  double y= fabs((double)dy);
  if (x>y) {
    y= y/x;
    return (gp_real_t)(x*sqrt(1.0+y*y));
  } else if (y) {
    x= x/y;
    return (gp_real_t)(y*sqrt(1.0+x*x));
  } else {
    return (gp_real_t)0.0;
  }
}

static gp_real_t OctagNorm(gp_real_t dx, gp_real_t dy)
{
  double x= fabs((double)dx);
  double y= fabs((double)dy);
  double z= (x+y)*SQRT_HALF;
  if (x>y) {
    return x>z? x : z;
  } else {
    return y>z? y : z;
  }
}

static void DecorateLines(long n, const gp_real_t *px, const gp_real_t *py,
                          int closed)
{
  gp_real_t dx, dy, x1, y1, x0, y0, x00= *px, y00= *py;
  gp_real_t len, trueLen, xL, yL, xW, yW, x[3], y[3];
  gp_real_t markSpacing, raySpacing, markPhase, rayPhase;
  gp_real_t arrowL, arrowW, frac;
  char markText[2];
  int marks= ga_attributes.dl.marks;
  int rays= ga_attributes.dl.rays;
  int type;

  /* Save transform map, set up for transform here */
  gp_real_t scalx, offx, scaly, offy;
  SwapNormMap(&scalx, &offx, &scaly, &offy);

  markSpacing= ga_attributes.dl.mSpace;
  markPhase= ga_attributes.dl.mPhase;
  if (marks) {
    markText[0]= SwapTextMark();
    markText[1]= '\0';
  }
  raySpacing= ga_attributes.dl.rSpace;
  rayPhase= ga_attributes.dl.rPhase;
  arrowL= ga_attributes.dl.arrowL * GA_DEFAULT_ARROW_LENGTH;
  arrowW= ga_attributes.dl.arrowW * GA_DEFAULT_ARROW_WIDTH;
  type= ga_attributes.l.type;
  if (rays) {
    ga_attributes.l.type= GP_LINE_SOLID; /* dashed ray arrows can be confusing */
  }

  x0= *px++;
  y0= *py++;
  while (--n>0 || closed) {
    if (n<=0) {
      closed= 0;
      x1= x00;
      y1= y00;
    } else {
      x1= *px++;
      y1= *py++;
    }
    dx= scalx*(x1-x0);
    dy= scaly*(y1-y0);
    len= OctagNorm(dx, dy);

    if (marks) {
      markPhase+= len;
      while (markPhase>=markSpacing) {
        markPhase-= markSpacing;
        frac= markPhase/len;   /* 0<=frac<1, pt=pt0*frac+pt1*(1-frac) */
        /* Since the point is guaranteed to be inside current window,
           no floating point clip is required here.  Rays are harder.  */
        gp_draw_text(x1*scalx+offx-dx*frac, y1*scaly+offy-dy*frac, markText);
      }
    }

    if (rays) {
      rayPhase+= len;
      if (rayPhase>=raySpacing) {
        trueLen= TrueNorm(dx, dy);
        xL= dx/trueLen;
        yW= -arrowW*xL;
        xL*= -arrowL;
        yL= dy/trueLen;
        xW= arrowW*yL;
        yL*= -arrowL;
        while (rayPhase>=raySpacing) {
          rayPhase-= raySpacing;
          frac= rayPhase/len;   /* 0<=frac<1, pt=pt0*frac+pt1*(1-frac) */
          x[1]= x1*scalx+offx-dx*frac;
          y[1]= y1*scaly+offy-dy*frac;
          x[0]= x[1]+xL+xW;
          y[0]= y[1]+yL+yW;
          x[2]= x[1]+xL-xW;
          y[2]= y[1]+yL-yW;
          /* Guaranteed that point of arrow (x[1],y[1]) is inside window,
             but points may hang outside.  Clipping is tricky because
             this routine is called from inside a clipping loop.  */
          ClipArrow(x,y);
          gpClipDone= 1;
          gp_draw_lines(3, x, y);
        }
      }
    }
    x0= x1;
    y0= y1;
  }

  SwapMapNorm();
  if (marks) SwapMarkText();
  if (rays) ga_attributes.l.type= type;
}

/* gp_draw_pseudo_mark is potentially useful as the DrawMarkers method
   for an gp_engine_t which has no polymarker primitive (e.g.- an X window).
   It is therefore declared as extern in gist/engine.h, although it is
   defined here so it can share the Swap... functions.  */
int gp_draw_pseudo_mark(gp_engine_t *engine, long n, const gp_real_t *px, const gp_real_t *py)
{
  int value= 0;
  char text[2];

  /* Set up marker for gp_draw_text, swap in appropriate text attributes */
  text[0]= SwapTextMark();
  text[1]= '\0';

  while (--n>=0) value|= engine->DrwText(engine, *px++, *py++, text);
  engine->marked= 1;

  /* Restore text attributes */
  SwapMarkText();
  return value;
}

int ga_draw_lines(long n, const gp_real_t *px, const gp_real_t *py)
     /* Like gp_draw_lines, but includes ga_attributes_t fancy line attributes
        as well as the line attributes from GpAttributes.  */
{
  int value= 0;

  /* Redirect to polymarker if no line type */
  if (ga_attributes.l.type==GP_LINE_NONE) {
    /* if (!ga_attributes.dl.marks) return 0;    makes no sense */
    return gp_draw_markers(n, px, py);
  }

  /* Use the (potentially faster) gp_draw_lines routine to draw
     an undecorated polyline.  */
  if (!ga_attributes.dl.marks && !ga_attributes.dl.rays) {
    gpCloseNext= ga_attributes.dl.closed;
    gpSmoothNext= ga_attributes.dl.smooth;
    return gp_draw_lines(n, px, py);
  }

  if (gp_clipping) InitializeClip();
  gpClipInit= 0;

  /* Note that a decorated line cannot be smooth... */
  if (!gp_clipping || gp_clip_begin(px, py, n, ga_attributes.dl.closed)) {
    gpCloseNext= ga_attributes.dl.closed;
    gpClipDone= 1;
    value= gp_draw_lines(n, px, py);
    DecorateLines(n, px, py, ga_attributes.dl.closed);
  } else while ((n= gp_clip_more())) {
    gpClipDone= 1;
    value|= gp_draw_lines(n, gp_xclip, gp_yclip);
    DecorateLines(n, gp_xclip, gp_yclip, 0);
  }

  return value;
}

/* ------------------------------------------------------------------------ */

/* ARGSUSED */
static int MeshRowF(long iMax, long ijMax, int *ireg, int region,
                    long *ii, long *k)
{
  long i= *ii;
  while (++i<ijMax)   /* scan till edge exists */
    if (ireg[i] || ireg[i+iMax]) break;
  if (i>=ijMax) return 1;
  *k= i-1;
  while (++i<ijMax)   /* scan till edge does not exist */
    if (!ireg[i] && !ireg[i+iMax]) break;
  *ii= i;
  return 0;
}

/* ARGSUSED */
static int MeshColF(long iMax, long ijMax, int *ireg, int region,
                    gp_real_t *x, gp_real_t *y, long *jj, long *kk)
{
  long k, j= *jj;
  while ((j+=iMax)<ijMax)   /* scan till edge exists */
    if (ireg[j] || ireg[j+1]) break;
  if (j>=ijMax) return 1;
  _ga_scratch_x[0]= x[j-iMax];   /* gather 1st segment into scratch */
  _ga_scratch_y[0]= y[j-iMax];
  _ga_scratch_x[1]= x[j];
  _ga_scratch_y[1]= y[j];
  k= 2;
  while ((j+=iMax)<ijMax) { /* scan till edge does not exist */
    if (!ireg[j] && !ireg[j+1]) break;
    _ga_scratch_x[k]= x[j];      /* gather next segment into scratch */
    _ga_scratch_y[k]= y[j];
    k++;
  }
  *jj= j;
  *kk= k;
  return 0;
}

static int MeshRowR(long iMax, long ijMax, int *ireg, int region,
                    long *ii, long *k)
{
  long i= *ii;
  while (++i<ijMax)   /* scan till edge exists */
    if (ireg[i]==region || ireg[i+iMax]==region) break;
  if (i>=ijMax) return 1;
  *k= i-1;
  while (++i<ijMax)   /* scan till edge does not exist */
    if (ireg[i]!=region && ireg[i+iMax]!=region ) break;
  *ii= i;
  return 0;
}

static int MeshColR(long iMax, long ijMax, int *ireg, int region,
                    gp_real_t *x, gp_real_t *y, long *jj, long *kk)
{
  long k, j= *jj;
  while ((j+=iMax)<ijMax)   /* scan till edge exists */
    if (ireg[j]==region || ireg[j+1]==region) break;
  if (j>=ijMax) return 1;
  _ga_scratch_x[0]= x[j-iMax];   /* gather 1st segment into scratch */
  _ga_scratch_y[0]= y[j-iMax];
  _ga_scratch_x[1]= x[j];
  _ga_scratch_y[1]= y[j];
  k= 2;
  while ((j+=iMax)<ijMax) { /* scan till edge does not exist */
    if (ireg[j]!=region && ireg[j+1]!=region) break;
    _ga_scratch_x[k]= x[j];      /* gather next segment into scratch */
    _ga_scratch_y[k]= y[j];
    k++;
  }
  *jj= j;
  *kk= k;
  return 0;
}

static int MeshRowB(long iMax, long ijMax, int *ireg, int region,
                    long *ii, long *k)
{
  long i= *ii;
  while (++i<ijMax)   /* scan till edge exists */
    if ((ireg[i]==region) ^ (ireg[i+iMax]==region)) break;
  if (i>=ijMax) return 1;
  *k= i-1;
  while (++i<ijMax)   /* scan till edge does not exist */
    if ((ireg[i]!=region) ^ (ireg[i+iMax]==region)) break;
  *ii= i;
  return 0;
}

static int MeshColB(long iMax, long ijMax, int *ireg, int region,
                    gp_real_t *x, gp_real_t *y, long *jj, long *kk)
{
  long k, j= *jj;
  while ((j+=iMax)<ijMax)   /* scan till edge exists */
    if ((ireg[j]==region) ^ (ireg[j+1]==region)) break;
  if (j>=ijMax) return 1;
  _ga_scratch_x[0]= x[j-iMax];   /* gather 1st segment into scratch */
  _ga_scratch_y[0]= y[j-iMax];
  _ga_scratch_x[1]= x[j];
  _ga_scratch_y[1]= y[j];
  k= 2;
  while ((j+=iMax)<ijMax) { /* scan till edge does not exist */
    if ((ireg[j]!=region) ^ (ireg[j+1]==region)) break;
    _ga_scratch_x[k]= x[j];      /* gather next segment into scratch */
    _ga_scratch_y[k]= y[j];
    k++;
  }
  *jj= j;
  *kk= k;
  return 0;
}

static int *tmpReg= 0;

static void FreeTmpReg(void)
{
  int *reg= tmpReg;
  tmpReg= 0;
  pl_free(reg);
}

static int *NewReg(long iMax, long ijMax)
{
  if (tmpReg) FreeTmpReg();
  tmpReg= (int *)pl_malloc(sizeof(int)*(ijMax+iMax+1));
  if (tmpReg) {
    long i, j=0;
    for (i=0 ; i<ijMax+iMax+1 ; i++) {
      if (i<1 || i>=iMax || j<1) tmpReg[i]= 0;
      else tmpReg[i]= 1;
      j++;
      if (j==iMax) j= 0;
    }
  } else {
    mem_error();
  }
  return tmpReg;
}

int ga_draw_mesh(ga_mesh_t *mesh, int region, int boundary, int inhibit)
{
  int value= 0;
  long iMax= mesh->iMax, jMax= mesh->jMax;
  long ijMax= iMax*jMax;
  gp_real_t *x= mesh->x, *y= mesh->y;
  int *ireg= mesh->reg;
  long i, j, k;
  int (*MeshRow)(long, long, int*, int, long*, long*);
  int (*MeshCol)(long, long, int*, int, gp_real_t*, gp_real_t*, long*, long*);

  /* Load appropriate edge existence scanners */
  if (!boundary) {
    if (region==0) {
      /* draw entire mesh */
      MeshRow= &MeshRowF;
      MeshCol= &MeshColF;
    } else {
      /* draw single region */
      MeshRow= &MeshRowR;
      MeshCol= &MeshColR;
    }
  } else {
    /* draw region boundary */
    MeshRow= &MeshRowB;
    MeshCol= &MeshColB;
  }

  /* Be sure there is enough scratch space to gather a column */
  if (!(inhibit&2) && _ga_get_scratch_p(jMax)) return 1;

  /* Create default region array if none supplied */
  if (!ireg) {
    ireg= NewReg(iMax, ijMax);
    if (!ireg) return 1;
    mesh->reg= ireg;
  }

  /* Draw rows */
  if (!(inhibit&1)) {
    for (i=0 ; i<ijMax ; /* i incremented in MeshRow */) {
      if (MeshRow(iMax, ijMax, ireg, region, &i, &j)) break;
      value|= gp_draw_lines(i-j, x+j, y+j);
    }
  }

  /* Draw columns */
  if (!(inhibit&2)) {
    for (i=0 ; i<iMax ; i++) {
      j= i;
      for (;;) {
        if (MeshCol(iMax, ijMax, ireg, region, x, y, &j, &k)) break;
        value|= gp_draw_lines(k, _ga_scratch_x, _ga_scratch_y);
        if (j>=ijMax) break;
      }
    }
  }

  if (tmpReg) FreeTmpReg();
  return value;
}

/* ------------------------------------------------------------------------ */

#define EXISTS(ij) (region? ireg[ij]==region : ireg[ij])

int ga_fill_mesh(ga_mesh_t *mesh, int region, const gp_color_t *colors,
               long nColumns)
{
  int value= 0;
  long iMax= mesh->iMax;
  long ijMax= iMax*mesh->jMax;
  gp_real_t *x= mesh->x, *y= mesh->y;
  int *ireg= mesh->reg;
  gp_real_t qx[4], qy[4];
  long ij, row, col;
  int rgb = colors? ga_attributes.rgb : 0;
  ga_attributes.rgb = 0;

  /* Create default region array if none supplied */
  if (!ireg) {
    ireg= NewReg(iMax, ijMax);
    if (!ireg) return 1;
    mesh->reg= ireg;
  }

  InitializeClip();

  /* The only filled area attribute set is the color.  */
  row= col= 0;
  if (!colors) ga_attributes.f.color = PL_BG;
  for (ij=iMax+1 ; ij<ijMax ; ij++) {
    if (EXISTS(ij)) {
      qx[0]= x[ij-iMax-1];  qy[0]= y[ij-iMax-1];
      qx[1]= x[ij-iMax  ];  qy[1]= y[ij-iMax  ];
      qx[2]= x[ij       ];  qy[2]= y[ij       ];
      qx[3]= x[ij     -1];  qy[3]= y[ij     -1];
      if (rgb) {
        ga_attributes.f.color = PL_RGB(colors[3*(row+col)],
                              colors[3*(row+col)+1],colors[3*(row+col)+2]);
      } else if (colors) {
        ga_attributes.f.color = colors[row+col];
      }
      gpClipInit= 1;
      value|= gp_draw_fill(4, qx, qy);
    }
    col++;
    if (col==iMax) {
      col= 0;
      row+= nColumns;
    }
  }

  if (tmpReg) FreeTmpReg();
  return value;
}

int ga_fill_marker(long n, const gp_real_t *px, const gp_real_t *py,
                 gp_real_t x0, gp_real_t y0)
{
  int value= 0;
  gp_engine_t *engine;
  long i;

  /* Save transform map, set up for transform here */
  gp_real_t scalx, offx, scaly, offy;
  SwapNormMap(&scalx, &offx, &scaly, &offy);
  x0= x0*scalx + offx;
  y0= y0*scaly + offy;

  /* get scratch space, copy points to scratch, adding specified offsets */
  _ga_get_scratch_p(n);
  for (i=0 ; i<n ; i++) {
    _ga_scratch_x[i]= px[i] + x0;
    _ga_scratch_y[i]= py[i] + y0;
  }
  px= _ga_scratch_x;
  py= _ga_scratch_y;

  if (gp_clipping) {
    gp_real_t xmin= gp_transform.viewport.xmin;
    gp_real_t xmax= gp_transform.viewport.xmax;
    gp_real_t ymin= gp_transform.viewport.ymin;
    gp_real_t ymax= gp_transform.viewport.ymax;
    if (xmin > xmax) { gp_real_t tmp= xmin; xmin= xmax; xmax= tmp; }
    if (ymin > ymax) { gp_real_t tmp= ymin; ymin= ymax; ymax= tmp; }
    gp_clip_setup(xmin, xmax, ymin, ymax);
    n= gp_clip_filled(px, py, n);
    px= gp_xclip;
    py= gp_yclip;
  }

  if (n>=2) {
    for (engine=gp_next_active_engine(0) ; engine ; engine=gp_next_active_engine(engine))
      if (!engine->inhibit)
        value|= engine->DrawFill(engine, n, px, py);
  }

  SwapMapNorm();
  return value;
}

/* ------------------------------------------------------------------------ */

int ga_draw_vectors(ga_mesh_t *mesh, int region,
              const gp_real_t *u, const gp_real_t *v, gp_real_t scale)
{
  int value= 0;
  long iMax= mesh->iMax;
  long ijMax= iMax*mesh->jMax;
  gp_real_t *x= mesh->x, *y= mesh->y;
  int *ireg= mesh->reg;
  int hollow= ga_attributes.vect.hollow;
  gp_real_t aspect= ga_attributes.vect.aspect;
  int etype= ga_attributes.e.type;
  gp_real_t xc, yc, dx, dy, vx[3], vy[3];
  long ij;
  gp_real_t scalx, offx, scaly, offy, dxscale, dyscale;

  /* Create default region array if none supplied */
  if (!ireg) {
    ireg= NewReg(iMax, ijMax);
    if (!ireg) return 1;
    mesh->reg= ireg;
  }

  /* Save transform map, set up for transform here */
  SwapNormMap(&scalx, &offx, &scaly, &offy);

  dxscale= 0.3333333333*scale*scalx;
  dyscale= 0.3333333333*scale*scaly;
  aspect*= 3.0;

  if (!hollow) ga_attributes.e.type= GP_LINE_NONE;

  InitializeClip();

  for (ij=0 ; ij<ijMax ; ij++) {
    if (EXISTS(ij) || EXISTS(ij+1) || EXISTS(ij+1+iMax) || EXISTS(ij+iMax)) {
      xc= scalx*x[ij]+offx;
      yc= scaly*y[ij]+offy;
      dx= dxscale*u[ij];
      dy= dyscale*v[ij];

      /* Dart has centroid at (xc,yc), length 3*(dx,dy), given aspect */
      vx[1]= xc + 2.0*dx;
      vy[1]= yc + 2.0*dy;
      vx[0]= xc - dx + aspect*dy;
      vx[2]= xc - dx - aspect*dy;
      vy[0]= yc - dy - aspect*dx;
      vy[2]= yc - dy + aspect*dx;

      if (hollow) {
        gpCloseNext= gpClipInit= 1;
        value|= gp_draw_lines(3, vx, vy);
      } else {
        gpClipInit= 1;
        value|= gp_draw_fill(3, vx, vy);
      }
    }
  }

  if (!hollow) ga_attributes.e.type= etype;
  if (tmpReg) FreeTmpReg();
  SwapMapNorm();
  return value;
}

/* ------------------------------------------------------------------------ */

/* To use the contouring routines:

      mesh= ...;          **( iMax, jMax, x, y, reg, triangle )**
      for (i=0 ; i<nlevels ; i++) {        **( loop on levels )**
        ga_attributes.dl= linestyle[i];
        if (ga_init_contour(mesh, 0, z, level[i]))
          while (ga_draw_contour(&n, &px, &py, &ga_attributes.dl.closed))
            ga_draw_lines(n, px, py);    **( loop on disjoint parts )**
      }
 */

/* ga_draw_contour state information */
static ga_mesh_t *mesh;
static int region;
static const gp_real_t *z;
static gp_real_t level;
static long ni, nj, nib, njb, ibegin, jbegin;
static short *iedges, *jedges;
static int keepLeft;

int ga_init_contour(ga_mesh_t *msh, int regn,
                  const gp_real_t *zz, gp_real_t lev)
       /* Find the edges cut by the current contour, remembering
          z, triangle, and level for the ga_draw_contour routine, which
          actually walks the contour.  The z array represents function
          values on the mesh msh.  If triangle!=0,
          it represents the initial value of the triangulation array,
          which determines the behavior of ga_draw_contour in saddle zones.
            triangle[j][i]= 1, -1, or 0 as the zone bounded by
              [j-1][i-1], [j-1][i], [j][i], and [j][i-1]
              has been triangulated from (-1,-1) to (0,0),
              from (-1,0) to (0,-1), or has not yet been
              triangulated.
          The msh->triangle array is updated by ga_draw_contour.
          If a contour passes through an untriangulated saddle zone,
          ga_draw_contour chooses a triangulation and marks the triangle
          array approriately, so that subsequent calls to ga_draw_contour
          will never produce intersecting contour curves.  */
{
  long iMax= msh->iMax;
  long ijMax= iMax*msh->jMax;
  int *ireg= msh->reg;
  long ij;

  /* Create default region array if none supplied */
  if (!ireg) {
    ireg= NewReg(iMax, ijMax);
    if (!ireg) return 0;
    msh->reg= ireg;
  }

  /* Remember data for ga_draw_contour */
  mesh= msh;
  region= regn;
  z= zz;
  level= lev;

  /* Get scratch space to hold edges */
  if (_ga_get_scratch_s(2*ijMax)) return 0;
  iedges= _ga_scratch_s;
  jedges= _ga_scratch_s+ijMax;

  /* Find all points above contour level */
  for (ij=0 ; ij<ijMax ; ij++) iedges[ij]= zz[ij]>lev;

  /* Find j=const edges which cut level plane */
  nj= njb= 0;
  for (ij=1 ; ij<ijMax ; ij++) {
    if ((iedges[ij]^iedges[ij-1]) && (EXISTS(ij)||EXISTS(ij+iMax))) {
      if ((ireg[ij]==region) ^ (ireg[ij+iMax]==region)) {
        jedges[ij]= 2;  /* contour enters mesh here */
        njb++;
      } else {
        jedges[ij]= 1;  /* interior edge */
        nj++;
      }
    } else {
      jedges[ij]= 0;
    }
  }
  jbegin= 1;

  /* Find i=const edges which cut level plane */
  ni= nib= 0;
  for (ij=ijMax-1 ; ij>=iMax ; ij--) {
    if ((iedges[ij]^iedges[ij-iMax]) && (EXISTS(ij)||EXISTS(ij+1))) {
      if ((ireg[ij]==region) ^ (ireg[ij+1]==region)) {
        iedges[ij]= 2;  /* contour enters mesh here */
        nib++;
      } else {
        iedges[ij]= 1;  /* interior edge */
        ni++;
      }
    } else {
      iedges[ij]= 0;
    }
  }
  ibegin= iMax;

  /* Set keepLeft to a known value (arbitrary but repeatable) */
  keepLeft= 0;

  /* Get scratch space for level curves */
  if (_ga_get_scratch_p(ni + nib + nj +njb + 1)) return 0;

  if (tmpReg) FreeTmpReg();
  return ni || nib || nj || njb;
}

static int DoSaddle(long zone, long step, long ij, long inc)
{
  /* The contour level plane intersects all four edges of this zone.
     DoSaddle examines the triangulation map to see if the decision
     has already been made.  If not, DoSaddle decides which way to
     turn on the basis of the keepLeft flag (set if the previous
     turn was to the right), the sets the triangulation map entry
     for this zone accordingly.  This ensures future triangulation
     decisions in this zone will be made consistently, so that
     contours at different levels never cross.  */
  short *triang= mesh->triangle;

  /* Return immediately if triangulation already decided.  */
  if (triang && triang[zone]) return triang[zone]>0? (step>0) : (step<0);

  if (inc==1) {  /* currently on j=const edge */
    if (triang) triang[zone]= keepLeft? -1 : 1;
    return keepLeft? (step<0) : (step>0);

  } else {       /* currently on i=const edge */
    if (triang) triang[zone]= keepLeft? 1 : -1;
    return keepLeft? (step>0) : (step<0);
  }
}

int ga_draw_contour(long *cn, gp_real_t **cx, gp_real_t **cy, int *closed)
       /* After a call to ga_init_contour, ga_draw_contour must be called
          repeatedly to generate the sequence of curves obtained by
          walking the edges cut by the contour level plane.  ga_draw_contour
          returns 1 until there are no more contours to be plotted,
          when it returns 0.  ga_draw_contour signals an error by returning
          0, but setting *cn!=0.  The curve coordinates (*cx,*cy)
          use internal scratch space and the associated storage must
          not be freed.  (*cx, *cy) are valid only until the next
          ga_draw_contour or ga_draw_mesh call.  */
{
  long iMax= mesh->iMax;
  long ijMax= iMax*mesh->jMax;
  gp_real_t *x= mesh->x, *y= mesh->y;
  int *ireg= mesh->reg;
  long ij, zone, step, inc, n;
  gp_real_t frac;
  int isClosed;
  short mark;

  /* Find a starting point -- get current zone and edge */
  if (nib>0) {
    for (ij=ibegin ; ij<ijMax ; ij++) if (iedges[ij]==2) break;
    if (ij>=ijMax)  return 0; /* this is a bug */
    iedges[ij]= 0;
    zone= EXISTS(ij)? ij : ij+1;
    step= EXISTS(ij)? -1 : 1;
    if (--nib) ibegin= ij+1;
    else ibegin= iMax;
    inc= iMax;

  } else if (njb>0) {
    for (ij=jbegin ; ij<ijMax ; ij++) if (jedges[ij]==2) break;
    if (ij>=ijMax)  return 0; /* this is a bug */
    jedges[ij]= 0;
    zone= EXISTS(ij)? ij : ij+iMax;
    step= EXISTS(ij)? -iMax : iMax;
    if (--njb) jbegin= ij+1;
    else jbegin= 1;
    inc= 1;

  } else if (ni>0) {
    for (ij=ibegin ; ij<ijMax ; ij++) if (iedges[ij]) break;
    if (ij>=ijMax)  return 0; /* this is a bug */
    iedges[ij]= 0;
    zone= ij+1;    /* or ij, doesn't really matter... */
    step= 1;       /* ...this choice tends to go counterclockwise */
    ni--;
    ibegin= ij+1;
    inc= iMax;

  } else if (nj>0) {
    for (ij=jbegin ; ij<ijMax ; ij++) if (jedges[ij]) break;
    if (ij>=ijMax)  return 0; /* this is a bug */
    jedges[ij]= 0;
    zone= ij;      /* or ij+iMax, doesn't really matter... */
    step= -iMax;   /* ...this choice tends to go counterclockwise */
    nj--;
    jbegin= ij+1;
    inc= 1;

  } else {
    return 0;      /* no more edges, only correct way out */
  }

  /* Salt away first point */
  frac= (z[ij]-level)/(z[ij]-z[ij-inc]);
  _ga_scratch_x[0]= frac*(x[ij-inc]-x[ij]) + x[ij];
  _ga_scratch_y[0]= frac*(y[ij-inc]-y[ij]) + y[ij];
  n= 1;

  /* Walk the contour */
  isClosed= 0;
  for(;;) {
    /* Find exit from current zone */
    if (inc==1) { /* this is a j=const edge */
      if (jedges[ij+step] && !iedges[zone]) {
        /* step, inc unchanged */
        ij+= step;
      } else if (iedges[zone] &&
                 (!iedges[zone-1] || DoSaddle(zone, step, ij, inc))) {
        ij= zone;
        if (step>0) keepLeft= 1;  /* just turned right */
        else keepLeft= 0;
        step= 1;
        inc= iMax;
      } else if (iedges[zone-1]) {
        ij= zone-1;
        if (step>0) keepLeft= 0;  /* just turned left */
        else keepLeft= 1;
        step= -1;
        inc= iMax;
      } else {
        isClosed= 1;  /* end of a closed contour */
        break;
      }
    } else {      /* this is a i=const edge */
      if (iedges[ij+step] && !jedges[zone]) {
        /* step, inc unchanged */
        ij+= step;
      } else if (jedges[zone] &&
                 (!jedges[zone-iMax] || DoSaddle(zone, step, ij, inc))) {
        ij= zone;
        if (step>0) keepLeft= 0;  /* just turned left */
        else keepLeft= 1;
        step= iMax;
        inc= 1;
      } else if (jedges[zone-iMax]) {
        ij= zone-iMax;
        if (step>0) keepLeft= 1;  /* just turned right */
        else keepLeft= 0;
        step= -iMax;
        inc= 1;
      } else {
        isClosed= 1;  /* end of a closed contour */
        break;
      }
    }

    /* Salt away current point */
    frac= (z[ij]-level)/(z[ij]-z[ij-inc]);
    _ga_scratch_x[n]= frac*(x[ij-inc]-x[ij]) + x[ij];
    _ga_scratch_y[n]= frac*(y[ij-inc]-y[ij]) + y[ij];
    n++;

    /* Step into next zone */
    zone+= step;

    /* Erase edge marker for entry edge */
    if (inc==1) {
      mark= jedges[ij];
      jedges[ij]= 0;
      if (mark==2) { /* end of an open contour */
        njb--;
        if (!njb) jbegin= 1;
        break;
      } else nj--;
    } else {
      mark= iedges[ij];
      iedges[ij]= 0;
      if (mark==2) { /* end of an open contour */
        nib--;
        if (!nib) ibegin= iMax;
        break;
      }
      else ni--;
    }
  }

  *cn= n;
  *cx= _ga_scratch_x;
  *cy= _ga_scratch_y;
  *closed= isClosed;

  /* Copy first point for closed curves (as a convenience) */
  if (isClosed) {
    _ga_scratch_x[n]= _ga_scratch_x[0];
    _ga_scratch_y[n]= _ga_scratch_y[0];
  }

  return 1;
}

/* ------------------------------------------------------------------------ */

static void DoSmoothing(long *n, const gp_real_t **px, const gp_real_t **py,
                        int closed, int smooth, gp_real_t scalx, gp_real_t offx,
                        gp_real_t scaly, gp_real_t offy)
     /* Each point is "split" into three colinear points:
        The central point of each triad is the original point, and
        the other two lie along the direction bisecting the original turn
        at the center point.  For open curves, the first and last points
        are simply duplicated.  Each group of 4 points is suitable as
        the initial, two control, and final points of a Bezier curve.  */
{
  long nn= *n;
  const gp_real_t *x= *px,  *y= *py;
  gp_real_t smoothness;

  /* temporaries required in loop */
  gp_real_t x0, y0, x1, y1, dx0, dy0, dx1, dy1, dsx, dsy, ds0, ds1;
  long i, j;

  if (get_scratch(3*nn + 2)) {
    *n = 0;
    return;
  }

  /* support four graded amounts of smoothing--
     the triad of points is spread out more and more as smooth increases */
  if (smooth==1) smoothness= 0.5*0.25/3.0;
  else if (smooth==2) smoothness= 0.5*0.50/3.0;
  else if (smooth==3) smoothness= 0.5*0.75/3.0;
  else smoothness= 0.5*1.0/3.0;

  /* initialize loop on segments */
  x1= scalx*x[0]+offx;
  y1= scaly*y[0]+offy;
  if (closed) {
    /* Previous segment connects last point to first point */
    x0= scalx*x[nn-1]+offx;
    y0= scaly*y[nn-1]+offy;
    dx1= x1-x0;
    dy1= y1-y0;
    ds1= sqrt(dx1*dx1 + dy1*dy1);
    dx1= ds1!=0.0? dx1/ds1 : 0.0;
    dy1= ds1!=0.0? dy1/ds1 : 0.0;
  } else {
    /* Previous segment is zero */
    dx1= dy1= ds1= 0.0;
  }
  j= 1;

  /* do nn-1 segments- (dx1,dy1) is current segment, (dx0,dy0) previous */
  for (i=1 ; i<nn ; i++) {
    scratch_x[j]= x0= x1;
    scratch_y[j]= y0= y1;
    x1= scalx*x[i]+offx;
    y1= scaly*y[i]+offy;

    dx0= dx1;
    dx1= x1-x0;
    dy0= dy1;
    dy1= y1-y0;
    ds0= ds1;
    /* Note- clipped, normalized coordinates   */
    /* ==> there is no danger of overflow here */
    ds1= sqrt(dx1*dx1 + dy1*dy1);
    dx1= ds1!=0.0? dx1/ds1 : 0.0;
    dy1= ds1!=0.0? dy1/ds1 : 0.0;
    dsx= smoothness * (dx0 + dx1);
    dsy= smoothness * (dy0 + dy1);
    scratch_x[j-1]= x0 - ds0*dsx;
    scratch_x[j+1]= x0 + ds1*dsx;
    scratch_y[j-1]= y0 - ds0*dsy;
    scratch_y[j+1]= y0 + ds1*dsy;

    j+= 3;
  }
  /* now i= n and j= 3*n-2, scratch_x[3*n-4] has been set */

  if (closed) {
    /* final segment connects last point to first */
    scratch_x[j]= x0= x1;
    scratch_y[j]= y0= y1;
    x1= scalx*x[0]+offx;
    y1= scaly*y[0]+offy;

    dx0= dx1;
    dx1= x1-x0;
    dy0= dy1;
    dy1= y1-y0;
    ds0= ds1;
    /* Note- clipped, normalized coordinates   */
    /* ==> there is no danger of overflow here */
    ds1= sqrt(dx1*dx1 + dy1*dy1);
    dx1= ds1!=0.0? dx1/ds1 : 0.0;
    dy1= ds1!=0.0? dy1/ds1 : 0.0;
    dsx= smoothness * (dx0 + dx1);
    dsy= smoothness * (dy0 + dy1);
    scratch_x[j-1]= x0 - ds0*dsx;
    scratch_x[j+1]= x0 + ds1*dsx;
    scratch_y[j-1]= y0 - ds0*dsy;
    scratch_y[j+1]= y0 + ds1*dsy;

    /* last control point was computed when i=1, and final knot point
       is first point */
    scratch_x[j+2]= scratch_x[0];
    scratch_y[j+2]= scratch_y[0];
    scratch_x[j+3]= x1;  /* == scratch_x[1] */
    scratch_y[j+3]= y1;  /* == scratch_y[1] */
    *n= j+3;  /* == 3*n+1 (counts first/last point twice) */

  } else {
    /* final control point and final knot point coincide */
    scratch_x[j]= scratch_x[j-1]= x1;
    scratch_y[j]= scratch_y[j-1]= y1;
    *n= j;  /* == 3*n-2 */
  }

  *px= scratch_x+1;
  *py= scratch_y+1;
}

static int SmoothLines(long n, const gp_real_t *px, const gp_real_t *py,
                       int closed, int smooth, int clip)
{
  int value= 0;
  gp_engine_t *engine;
  gp_real_t scalx, offx, scaly, offy;

  if (clip && !gpClipInit) InitializeClip();
  else gpClipInit= 0;

  /* Now that clip has been set up, can change coordiante transform--
     output from DoSmoothing is in normalized coordinates, while input
     is in world coordinates...  */
  SwapNormMap(&scalx, &offx, &scaly, &offy);

  if (!clip || gp_clip_begin(px, py, n, closed)) {
    DoSmoothing(&n, &px, &py, closed, smooth, scalx, offx, scaly, offy);
    /* Note: if closed, n= 3*n+1, last point same as first */
    for (engine=gp_next_active_engine(0) ; engine ; engine=gp_next_active_engine(engine))
      if (!engine->inhibit)
        value|= engine->DrawLines(engine, n, px, py, 0, smooth);
  } else while ((n=gp_clip_more())) {
    px= gp_xclip;
    py= gp_yclip;
    DoSmoothing(&n, &px, &py, 0, smooth, scalx, offx, scaly, offy);
    for (engine=gp_next_active_engine(0) ; engine ; engine=gp_next_active_engine(engine))
      if (!engine->inhibit)
        value|= engine->DrawLines(engine, n, px, py, 0, smooth);
  }

  SwapMapNorm();

  return value;
}

/* ------------------------------------------------------------------------ */
