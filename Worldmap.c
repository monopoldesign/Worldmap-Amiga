#include <exec/types.h>
#include <intuition/intuition.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxmacros.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>
#include <proto/layers.h>
#include <proto/timer.h>
#include <stdio.h>

#include "gui.h"
#include "worldmap_mcc.h"
#include "coastline.c"
#include "land_triangles.c"

struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase *GfxBase = NULL;
struct Library *GadToolsBase = NULL;

short proj_x[COASTLINE_TOTAL_POINTS];
short proj_y[COASTLINE_TOTAL_POINTS];

#define MAX_VECTORS 6
struct AreaInfo MyAreaInfo;
struct TmpRas MyTmpRas;
PLANEPTR TmpRasBuffer = NULL;
WORD AreaBuffer[MAX_VECTORS * 5];	// 5 words per vector
short proj_tri_x[LAND_TRIANGLE_COUNT * 3];
short proj_tri_y[LAND_TRIANGLE_COUNT * 3];
BOOL proj_tri_visible[LAND_TRIANGLE_COUNT];

long zoom = 100;
BOOL zoom_center_set = FALSE;
short zoom_center_lon = 0;
short zoom_center_lat = 0;
short zoom_cx = 0;
short zoom_cy = 0;
long pan_x = 0;
long pan_y = 0;

void update_zoom_display(void)
{
	char buf[8];
	sprintf(buf, "%ld%%", zoom);
	GT_SetGadgetAttrs(WorldmapGadgets[GD_Gadget70], WorldmapWnd, NULL,
		GTTX_Text, (ULONG)buf,
		TAG_DONE);
}

void get_map_coords(struct Window *win, short *x0, short *y0, short *cx, short *cy)
{
    short offx = win->BorderLeft;
    short offy = win->BorderTop;

    *x0 = offx + MAP_X0;
    *y0 = offy + MAP_Y0;

    if (zoom_center_set)
    {
        *cx = (short)(*x0 + ((long)(zoom_center_lon + 18000) * MAP_W) / 36000);
        *cy = (short)(*y0 + ((long)(9000 - zoom_center_lat) * MAP_H) / 18000);
    }
    else
    {
        *cx = *x0 + MAP_W / 2;
        *cy = *y0 + MAP_H / 2;
    }
}

void screen_to_lonlat(struct Window *win, short mx, short my, short *lon, short *lat)
{
	short x0, y0, cx, cy;
	long bx, by;

	get_map_coords(win, &x0, &y0, &cx, &cy);
	bx = cx + ((long)(mx - cx) * 100L) / zoom;
	by = cy + ((long)(my - cy) * 100L) / zoom;

	*lon = (short)(((bx - x0) * 36000L) / MAP_W) - 18000;
	*lat = 9000 - (short)(((by - y0) * 18000L) / MAP_H);
}

void project_points(struct Window *win)
{
	short x0, y0, cx, cy;
	int i;

	get_map_coords(win, &x0, &y0, &cx, &cy);
	zoom_cx = cx;
	zoom_cy = cy;

	for (i = 0; i < COASTLINE_TOTAL_POINTS; i++)
	{
		short bx = (short)(x0 + ((long)(coastline_points[i * 2] + 18000) * MAP_W) / 36000);
		short by = (short)(y0 + ((long)(9000 - coastline_points[i * 2 + 1]) * MAP_H) / 18000);
		proj_x[i] = (short)(cx + ((long)(bx - cx) * zoom) / 100);
		proj_y[i] = (short)(cy + ((long)(by - cy) * zoom) / 100);
	}
}

