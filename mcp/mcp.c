#include <exec/libraries.h>
#include <exec/memory.h>
#include <libraries/mui.h>
#include <mui/muiextra.h>
#include <proto/exec.h>
#include <pragma/muimaster_lib.h>
#include <pragma/graphics_lib.h>

#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <pragma/dos_lib.h>

#include "mcp.h"

#pragma libbase WorldmapBase

/******************************************************************************
* Definitions
*******************************************************************************/
struct Library *MUIMasterBase;
struct MUI_CustomClass *WorldMapMCP = NULL;

struct MCPData
{
	APTR cy_resolution;
	APTR pd_coast;
	APTR pd_cross;
	APTR sl_zoom;
	APTR sl_pan;
	APTR sl_cross;
};

/******************************************************************************
* Library-Functions
*******************************************************************************/
/*-----------------------------------------------------------------------------
- __LibOpen
------------------------------------------------------------------------------*/
BOOL __LibOpen(register __a6 struct WorldmapBase *base)
{
	return TRUE;
}

/*-----------------------------------------------------------------------------
- __LibClose
------------------------------------------------------------------------------*/
void __LibClose(register __a6 struct WorldmapBase *base)
{
}

/*-----------------------------------------------------------------------------
- MCP_Query
------------------------------------------------------------------------------*/
struct MUI_CustomClass * MCP_Query(register __d0 LONG which, register __a6 struct WorldmapBase *base)
{
	switch (which)
	{
		case 1: return WorldMapMCP;
		case 2: return NULL;
		case 3: return NULL;
	}
	return NULL;
}

/*-----------------------------------------------------------------------------
- INIT_5_UserInit called by __LibInit()
------------------------------------------------------------------------------*/
void INIT_5_UserInit(void)
{
	if (!WorldMapMCP)
	{
		MUIMasterBase = OpenLibrary(MUIMASTER_NAME, MUIMASTER_VMIN);
		if (MUIMasterBase)
		{
			WorldMapMCP = MUI_CreateCustomClass(
				NULL,
				MUIC_Mccprefs, NULL,
				sizeof(struct MCPData),
				WorldmapDispatcher);
					
			if (!WorldMapMCP)
			{
				CloseLibrary(MUIMasterBase);
				MUIMasterBase = NULL;
			}
		}
	}
}

/*-----------------------------------------------------------------------------
- EXIT_5_UserExit called by __LibExpunge()
------------------------------------------------------------------------------*/
void EXIT_5_UserExit(void)
{
	if (WorldMapMCP)
	{
		MUI_DeleteCustomClass(WorldMapMCP);
		WorldMapMCP = NULL;
	}

	if (MUIMasterBase)
	{
		CloseLibrary(MUIMasterBase);
		MUIMasterBase = NULL;
	}
}

/******************************************************************************
* Methods
*******************************************************************************/
/*-----------------------------------------------------------------------------
- mConfigToGadgets
------------------------------------------------------------------------------*/
LONG mConfigToGadgets(Class *cl, Object *obj, struct MUIP_Settingsgroup_ConfigToGadgets *msg)
{
	struct MCPData *data = INST_DATA(cl, obj);
	APTR cfg = msg->configdata;

	APTR res    = (APTR)DoMethod(cfg, MUIM_Dataspace_Find, MUICFG_Worldmap_Resolution);
	APTR coast  = (APTR)DoMethod(cfg, MUIM_Dataspace_Find, MUICFG_Worldmap_CoastPen);
	APTR pcross = (APTR)DoMethod(cfg, MUIM_Dataspace_Find, MUICFG_Worldmap_CrossPen);
	APTR zoom   = (APTR)DoMethod(cfg, MUIM_Dataspace_Find, MUICFG_Worldmap_ZoomStep);
	APTR pan    = (APTR)DoMethod(cfg, MUIM_Dataspace_Find, MUICFG_Worldmap_PanStep);
	APTR scross = (APTR)DoMethod(cfg, MUIM_Dataspace_Find, MUICFG_Worldmap_CrossSize);

	set(data->cy_resolution, MUIA_Cycle_Active,    res           ? *(LONG *)res    : DEFAULT_RESOLUTION);
	set(data->pd_coast,      MUIA_Pendisplay_Spec, coast 		 ? coast           : DEFAULT_COAST_PEN);
	set(data->pd_cross,      MUIA_Pendisplay_Spec, pcross 		 ? pcross          : DEFAULT_CROSS_PEN);
	set(data->sl_zoom,       MUIA_Numeric_Value,   zoom          ? *(LONG *)zoom   : DEFAULT_ZOOM_STEP);
	set(data->sl_pan,        MUIA_Numeric_Value,   pan           ? *(LONG *)pan    : DEFAULT_PAN_STEP);
	set(data->sl_cross,		 MUIA_Numeric_Value,   scross		 ? *(LONG *)scross : DEFAULT_CROSS_SIZE);

	return 0;
}

