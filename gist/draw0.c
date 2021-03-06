/*
 * $Id: draw0.c,v 1.1 2005-09-18 22:04:29 dhmunro Exp $
 * Virtual functions for gd_drawing_t class.
 */
/* Copyright (c) 2005, The Regents of the University of California.
 * All rights reserved.
 * This file is part of yorick (http://yorick.sourceforge.net).
 * Read the accompanying LICENSE file for details.
 */

#include "gist/private.h"
#include "gist/draw.h"
#include "gist/text.h"
#include "play2.h"
#include "play/std.h"

extern char *strcpy(char *, const char *);

extern double log10(double);
#define SAFELOG0 (-999.)
#define SAFELOG(x) ((x)>0? log10(x) : ((x)<0? log10(-(x)) : -999.))

static void KillElement(gd_element_t *el);
static void LinesKill(void *el);
static void DisjointKill(void *el);
static void TextKill(void *el);
static void CellsKill(void *el);
static void PolysKill(void *el);
static void MeshKill(void *el);
static void FilledKill(void *el);
static void VectorsKill(void *el);
static void KillConGrps(ge_lines_t **grps, int ngrps);
static void ContoursKill(void *el);
static void SystemKill(void *el);

static int LinesGet(void *el);
static int DisjointGet(void *el);
static int TextGet(void *el);
static int CellsGet(void *el);
static int PolysGet(void *el);
static int MeshGet(void *el);
static int FilledGet(void *el);
static int VectorsGet(void *el);
static int ContoursGet(void *el);
static int SystemGet(void *el);

static int LinesSet(void *el, int xyzChanged);
static int DisjointSet(void *el, int xyzChanged);
static int TextSet(void *el, int xyzChanged);
static int CellsSet(void *el, int xyzChanged);
static int PolysSet(void *el, int xyzChanged);
static void MeshXYSet(void *vMeshEl, int xyzChanged);
static int MeshSet(void *el, int xyzChanged);
static int FilledSet(void *el, int xyzChanged);
static int VectorsSet(void *el, int xyzChanged);
static int ContoursSet(void *el, int xyzChanged);
static int SystemSet(void *el, int xyzChanged);

static int LinesDraw(void *el, int xIsLog, int yIsLog);
static int DisjointDraw(void *el, int xIsLog, int yIsLog);
static int TextDraw(void *el, int xIsLog, int yIsLog);
static int CellsDraw(void *el, int xIsLog, int yIsLog);
static int PolysDraw(void *el, int xIsLog, int yIsLog);
static void MeshDrawSet(ga_mesh_t *mesh, void *vMeshEl,
                        int xIsLog, int yIsLog);
static int MeshDraw(void *el, int xIsLog, int yIsLog);
static int FilledDraw(void *el, int xIsLog, int yIsLog);
static int VectorsDraw(void *el, int xIsLog, int yIsLog);
static int ContoursDraw(void *el, int xIsLog, int yIsLog);
static int SystemDraw(void *el, int xIsLog, int yIsLog);

static int LinesScan(void *el, int flags, gp_box_t *limits);
static int DisjointScan(void *el, int flags, gp_box_t *limits);
static int TextScan(void *el, int flags, gp_box_t *limits);
static int CellsScan(void *el, int flags, gp_box_t *limits);
static int PolysScan(void *el, int flags, gp_box_t *limits);
static int MeshXYScan(void *vMeshEl, int flags, gp_box_t *limits, gp_box_t *box);
static int MeshScan(void *el, int flags, gp_box_t *limits);
static int FilledScan(void *el, int flags, gp_box_t *limits);
static int VectorsScan(void *el, int flags, gp_box_t *limits);
static int ContoursScan(void *el, int flags, gp_box_t *limits);
static int SystemScan(void *el, int flags, gp_box_t *limits);

static void NoMargin(void *el, gp_box_t *margin);
static void LinesMargin(void *el, gp_box_t *margin);
static void DisjointMargin(void *el, gp_box_t *margin);
static void TextMargin(void *el, gp_box_t *margin);
static void MeshMargin(void *el, gp_box_t *margin);
static void VectorsMargin(void *el, gp_box_t *margin);
static void ContoursMargin(void *el, gp_box_t *margin);

static int ScanMn(long n, gp_real_t *x, gp_real_t *y, gp_real_t ymin,
                  gp_real_t *xmin, gp_real_t *xmax);
static int ScanMx(long n, gp_real_t *x, gp_real_t *y, gp_real_t ymax,
                  gp_real_t *xmin, gp_real_t *xmax);
static int ScanMnMx(long n, gp_real_t *x, gp_real_t *y, gp_real_t ymin, gp_real_t ymax,
                    gp_real_t *xmin, gp_real_t *xmax);
static void ScanRXY(long n, gp_real_t *x, gp_real_t *y,
                    int flags, gp_box_t *limits, gp_box_t *box);
static int GetLogZ(long n, gp_real_t *z, gp_real_t **zlog,
                   gp_real_t *zmin, gp_real_t *zmax);
static int Get_LogZ(long n, long nndc, gp_real_t *z, gp_real_t **zlog,
                    gp_real_t *zmin, gp_real_t *zmax);

/* ------------------------------------------------------------------------ */
/* Set virtual function tables */

static gd_operations_t operations[GD_ELEM_SYSTEM + 1] = {
  { GD_ELEM_NONE, 0, 0, 0, 0, 0, 0 },
  { GD_ELEM_LINES, &LinesKill, &LinesGet, &LinesSet,
      &LinesDraw, &LinesScan, &LinesMargin },
  { GD_ELEM_DISJOINT, &DisjointKill, &DisjointGet, &DisjointSet,
      &DisjointDraw, &DisjointScan, &DisjointMargin },
  { GD_ELEM_TEXT, &TextKill, &TextGet, &TextSet,
      &TextDraw, &TextScan, &TextMargin },
  { GD_ELEM_MESH, &MeshKill, &MeshGet, &MeshSet,
      &MeshDraw, &MeshScan, &MeshMargin },
  { GD_ELEM_FILLED, &FilledKill, &FilledGet, &FilledSet,
      &FilledDraw, &FilledScan, &NoMargin },
  { GD_ELEM_VECTORS, &VectorsKill, &VectorsGet, &VectorsSet,
      &VectorsDraw, &VectorsScan, &VectorsMargin },
  { GD_ELEM_CONTOURS, &ContoursKill, &ContoursGet, &ContoursSet,
      &ContoursDraw, &ContoursScan, &ContoursMargin },
  { GD_ELEM_CELLS, &CellsKill, &CellsGet, &CellsSet,
      &CellsDraw, &CellsScan, &NoMargin },
  { GD_ELEM_POLYS, &PolysKill, &PolysGet, &PolysSet,
      &PolysDraw, &PolysScan, &NoMargin },
  { GD_ELEM_SYSTEM, &SystemKill, &SystemGet, &SystemSet,
      &SystemDraw, &SystemScan, &NoMargin }
};

/* this is called at the first call to gd_new_drawing */
gd_operations_t *_gd_get_drawing_operations(void)
{
  return operations;
}

/* ------------------------------------------------------------------------ */
/* Destructors for drawing elements are private, accessed via the
   Kill virtual function */

