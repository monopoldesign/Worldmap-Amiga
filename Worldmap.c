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

#include "coastline.c"

struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase *GfxBase = NULL;

void draw(struct Window *win)
{
	struct RastPort *rp = win->RPort;
	const short *p = coastline_points;
	int i, j;

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

	// draw Line
	SetAPen(rp, 1);
	for (i = 0; i < COASTLINE_POLYLINE_COUNT; i++)
	{
		int count = coastline_lengths[i];

		long px = x0 + ((long)(p[0] + 18000) * iw) / 36000;
		long py = y0 + ((long)(9000 - p[1]) * ih) / 18000;
		Move(rp, (short)px, (short)py);

		for (j = 1; j < count; j++)
		{
			px = x0 + ((long)(p[j * 2] + 18000) * iw) / 36000;
			py = y0 + ((long)(9000 - p[(j * 2) + 1]) * ih) / 18000;
			Draw(rp, (short)px, (short)py);
		}
		p += count * 2;
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
						IDCMP_NEWSIZE,
		TAG_DONE);

	if (!win)
	{
		CloseLibrary((struct Library *)GfxBase);
		CloseLibrary((struct Library *)IntuitionBase);
		return 1;
	}

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
					draw(win);
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
