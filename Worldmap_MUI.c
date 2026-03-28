/******************************************************************************
* Header-Files
*******************************************************************************/
#include <exec/memory.h>
#include <libraries/gadtools.h>
#include <libraries/mui.h>
#include <pragma/muimaster_lib.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/timer.h>
#include <stdio.h>

#include "worldmap_mcc.h"
#include "coastline2.c"

/******************************************************************************
* Definitions
*******************************************************************************/
#define MAKE_ID(a, b, c, d) ((ULONG)(a) << 24 | (ULONG)(b) << 16 | (ULONG)(c) << 8 | (ULONG)(d))

struct ObjApp
{
	APTR	App;
	APTR    WI_main;
	APTR    AR_map;
	APTR    BT_n, BT_s, BT_w, BT_e;
	APTR    BT_plus, BT_minus, BT_reset;
	APTR    TX_zoom, TX_lon, TX_lat;
};

#define MN_ZOOM_IN    	1
#define MN_ZOOM_OUT   	2
#define MN_RESET      	3
#define MN_QUIT       	4
#define MN_PAN_N      	5
#define MN_PAN_S      	6
#define MN_PAN_W      	7
#define MN_PAN_E      	8
#define ID_REDRAW     	9

/******************************************************************************
* Prototypes
*******************************************************************************/
void init(void);
void end(void);
struct ObjApp * CreateApp(void);
void DisposeApp(struct ObjApp * ObjectApp);

void get_map_coords(short *x0, short *y0, short *cx, short *cy);
void project_points(void);
void screen_to_lonlat(short mx, short my, short *lon, short *lat);
void draw_cross(struct RastPort *rp, short x, short y, short size);
void draw_map(void);
void update_display(void);

/******************************************************************************
* Global Variables
*******************************************************************************/
struct IntuitionBase *IntuitionBase;
struct Library *MUIMasterBase;

struct ObjApp *App = NULL;

long  zoom            = 100;
BOOL  zoom_center_set = FALSE;
short zoom_center_lon = 0;
short zoom_center_lat = 0;
short zoom_cx         = 0;
short zoom_cy         = 0;

short proj_x[COASTLINE_TOTAL_POINTS];
short proj_y[COASTLINE_TOTAL_POINTS];

char lon_buf[16];
char lat_buf[16];
char zoom_buf[8];
char win_title[64];

short last_w = 0;
short last_h = 0;

/******************************************************************************
* Main-Program
*******************************************************************************/

/*-----------------------------------------------------------------------------
- main()
------------------------------------------------------------------------------*/
int main(void)
{
	BOOL running = TRUE;
	ULONG signal;
	BOOL needs_draw = FALSE;
	short cur_w;
	short cur_h;

	init();

	if (!(App = CreateApp()))
	{
		printf("Can't Create App\n");
		end();
	}

	project_points();
	draw_map();

	while (running)
	{
		ULONG ret = DoMethod(App->App, MUIM_Application_NewInput, &signal);
		needs_draw = FALSE;

		cur_w = _mwidth(App->AR_map);
		cur_h = _mheight(App->AR_map);

		if (cur_w != last_w || cur_h != last_h)
		{
			last_w = cur_w;
			last_h = cur_h;
			needs_draw = TRUE;
		}

		switch (ret)
		{
			// Window close
			case MUIV_Application_ReturnID_Quit:
			case MN_QUIT:
				if ((MUI_RequestA(App->App, 0, 0, "Quit?", "_Yes|_No", "\33cAre you sure?", 0)) == 1)
					running = FALSE;
			break;

			case ID_REDRAW:
				needs_draw = TRUE;
				break;

			case MN_ZOOM_IN:
				zoom = zoom * (100 + ZOOM_STEP) / 100;
				if (zoom > 1600) zoom = 1600;
				if (!zoom_center_set) zoom_center_set = TRUE;
				needs_draw = TRUE;
				break;

			case MN_ZOOM_OUT:
				zoom = zoom * 100 / (100 + ZOOM_STEP);
				if (zoom < 100) zoom = 100;
				needs_draw = TRUE;
				break;

			case MN_RESET:
				zoom = 100;
				zoom_center_set = FALSE;
				zoom_center_lon = 0;
				zoom_center_lat = 0;
				needs_draw = TRUE;
				break;

			case MN_PAN_N:
				if (zoom_center_set)
				{
					zoom_center_lat += PAN_STEP;
					if (zoom_center_lat > 9000) zoom_center_lat = 9000;
					needs_draw = TRUE;
				}
				break;

			case MN_PAN_S:
				if (zoom_center_set)
				{
					zoom_center_lat -= PAN_STEP;
					if (zoom_center_lat < -9000) zoom_center_lat = -9000;
					needs_draw = TRUE;
				}
				break;

			case MN_PAN_W:
				if (zoom_center_set)
				{
					zoom_center_lon -= PAN_STEP;
					if (zoom_center_lon < -18000) zoom_center_lon = -18000;
					needs_draw = TRUE;
				}
				break;

			case MN_PAN_E:
				if (zoom_center_set)
				{
					zoom_center_lon += PAN_STEP;
					if (zoom_center_lon > 18000) zoom_center_lon = 18000;
					needs_draw = TRUE;
				}
				break;

			default:
				break;
		}

		if (needs_draw)
		{
			project_points();
			draw_map();
			update_display();
		}

		if (running && signal)
			Wait(signal);
	}

	DisposeApp(App);
	end();
	return 0;
}