void _gd_kill_ring(void *elv)
{
  gd_element_t *el, *next= elv;
  while ((el= next)) {
    next= el->next;
    if (el == next) next= 0;
    el->ops->Kill(el);
  }
}

static void KillElement(gd_element_t *el)
{
  gd_element_t *next= el->next;
  if (el->legend) pl_free(el->legend);
  if (next) {
    if (next==el) next= 0;
    else { next->prev= el->prev;   el->prev->next= next; }
  }
  pl_free(el);
  return;
}

static void LinesKill(void *el)
{
  ge_lines_t *lines= el;
  if (lines->x) pl_free(lines->x);
  if (lines->y) pl_free(lines->y);
  if (lines->xlog) pl_free(lines->xlog);
  if (lines->ylog) pl_free(lines->ylog);
  KillElement(el);
}

static void DisjointKill(void *el)
{
  ge_disjoint_t *lines= el;
  if (lines->x) pl_free(lines->x);
  if (lines->y) pl_free(lines->y);
  if (lines->xlog) pl_free(lines->xlog);
  if (lines->ylog) pl_free(lines->ylog);
  if (lines->xq) pl_free(lines->xq);
  if (lines->yq) pl_free(lines->yq);
  if (lines->xqlog) pl_free(lines->xqlog);
  if (lines->yqlog) pl_free(lines->yqlog);
  KillElement(el);
}

static void TextKill(void *el)
{
  ge_text_t *text= el;
  if (text->text) pl_free(text->text);
  KillElement(el);
}

static void CellsKill(void *el)
{
  ge_cells_t *cells= el;
  if (cells->colors) pl_free(cells->colors);
  KillElement(el);
}

static void PolysKill(void *el)
{
  ge_polys_t *polys= el;
  if (polys->x) pl_free(polys->x);
  if (polys->y) pl_free(polys->y);
  if (polys->xlog) pl_free(polys->xlog);
  if (polys->ylog) pl_free(polys->ylog);
  if (polys->pn) pl_free(polys->pn);
  if (polys->colors) pl_free(polys->colors);
  KillElement(el);
}

static void MeshKill(void *el)
{
  _gd_kill_mesh_xy(el);
  KillElement(el);
}

static void FilledKill(void *el)
{
  ge_fill_t *fill= el;
  _gd_kill_mesh_xy(el);
  if (fill->colors) {
    if (!(fill->noCopy&GD_NOCOPY_COLORS)) pl_free(fill->colors);
    else if (gd_free) gd_free(fill->colors);
  }
  KillElement(el);
}

static void VectorsKill(void *el)
{
  ge_vectors_t *vec= el;
  _gd_kill_mesh_xy(el);
  if (!(vec->noCopy&GD_NOCOPY_UV)) {
    if (vec->u) pl_free(vec->u);
    if (vec->v) pl_free(vec->v);
  } else if (gd_free) {
    if (vec->u) gd_free(vec->u);
    if (vec->v) gd_free(vec->v);
  }
  KillElement(el);
}

static void KillConGrps(ge_lines_t **grps, int ngrps)
{
  int i;
  for (i=0 ; i<ngrps ; i++) { _gd_kill_ring(grps[i]);  grps[i]= 0; }
}

static void ContoursKill(void *el)
{
  ge_contours_t *con= el;
  _gd_kill_mesh_xy(el);
  if (con->z) {
    if (!(con->noCopy&GD_NOCOPY_Z)) pl_free(con->z);
    else if (gd_free) gd_free(con->z);
  }
  if (con->levels) pl_free(con->levels);
  if (con->groups) {
    KillConGrps(con->groups, con->nLevels);
    pl_free(con->groups);
  }
  KillElement(el);
}

static void SystemKill(void *el)
{
  ge_system_t *sys= el;
  _gd_kill_ring(sys->elements);
  KillElement(el);
}

void _gd_kill_mesh_xy(void *vMeshEl)
{
  ge_mesh_t *meshEl= vMeshEl;
  ga_mesh_t *mesh= &meshEl->mesh;
  int noCopy= meshEl->noCopy;
  if (!(noCopy&GD_NOCOPY_MESH)) {
    if (mesh->x) pl_free(mesh->x);
    if (mesh->y) pl_free(mesh->y);
  } else if (gd_free) {
    if (mesh->x) gd_free(mesh->x);
    if (mesh->y) gd_free(mesh->y);
  }
  if (mesh->reg) {
    if (!(noCopy&GD_NOCOPY_REG)) pl_free(mesh->reg);
    else if (gd_free) gd_free(mesh->reg);
  }
  if (mesh->triangle) {
    if (!(noCopy&GD_NOCOPY_TRI)) pl_free(mesh->triangle);
    else if (gd_free) gd_free(mesh->triangle);
  }
}

/* ------------------------------------------------------------------------ */
/* GetProps virtual function loads ga_attributes, gd_properties from
   gd_element_t */

static int LinesGet(void *el)
{
  ge_lines_t *e= el;
  gd_properties.hidden= e->el.hidden;
  gd_properties.legend= e->el.legend;
  gd_properties.n= e->n;
  gd_properties.x= e->x;
  gd_properties.y= e->y;
  ga_attributes.l= e->l;
  ga_attributes.dl= e->dl;
  ga_attributes.m= e->m;
  return GD_ELEM_LINES;
}

static int DisjointGet(void *el)
{
  ge_disjoint_t *e= el;
  gd_properties.hidden= e->el.hidden;
  gd_properties.legend= e->el.legend;
  gd_properties.n= e->n;
  gd_properties.x= e->x;
  gd_properties.y= e->y;
  gd_properties.xq= e->xq;
  gd_properties.yq= e->yq;
  ga_attributes.l= e->l;
  return GD_ELEM_DISJOINT;
}

static int TextGet(void *el)
{
  ge_text_t *e= el;
  gd_properties.hidden= e->el.hidden;
  gd_properties.legend= e->el.legend;
  gd_properties.x0= e->x0;
  gd_properties.y0= e->y0;
  gd_properties.text= e->text;
  ga_attributes.t= e->t;
  return GD_ELEM_TEXT;
}

static int CellsGet(void *el)
{
  ge_cells_t *e= el;
  gd_properties.hidden= e->el.hidden;
  gd_properties.legend= e->el.legend;
  gd_properties.px= e->px;
  gd_properties.py= e->py;
  gd_properties.qx= e->qx;
  gd_properties.qy= e->qy;
  gd_properties.width= e->width;
  gd_properties.height= e->height;
  gd_properties.colors= e->colors;
  ga_attributes.rgb = e->rgb;
  return GD_ELEM_CELLS;
}

static int PolysGet(void *el)
{
  ge_polys_t *e= el;
  gd_properties.hidden= e->el.hidden;
  gd_properties.legend= e->el.legend;
  gd_properties.n= e->n;
  gd_properties.x= e->x;
  gd_properties.y= e->y;
  gd_properties.pn= e->pn;
  gd_properties.colors= e->colors;
  ga_attributes.e = e->e;
  ga_attributes.rgb = e->rgb;
  return GD_ELEM_POLYS;
}

