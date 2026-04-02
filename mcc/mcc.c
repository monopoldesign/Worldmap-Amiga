#include <clib/utility_protos.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <libraries/mui.h>
#include <pragma/muimaster_lib.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/timer.h>
#include <stdio.h>

#include "mcc.h"
#include "mcp.h"
#include "coastline_reduced.c"
#include "coastline_full.c"

#pragma libbase WorldmapBase

struct MapData
{
	struct MUI_EventHandlerNode EHNode;
	short proj_reduced_x[COASTLINE_REDUCED_TOTAL_POINTS];
	short proj_reduced_y[COASTLINE_REDUCED_TOTAL_POINTS];
	short proj_full_x[COASTLINE_FULL_TOTAL_POINTS];
	short proj_full_y[COASTLINE_FULL_TOTAL_POINTS];
	short zoom_cx;
	short zoom_cy;
	long  zoom;
	BOOL  zoom_center_set;
	short zoom_center_lon;
	short zoom_center_lat;
	ULONG draw_time_ms;

	LONG resolution, zoom_step, pan_step, cross_size;
	struct MUI_PenSpec coast_penspec, cross_penspec;
	LONG coast_pen, cross_pen;
	BOOL coast_penchange, cross_penchange, resolution_changed;
};

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *UtilityBase;
struct Library *MUIMasterBase;

struct timerequest TimerRequest;
struct MsgPort *TimerPort = NULL;
struct Device *MyTimerBase = NULL;

struct MUI_CustomClass *WorldMapMCC = NULL;

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
- MCC_Query
------------------------------------------------------------------------------*/
ULONG MCC_Query(register __d0 LONG which, register __a6 struct WorldmapBase *base)
{
	switch (which)
	{
		case 0: return (ULONG)WorldMapMCC;
		case 3: return NULL;
	}
	return NULL;
}

