/******************************************************************************
* Header-Files
*******************************************************************************/
#include <exec/memory.h>
#include <exec/libraries.h>
#include <libraries/gadtools.h>
#include <libraries/mui.h>
#include <pragma/muimaster_lib.h>
#include <proto/exec.h>
#include <stdio.h>

#include "worldmap_mcc.h"

/******************************************************************************
* Definitions
*******************************************************************************/
struct ObjApp
{
	APTR	App;
	APTR    WI_main;
	APTR    AR_map;
	APTR    TX_zoom, TX_lon, TX_lat;
};

struct WorldMapBase
{
	struct Library base;
	LONG dummy;
};

#define MN_ZOOM_IN    	1
#define MN_ZOOM_OUT   	2
#define MN_RESET      	3
#define MN_QUIT       	4
#define MN_PAN_N      	5
#define MN_PAN_S      	6
#define MN_PAN_W      	7
#define MN_PAN_E      	8

/******************************************************************************
* Macros
*******************************************************************************/
#define MAKE_ID(a, b, c, d) ((ULONG)(a) << 24 | (ULONG)(b) << 16 | (ULONG)(c) << 8 | (ULONG)(d))
#define HOOKPROTONH(name, ret, obj, param) ret name(register __a2 obj, register __a1 param)
#define MakeHook(hookName, hookFunc) struct Hook hookName = {{NULL, NULL}, (HOOKFUNC)hookFunc, NULL, NULL}

/******************************************************************************
* Prototypes
*******************************************************************************/
void update_display(void);
void init(void);
void end(void);
struct ObjApp * CreateApp(void);
void DisposeApp(struct ObjApp * ObjectApp);

/******************************************************************************
* Global Variables
*******************************************************************************/
struct IntuitionBase *IntuitionBase;
struct Library *MUIMasterBase;
struct LibBase *TestBase;

struct WorldMapBase *WorldMapBase = NULL;
struct ObjApp *App = NULL;

/******************************************************************************
* Main-Program
*******************************************************************************/

/*-----------------------------------------------------------------------------
- main
------------------------------------------------------------------------------*/
int main(void)
{
	BOOL running = TRUE;
	ULONG signal;

	init();

	if (!(App = CreateApp()))
	{
		printf("Can't Create App\n");
		end();
	}

	update_display();

	while (running)
	{
		ULONG ret = DoMethod(App->App, MUIM_Application_NewInput, &signal);

		switch (ret)
		{
			// Window close
			case MUIV_Application_ReturnID_Quit:
			case MN_QUIT:
				if ((MUI_RequestA(App->App, 0, 0, "Quit?", "_Yes|_No", "\33cAre you sure?", 0)) == 1)
					running = FALSE;
			break;

			case MN_ZOOM_IN:
				DoMethod(App->AR_map, MYMETH_ZoomIn);
				update_display();
				break;

			case MN_ZOOM_OUT:
				DoMethod(App->AR_map, MYMETH_ZoomOut);
				update_display();
				break;

			case MN_RESET:
				DoMethod(App->AR_map, MYMETH_Reset);
				update_display();
				break;

			case MN_PAN_N:
				DoMethod(App->AR_map, MYMETH_PanNorth);
				update_display();
				break;

			case MN_PAN_S:
				DoMethod(App->AR_map, MYMETH_PanSouth);
				update_display();
				break;

			case MN_PAN_W:
				DoMethod(App->AR_map, MYMETH_PanWest);
				update_display();
				break;

			case MN_PAN_E:
				DoMethod(App->AR_map, MYMETH_PanEast);
				update_display();
				break;

			default:
				break;
		}

		if (running && signal)
			Wait(signal);
	}

	DisposeApp(App);
	end();
	return 0;
}

/*-----------------------------------------------------------------------------
- init
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
- end
------------------------------------------------------------------------------*/
void end(void)
{
	CloseLibrary((struct Library *)MUIMasterBase);
	CloseLibrary((struct Library *)IntuitionBase);
	exit(0);
}

/*-----------------------------------------------------------------------------
- CreateApp
------------------------------------------------------------------------------*/
struct ObjApp * CreateApp(void)
{
	struct MUI_CustomClass *mcc = NULL;

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

	ObjectApp->TX_zoom = TextObject,
		MUIA_Frame, MUIV_Frame_Text,
		MUIA_Text_Contents, "100%",
		MUIA_Text_SetMin, TRUE,
	End;

	/*
	if (TestBase = (struct LibBase *) OpenLibrary("worldmap.library",1))
	{
		printf("worldmap.library opened\n");
		mcc = (struct MUI_CustomClass *)MCC_Query(2);
		printf("Class = %lx\n", (ULONG)mcc);

		if (mcc)
		{
			ObjectApp->AR_map = NewObject(mcc->mcc_Class, NULL,
				MUIA_Background, MUII_BACKGROUND,
				MUIA_Frame, MUIV_Frame_InputList,
				MUIA_HorizWeight, 100,
				MUIA_VertWeight, 100,
			TAG_END);
		}
	}
	*/

	ObjectApp->AR_map = MUI_NewObject("worldmap.mcc",
		MUIA_Background, MUII_BACKGROUND,
		MUIA_Frame, MUIV_Frame_InputList,
		MUIA_HorizWeight, 100,
		MUIA_VertWeight, 100,
	TAG_END);

	if (!ObjectApp->AR_map)
	{
		printf("Can't create worldmap.mcc object\n");
		FreeVec(ObjectApp);
		return NULL;
	}

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
		return NULL;
	}

	// Window-Close-Method
	DoMethod(ObjectApp->WI_main, MUIM_Notify,
		MUIA_Window_CloseRequest, TRUE,
		ObjectApp->App, 2,
		MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit
	);

	// Open Window
	set(ObjectApp->WI_main, MUIA_Window_Open, TRUE);

	return(ObjectApp);
}

/*-----------------------------------------------------------------------------
- DisposeApp
------------------------------------------------------------------------------*/
void DisposeApp(struct ObjApp * ObjectApp)
{
	MUI_DisposeObject(ObjectApp->App);
	FreeVec(ObjectApp);
}

/*-----------------------------------------------------------------------------
- update_display
------------------------------------------------------------------------------*/
void update_display(void)
{
	static char lon_buf[16];
	static char lat_buf[16];
	static char zoom_buf[16];
	static char title_buf[64];
	LONG lon, lat, zoom, ms;

	get(App->AR_map, MYATTR_CenterLon, &lon);
	get(App->AR_map, MYATTR_CenterLat, &lat);
	get(App->AR_map, MYATTR_Zoom, &zoom);
	get(App->AR_map, MYATTR_DrawTime, &ms);

	sprintf(lon_buf, "%.2f", lon / 100.0);
	sprintf(lat_buf, "%.2f", lat / 100.0);
	sprintf(zoom_buf, "%ld%%", zoom);
	sprintf(title_buf, "World Map - %ld ms", ms);

	set(App->TX_lon, MUIA_Text_Contents, lon_buf);
	set(App->TX_lat, MUIA_Text_Contents, lat_buf);
	set(App->TX_zoom, MUIA_Text_Contents, zoom_buf);
	set(App->WI_main, MUIA_Window_Title, title_buf);
}