static int MeshGet(void *el)
{
  ge_mesh_t *e= el;
  _gd_get_mesh_xy(el);
  gd_properties.hidden= e->el.hidden;
  gd_properties.legend= e->el.legend;
  gd_properties.boundary= e->boundary;
  gd_properties.inhibit= e->inhibit;
  ga_attributes.l= e->l;
  return GD_ELEM_MESH;
}

static int FilledGet(void *el)
{
  ge_fill_t *e= el;
  _gd_get_mesh_xy(el);
  gd_properties.hidden= e->el.hidden;
  gd_properties.legend= e->el.legend;
  gd_properties.nColumns= e->nColumns;
  gd_properties.colors= e->colors;
  ga_attributes.e= e->e;
  ga_attributes.rgb = e->rgb;
  return GD_ELEM_FILLED;
}

static int VectorsGet(void *el)
{
  ge_vectors_t *e= el;
  _gd_get_mesh_xy(el);
  gd_properties.hidden= e->el.hidden;
  gd_properties.legend= e->el.legend;
  gd_properties.u= e->u;
  gd_properties.v= e->v;
  gd_properties.scale= e->scale;
  ga_attributes.l= e->l;
  ga_attributes.f= e->f;
  ga_attributes.vect= e->vect;
  return GD_ELEM_VECTORS;
}

static int ContoursGet(void *el)
{
  ge_contours_t *e= el;
  _gd_get_mesh_xy(el);
  gd_properties.hidden= e->el.hidden;
  gd_properties.legend= e->el.legend;
  gd_properties.z= e->z;
  gd_properties.nLevels= e->nLevels;
  gd_properties.levels= e->levels;
  ga_attributes.l= e->l;
  ga_attributes.dl= e->dl;
  ga_attributes.m= e->m;
  return GD_ELEM_CONTOURS;
}

static int SystemGet(void *el)
{
  ge_system_t *e= el;
  gd_properties.hidden= e->el.hidden;
  gd_properties.legend= e->el.legend;
  return GD_ELEM_SYSTEM;
}

void _gd_get_mesh_xy(void *vMeshEl)
{
  ge_mesh_t *meshEl= vMeshEl;
  ga_mesh_t *mesh= &meshEl->mesh;
  gd_properties.noCopy= meshEl->noCopy;
  gd_properties.mesh.iMax= mesh->iMax;
  gd_properties.mesh.jMax= mesh->jMax;
  gd_properties.mesh.x= mesh->x;
  gd_properties.mesh.y= mesh->y;
  gd_properties.mesh.reg= mesh->reg;
  gd_properties.mesh.triangle= mesh->triangle;
  gd_properties.region= meshEl->region;
}

/* ------------------------------------------------------------------------ */
/* SetProps virtual function loads gd_element_t from ga_attributes, gd_properties */

static int LinesSet(void *el, int xyzChanged)
{
  ge_lines_t *e= el;
  _gd_lines_subset(el);
  e->el.legend= gd_properties.legend;
  if (xyzChanged & GD_CHANGE_XY) {
    e->n= gd_properties.n;
    e->x= gd_properties.x;
    e->y= gd_properties.y;
    if (e->xlog) { pl_free(e->xlog); e->xlog= 0; }
    if (e->ylog) { pl_free(e->ylog); e->ylog= 0; }
  }
  return 0;
}

void _gd_lines_subset(void *el)
{
  ge_lines_t* e = el;
  e->el.hidden = gd_properties.hidden;
  e->l  = ga_attributes.l;
  e->dl = ga_attributes.dl;
  e->m  = ga_attributes.m;
}

static int DisjointSet(void *el, int xyzChanged)
{
  ge_disjoint_t *e= el;
  e->el.hidden= gd_properties.hidden;
  e->el.legend= gd_properties.legend;
  e->n= gd_properties.n;
  e->x= gd_properties.x;
  e->y= gd_properties.y;
  e->xq= gd_properties.xq;
  e->yq= gd_properties.yq;
  e->l= ga_attributes.l;
  if (xyzChanged & GD_CHANGE_XY) {
    if (e->xlog) { pl_free(e->xlog); e->xlog= 0; }
    if (e->ylog) { pl_free(e->ylog); e->ylog= 0; }
    if (e->xqlog) { pl_free(e->xqlog); e->xqlog= 0; }
    if (e->yqlog) { pl_free(e->yqlog); e->yqlog= 0; }
  }
  return 0;
}

/* ARGSUSED */
static int TextSet(void *el, int xyzChanged)
{
  ge_text_t *e= el;
  e->el.hidden= gd_properties.hidden;
  e->el.legend= gd_properties.legend;
  e->x0= gd_properties.x0;
  e->y0= gd_properties.y0;
  e->text= gd_properties.text;
  e->t= ga_attributes.t;
  return 0;
}

/* ARGSUSED */
static int CellsSet(void *el, int xyzChanged)
{
  ge_cells_t *e= el;
  e->el.hidden= gd_properties.hidden;
  e->el.legend= gd_properties.legend;
  e->px= gd_properties.px;
  e->py= gd_properties.py;
  e->qx= gd_properties.qx;
  e->qy= gd_properties.qy;
  e->width= gd_properties.width;
  e->height= gd_properties.height;
  e->colors= gd_properties.colors;
  return 0;
}

static int PolysSet(void *el, int xyzChanged)
{
  ge_polys_t *e= el;
  _gd_lines_subset(el);
  e->el.legend= gd_properties.legend;
  if (xyzChanged & GD_CHANGE_XY) {
    e->n= gd_properties.n;
    e->x= gd_properties.x;
    e->y= gd_properties.y;
    if (e->xlog) { pl_free(e->xlog); e->xlog= 0; }
    if (e->ylog) { pl_free(e->ylog); e->ylog= 0; }
  }
  e->pn= gd_properties.pn;
  e->colors= gd_properties.colors;
  return 0;
}

static void MeshXYSet(void *vMeshEl, int xyzChanged)
{
  ge_mesh_t *meshEl= vMeshEl;
  ga_mesh_t *mesh= &meshEl->mesh;
  meshEl->el.legend= gd_properties.legend;
  meshEl->el.hidden= gd_properties.hidden;
  meshEl->noCopy= gd_properties.noCopy;
  mesh->iMax= gd_properties.mesh.iMax;
  mesh->jMax= gd_properties.mesh.jMax;
  mesh->x= gd_properties.mesh.x;
  mesh->y= gd_properties.mesh.y;
  mesh->reg= gd_properties.mesh.reg;
  mesh->triangle= gd_properties.mesh.triangle;
  if (xyzChanged & GD_CHANGE_XY) {
    if (meshEl->xlog) { pl_free(meshEl->xlog); meshEl->xlog= 0; }
    if (meshEl->ylog) { pl_free(meshEl->ylog); meshEl->ylog= 0; }
  }
  meshEl->region= gd_properties.region;
}

static int MeshSet(void *el, int xyzChanged)
{
  ge_mesh_t *e= el;
  MeshXYSet(el, xyzChanged);
  e->boundary= gd_properties.boundary;
  e->inhibit= gd_properties.inhibit;
  e->l= ga_attributes.l;
  return 0;
}