/*-----------------------------------------------------------------------------
- mGadgetsToConfig
------------------------------------------------------------------------------*/
LONG mGadgetsToConfig(Class *cl, Object *obj, struct MUIP_Settingsgroup_GadgetsToConfig *msg)
{
	struct MCPData *data = INST_DATA(cl, obj);
	APTR cfg = msg->configdata;
	LONG val;
	struct MUI_PenSpec *ps;

	val = xget(data->cy_resolution, MUIA_Cycle_Active);
	DoMethod(cfg, MUIM_Dataspace_Add, &val, sizeof(LONG), MUICFG_Worldmap_Resolution);

	get(data->pd_coast, MUIA_Pendisplay_Spec, &ps);
	DoMethod(cfg, MUIM_Dataspace_Add, ps, sizeof(struct MUI_PenSpec), MUICFG_Worldmap_CoastPen);

	get(data->pd_cross, MUIA_Pendisplay_Spec, &ps);
	DoMethod(cfg, MUIM_Dataspace_Add, ps, sizeof(struct MUI_PenSpec), MUICFG_Worldmap_CrossPen);

	val = xget(data->sl_zoom, MUIA_Numeric_Value);
	DoMethod(cfg, MUIM_Dataspace_Add, &val, sizeof(LONG), MUICFG_Worldmap_ZoomStep);

	val = xget(data->sl_pan, MUIA_Numeric_Value);
	DoMethod(cfg, MUIM_Dataspace_Add, &val, sizeof(LONG), MUICFG_Worldmap_PanStep);

	val = xget(data->sl_cross, MUIA_Numeric_Value);
	DoMethod(cfg, MUIM_Dataspace_Add, &val, sizeof(LONG), MUICFG_Worldmap_CrossSize);

	return 0;
}