void project_triangles(struct Window *win)
{
	short x0, y0, cx, cy;
	short x1, y1;
	int i;

	get_map_coords(win, &x0, &y0, &cx, &cy);
	x1 = x0 + MAP_W - 1;
	y1 = y0 + MAP_H - 1;

	for (i = 0; i < LAND_TRIANGLE_COUNT; i++)
	{
		int idx = i * 3;
		short ax, ay, bx, by, cx2, cy2;

		ax  = (short)(x0 + ((long)(land_triangles[idx * 2] + 18000) * MAP_W) / 36000);
		ay  = (short)(y0 + ((long)(9000 - land_triangles[idx * 2 + 1]) * MAP_H) / 18000);
		bx  = (short)(x0 + ((long)(land_triangles[idx * 2 + 2] + 18000) * MAP_W) / 36000);
		by  = (short)(y0 + ((long)(9000 - land_triangles[idx * 2 + 3]) * MAP_H) / 18000);
		cx2 = (short)(x0 + ((long)(land_triangles[idx * 2 + 4] + 18000) * MAP_W) / 36000);
		cy2 = (short)(y0 + ((long)(9000 - land_triangles[idx * 2 + 5]) * MAP_H) / 18000);

		proj_tri_x[idx]     = (short)(cx + ((long)(ax - cx) * zoom) / 100);
		proj_tri_y[idx]     = (short)(cy + ((long)(ay - cy) * zoom) / 100);
		proj_tri_x[idx + 1] = (short)(cx + ((long)(bx - cx) * zoom) / 100);
		proj_tri_y[idx + 1] = (short)(cy + ((long)(by - cy) * zoom) / 100);
		proj_tri_x[idx + 2] = (short)(cx + ((long)(cx2 - cx) * zoom) / 100);
		proj_tri_y[idx + 2] = (short)(cy + ((long)(cy2 - cy) * zoom) / 100);

		if (proj_tri_y[idx]     > y1) { proj_tri_y[idx]     = y1; }
		if (proj_tri_y[idx + 1] > y1) { proj_tri_y[idx + 1] = y1; }
		if (proj_tri_y[idx + 2] > y1) { proj_tri_y[idx + 2] = y1; }

		proj_tri_visible[i] = TRUE;
		if (proj_tri_x[idx] < x0 && proj_tri_x[idx + 1] < x0 && proj_tri_x[idx + 2] < x0) { proj_tri_visible[i] = FALSE; }
		if (proj_tri_x[idx] > x1 && proj_tri_x[idx + 1] > x1 && proj_tri_x[idx + 2] > x1) { proj_tri_visible[i] = FALSE; }
		if (proj_tri_y[idx] < y0 && proj_tri_y[idx + 1] < y0 && proj_tri_y[idx + 2] < y0) { proj_tri_visible[i] = FALSE; }
	}
}

void project_triangles2(struct Window *win)
{
	short x0, y0, cx, cy;
	short x1, y1;
	int i;

	get_map_coords(win, &x0, &y0, &cx, &cy);
	x1 = x0 + MAP_W - 1;
	y1 = y0 + MAP_H - 1;

	for (i = 0; i < LAND_TRIANGLE_COUNT * 3; i++)
	{
		short bx = (short)(x0 + ((long)(land_triangles[i * 2] + 18000) * MAP_W) / 36000);
		short by = (short)(y0 + ((long)(9000 - land_triangles[i * 2 + 1]) * MAP_H) / 18000);
		long px = (long)(cx + ((long)(bx - cx) * zoom) / 100);
		long py = (long)(cy + ((long)(by - cy) * zoom) / 100);

		if (px < -30000) { px = -30000; }
		if (px >  30000) { px =  30000; }
		if (py < -30000) { py = -30000; }
		if (px >  30000) { py =  30000; }

		proj_tri_x[i] = (short)px;
		proj_tri_y[i] = (short)py;
	}

	for (i = 0; i < LAND_TRIANGLE_COUNT; i++)
	{
		int idx = i * 3;

		short ax  = proj_tri_x[idx],     ay  = proj_tri_y[idx];
		short bx  = proj_tri_x[idx + 1], by  = proj_tri_y[idx + 1];
		short cx2 = proj_tri_x[idx + 2], cy2 = proj_tri_y[idx + 2];

		proj_tri_visible[i] = TRUE;
		if (ax < x0 && bx < x0 && cx2 < x0) { proj_tri_visible[i] = FALSE; }
		if (ax > x1 && bx > x1 && cx2 > x1) { proj_tri_visible[i] = FALSE; }
		if (ay < y0 && by < y0 && cy2 < y0) { proj_tri_visible[i] = FALSE; }
	}
}