static int FilledSet(void *el, int xyzChanged)
{
  ge_fill_t *e= el;
  MeshXYSet(el, xyzChanged);
  e->nColumns= gd_properties.nColumns;
  e->colors= gd_properties.colors;
  e->e= ga_attributes.e;
  return 0;
}

static int VectorsSet(void *el, int xyzChanged)
{
  ge_vectors_t *e= el;
  MeshXYSet(el, xyzChanged);
  e->u= gd_properties.u;
  e->v= gd_properties.v;
  e->scale= gd_properties.scale;
  e->l= ga_attributes.l;
  e->f= ga_attributes.f;
  e->vect= ga_attributes.vect;
  return 0;
}

static int ContoursSet(void *el, int xyzChanged)
{
  ge_contours_t *e= el;
  int oldN= e->nLevels;
  MeshXYSet(el, xyzChanged);
  e->z= gd_properties.z;
  e->nLevels= gd_properties.nLevels;
  e->levels= gd_properties.levels;
  e->l= ga_attributes.l;
  e->dl= ga_attributes.dl;
  e->m= ga_attributes.m;
  if (xyzChanged & GD_CHANGE_Z) {
    if (e->groups) {
      KillConGrps(e->groups, oldN);
      if (oldN!=gd_properties.nLevels) {
        pl_free(e->groups);
        e->groups= 0;
      }
    }
    if (gd_properties.nLevels>0) {
      if (!e->groups)
        e->groups= (ge_lines_t **)pl_malloc(sizeof(ge_lines_t *)*gd_properties.nLevels);
      if (!e->groups || _gd_make_contours(e)) return 1;
    }
  }
  return 0;
}

/* ARGSUSED */
static int SystemSet(void *el, int xyzChanged)
{
  ge_system_t *e= el;
  e->el.hidden= gd_properties.hidden;
  e->el.legend= gd_properties.legend;
  return 0;
}

/* ------------------------------------------------------------------------ */
/* Draw virtual function calls Gp and Ga level rendering routines */

static int LinesDraw(void *el, int xIsLog, int yIsLog)
{
  ge_lines_t *e= el;
  gp_real_t *px= xIsLog? e->xlog : e->x;
  gp_real_t *py= yIsLog? e->ylog : e->y;
  long n= e->n;
  if (e->el.hidden || n<=0) return 0;
  ga_attributes.l= e->l;
  ga_attributes.dl= e->dl;
  ga_attributes.m= e->m;
  return ga_draw_lines(n, px, py);
}

static int DisjointDraw(void *el, int xIsLog, int yIsLog)
{
  ge_disjoint_t *e= el;
  gp_real_t *px= xIsLog? e->xlog : e->x;
  gp_real_t *py= yIsLog? e->ylog : e->y;
  gp_real_t *qx= xIsLog? e->xqlog : e->xq;
  gp_real_t *qy= yIsLog? e->yqlog : e->yq;
  long n= e->n;
  if (e->el.hidden || n<=0) return 0;
  ga_attributes.l= e->l;
  return gp_draw_disjoint(n, px, py, qx, qy);
}

static int TextDraw(void *el, int xIsLog, int yIsLog)
{
  ge_text_t *e= el;
  gp_real_t x0, y0;
  if (e->el.hidden || !e->text) return 0;
  x0= xIsLog? SAFELOG(e->x0) : e->x0;
  y0= yIsLog? SAFELOG(e->y0) : e->y0;
  ga_attributes.t= e->t;
  return gp_draw_text(x0, y0, e->text);
}

static int CellsDraw(void *el, int xIsLog, int yIsLog)
{
  ge_cells_t *e= el;
  gp_real_t px, py, qx, qy;
  int value;
  if (e->el.hidden) return 0;
  px= xIsLog? SAFELOG(e->px) : e->px;
  py= yIsLog? SAFELOG(e->py) : e->py;
  qx= xIsLog? SAFELOG(e->qx) : e->qx;
  qy= yIsLog? SAFELOG(e->qy) : e->qy;
  ga_attributes.rgb = e->rgb;
  value = gp_draw_cells(px, py, qx, qy, e->width, e->height, e->width, e->colors);
  ga_attributes.rgb = 0;  /* modest attempt at backward compatibility */
  return value;
}

static int PolysDraw(void *el, int xIsLog, int yIsLog)
{
  int result= 0;
  ge_polys_t *e= el;
  gp_real_t *px= xIsLog? e->xlog : e->x;
  gp_real_t *py= yIsLog? e->ylog : e->y;
  gp_color_t *colors= e->colors;
  int rgb = colors? e->rgb : 0;
  long n= e->n;
  long *pn= e->pn;
  long i;
  if (e->el.hidden || n<=0) return 0;
  ga_attributes.e= e->e;
  if (!colors) ga_attributes.f.color =  PL_BG;
  if (n<2 || pn[1]>1) {
    long j = 0;
    for (i=0 ; i<n ; i++) {
      if (rgb)
        ga_attributes.f.color=PL_RGB(colors[j],colors[j+1],colors[j+2]), j+=3;
      else if (colors)
        ga_attributes.f.color = colors[i];
      result|= gp_draw_fill(pn[i], px, py);
      px+= pn[i];
      py+= pn[i];
    }
  } else {
    long j = 3;
    long i0= pn[0]-1;
    for (i=1 ; i<n ; i++) {
      if (rgb)
        ga_attributes.f.color=PL_RGB(colors[j],colors[j+1],colors[j+2]), j+=3;
      else if (colors)
        ga_attributes.f.color = colors[i];
      result|= ga_fill_marker(pn[0], px, py, px[i0+i], py[i0+i]);
    }
  }
  return result;
}

static void MeshDrawSet(ga_mesh_t *mesh, void *vMeshEl,
                        int xIsLog, int yIsLog)
{
  ge_mesh_t *meshEl= vMeshEl;
  ga_mesh_t *msh= &meshEl->mesh;
  gp_real_t *x= xIsLog? meshEl->xlog : msh->x;
  gp_real_t *y= yIsLog? meshEl->ylog : msh->y;
  mesh->iMax= msh->iMax;
  mesh->jMax= msh->jMax;
  mesh->x= x;
  mesh->y= y;
  mesh->reg= msh->reg;
  mesh->triangle= msh->triangle;
}

static int MeshDraw(void *el, int xIsLog, int yIsLog)
{
  ge_mesh_t *e= el;
  ga_mesh_t mesh;
  if (e->el.hidden) return 0;
  MeshDrawSet(&mesh, el, xIsLog, yIsLog);
  ga_attributes.l= e->l;
  return ga_draw_mesh(&mesh, e->region, e->boundary, e->inhibit);
}

static int FilledDraw(void *el, int xIsLog, int yIsLog)
{
  ge_fill_t *e= el;
  ga_mesh_t mesh;
  if (e->el.hidden) return 0;
  MeshDrawSet(&mesh, el, xIsLog, yIsLog);
  ga_attributes.e= e->e;
  ga_attributes.rgb = e->rgb;
  return ga_fill_mesh(&mesh, e->region, e->colors, e->nColumns);
}

