#include <exec/types.h>
#include <intuition/intuition.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxmacros.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/timer.h>
#include <stdio.h>

#include "coastline2.c"

#define RAWKEY_UP	0x4c
#define RAWKEY_DOWN	0x4d

struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase *GfxBase = NULL;

BOOL zoom_center_set = FALSE;
short zoom_center_lon = 0;
short zoom_center_lat = 0;
long zoom = 100;

short proj_x[COASTLINE_TOTAL_POINTS];
short proj_y[COASTLINE_TOTAL_POINTS];

void screen_to_lonlat(struct Window *win, short mx, short my, short *lon, short *lat)
{
	short x0 = win->BorderLeft;
	short y0 = win->BorderTop;
	short iw = win->Width - win->BorderLeft - win->BorderRight - 1;
	short ih = win->Height - win->BorderTop - win->BorderBottom - 1;

	*lon = (short)(((long)(mx - x0) * 36000L) / iw) - 18000;
	*lat = 9000 - (short)(((long)(my - y0) * 18000L) / ih);
}

void draw_cross(struct RastPort *rp, short x, short y)
{
	short size = 5;
	SetAPen(rp, 2);
	Move(rp, x - size, y);
	Draw(rp, x + size, y);
	Move(rp, x, y - size);
	Draw(rp, x, y + size);
}

void project_points(struct Window *win)
{
	short x0 = win->BorderLeft;
	short y0 = win->BorderTop;
	short iw = win->Width - win->BorderLeft - win->BorderRight - 1;
	short ih = win->Height - win->BorderTop - win->BorderBottom - 1;
	int i;

	short cx, cy;
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

		if (px < x0) px = x0;
		if (px > x0 + iw) px = x0 + iw;
		if (py < y0) py = y0;
		if (py > y0 + ih) py = y0 + ih;

		proj_x[i] = px;
		proj_y[i] = py;
	}
}

void draw(struct Window *win)
{
	struct RastPort *rp = win->RPort;
	const short *p = coastline_points;
	int i, j;
	int idx = 0;

	short x0 = win->BorderLeft;
	short y0 = win->BorderTop;
	short iw = win->Width - win->BorderLeft - win->BorderRight - 1;
	short ih = win->Height - win->BorderTop - win->BorderBottom - 1;

	struct EClockVal t1, t2;
	ULONG freq;
	ULONG ticks;
	ULONG ms;
	char title[64];

	freq = ReadEClock(&t1);

	// clear Window
	SetAPen(rp, 0);
	RectFill(rp, x0, y0, x0 + iw - 1, y0 + ih - 1);

	// draw Coast-Line
	SetAPen(rp, 1);
	for (i = 0; i < COASTLINE_POLYLINE_COUNT; i++)
	{
		int count = coastline_lengths[i];
		Move(rp, proj_x[idx], proj_y[idx]);

		for (j = 1; j < count; j++)
		{
			Draw(rp, proj_x[idx + j], proj_y[idx + j]);
		}
		idx += count;
	}

	if (zoom_center_set)
	{
		short cx = (short)(x0 + ((long)(zoom_center_lon + 18000) * iw) / 36000);
		short cy = (short)(y0 + ((long)(9000 - zoom_center_lat) * ih) / 18000);
		draw_cross(rp, cx, cy);
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

	win = OpenWindowTags(NULL,
		WA_Title,       "World Map",
		WA_Left,        50,
		WA_Top,         50,
		WA_Width,       600,
		WA_Height,      400,
		WA_MinWidth,    160,
		WA_MinHeight,   100,
		WA_MaxWidth,    -1,
		WA_MaxHeight,   -1,
		WA_Flags,       WFLG_CLOSEGADGET	|
						WFLG_DRAGBAR		|
						WFLG_DEPTHGADGET	|
						WFLG_SIZEGADGET		|
						WFLG_ACTIVATE		|
						WFLG_RMBTRAP,
		WA_IDCMP,       IDCMP_CLOSEWINDOW	|
						IDCMP_NEWSIZE		|
						IDCMP_MOUSEBUTTONS	|
						IDCMP_RAWKEY,
		TAG_DONE);

	if (!win)
	{
		CloseLibrary((struct Library *)GfxBase);
		CloseLibrary((struct Library *)IntuitionBase);
		return 1;
	}

	project_points(win);
	draw(win);

	while (running)
	{
		WaitPort(win->UserPort);
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
	}

	CloseWindow(win);
	CloseLibrary((struct Library *)GfxBase);
	CloseLibrary((struct Library *)IntuitionBase);
	return 0;
}