/*-----------------------------------------------------------------------------
- mNew
------------------------------------------------------------------------------*/
LONG mNew(Class *cl, Object *obj, struct opSet *msg)
{
	struct MCPData *data;
	APTR cy_resolution, pd_coast, pd_cross, sl_zoom, sl_pan, sl_cross;
	static const char *res_entries[] = {"Full", "Reduced", NULL};
	static const char infotext1[] = "\033bWorldmap.mcp 11.0\033n  (31.03.2026)\n"
									LIB_COPYRIGHT;
	static const char infotext2[] =	"\n"
									"A MUI Custom Class for \n"
									"displaying a world map\n"
									"on the Amiga.\n"
									"Based on Natural Earth vector data.\n"
									"Rust prototype\n"
									LIB_COPYRIGHT
									"\n";
									
	obj = (Object *)DoSuperMethodA(cl, obj, (Msg)msg);
	if (!obj) return 0;

	data = INST_DATA(cl, obj);

	cy_resolution = MUI_NewObject(MUIC_Cycle,
		MUIA_Cycle_Entries, res_entries,
		MUIA_Cycle_Active, DEFAULT_RESOLUTION,
	TAG_END);

	pd_coast = MUI_NewObject(MUIC_Poppen,
		MUIA_Pendisplay_Spec, "m1",
		MUIA_Frame, MUIV_Frame_Text,
	TAG_END);

	pd_cross = MUI_NewObject(MUIC_Poppen,
		MUIA_Pendisplay_Spec, "m1",
		MUIA_Frame, MUIV_Frame_Text,
	TAG_END);

	sl_zoom = MUI_NewObject(MUIC_Slider,
		MUIA_Slider_Horiz, TRUE,
		MUIA_Numeric_Min, 5,
		MUIA_Numeric_Max, 50,
		MUIA_Numeric_Value, DEFAULT_ZOOM_STEP,
	TAG_END);

	sl_pan = MUI_NewObject(MUIC_Slider,
		MUIA_Slider_Horiz, TRUE,
		MUIA_Numeric_Min, 100,
		MUIA_Numeric_Max, 2000,
		MUIA_Numeric_Value, DEFAULT_PAN_STEP,
	TAG_END);

	sl_cross = MUI_NewObject(MUIC_Slider,
		MUIA_Slider_Horiz, TRUE,
		MUIA_Numeric_Min, 1,
		MUIA_Numeric_Max, 8,
		MUIA_Numeric_Value, DEFAULT_CROSS_SIZE,
	TAG_END);

	data->cy_resolution = cy_resolution;
	data->pd_coast = pd_coast;
	data->pd_cross = pd_cross;
	data->sl_zoom = sl_zoom;
	data->sl_pan = sl_pan;
	data->sl_cross = sl_cross;

	DoMethod(obj, OM_ADDMEMBER, VGroup,

		Child, MUI_MakeObject(MUIO_BarTitle, "Preview"),
		Child, MUI_NewObject("Worldmap.mcc", TAG_END),
		
		Child, MUI_MakeObject(MUIO_BarTitle, "Settings"),

		Child, MUI_NewObject(MUIC_Group,
			MUIA_Group_Columns, 2,
			Child, LLabel("Resolution:"),
			Child, cy_resolution,
			Child, LLabel("Coast Colour:"),
			Child, pd_coast,
			Child, LLabel("Cross Colour:"),
			Child, pd_cross,
			Child, LLabel("Zoom step:"),
			Child, sl_zoom,
			Child, LLabel("Pan step:"),
			Child, sl_pan,
			Child, LLabel("Cross size:"),
			Child, sl_cross,
		End,

		Child, MUI_NewObject("Crawling.mcc",
			TextFrame,
			MUIA_FixHeightTxt, infotext1,
			MUIA_Background, "m1",

			Child, TextObject,
				MUIA_Text_Copy, FALSE,
				MUIA_Text_PreParse, "\033c",
				MUIA_Text_Contents, infotext1,
			End,

			Child, TextObject,
				MUIA_Text_Copy, FALSE,
				MUIA_Text_PreParse, "\033c",
				MUIA_Text_Contents, infotext2,
			End,

			Child, TextObject,
				MUIA_Text_Copy, FALSE,
				MUIA_Text_PreParse, "\033c",
				MUIA_Text_Contents, infotext1,
			End,
		End,
	End);

	return (ULONG)obj;
}

/*-----------------------------------------------------------------------------
- WorldmapMCPDispatcher
------------------------------------------------------------------------------*/
LONG WorldmapDispatcher(register __a0 Class *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	switch (msg->MethodID)
	{
		case OM_NEW:
			return mNew(cl, obj, (struct opSet *)msg);

		case MUIM_Settingsgroup_ConfigToGadgets:
			return mConfigToGadgets(cl, obj, (struct MUIP_Settingsgroup_ConfigToGadgets *)msg);

		case MUIM_Settingsgroup_GadgetsToConfig:
			return mGadgetsToConfig(cl, obj, (struct MUIP_Settingsgroup_GadgetsToConfig *)msg);

		default:
			return DoSuperMethodA(cl, obj, msg);
	}
}

ULONG xget(Object *obj, ULONG attr)
{
	ULONG val = 0;
	get(obj, attr, &val);
	return val;
}

void DebugWrite(char *msg)
{
	struct DosLibrary *DOSBase;
	BPTR f;
	LONG len = 0;
	
	/* Count string length manually */
	while (msg[len]) len++;
	
	DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 37);
	if (!DOSBase) return;
	
	f = Open("T:worldmap_debug.txt", MODE_READWRITE);
	if (!f) f = Open("T:worldmap_debug.txt", MODE_NEWFILE);
	if (f)
	{
		Seek(f, 0, OFFSET_END);
		Write(f, msg, len);
		Close(f);
	}
	CloseLibrary((struct Library *)DOSBase);
}