static int VectorsDraw(void *el, int xIsLog, int yIsLog)
{
  ge_vectors_t *e= el;
  ga_mesh_t mesh;
  if (e->el.hidden) return 0;
  MeshDrawSet(&mesh, el, xIsLog, yIsLog);
  ga_attributes.l= e->l;
  ga_attributes.f= e->f;
  ga_attributes.vect= e->vect;
  return ga_draw_vectors(&mesh, e->region, e->u, e->v, e->scale);
}

static int ContoursDraw(void *el, int xIsLog, int yIsLog)
{
  ge_contours_t *e= el;
  int nLevels= e->nLevels;
  ge_lines_t **groups= e->groups;
  int value= 0;
  if (e->el.hidden || nLevels<=0) return 0;
  if (!groups) return 1;
  while (nLevels--) value|= _gd_draw_ring(*groups++, xIsLog, yIsLog, 0, 1);
  return value;
}

static int SystemDraw(void *el, int xIsLog, int yIsLog)
     /* NOTE: xIsLog input is used in a very non-standard way as the
        index of this system.  This is possible since xIsLog and yIsLog
        are otherwise meaningless in SystemDraw.  */
{
  ge_system_t *e= el;
  int vflags, hflags= e->flags;
  int systemCounter= xIsLog;   /* Yes, this is non-standard usage */
  gp_box_t port, *tickIn;
  if (e->el.hidden || !e->elements) return 0;

  xIsLog= hflags & GD_LIM_LOGX;
  yIsLog= hflags & GD_LIM_LOGY;
  gp_set_transform(&e->trans);

  /* In order to prevent needless GaTick calls, a special feature
     is built into _gd_begin_sy.  */
  hflags= e->ticks.horiz.flags;
  vflags= e->ticks.vert.flags;
  if (vflags & GA_TICK_C || hflags & GA_TICK_C) tickIn= 0;
  else {
    gp_real_t tlen= e->ticks.vert.tickLen[0];
    gp_real_t twid= 0.5*e->ticks.vert.tickStyle.width*GP_DEFAULT_LINE_WIDTH;
    tlen= (vflags&GA_TICK_IN)? ((vflags&GA_TICK_OUT)? 0.5 : 1.0)*tlen : 0.0;

    tickIn= &port;
    port= e->trans.viewport;
    if (vflags & GA_TICK_L) port.xmin-= e->ticks.vert.tickOff + tlen + twid;
    if (vflags & GA_TICK_U) port.xmax+= e->ticks.vert.tickOff - tlen - twid;

    tlen= e->ticks.horiz.tickLen[0];
    twid= 0.5*e->ticks.horiz.tickStyle.width*GP_DEFAULT_LINE_WIDTH;
    tlen= (hflags&GA_TICK_IN)? ((hflags&GA_TICK_OUT)? 0.5 : 1.0)*tlen : 0.0;
    if (hflags & GA_TICK_L) port.ymin-= e->ticks.horiz.tickOff + tlen + twid;
    if (hflags & GA_TICK_U) port.ymax+= e->ticks.horiz.tickOff - tlen - twid;
  }

  hflags= _gd_begin_sy(&e->el.box, tickIn, &e->trans.viewport,
                    e->el.number, systemCounter);

  /* Draw the elements for this coordinate system before the ticks.  */
  gp_clipping= 1;   /* turn on clipping for elements */
  if (hflags & 1) _gd_draw_ring(e->elements, xIsLog, yIsLog, e, 0);

  /* Draw tick marks on top of elements.  If the user has chosen a style
     where the ticks overlap the viewport, he probably wants the ticks
     to obstruct his data anyway. */
  gp_clipping= 0;   /* turn off clipping for ticks */
  if (hflags & 2) ga_draw_alt_ticks(&e->ticks, xIsLog, yIsLog,
                            e->xtick, e->xlabel, e->ytick, e->ylabel);
  return 0;
}

/* ------------------------------------------------------------------------ */
/* Scan virtual function gets logs if reqd, sets box, scans xy  */

static int ScanMn(long n, gp_real_t *x, gp_real_t *y, gp_real_t ymin,
                  gp_real_t *xmin, gp_real_t *xmax)
{
  gp_real_t xn, xx;
  long i;
  for (i=0 ; i<n ; i++) if (y[i]>=ymin) break;
  if (i>=n) return 0;
  xn= xx= x[i++];
  for ( ; i<n ; i++) if (y[i]>=ymin) {
    if (x[i]<xn) xn= x[i];
    else if (x[i]>xx) xx= x[i];
  }
  *xmin= xn;
  *xmax= xx;
  return 1;
}

static int ScanMx(long n, gp_real_t *x, gp_real_t *y, gp_real_t ymax,
                  gp_real_t *xmin, gp_real_t *xmax)
{
  gp_real_t xn, xx;
  long i;
  for (i=0 ; i<n ; i++) if (y[i]<=ymax) break;
  if (i>=n) return 0;
  xn= xx= x[i++];
  for ( ; i<n ; i++) if (y[i]<=ymax) {
    if (x[i]<xn) xn= x[i];
    else if (x[i]>xx) xx= x[i];
  }
  *xmin= xn;
  *xmax= xx;
  return 1;
}

static int ScanMnMx(long n, gp_real_t *x, gp_real_t *y, gp_real_t ymin, gp_real_t ymax,
                    gp_real_t *xmin, gp_real_t *xmax)
{
  gp_real_t xn, xx;
  long i;
  for (i=0 ; i<n ; i++) if (y[i]>=ymin && y[i]<=ymax) break;
  if (i>=n) return 0;
  xn= xx= x[i++];
  for ( ; i<n ; i++) if (y[i]>=ymin && y[i]<=ymax) {
    if (x[i]<xn) xn= x[i];
    else if (x[i]>xx) xx= x[i];
  }
  *xmin= xn;
  *xmax= xx;
  return 1;
}