void clear_map(struct Window *win)
{
	struct RastPort *rp = win->RPort;
	short offx = win->BorderLeft;
	short offy = win->BorderTop;
	short x0 = offx + MAP_X0;
	short y0 = offy + MAP_Y0;

	SetAPen(rp, 0);
	RectFill(rp, x0, y0, x0 + MAP_W - 1, y0 + MAP_H - 1);
}

void draw_fill(struct Window *win)
{
	struct RastPort *rp = win->RPort;
	int i;

	SetAPen(rp, 3);

	for (i = 0; i < LAND_TRIANGLE_COUNT; i++)
	{
		int idx = i * 3;

		if (!proj_tri_visible[i]) { continue; }

		AreaMove(rp, proj_tri_x[idx]    , proj_tri_y[idx]);
		AreaDraw(rp, proj_tri_x[idx + 1], proj_tri_y[idx + 1]);
		AreaDraw(rp, proj_tri_x[idx + 2], proj_tri_y[idx + 2]);
		AreaEnd(rp);
	}
}

void draw_cross(struct RastPort *rp, short x, short y, short size)
{
	SetAPen(rp, 2);
	Move(rp, x - size, y);
	Draw(rp, x + size, y);
	Move(rp, x, y - size);
	Draw(rp, x, y + size);
}

void draw_coast(struct Window *win)
{
	struct RastPort *rp = win->RPort;
	short offx = win->BorderLeft;
	short offy = win->BorderTop;
	short x0 = offx + MAP_X0;
	short y0 = offy + MAP_Y0;
	short x1 = x0 + MAP_W;
	short y1 = y0 + MAP_H;
	int i, j, idx = 0;
	BOOL pendown;

	const short *p = coastline_points;

	// draw Coast-Line
	SetAPen(rp, 1);
	for (i = 0; i < COASTLINE_POLYLINE_COUNT; i++)
	{
		int count = coastline_lengths[i];
		pendown = FALSE;

		for (j = 0; j < count; j++)
		{
			short px = proj_x[idx + j];
			short py = proj_y[idx + j];

			if (px >= x0 && px < x1 && py >= y0 && py < y1)
			{
				if (!pendown)
				{
					Move(rp, px, py);
					pendown = TRUE;
				}
				else
				{
					Draw(rp, px, py);
				}
			}
			else
			{
				pendown = FALSE;
			}
		}
		idx += count;
	}

	if (zoom_center_set)
	{
		short cx = (short)(x0 + ((long)(zoom_center_lon + 18000) * MAP_W) / 36000);
		short cy = (short)(y0 + ((long)(9000 - zoom_center_lat) * MAP_H) / 18000);
		draw_cross(rp, zoom_cx, zoom_cy, 5);
	}
}

void draw_map(struct Window *win)
{
	struct Region *clip;
	struct Region *old_clip;
	struct Rectangle rect;
	short x0, y0, cx, cy;

	struct EClockVal t1, t2;
	ULONG freq, ticks, ms;
	char title[64];

	// start Timer
	freq = ReadEClock(&t1);

	// set Clipping
	get_map_coords(win, &x0, &y0, &cx, &cy);

	rect.MinX = x0;
	rect.MinY = y0;
	rect.MaxX = x0 + MAP_W - 1;
	rect.MaxY = y0 + MAP_H - 1;

	clip = NewRegion();
	OrRectRegion(clip, &rect);
	old_clip = InstallClipRegion(win->WLayer, clip);

	// Projection
	project_triangles(win);
	project_points(win);

	// Drawing
	clear_map(win);
	draw_fill(win);
	draw_coast(win);

	// Remove Clipping
	InstallClipRegion(win->WLayer, old_clip);
	DisposeRegion(clip);

	// Calculate total Time
	ReadEClock(&t2);
	ticks = t2.ev_lo - t1.ev_lo;
	ms = (ticks * 1000) / freq;

	sprintf(title, "World Map - Draw Time: %1u ms (%1u ticks)", ms, ticks);
	SetWindowTitles(win, title, (STRPTR)~0);
}