/*-----------------------------------------------------------------------------
- init()
------------------------------------------------------------------------------*/
void init(void)
{
	if (!(IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37)))
	{
		printf("Can't Open Intuition Library\n");
		exit(20);
	}

	if (!(MUIMasterBase = OpenLibrary(MUIMASTER_NAME, MUIMASTER_VMIN)))
	{
		printf("Can't Open MUIMaster Library\n");
		CloseLibrary((struct Library *)IntuitionBase);
		exit(20);
	}
}

/*-----------------------------------------------------------------------------
- end()
------------------------------------------------------------------------------*/
void end(void)
{
	CloseLibrary((struct Library *)MUIMasterBase);
	CloseLibrary((struct Library *)IntuitionBase);
	exit(0);
}

/*-----------------------------------------------------------------------------
- CreateApp()
------------------------------------------------------------------------------*/
struct ObjApp * CreateApp(void)
{
	struct ObjApp * ObjectApp;

	APTR	GROUP_ROOT;
	APTR    GROUP_RIGHT;
	APTR    GROUP_POS;
	APTR	GROUP_ZOOM;

	static struct NewMenu MenuData[] =
	{
	    { NM_TITLE, "Map",          0,  0, 0, (APTR)0          },
	    { NM_ITEM,  "Zoom In",     "I", 0, 0, (APTR)MN_ZOOM_IN  },
	    { NM_ITEM,  "Zoom Out",    "O", 0, 0, (APTR)MN_ZOOM_OUT },
	    { NM_ITEM,  NM_BARLABEL,    0,  0, 0, (APTR)0           },
	    { NM_ITEM,  "Reset",       "R", 0, 0, (APTR)MN_RESET    },
	    { NM_ITEM,  NM_BARLABEL,    0,  0, 0, (APTR)0           },
	    { NM_ITEM,  "Quit",        "Q", 0, 0, (APTR)MN_QUIT     },
	    { NM_TITLE, "Pan",          0,  0, 0, (APTR)0           },
	    { NM_ITEM,  "North",       "N", 0, 0, (APTR)MN_PAN_N    },
	    { NM_ITEM,  "South",       "S", 0, 0, (APTR)MN_PAN_S    },
	    { NM_ITEM,  "West",        "W", 0, 0, (APTR)MN_PAN_W    },
	    { NM_ITEM,  "East",        "E", 0, 0, (APTR)MN_PAN_E    },
	    { NM_END,   NULL,           0,  0, 0, (APTR)0           },
	};

	if (!(ObjectApp = AllocVec(sizeof(struct ObjApp), MEMF_CLEAR)))
		return(NULL);

	ObjectApp->TX_zoom = TextObject,
		MUIA_Frame, MUIV_Frame_Text,
		MUIA_Text_Contents, "100%",
		MUIA_Text_SetMin, TRUE,
	End;