static void ScanRXY(long n, gp_real_t *x, gp_real_t *y,
                    int flags, gp_box_t *limits, gp_box_t *box)
{
  int dxmin= flags & GD_LIM_XMIN,  dxmax= flags & GD_LIM_XMAX;
  int dymin= flags & GD_LIM_YMIN,  dymax= flags & GD_LIM_YMAX;
  int any;

  if (dxmin || dxmax) {
    gp_real_t xmin, xmax;
    if (dymin) {
      if (dymax) { xmin= box->xmin;  xmax= box->xmax; any= 1; }
      else if (box->ymin>limits->ymax) any= 0;
      else any= ScanMx(n, x, y, limits->ymax, &xmin, &xmax);
    } else if (box->ymax<limits->ymin) {
      any= 0;
    } else {
      if (dymax) any= ScanMn(n, x, y, limits->ymin, &xmin, &xmax);
      else if (box->ymin>limits->ymax) any= 0;
      else any= ScanMnMx(n, x, y, limits->ymin, limits->ymax, &xmin, &xmax);
    }
    if (any) {
      if (dxmin) limits->xmin= xmin;
      if (dxmax) limits->xmax= xmax;
    } else {  /* gd_scan requires min>max if no curves visible */
      if (dxmin) {
        if (dxmax) limits->xmax= 0.0;
        if (limits->xmax>0.0) limits->xmin= 1.1*limits->xmax;
        else limits->xmin= 0.9*limits->xmax+1.0;
      } else { /* dxmax is set */
        if (limits->xmin>0.0) limits->xmax= 0.9*limits->xmin;
        else limits->xmax= 1.1*limits->xmin-1.0;
      }
    }
  }
  if (dymin || dymax) {
    gp_real_t ymin, ymax;
    if (dxmin) {
      if (dxmax) { ymin= box->ymin;  ymax= box->ymax; any= 1; }
      else if (box->xmin>limits->xmax) any= 0;
      else any= ScanMx(n, y, x, limits->xmax, &ymin, &ymax);
    } else if (box->xmax<limits->xmin) {
      any= 0;
    } else {
      if (dxmax) any= ScanMn(n, y, x, limits->xmin, &ymin, &ymax);
      else if (box->xmin>limits->xmax) any= 0;
      else any= ScanMnMx(n, y, x, limits->xmin, limits->xmax, &ymin, &ymax);
    }
    if (any) {
      if (dymin) limits->ymin= ymin;
      if (dymax) limits->ymax= ymax;
    } else {  /* gd_scan requires min>max if no curves visible */
      if (dymin) {
        if (dymax) limits->ymax= 0.0;
        if (limits->ymax>0.0) limits->ymin= 1.1*limits->ymax;
        else limits->ymin= 0.9*limits->ymax+1.0;
      } else { /* dymax is set */
        if (limits->ymin>0.0) limits->ymax= 0.9*limits->ymin;
        else limits->ymax= 1.1*limits->ymin-1.0;
      }
    }
  }
}

static int GetLogZ(long n, gp_real_t *z, gp_real_t **zlog,
                   gp_real_t *zmin, gp_real_t *zmax)
{
  gp_real_t *zl= (gp_real_t *)pl_malloc(sizeof(gp_real_t)*n);
  *zlog= zl;
  if (zl) {
    long i;
    for (i=0 ; i<n ; i++) zl[i]= SAFELOG(z[i]);
    if (zmin) _gd_scan_z(n, zl, zmin, zmax);
  } else {
    strcpy(gp_error, "memory manager failed in Gd log function");
    return -1;
  }
  return 0;
}

static int Get_LogZ(long n, long nndc, gp_real_t *z, gp_real_t **zlog,
                    gp_real_t *zmin, gp_real_t *zmax)
{
  gp_real_t *zl= (gp_real_t *)pl_malloc(sizeof(gp_real_t)*n);
  *zlog= zl;
  if (zl) {
    long i;
    for (i=0 ; i<nndc ; i++) zl[i]= z[i];
    for ( ; i<n ; i++) zl[i]= SAFELOG(z[i]);
    if (zmin) _gd_scan_z(n-nndc, zl+nndc, zmin, zmax);
  } else {
    strcpy(gp_error, "memory manager failed in Gd_log function");
    return -1;
  }
  return 0;
}

static int LinesScan(void *el, int flags, gp_box_t *limits)
{
  ge_lines_t *e= el;
  gp_real_t *x, *y;

  /* First, get log values if necessary, and set box */
  if (flags & GD_LIM_LOGX) {
    if (!e->xlog && GetLogZ(e->n, e->x, &e->xlog,
                            &e->logBox.xmin, &e->logBox.xmax)) return 1;
    e->el.box.xmin= e->logBox.xmin;
    e->el.box.xmax= e->logBox.xmax;
    x= e->xlog;
  } else {
    e->el.box.xmin= e->linBox.xmin;
    e->el.box.xmax= e->linBox.xmax;
    x= e->x;
  }
  if (flags & GD_LIM_LOGY) {
    if (!e->ylog && GetLogZ(e->n, e->y, &e->ylog,
                            &e->logBox.ymin, &e->logBox.ymax)) return 1;
    e->el.box.ymin= e->logBox.ymin;
    e->el.box.ymax= e->logBox.ymax;
    y= e->ylog;
  } else {
    e->el.box.ymin= e->linBox.ymin;
    e->el.box.ymax= e->linBox.ymax;
    y= e->y;
  }

  if (flags & GD_LIM_RESTRICT) {
    /* Scan points, restricting x limits to lie within fixed y limits
       and vice-versa.  Assume that limits.min<limits.max.  */
    ScanRXY(e->n, x, y, flags, limits, &e->el.box);
  } else {
    /* Unrestricted limits are either fixed or same as bounding box.  */
    if (flags & GD_LIM_XMIN) limits->xmin= e->el.box.xmin;
    if (flags & GD_LIM_XMAX) limits->xmax= e->el.box.xmax;
    if (flags & GD_LIM_YMIN) limits->ymin= e->el.box.ymin;
    if (flags & GD_LIM_YMAX) limits->ymax= e->el.box.ymax;
  }

  return 0;
}

static int DisjointScan(void *el, int flags, gp_box_t *limits)
{
  ge_disjoint_t *e= el;
  gp_real_t *x, *y, *xq, *yq;
  gp_real_t xymin, xymax;

  /* First, get log values if necessary, and set box */
  if (flags & GD_LIM_LOGX) {
    if (!e->xlog && GetLogZ(e->n, e->x, &e->xlog,
                            &e->logBox.xmin, &e->logBox.xmax)) return 1;
    e->el.box.xmin= e->logBox.xmin;
    e->el.box.xmax= e->logBox.xmax;
    x= e->xlog;
    if (!e->xqlog) {
      if (GetLogZ(e->n, e->xq, &e->xqlog, &xymin, &xymax)) return 1;
      if (xymin<e->el.box.xmin) e->el.box.xmin= e->logBox.xmin;
      if (xymax>e->el.box.xmax) e->el.box.xmax= e->logBox.xmax;
    }
    xq= e->xqlog;
  } else {
    e->el.box.xmin= e->linBox.xmin;
    e->el.box.xmax= e->linBox.xmax;
    x= e->x;
    xq= e->xq;
  }
  if (flags & GD_LIM_LOGY) {
    if (!e->ylog && GetLogZ(e->n, e->y, &e->ylog,
                            &e->logBox.ymin, &e->logBox.ymax)) return 1;
    e->el.box.ymin= e->logBox.ymin;
    e->el.box.ymax= e->logBox.ymax;
    y= e->ylog;
    if (!e->yqlog) {
      if (GetLogZ(e->n, e->yq, &e->yqlog, &xymin, &xymax)) return 1;
      if (xymin<e->el.box.ymin) e->el.box.ymin= e->logBox.ymin;
      if (xymax>e->el.box.ymax) e->el.box.ymax= e->logBox.ymax;
    }
    yq= e->yqlog;
  } else {
    e->el.box.ymin= e->linBox.ymin;
    e->el.box.ymax= e->linBox.ymax;
    y= e->y;
    yq= e->yq;
  }

  if (flags & GD_LIM_RESTRICT) {
    /* Scan points, restricting x limits to lie within fixed y limits
       and vice-versa.  Assume that limits.min<limits.max.  */
    gp_box_t box;
    ScanRXY(e->n, x, y, flags, limits, &e->el.box);
    ScanRXY(e->n, xq, yq, flags, &box, &e->el.box);
    gp_swallow(limits, &box);
  } else {
    /* Unrestricted limits are either fixed or same as bounding box.  */
    if (flags & GD_LIM_XMIN) limits->xmin= e->el.box.xmin;
    if (flags & GD_LIM_XMAX) limits->xmax= e->el.box.xmax;
    if (flags & GD_LIM_YMIN) limits->ymin= e->el.box.ymin;
    if (flags & GD_LIM_YMAX) limits->ymax= e->el.box.ymax;
  }

  return 0;
}