void draw_map2(struct Window *win)
{
	struct Region *clip;
	struct Region *old_clip;
	struct Rectangle rect;
	short x0, y0, cx, cy;

	struct EClockVal t1, t2;
	ULONG freq, ticks, ms;
	char title[64];

	get_map_coords(win, &x0, &y0, &cx, &cy);

	rect.MinX = x0;
	rect.MinY = y0;
	rect.MaxX = x0 + MAP_W - 1;
	rect.MaxY = y0 + MAP_H - 1;

	clip = NewRegion();
	OrRectRegion(clip, &rect);
	old_clip = InstallClipRegion(win->WLayer, clip);

	freq = ReadEClock(&t1);
	project_triangles(win);
	ReadEClock(&t2);
	ticks = t2.ev_lo - t1.ev_lo;
	ms = (ticks * 1000) / freq;
	printf("project_triangles(): %1u ms (%1u ticks)\n", ms, ticks);

	freq = ReadEClock(&t1);
	project_points(win);
	ReadEClock(&t2);
	ticks = t2.ev_lo - t1.ev_lo;
	ms = (ticks * 1000) / freq;
	printf("project_points(): %1u ms (%1u ticks)\n", ms, ticks);

	freq = ReadEClock(&t1);
	clear_map(win);
	ReadEClock(&t2);
	ticks = t2.ev_lo - t1.ev_lo;
	ms = (ticks * 1000) / freq;
	printf("clear_map(): %1u ms (%1u ticks)\n", ms, ticks);

	freq = ReadEClock(&t1);
	draw_fill(win);
	ReadEClock(&t2);
	ticks = t2.ev_lo - t1.ev_lo;
	ms = (ticks * 1000) / freq;
	printf("draw_fill(): %1u ms (%1u ticks)\n", ms, ticks);

	freq = ReadEClock(&t1);
	draw_coast(win);
	ReadEClock(&t2);
	ticks = t2.ev_lo - t1.ev_lo;
	ms = (ticks * 1000) / freq;
	printf("draw_coast(): %1u ms (%1u ticks)\n", ms, ticks);

	InstallClipRegion(win->WLayer, old_clip);
	DisposeRegion(clip);

	ReadEClock(&t2);

	ticks = t2.ev_lo - t1.ev_lo;
	ms = (ticks * 1000) / freq;

	sprintf(title, "World Map - Draw Time: %1u ms (%1u ticks)", ms, ticks);
	SetWindowTitles(win, title, (STRPTR)~0);
}

int main(void)
{
	struct Window *win = NULL;
	struct RastPort *rp = NULL;
	BOOL running = TRUE;

	IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37);
	if (!IntuitionBase) { return 1; }

	GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 37);
	if (!GfxBase)
	{
		CloseLibrary((struct Library *)IntuitionBase);
		return 1;
	}

	GadToolsBase = OpenLibrary("gadtools.library", 37);
	if (!GadToolsBase)
	{
		CloseLibrary((struct Library *)GfxBase);
		CloseLibrary((struct Library *)IntuitionBase);
		return 1;
	}

	if (SetupScreen())
	{
		CloseLibrary((struct Library *)GadToolsBase);
		CloseLibrary((struct Library *)GfxBase);
		CloseLibrary((struct Library *)IntuitionBase);
		return 1;
	}

	if (OpenWorldmapWindow())
	{
		CloseDownScreen();
		CloseLibrary((struct Library *)GadToolsBase);
		CloseLibrary((struct Library *)GfxBase);
		CloseLibrary((struct Library *)IntuitionBase);
		return 1;
	}

	InitArea(&MyAreaInfo, AreaBuffer, MAX_VECTORS);
	TmpRasBuffer = AllocRaster(1280, 512);
	InitTmpRas(&MyTmpRas, TmpRasBuffer, RASSIZE(1280, 512));

	rp = WorldmapWnd->RPort;
	rp->TmpRas = &MyTmpRas;
	rp->AreaInfo = &MyAreaInfo;

	draw_map(WorldmapWnd);
	update_gadgets();

	while (running)
	{
		WaitPort(WorldmapWnd->UserPort);
		running = HandleWorldmapIDCMP();
	}

	CloseWorldmapWindow();
	CloseLibrary((struct Library *)GadToolsBase);
	CloseLibrary((struct Library *)GfxBase);
	CloseLibrary((struct Library *)IntuitionBase);
	return 0;
}
