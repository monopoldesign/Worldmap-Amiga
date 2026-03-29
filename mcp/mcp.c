#include <exec/libraries.h>
#include <exec/memory.h>
#include <libraries/mui.h>
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
	APTR sl_zoom;
	APTR sl_pan;
	APTR cb_cross;
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
		case 1:
			DebugWrite("Query(1) called");
			return WorldMapMCP;

		case 2:
			DebugWrite("Query(2) called");
			return NULL;

		case 3:
			DebugWrite("Query(3) called");
			return NULL;
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

	LONG zoom = DoMethod(cfg, MUIM_Dataspace_Find, MUICFG_Worldmap_ZoomStep);
	LONG pan = DoMethod(cfg, MUIM_Dataspace_Find, MUICFG_Worldmap_PanStep);
	LONG cross = DoMethod(cfg, MUIM_Dataspace_Find, MUICFG_Worldmap_ShowCross);

	set(data->sl_zoom, MUIA_Numeric_Value, zoom ? *(LONG *)zoom : DEFAULT_ZOOM_STEP);
	set(data->sl_pan, MUIA_Numeric_Value, pan ? *(LONG *)pan : DEFAULT_PAN_STEP);
	set(data->cb_cross, MUIA_Selected, cross ? *(LONG *)cross : DEFAULT_SHOW_CROSS);

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

	val = xget(data->sl_zoom, MUIA_Numeric_Value);
	DoMethod(cfg, MUIM_Dataspace_Add, &val, sizeof(LONG), MUICFG_Worldmap_ZoomStep);

	val = xget(data->sl_pan, MUIA_Numeric_Value);
	DoMethod(cfg, MUIM_Dataspace_Add, &val, sizeof(LONG), MUICFG_Worldmap_PanStep);

	val = xget(data->cb_cross, MUIA_Selected);
	DoMethod(cfg, MUIM_Dataspace_Add, &val, sizeof(LONG), MUICFG_Worldmap_ShowCross);

	return 0;
}

/*-----------------------------------------------------------------------------
- mNew
------------------------------------------------------------------------------*/
LONG mNew(Class *cl, Object *obj, struct opSet *msg)
{
	struct MCPData *data;
	APTR sl_zoom, sl_pan, cb_cross;

	obj = (Object *)DoSuperMethodA(cl, obj, (Msg)msg);
	if (!obj) return 0;

	data = INST_DATA(cl, obj);

	sl_zoom = SliderObject,
		MUIA_Slider_Horiz, TRUE,
		MUIA_Numeric_Min, 5,
		MUIA_Numeric_Max, 50,
		MUIA_Numeric_Value, DEFAULT_ZOOM_STEP,
	End;

	sl_pan = SliderObject,
		MUIA_Slider_Horiz, TRUE,
		MUIA_Numeric_Min, 100,
		MUIA_Numeric_Max, 2000,
		MUIA_Numeric_Value, DEFAULT_PAN_STEP,
	End;

	cb_cross = CheckMark(DEFAULT_SHOW_CROSS);

	data->sl_zoom = sl_zoom;
	data->sl_pan = sl_pan;
	data->cb_cross = cb_cross;

	DoMethod(obj, OM_ADDMEMBER, GroupObject,
		MUIA_Frame, MUIV_Frame_Group,
		MUIA_FrameTitle, "Settings",
		MUIA_Group_Horiz, FALSE,

		Child, GroupObject,
			MUIA_Group_Horiz, TRUE,
			Child, LLabel("Zoom step:"),
			Child, sl_zoom,
		End,

		Child, GroupObject,
			MUIA_Group_Horiz, TRUE,
			Child, LLabel("Pan step:"),
			Child, sl_pan,
		End,

		Child, GroupObject,
			MUIA_Group_Horiz, TRUE,
			Child, LLabel("Show cross:"),
			Child, cb_cross,
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