/*-----------------------------------------------------------------------------
- INIT_5_UserInit called by __LibInit()
------------------------------------------------------------------------------*/
void INIT_5_UserInit(void)
{
	if (!WorldMapMCC)
	{
		MUIMasterBase = OpenLibrary(MUIMASTER_NAME, MUIMASTER_VMIN);
		if (MUIMasterBase)
		{
			WorldMapMCC = MUI_CreateCustomClass(
				NULL,
				MUIC_Area, NULL,
				sizeof(struct MapData),
				WorldmapDispatcher
			);

			if (WorldMapMCC)
			{
				IntuitionBase = (struct IntuitionBase *)WorldMapMCC->mcc_IntuitionBase;
				GfxBase = (struct GfxBase *)WorldMapMCC->mcc_GfxBase;
				UtilityBase = WorldMapMCC->mcc_UtilityBase;

				TimerPort = CreateMsgPort();
				if (TimerPort)
				{
					TimerRequest.tr_node.io_Message.mn_ReplyPort = TimerPort;
					if (!OpenDevice("timer.device", UNIT_MICROHZ, (struct IORequest *)&TimerRequest, 0))
						MyTimerBase = (struct Device *)TimerRequest.tr_node.io_Device;
				}
			}
			else
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
	if (WorldMapMCC)
	{
		MUI_DeleteCustomClass(WorldMapMCC);
		WorldMapMCC = NULL;
	}

	if (MyTimerBase)
	{
		CloseDevice((struct IORequest *)&TimerRequest);
		MyTimerBase = NULL;
	}

	if (TimerPort)
	{
		DeleteMsgPort(TimerPort);
		TimerPort = NULL;
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
- mSetup
------------------------------------------------------------------------------*/
LONG mSetup(Class *cl, Object *obj, Msg msg)
{
	struct MapData *data = INST_DATA(cl,obj);

	char buf[16];
	char *p = buf;
	LONG val = data->zoom_step;

	if (DoSuperMethodA(cl, obj, msg))
	{
		read_prefs(cl, obj);

		data->coast_pen = MUI_ObtainPen(muiRenderInfo(obj), &data->coast_penspec, 0);
		data->cross_pen = MUI_ObtainPen(muiRenderInfo(obj), &data->cross_penspec, 0);

		data->zoom = 100;
		data->zoom_center_set = FALSE;
		data->zoom_center_lon = 0;
		data->zoom_center_lat = 0;

		data->EHNode.ehn_Priority = 0;
		data->EHNode.ehn_Flags = 0;
		data->EHNode.ehn_Object = obj;
		data->EHNode.ehn_Class = cl;
		data->EHNode.ehn_Events = IDCMP_MOUSEBUTTONS;
		DoMethod(_win(obj), MUIM_Window_AddEventHandler, &data->EHNode);
		return TRUE;
	}
	return FALSE;
}

/*-----------------------------------------------------------------------------
- mCleanup
------------------------------------------------------------------------------*/
LONG mCleanup(Class *cl, Object *obj, Msg msg)
{
	struct MapData *data = INST_DATA(cl, obj);

	MUI_ReleasePen(muiRenderInfo(obj), data->coast_pen);
	MUI_ReleasePen(muiRenderInfo(obj), data->cross_pen);
	DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->EHNode);

	return DoSuperMethodA(cl, obj, msg);
}

/*-----------------------------------------------------------------------------
- mAskMinMax
------------------------------------------------------------------------------*/
LONG mAskMinMax(Class *cl, Object *obj, struct MUIP_AskMinMax *msg)
{
	DoSuperMethodA(cl, obj, (Msg)msg);

	msg->MinMaxInfo->MinWidth += 100;
	msg->MinMaxInfo->DefWidth += 200;
	msg->MinMaxInfo->MaxWidth += MUI_MAXMAX;
	msg->MinMaxInfo->MinHeight += 60;
	msg->MinMaxInfo->DefHeight += 120;
	msg->MinMaxInfo->MaxHeight += MUI_MAXMAX;

	return 0;
}

/*-----------------------------------------------------------------------------
- mDraw
------------------------------------------------------------------------------*/
LONG mDraw(Class *cl, Object *obj, struct MUIP_Draw *msg)
{
	struct MapData *data = INST_DATA(cl, obj);
	struct RastPort *rp = _rp(obj);
	short x0 = _mleft(obj);
	short y0 = _mtop(obj);
	short x1 = x0 + _mwidth(obj);
	short y1 = y0 + _mheight(obj);

	struct EClockVal t1, t2;
	ULONG freq, ticks;

	DoSuperMethodA(cl, obj, (Msg)msg);
	if (!(msg->flags & MADF_DRAWOBJECT))
		return 0;

	if (MyTimerBase) freq = ReadEClock(&t1);
	project_points(obj, data);

	// clear Window
	SetAPen(rp, 0);
	RectFill(rp, x0, y0, x1 - 1, y1 - 1);

	// draw Coast-Line
	if (data->coast_penchange)
	{
		data->coast_penchange = FALSE;
		MUI_ReleasePen(muiRenderInfo(obj), data->coast_pen);
		data->coast_pen = MUI_ObtainPen(muiRenderInfo(obj), &data->coast_penspec, 0);
	}
	SetAPen(rp, MUIPEN(data->coast_pen));

	if (data->resolution == 0)
		draw_coastline(rp, obj, data, data->proj_full_x, data->proj_full_y, coastline_full_lengths, COASTLINE_FULL_POLYLINE_COUNT);
	else
		draw_coastline(rp, obj, data, data->proj_reduced_x, data->proj_reduced_y, coastline_reduced_lengths, COASTLINE_REDUCED_POLYLINE_COUNT);

	if (data->zoom_center_set) { draw_cross(rp, obj, data); }

	if (MyTimerBase)
	{
		ReadEClock(&t2);
		ticks = t2.ev_lo - t1.ev_lo;
		data->draw_time_ms = (ticks * 1000) / freq;
	}

	return 0;
}

/*-----------------------------------------------------------------------------
- mHandleEvent
------------------------------------------------------------------------------*/
LONG mHandleEvent (Class *cl, Object *obj, struct MUIP_HandleEvent *msg)
{
	struct MapData *data = INST_DATA(cl, obj);

	if (msg->imsg)
	{
		if (msg->imsg->Class == IDCMP_MOUSEBUTTONS &&
			msg->imsg->Code == SELECTDOWN)
		{
			short mx = msg->imsg->MouseX;
			short my = msg->imsg->MouseY;
			short x0 = _mleft(obj);
			short y0 = _mtop(obj);
			short x1 = x0 + _mwidth(obj);
			short y1 = y0 + _mheight(obj);

			if (mx >= x0 && mx < x1 && my >= y0 && my < y1)
			{
				short lon, lat;

				screen_to_lonlat(obj, data, mx, my, &lon, &lat);

				if (lon >= -18000 && lon <= 18000 && lat >= -9000 && lat <= 9000)
				{
					data->zoom_center_lon = lon;
					data->zoom_center_lat = lat;
					data->zoom_center_set = TRUE;
					SetAttrs(obj,
						MYATTR_CenterLon, (LONG)lon,
						MYATTR_CenterLat, (LONG)lat,
						MYATTR_CenterSet, TRUE,
					TAG_DONE);
				}
			}
		}
	}
	return 0;
}

/*-----------------------------------------------------------------------------
- mShow
------------------------------------------------------------------------------*/
LONG mShow (Class *cl, Object *obj, Msg msg)
{
	read_prefs(cl, obj);
	MUI_Redraw(obj, MADF_DRAWOBJECT);
	return DoSuperMethodA(cl, obj, (Msg)msg);
}

/*-----------------------------------------------------------------------------
- mGet
------------------------------------------------------------------------------*/
LONG mGet(Class *cl, Object *obj, struct opGet *msg)
{
	struct MapData *data = INST_DATA(cl, obj);

	switch (msg->opg_AttrID)
	{
		case MYATTR_Zoom:
			*msg->opg_Storage = (ULONG)data->zoom;
			return TRUE;

		case MYATTR_CenterLon:
			*msg->opg_Storage = (ULONG)data->zoom_center_lon;
			return TRUE;

		case MYATTR_CenterLat:
			*msg->opg_Storage = (ULONG)data->zoom_center_lat;
			return TRUE;

		case MYATTR_CenterSet:
			*msg->opg_Storage = (ULONG)data->zoom_center_set;
			return TRUE;

		case MYATTR_DrawTime:
			*msg->opg_Storage = (ULONG)data->draw_time_ms;
			return TRUE;
	}

	return DoSuperMethodA(cl, obj, (Msg)msg);
}

/*-----------------------------------------------------------------------------
- mSet
------------------------------------------------------------------------------*/
LONG mSet(Class *cl, Object *obj, struct opSet *msg)
{
	struct MapData *data = INST_DATA(cl, obj);
	struct TagItem *tag;
	struct TagItem *tstate = msg->ops_AttrList;
	BOOL redraw = FALSE;

	while ((tag = NextTagItem(&tstate)))
	{
		switch (tag->ti_Tag)
		{
			case MYATTR_Zoom:
				data->zoom = (long)tag->ti_Data;
				if (data->zoom < 100) data->zoom = 100;
				if (data->zoom > 1600) data->zoom = 1600;
				redraw = TRUE;
				break;

			case MYATTR_CenterLon:
				data->zoom_center_lon = (short)tag->ti_Data;
				break;

			case MYATTR_CenterLat:
				data->zoom_center_lat = (short)tag->ti_Data;
				break;

			case MYATTR_CenterSet:
				data->zoom_center_set = (BOOL)tag->ti_Data;
				redraw = TRUE;
				break;
		}
	}

	if (redraw) MUI_Redraw(obj, MADF_DRAWOBJECT);
	return DoSuperMethodA(cl, obj, (Msg)msg);
}

/*-----------------------------------------------------------------------------
- mZoomIn
------------------------------------------------------------------------------*/
LONG mZoomIn(Class *cl, Object *obj, Msg msg)
{
	struct MapData *data = INST_DATA(cl, obj);

	data->zoom = data->zoom * (100 + data->zoom_step) / 100;
	if (data->zoom > 1600) data->zoom = 1600;
	data->zoom_center_set = TRUE;
	MUI_Redraw(obj, MADF_DRAWOBJECT);

	return 0;
}

/*-----------------------------------------------------------------------------
- mZoomOut
------------------------------------------------------------------------------*/
LONG mZoomOut(Class *cl, Object *obj, Msg msg)
{
	struct MapData *data = INST_DATA(cl, obj);

	data->zoom = data->zoom * 100 / (100 + data->zoom_step);
	if (data->zoom < 100) data->zoom = 100;
	data->zoom_center_set = TRUE;
	MUI_Redraw(obj, MADF_DRAWOBJECT);

	return 0;
}

/*-----------------------------------------------------------------------------
- mReset
------------------------------------------------------------------------------*/
LONG mReset(Class *cl, Object *obj, Msg msg)
{
	struct MapData *data = INST_DATA(cl, obj);

	data->zoom = 100;
	data->zoom_center_set = FALSE;
	data->zoom_center_lon = 0;
	data->zoom_center_lat = 0;
	MUI_Redraw(obj, MADF_DRAWOBJECT);

	return 0;
}

/*-----------------------------------------------------------------------------
- mPanNorth
------------------------------------------------------------------------------*/
LONG mPanNorth(Class *cl, Object *obj, Msg msg)
{
	struct MapData *data = INST_DATA(cl, obj);

	if (data->zoom_center_set)
	{
		data->zoom_center_lat += data->pan_step;
		if (data->zoom_center_lat > 9000) data->zoom_center_lat = 9000;
		MUI_Redraw(obj, MADF_DRAWOBJECT);
	}

	return 0;
}

/*-----------------------------------------------------------------------------
- mPanSouth
------------------------------------------------------------------------------*/
LONG mPanSouth(Class *cl, Object *obj, Msg msg)
{
	struct MapData *data = INST_DATA(cl, obj);

	if (data->zoom_center_set)
	{
		data->zoom_center_lat -= data->pan_step;
		if (data->zoom_center_lat < -9000) data->zoom_center_lat = -9000;
		MUI_Redraw(obj, MADF_DRAWOBJECT);
	}

	return 0;
}

/*-----------------------------------------------------------------------------
- mPanWest
------------------------------------------------------------------------------*/
LONG mPanWest(Class *cl, Object *obj, Msg msg)
{
	struct MapData *data = INST_DATA(cl, obj);

	if (data->zoom_center_set)
	{
		data->zoom_center_lon -= data->pan_step;
		if (data->zoom_center_lon < -18000) data->zoom_center_lon = -18000;
		MUI_Redraw(obj, MADF_DRAWOBJECT);
	}

	return 0;
}

/*-----------------------------------------------------------------------------
- mPanEast
------------------------------------------------------------------------------*/
LONG mPanEast(Class *cl, Object *obj, Msg msg)
{
	struct MapData *data = INST_DATA(cl, obj);

	if (data->zoom_center_set)
	{
		data->zoom_center_lon += data->pan_step;
		if (data->zoom_center_lon > 18000) data->zoom_center_lon = 18000;
		MUI_Redraw(obj, MADF_DRAWOBJECT);
	}

	return 0;
}

/*-----------------------------------------------------------------------------
- WorldmapDispatcher
------------------------------------------------------------------------------*/
LONG WorldmapDispatcher(register __a0 Class *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	switch (msg->MethodID)
	{
		case MUIM_Setup:		return mSetup(cl, obj, msg);
		case MUIM_Cleanup:		return mCleanup(cl, obj, msg);
		case MUIM_AskMinMax:	return mAskMinMax(cl, obj, (struct MUIP_AskMinMax *)msg);
		case MUIM_Draw:			return mDraw(cl, obj, (struct MUIP_Draw *)msg);
		case MUIM_HandleEvent:	return mHandleEvent(cl, obj, (struct MUIP_HandleEvent *)msg);
		case MUIM_Show:			return mShow(cl, obj, msg);
		case OM_GET:			return mGet(cl, obj, (struct opGet *)msg);
		case OM_SET:			return mSet(cl, obj, (struct opSet *)msg);
		case MYMETH_ZoomIn:		return mZoomIn(cl, obj, msg);
		case MYMETH_ZoomOut:	return mZoomOut(cl, obj, msg);
		case MYMETH_Reset:		return mReset(cl, obj, msg);
		case MYMETH_PanNorth:	return mPanNorth(cl, obj, msg);
		case MYMETH_PanSouth:	return mPanSouth(cl, obj, msg);
		case MYMETH_PanWest:	return mPanWest(cl, obj, msg);
		case MYMETH_PanEast:	return mPanEast(cl, obj, msg);
		default:				return DoSuperMethodA(cl, obj, msg);
	}
}

/******************************************************************************
* Private Functions
*******************************************************************************/
/*-----------------------------------------------------------------------------
- read_prefs
------------------------------------------------------------------------------*/
void read_prefs(Class *cl, Object *obj)
{
	struct MapData *data = INST_DATA(cl, obj);
	LONG *val;

	if (DoMethod(obj, MUIM_GetConfigItem, MUICFG_Worldmap_Resolution, &val))
	{
		if (data->resolution != *val)
		{
			data->resolution = *val;
			data->resolution_changed = TRUE;
		}
	}
	else
		data->resolution = DEFAULT_RESOLUTION;

	if (DoMethod(obj, MUIM_GetConfigItem, MUICFG_Worldmap_CoastPen, &val))
	{
		data->coast_penspec = *(struct MUI_PenSpec *)val;
		data->coast_penchange = TRUE;
	}
	else
	{
		DoMethod(obj, MUIM_KillNotify, MUICFG_Worldmap_CoastPen);
	}

	if (DoMethod(obj, MUIM_GetConfigItem, MUICFG_Worldmap_CrossPen, &val))
	{
		data->cross_penspec = *(struct MUI_PenSpec *)val;
		data->cross_penchange = TRUE;
	}
	else
	{
		DoMethod(obj, MUIM_KillNotify, MUICFG_Worldmap_CrossPen, &val);
	}

	if (DoMethod(obj, MUIM_GetConfigItem, MUICFG_Worldmap_ZoomStep, &val))
		data->zoom_step = *val;
	else
		data->zoom_step = DEFAULT_ZOOM_STEP;

	if (DoMethod(obj, MUIM_GetConfigItem, MUICFG_Worldmap_PanStep, &val))
		data->pan_step = *val;
	else
		data->pan_step = DEFAULT_PAN_STEP;

	if (DoMethod(obj, MUIM_GetConfigItem, MUICFG_Worldmap_CrossSize, &val))
		data->cross_size = *val;
	else
		data->cross_size = DEFAULT_CROSS_SIZE;
}

/*-----------------------------------------------------------------------------
- get_map_coords
------------------------------------------------------------------------------*/
void get_map_coords(Object *obj, struct MapData *data, short *x0, short *y0, short *cx, short *cy)
{
	*x0 = _mleft(obj);
	*y0 = _mtop(obj);

	if (data->zoom_center_set)
	{
		*cx = (short)(*x0 + ((long)(data->zoom_center_lon + 18000) * _mwidth(obj)) / 36000);
		*cy = (short)(*y0 + ((long)(9000 - data->zoom_center_lat) * _mheight(obj)) / 18000);
	}
	else
	{
		*cx = *x0 + _mwidth(obj) / 2;
		*cy = *y0 + _mheight(obj) / 2;
	}
}

/*-----------------------------------------------------------------------------
- project_points
------------------------------------------------------------------------------*/
void project_points(Object *obj, struct MapData *data)
{
	if (data->resolution == 0)
	{
		project_dataset(obj, data, coastline_full_points, COASTLINE_FULL_TOTAL_POINTS,
			data->proj_full_x, data->proj_full_y);
	}
	else
	{
		project_dataset(obj, data, coastline_reduced_points, COASTLINE_REDUCED_TOTAL_POINTS,
			data->proj_reduced_x, data->proj_reduced_y);
	}
}

/*-----------------------------------------------------------------------------
- screen_to_lonlat
------------------------------------------------------------------------------*/
void screen_to_lonlat(Object *obj, struct MapData *data, short mx, short my, short *lon, short *lat)
{
	short x0, y0, cx, cy;
	long bx, by;

	get_map_coords(obj, data, &x0, &y0, &cx, &cy);
	bx = cx + ((long)(mx - cx) * 100L) / data->zoom;
	by = cy + ((long)(my - cy) * 100L) / data->zoom;

	*lon = (short)(((bx - x0) * 36000L) / _mwidth(obj)) - 18000;
	*lat = 9000 - (short)(((by - y0) * 18000L) / _mheight(obj));
}

/*-----------------------------------------------------------------------------
- draw_cross
------------------------------------------------------------------------------*/
void draw_cross(struct RastPort *rp, Object *obj, struct MapData *data)
{
	if (data->cross_penchange)
	{
		data->cross_penchange = FALSE;
		MUI_ReleasePen(muiRenderInfo(obj), data->cross_pen);
		data->cross_pen = MUI_ObtainPen(muiRenderInfo(obj), &data->cross_penspec, 0);
	}

	SetAPen(rp, MUIPEN(data->cross_pen));
	Move(rp, data->zoom_cx - data->cross_size, data->zoom_cy);
	Draw(rp, data->zoom_cx + data->cross_size, data->zoom_cy);
	Move(rp, data->zoom_cx, data->zoom_cy - data->cross_size);
	Draw(rp, data->zoom_cx, data->zoom_cy + data->cross_size);
}

/*-----------------------------------------------------------------------------
- draw_coastline
------------------------------------------------------------------------------*/
void draw_coastline(struct RastPort *rp, Object *obj, struct MapData *data,
	const short *proj_x, const short *proj_y, const short *lengths, short linecount)
{
	short x0 = _mleft(obj);
	short y0 = _mtop(obj);
	short x1 = x0 + _mwidth(obj);
	short y1 = y0 + _mheight(obj);

	BOOL pendown;
	int i, j, idx = 0;

	for (i = 0; i < linecount; i++)
	{
		int count = lengths[i];
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
}

/*-----------------------------------------------------------------------------
- project_dataset
------------------------------------------------------------------------------*/
void project_dataset(Object *obj, struct MapData *data,
	const short *points, int total_points,
	short *proj_x, short *proj_y)
{
	short x0, y0, cx, cy;
	int i;

	get_map_coords(obj, data, &x0, &y0, &cx, &cy);
	data->zoom_cx = cx;
	data->zoom_cy = cy;

	for (i = 0; i < total_points; i++)
	{
		short bx = (short)(x0 + ((long)(points[i * 2] + 18000) * _mwidth(obj)) / 36000);
		short by = (short)(y0 + ((long)(9000 - points[i * 2 + 1]) * _mheight(obj)) / 18000);
		proj_x[i] = (short)(cx + ((long)(bx - cx) * data->zoom) / 100);
		proj_y[i] = (short)(cy + ((long)(by - cy) * data->zoom) / 100);
	}
}