	ObjectApp->TX_lon = TextObject,
		MUIA_Frame, MUIV_Frame_Text,
		MUIA_Text_Contents, "---",
		MUIA_Text_SetMin, TRUE,
	End;

	ObjectApp->TX_lat = TextObject,
		MUIA_Frame, MUIV_Frame_Text,
		MUIA_Text_Contents, "---",
		MUIA_Text_SetMin, TRUE,
	End;

	GROUP_POS = GroupObject,
		MUIA_Frame, MUIV_Frame_Group,
		MUIA_FrameTitle, "Position",
		Child, LLabel("Lon:"),
		Child, ObjectApp->TX_lon,
		Child, LLabel("Lat:"),
		Child, ObjectApp->TX_lat,
	End;

	GROUP_ZOOM = GroupObject,
		MUIA_Frame, MUIV_Frame_Group,
		MUIA_FrameTitle, "Zoom",
		MUIA_Group_Horiz, FALSE,
		Child, ObjectApp->TX_zoom,
	End;

	ObjectApp->AR_map = RectangleObject,
		MUIA_Background, MUII_BACKGROUND,
		MUIA_Frame, MUIV_Frame_InputList,
		MUIA_HorizWeight, 100,
		MUIA_VertWeight, 100,
	End;

	GROUP_RIGHT = GroupObject,
		MUIA_Group_Horiz, FALSE,
		MUIA_HorizWeight, 0,
		MUIA_FixWidth, 60,
		Child, GROUP_POS,
		Child, GROUP_ZOOM,
		Child, RectangleObject, MUIA_VertWeight, 100, End,
	End;

	GROUP_ROOT = GroupObject,
		MUIA_Group_Horiz, TRUE,
		Child, ObjectApp->AR_map,
		Child, GROUP_RIGHT,
	End;

	ObjectApp->WI_main = WindowObject,
		MUIA_Window_Title,	"World Map",
		MUIA_Window_ID,		MAKE_ID('W', 'M', 'A', 'P'),
		MUIA_Window_Width, 640,
		MUIA_Window_Height, 360,
		WindowContents,		GROUP_ROOT,
	End;

	ObjectApp->App = ApplicationObject,
		MUIA_Application_Title,			"World Map",
		MUIA_Application_Base,			"WORLDMAP",
		MUIA_Application_Version,		"$VER: WorldMap V0.1",
		MUIA_Application_Copyright,		"2026",
		MUIA_Application_Author,		"M.Volkel",
		MUIA_Application_Description,	"Amiga World Map",
		MUIA_Application_Menustrip,		MUI_MakeObject(MUIO_MenustripNM, MenuData, 0),
		SubWindow,						ObjectApp->WI_main,
	End;

	if (!ObjectApp->App)
	{
		FreeVec(ObjectApp);
		return(NULL);
	}

	// Window-Close-Method
	DoMethod(ObjectApp->WI_main, MUIM_Notify,
		MUIA_Window_CloseRequest, TRUE,
		ObjectApp->App, 2,
		MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit
	);

	// Resize-Methods
	DoMethod(ObjectApp->AR_map, MUIM_Notify, MUIA_Width, MUIV_EveryTime, ObjectApp->App, 2, MUIM_Application_ReturnID, ID_REDRAW);
	DoMethod(ObjectApp->AR_map, MUIM_Notify, MUIA_Height, MUIV_EveryTime, ObjectApp->App, 2, MUIM_Application_ReturnID, ID_REDRAW);

	// Open Window
	set(ObjectApp->WI_main, MUIA_Window_Open, TRUE);

	return(ObjectApp);
}

/*-----------------------------------------------------------------------------
- DisposeApp()
------------------------------------------------------------------------------*/
void DisposeApp(struct ObjApp * ObjectApp)
{
	MUI_DisposeObject(ObjectApp->App);
	FreeVec(ObjectApp);
}

