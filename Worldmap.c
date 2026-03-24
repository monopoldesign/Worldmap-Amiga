#include <exec/types.h>
#include <intuition/intuition.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxmacros.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>
#include <proto/timer.h>
#include <stdio.h>

#include "gui.h"
#include "coastline2.c"

struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase *GfxBase = NULL;
struct Library *GadToolsBase = NULL;

short proj_x[COASTLINE_TOTAL_POINTS];
short proj_y[COASTLINE_TOTAL_POINTS];

long zoom = 100;
BOOL zoom_center_set = FALSE;
short zoom_center_lon = 0;
short zoom_center_lat = 0;
long pan_x = 0;
long pan_y = 0;

#define MAP_X0	10
#define MAP_Y0	10
#define MAP_W	383 - 3
#define MAP_H	234 - 3

void update_zoom_display(void)
{
	char buf[8];
	sprintf(buf, "%ld%%", zoom);
	GT_SetGadgetAttrs(WorldmapGadgets[GD_Gadget70], WorldmapWnd, NULL,
		GTTX_Text, (ULONG)buf,
		TAG_DONE);
}

void project_points(struct Window *win)
{
	short offx = win->BorderLeft;
	short offy = win->BorderTop;
	short x0 = offx + MAP_X0;
	short y0 = offy + MAP_Y0;
	short iw = MAP_W;
	short ih = MAP_H;
	short cx, cy;
	int i;

	if (zoom_center_set)
	{
		cx = (short)(x0 + ((long)(zoom_center_lon + 18000) * iw) / 36000);
		cy = (short)(y0 + ((long)(9000 - zoom_center_lat) * ih) / 18000);
	}
	else
	{
		cx = x0 + iw / 2;
		cy = y0 + ih / 2;
	}

	for (i = 0; i < COASTLINE_TOTAL_POINTS; i++)
	{
		short bx = (short)(x0 + ((long)(coastline_points[i * 2] + 18000) * iw) / 36000);
		short by = (short)(y0 + ((long)(9000 - coastline_points[i * 2 + 1]) * ih) / 18000);

		short px = (short)(cx + ((long)(bx - cx) * zoom) / 100);
		short py = (short)(cy + ((long)(by - cy) * zoom) / 100);

		proj_x[i] = px;
		proj_y[i] = py;
	}
}

void screen_to_lonlat(struct Window *win, short mx, short my, short *lon, short *lat)
{
	short offx = win->BorderLeft;
	short offy = win->BorderTop;
	short x0 = offx + MAP_X0;
	short y0 = offy + MAP_Y0;

	*lon = (short)(((long)(mx - x0) * 36000L) / MAP_W) - 18000;
	*lat = 9000 - (short)(((long)(my - y0) * 18000L) / MAP_H);
}

void draw_cross(struct RastPort *rp, short x, short y, short size)
{
	SetAPen(rp, 2);
	Move(rp, x - size, y);
	Draw(rp, x + size, y);
	Move(rp, x, y - size);
	Draw(rp, x, y + size);
}

void draw(struct Window *win)
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

	struct EClockVal t1, t2;
	ULONG freq;
	ULONG ticks;
	ULONG ms;
	char title[64];

	freq = ReadEClock(&t1);

	// clear Window
	SetAPen(rp, 0);
	RectFill(rp, x0, y0, x0 + MAP_W - 1, y0 + MAP_H - 1);

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
		draw_cross(rp, cx, cy, 5);
	}

	ReadEClock(&t2);

	ticks = t2.ev_lo - t1.ev_lo;
	ms = (ticks * 1000) / freq;

	sprintf(title, "World Map - Draw Time: %1u ms (%1u ticks)", ms, ticks);
	SetWindowTitles(win, title, (STRPTR)~0);
}

int main(void)
{
	struct Window *win = NULL;
	struct IntuiMessage *msg;
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

	project_points(WorldmapWnd);
	draw(WorldmapWnd);

	while (running)
	{
		WaitPort(WorldmapWnd->UserPort);
		running = HandleWorldmapIDCMP();
		/*
		while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort)))
		{
			switch (msg->Class)
			{
				case IDCMP_CLOSEWINDOW:
					running = FALSE;
					break;

				case IDCMP_NEWSIZE:
					project_points(win);
					draw(win);
					break;

				case IDCMP_MOUSEBUTTONS:
					if (msg->Code == SELECTDOWN)
					{
						short mx = msg->MouseX;
						short my = msg->MouseY;

						if (mx >= win->BorderLeft &&
							my >= win->BorderTop &&
							mx < win->Width - win->BorderRight &&
							my < win->Height - win->BorderBottom)
						{
							screen_to_lonlat(win, mx, my, &zoom_center_lon, &zoom_center_lat);
							zoom_center_set = TRUE;
							draw(win);
						}
					}
					break;

				case IDCMP_RAWKEY:
					if (!(msg->Code & 0x80) && zoom_center_set)
					{
						switch (msg->Code)
						{
							case RAWKEY_UP:
								zoom = zoom * 120 / 100;
								if (zoom > 1600) zoom = 1600;
								project_points(win);
								draw(win);
								break;

							case RAWKEY_DOWN:
								zoom = zoom * 100 / 120;
								if (zoom < 100) zoom = 100;
								project_points(win);
								draw(win);
								break;
						}
					}
					break;
			}
			ReplyMsg((struct Message *)msg);
		}
		*/
	}

	CloseWorldmapWindow();
	CloseLibrary((struct Library *)GadToolsBase);
	CloseLibrary((struct Library *)GfxBase);
	CloseLibrary((struct Library *)IntuitionBase);
	return 0;
}