static int TextScan(void *el, int flags, gp_box_t *limits)
{
  ge_text_t *e= el;
  gp_real_t x0= e->x0;
  gp_real_t y0= e->y0;

  if (flags & GD_LIM_LOGX) x0= SAFELOG(x0);
  if (flags & GD_LIM_LOGY) y0= SAFELOG(y0);

  if (flags & GD_LIM_XMIN) limits->xmin= x0;
  if (flags & GD_LIM_XMAX) limits->xmax= x0;
  if (flags & GD_LIM_YMIN) limits->ymin= y0;
  if (flags & GD_LIM_YMAX) limits->ymax= y0;
  return 0;
}

static int CellsScan(void *el, int flags, gp_box_t *limits)
{
  ge_cells_t *e= el;
  gp_real_t x[2], y[2];

  if (e->px<e->qx) { x[0]= e->px;  x[1]= e->qx; }
  else { x[0]= e->qx;  x[1]= e->px; }
  if (e->py<e->qy) { y[0]= e->py;  y[1]= e->qy; }
  else { y[0]= e->qy;  y[1]= e->py; }

  /* First, get log values if necessary, and set box */
  if (flags & GD_LIM_LOGX) {
    e->el.box.xmin= SAFELOG(x[0]);
    e->el.box.xmax= SAFELOG(x[1]);
  } else {
    e->el.box.xmin= x[0];
    e->el.box.xmax= x[1];
  }
  if (flags & GD_LIM_LOGY) {
    e->el.box.ymin= SAFELOG(y[0]);
    e->el.box.ymax= SAFELOG(y[1]);
  } else {
    e->el.box.ymin= y[0];
    e->el.box.ymax= y[1];
  }

  if (flags & GD_LIM_XMIN) limits->xmin= e->el.box.xmin;
  if (flags & GD_LIM_XMAX) limits->xmax= e->el.box.xmax;
  if (flags & GD_LIM_YMIN) limits->ymin= e->el.box.ymin;
  if (flags & GD_LIM_YMAX) limits->ymax= e->el.box.ymax;

  return 0;
}

static int PolysScan(void *el, int flags, gp_box_t *limits)
{
  ge_polys_t *e= el;
  gp_real_t *x, *y;
  long i, ntot= 0;
  long nndc= (e->n<2 || e->pn[1]>1)? 0 : e->pn[0];

  /* compute total number of points */
  for (i=0 ; i<e->n ; i++) ntot+= e->pn[i];

  /* First, get log values if necessary, and set box */
  if (flags & GD_LIM_LOGX) {
    if (!e->xlog && Get_LogZ(ntot, nndc, e->x, &e->xlog,
                             &e->logBox.xmin, &e->logBox.xmax)) return 1;
    e->el.box.xmin= e->logBox.xmin;
    e->el.box.xmax= e->logBox.xmax;
    x= e->xlog;
  } else {
    e->el.box.xmin= e->linBox.xmin;
    e->el.box.xmax= e->linBox.xmax;
    x= e->x;
  }
  if (flags & GD_LIM_LOGY) {
    if (!e->ylog && Get_LogZ(ntot, nndc, e->y, &e->ylog,
                             &e->logBox.ymin, &e->logBox.ymax)) return 1;
    e->el.box.ymin= e->logBox.ymin;
    e->el.box.ymax= e->logBox.ymax;
    y= e->ylog;
  } else {
    e->el.box.ymin= e->linBox.ymin;
    e->el.box.ymax= e->linBox.ymax;
    y= e->y;
  }

  if (flags & GD_LIM_RESTRICT) {
    /* Scan points, restricting x limits to lie within fixed y limits
       and vice-versa.  Assume that limits.min<limits.max.  */
    ScanRXY(ntot-nndc, x+nndc, y+nndc, flags, limits, &e->el.box);
  } else {
    /* Unrestricted limits are either fixed or same as bounding box.  */
    if (flags & GD_LIM_XMIN) limits->xmin= e->el.box.xmin;
    if (flags & GD_LIM_XMAX) limits->xmax= e->el.box.xmax;
    if (flags & GD_LIM_YMIN) limits->ymin= e->el.box.ymin;
    if (flags & GD_LIM_YMAX) limits->ymax= e->el.box.ymax;
  }

  return 0;
}

static int MeshXYScan(void *vMeshEl, int flags, gp_box_t *limits, gp_box_t *box)
{
  ge_mesh_t *meshEl= vMeshEl;
  ga_mesh_t *mesh= &meshEl->mesh;
  gp_real_t *x, *y;

  /* First, get log values if necessary, and set box */
  if (flags & GD_LIM_LOGX) {
    long len= mesh->iMax*mesh->jMax;
    int region= meshEl->region;
    gp_real_t xmin, xmax;
    long i, j, iMax= mesh->iMax;
    int *reg= mesh->reg, first= 1;

    if (!meshEl->xlog && GetLogZ(len, mesh->x, &meshEl->xlog, 0, 0))
      return 1;
    for (i=0 ; i<len ; ) {
      _gd_next_mesh_block(&i, &j, len, iMax, reg, region);
      if (i>=len) break;
      _gd_scan_z(j-i, meshEl->xlog+i, &xmin, &xmax);
      if (first) {
        meshEl->logBox.xmin= xmin;
        meshEl->logBox.xmax= xmax;
        first= 0;
      } else {
        if (xmin<meshEl->logBox.xmin) meshEl->logBox.xmin= xmin;
        if (xmax>meshEl->logBox.xmax) meshEl->logBox.xmax= xmax;
      }
      i= j+1;
    }
    box->xmin= meshEl->logBox.xmin;
    box->xmax= meshEl->logBox.xmax;
    x= meshEl->xlog;
  } else {
    box->xmin= meshEl->linBox.xmin;
    box->xmax= meshEl->linBox.xmax;
    x= mesh->x;
  }
  if (flags & GD_LIM_LOGY) {
    long len= mesh->iMax*mesh->jMax;
    int region= meshEl->region;
    gp_real_t ymin, ymax;
    long i, j, iMax= mesh->iMax;
    int *reg= mesh->reg, first= 1;

    if (!meshEl->ylog && GetLogZ(len, mesh->y, &meshEl->ylog, 0, 0))
      return 1;
    for (i=0 ; i<len ; ) {
      _gd_next_mesh_block(&i, &j, len, iMax, reg, region);
      if (i>=len) break;
      _gd_scan_z(j-i, meshEl->ylog+i, &ymin, &ymax);
      if (first) {
        meshEl->logBox.ymin= ymin;
        meshEl->logBox.ymax= ymax;
        first= 0;
      } else {
        if (ymin<meshEl->logBox.ymin) meshEl->logBox.ymin= ymin;
        if (ymax>meshEl->logBox.ymax) meshEl->logBox.ymax= ymax;
      }
      i= j+1;
    }
    box->ymin= meshEl->logBox.ymin;
    box->ymax= meshEl->logBox.ymax;
    y= meshEl->ylog;
  } else {
    box->ymin= meshEl->linBox.ymin;
    box->ymax= meshEl->linBox.ymax;
    y= mesh->y;
  }

  if (flags & GD_LIM_RESTRICT) {
    /* Scan points, restricting x limits to lie within fixed y limits
       and vice-versa.  Assume that limits.min<limits.max.  */
    long len= mesh->iMax*mesh->jMax;
    int region= meshEl->region;
    gp_box_t tbox;
    long i, j, iMax= mesh->iMax;
    int *reg= mesh->reg, first= 1;
    tbox= *limits;
    for (i=0 ; i<len ; ) {
      _gd_next_mesh_block(&i, &j, len, iMax, reg, region);
      if (i>=len) break;
      ScanRXY(j-i, x+i, y+i, flags, limits, &tbox);
      if (first) { *box= tbox;  first= 0; }
      else gp_swallow(box, &tbox);
      i= j+1;
    }
  } else {
    /* Unrestricted limits are either fixed or same as bounding box.  */
    if (flags & GD_LIM_XMIN) limits->xmin= box->xmin;
    if (flags & GD_LIM_XMAX) limits->xmax= box->xmax;
    if (flags & GD_LIM_YMIN) limits->ymin= box->ymin;
    if (flags & GD_LIM_YMAX) limits->ymax= box->ymax;
  }

  return 0;
}