/*-----------------------------------------------------------------------------
- DisposeApp()
------------------------------------------------------------------------------*/
void get_map_coords(short *x0, short *y0, short *cx, short *cy)
{
	APTR area = App->AR_map;

    *x0 = _mleft(area);
    *y0 = _mtop(area);

    if (zoom_center_set)
    {
        *cx = (short)(*x0 + ((long)(zoom_center_lon + 18000) * _mwidth(area)) / 36000);
        *cy = (short)(*y0 + ((long)(9000 - zoom_center_lat) * _mheight(area)) / 18000);
    }
    else
    {
        *cx = *x0 + _mwidth(area) / 2;
        *cy = *y0 + _mheight(area) / 2;
    }
}

/*-----------------------------------------------------------------------------
- DisposeApp()
------------------------------------------------------------------------------*/
void project_points(void)
{
	APTR area = App->AR_map;
	short x0, y0, cx, cy;
	int i;

	get_map_coords(&x0, &y0, &cx, &cy);
	zoom_cx = cx;
	zoom_cy = cy;

	for (i = 0; i < COASTLINE_TOTAL_POINTS; i++)
	{
		short bx = (short)(x0 + ((long)(coastline_points[i * 2] + 18000) * _mwidth(area)) / 36000);
		short by = (short)(y0 + ((long)(9000 - coastline_points[i * 2 + 1]) * _mheight(area)) / 18000);
		proj_x[i] = (short)(cx + ((long)(bx - cx) * zoom) / 100);
		proj_y[i] = (short)(cy + ((long)(by - cy) * zoom) / 100);
	}
}

/*-----------------------------------------------------------------------------
- DisposeApp()
------------------------------------------------------------------------------*/
void screen_to_lonlat(short mx, short my, short *lon, short *lat)
{
	APTR area = App->AR_map;
	short x0, y0, cx, cy;
	long bx, by;

	get_map_coords(&x0, &y0, &cx, &cy);
	bx = cx + ((long)(mx - cx) * 100L) / zoom;
	by = cy + ((long)(my - cy) * 100L) / zoom;

	*lon = (short)(((bx - x0) * 36000L) / _mwidth(area)) - 18000;
	*lat = 9000 - (short)(((by - y0) * 18000L) / _mheight(area));
}

/*-----------------------------------------------------------------------------
- DisposeApp()
------------------------------------------------------------------------------*/
void draw_cross(struct RastPort *rp, short x, short y, short size)
{
	SetAPen(rp, 2);
	Move(rp, x - size, y);
	Draw(rp, x + size, y);
	Move(rp, x, y - size);
	Draw(rp, x, y + size);
}

/*-----------------------------------------------------------------------------
- DisposeApp()
------------------------------------------------------------------------------*/
void draw_map(void)
{
	APTR area = App->AR_map;
	struct RastPort *rp = _rp(area);
	short x0 = _mleft(area);
	short y0 = _mtop(area);
	short x1 = x0 + _mwidth(area);
	short y1 = y0 + _mheight(area);
	int i, j, idx = 0;
	BOOL pendown;

	struct EClockVal t1, t2;
	ULONG freq;
	ULONG ticks;
	ULONG ms;

	freq = ReadEClock(&t1);

	// clear Window
	SetAPen(rp, 0);
	RectFill(rp, x0, y0, x1 - 1, y1 - 1);

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

	if (zoom_center_set) { draw_cross(rp, zoom_cx, zoom_cy, 5); }

	ReadEClock(&t2);

	ticks = t2.ev_lo - t1.ev_lo;
	ms = (ticks * 1000) / freq;

	sprintf(win_title, "World Map - %1u ms (%1u ticks)", ms, ticks);
	set(App->WI_main, MUIA_Window_Title, win_title);
}

/*-----------------------------------------------------------------------------
- DisposeApp()
------------------------------------------------------------------------------*/
void update_display(void)
{
	sprintf(zoom_buf, "%ld%%", zoom);
	set(App->TX_zoom, MUIA_Text_Contents, zoom_buf);

	if (zoom_center_set)
	{
		sprintf(lon_buf, "%.2f", zoom_center_lon / 100.0);
		sprintf(lat_buf, "%.2f", zoom_center_lat / 100.0);
	}
	else
	{
		sprintf(lon_buf, "---");
		sprintf(lat_buf, "---");
	}

	set(App->TX_lon, MUIA_Text_Contents, lon_buf);
	set(App->TX_lat, MUIA_Text_Contents, lat_buf);
}