static int MeshScan(void *el, int flags, gp_box_t *limits)
{
  ge_mesh_t *e= el;
  return MeshXYScan(el, flags, limits, &e->el.box);
}

static int FilledScan(void *el, int flags, gp_box_t *limits)
{
  ge_fill_t *e= el;
  return MeshXYScan(el, flags, limits, &e->el.box);
}

static int VectorsScan(void *el, int flags, gp_box_t *limits)
{
  ge_vectors_t *e= el;
  return MeshXYScan(el, flags, limits, &e->el.box);
}

static int ContoursScan(void *el, int flags, gp_box_t *limits)
{
  ge_contours_t *e= el;
  gp_box_t lims= *limits;
  ge_lines_t *elx, *el0, **groups= e->groups;
  int i, value= 0, none= 1;
  for (i=0 ; i<e->nLevels ; i++) {
    el0= *groups++;
    if ((elx= el0)) do {
      value|= LinesScan(elx, flags, &lims);
      if (none) { *limits= lims;   e->el.box= lims; }
      else { gp_swallow(limits, &lims);   gp_swallow(&e->el.box, &lims); }
      none= 0;
      elx= (ge_lines_t *)elx->el.next;
    } while (elx != el0);
  }
  if (none) value= MeshXYScan(el, flags, limits, &e->el.box);
  return value;
}

/* ARGSUSED */
static int SystemScan(void *el, int flags, gp_box_t *limits)
{
  return 0;   /* cannot ever happen... */
}

/* ------------------------------------------------------------------------ */
/* Margin virtual function returns margin box */

/* ARGSUSED */
static void NoMargin(void *el, gp_box_t *margin)
{
  margin->xmin= margin->xmax= margin->ymin= margin->ymax= 0.0;
}

static void LinesMargin(void *el, gp_box_t *margin)
{
  /* This only accounts for line width, ignoring other decorations--
     other decorations seem unlikely outside coordinate systems */
  ge_lines_t *lines= el;
  margin->xmin= margin->xmax= margin->ymin= margin->ymax=
    0.5*lines->l.width*GP_DEFAULT_LINE_WIDTH;
}

static void DisjointMargin(void *el, gp_box_t *margin)
{
  ge_disjoint_t *lines= el;
  margin->xmin= margin->xmax= margin->ymin= margin->ymax=
    0.5*lines->l.width*GP_DEFAULT_LINE_WIDTH;
}

static void TextMargin(void *el, gp_box_t *margin)
{
  /* The actual text box cannot be computed without text metric data--
     the following is a crude guess based on character counts and an
     assumed width/height ratio of 0.6 (as in 9x15) and descent/height
     ratio of 0.2.  This should be close for Courier, but it probably
     way off in width for the proportional fonts.  */
  ge_text_t *text= el;
  gp_real_t width, x0, y0, dx, dy;
  int alignH, alignV;
  int nLines= gp_get_text_shape(text->text, &text->t, (gp_text_width_computer_t)0, &width);

  dx= text->t.height*width*0.6;
  dy= text->t.height*((gp_real_t)nLines);

  gp_get_text_alignment(&text->t, &alignH, &alignV);
  if (alignH==GP_HORIZ_ALIGN_LEFT) {
    x0= 0.0;
  } else if (alignH==GP_HORIZ_ALIGN_CENTER) {
    x0= -0.5*dx;
  } else {
    x0= -dx;
  }
  if (alignV==GP_VERT_ALIGN_TOP || alignV==GP_VERT_ALIGN_CAP) {
    y0= -dy;
  } else if (alignH==GP_VERT_ALIGN_HALF) {
    y0= -0.1*text->t.height - 0.5*dy;
  } else if (alignH==GP_VERT_ALIGN_BASE) {
    y0= -0.2*text->t.height;
  } else {
    y0= 0.0;
  }

  if (ga_attributes.t.orient==GP_ORIENT_RIGHT) {
    margin->xmin= x0;
    margin->xmax= x0 + dx;
    margin->ymin= y0;
    margin->ymax= y0 + dy;
  } else if (ga_attributes.t.orient==GP_ORIENT_LEFT) {
    margin->xmin= x0 - dx;
    margin->xmax= x0;
    margin->ymin= y0 - dy;
    margin->ymax= y0;
  } else if (ga_attributes.t.orient==GP_ORIENT_UP) {
    margin->xmin= y0;
    margin->xmax= y0 + dy;
    margin->ymin= x0;
    margin->ymax= x0 + dx;
  } else {
    margin->xmin= y0 - dy;
    margin->xmax= y0;
    margin->ymin= x0 - dx;
    margin->ymax= x0;
  }
}

static void MeshMargin(void *el, gp_box_t *margin)
{
  ge_mesh_t *mesh= el;
  margin->xmin= margin->xmax= margin->ymin= margin->ymax=
    0.5*mesh->l.width*GP_DEFAULT_LINE_WIDTH;
}

/* ARGSUSED */
static void VectorsMargin(void *el, gp_box_t *margin)
{
  /* This is a wild guess-- otherwise must scan (u, v) --
     should never arise in practice */
  /* ge_vectors_t *vec= el; */
  margin->xmin= margin->xmax= margin->ymin= margin->ymax= 0.05;
}

static void ContoursMargin(void *el, gp_box_t *margin)
{
  /* Should never actually happen */
  ge_contours_t *con= el;
  margin->xmin= margin->xmax= margin->ymin= margin->ymax=
    0.5*con->l.width*GP_DEFAULT_LINE_WIDTH;
}

/* ------------------------------------------------------------------------ */
