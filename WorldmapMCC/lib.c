#include <clib/utility_protos.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <libraries/mui.h>
#include <pragma/muimaster_lib.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/timer.h>

#include "lib.h"
#include "worldmap.h"
#include "coastline2.c"

#pragma libbase LibBase

struct MapData
{
	struct MUI_EventHandlerNode EHNode;
	short proj_x[COASTLINE_TOTAL_POINTS];
	short proj_y[COASTLINE_TOTAL_POINTS];
	short zoom_cx;
	short zoom_cy;
	long  zoom;
	BOOL  zoom_center_set;
	short zoom_center_lon;
	short zoom_center_lat;
	ULONG draw_time_ms;
};

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *UtilityBase;
struct Library *MUIMasterBase;

struct timerequest TimerRequest;
struct MsgPort *TimerPort = NULL;
struct Device *MyTimerBase = NULL;

struct MUI_CustomClass *WorldMapMCC = NULL;

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

void project_points(Object *obj, struct MapData *data)
{
	short x0, y0, cx, cy;
	int i;

	get_map_coords(obj, data, &x0, &y0, &cx, &cy);
	data->zoom_cx = cx;
	data->zoom_cy = cy;

	for (i = 0; i < COASTLINE_TOTAL_POINTS; i++)
	{
		short bx = (short)(x0 + ((long)(coastline_points[i * 2] + 18000) * _mwidth(obj)) / 36000);
		short by = (short)(y0 + ((long)(9000 - coastline_points[i * 2 + 1]) * _mheight(obj)) / 18000);
		data->proj_x[i] = (short)(cx + ((long)(bx - cx) * data->zoom) / 100);
		data->proj_y[i] = (short)(cy + ((long)(by - cy) * data->zoom) / 100);
	}
}

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

void draw_cross(struct RastPort *rp, short x, short y, short size)
{
	SetAPen(rp, 2);
	Move(rp, x - size, y);
	Draw(rp, x + size, y);
	Move(rp, x, y - size);
	Draw(rp, x, y + size);
}

/*-----------------------------------------------------------------------------
- mAskMinMax
------------------------------------------------------------------------------*/
LONG mAskMinMax(Class *cl, Object *obj, struct MUIP_AskMinMax *msg)
{
	DoSuperMethodA(cl, obj, (Msg)msg);

	msg->MinMaxInfo->MinWidth += 100;
	msg->MinMaxInfo->DefWidth += 400;
	msg->MinMaxInfo->MaxWidth += MUI_MAXMAX;
	msg->MinMaxInfo->MinHeight += 100;
	msg->MinMaxInfo->DefHeight += 200;
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
	int i, j, idx = 0;
	BOOL pendown;

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
	SetAPen(rp, 1);
	for (i = 0; i < COASTLINE_POLYLINE_COUNT; i++)
	{
		int count = coastline_lengths[i];
		pendown = FALSE;

		for (j = 0; j < count; j++)
		{
			short px = data->proj_x[idx + j];
			short py = data->proj_y[idx + j];

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

	if (data->zoom_center_set) { draw_cross(rp, data->zoom_cx, data->zoom_cy, 5); }

	if (MyTimerBase)
	{
		ReadEClock(&t2);
		ticks = t2.ev_lo - t1.ev_lo;
		data->draw_time_ms = (ticks * 1000) / freq;
	}

	return 0;
}

/*-----------------------------------------------------------------------------
- mSetup
------------------------------------------------------------------------------*/
LONG mSetup(Class *cl, Object *obj, Msg msg)
{
	struct MapData *data = INST_DATA(cl,obj);

	if (DoSuperMethodA(cl, obj, msg))
	{
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
	DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->EHNode);
	return DoSuperMethodA(cl, obj, msg);
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
					MUI_Redraw(obj, MADF_DRAWOBJECT);
				}
			}
		}
	}
	return 0;
}

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

LONG mSet(Class *cl, Object *obj, struct opSet *msg)
{
	struct MapData *data = INST_DATA(cl, obj);
	struct TagItem *tag;
	struct TagItem *tstate = msg->ops_AttrList;
	BOOL redraw = FALSE;

	while ((tag = NextTagItem(&tag)))
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
				redraw = TRUE;
				break;

			case MYATTR_CenterLat:
				data->zoom_center_lat = (short)tag->ti_Data;
				redraw = TRUE;
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

LONG mZoomIn(Class *cl, Object *obj, Msg msg)
{
	struct MapData *data = INST_DATA(cl, obj);
	data->zoom = data->zoom * 120 / 100;
	if (data->zoom > 1600) data->zoom = 1600;
	data->zoom_center_set = TRUE;
	MUI_Redraw(obj, MADF_DRAWOBJECT);
	return 0;
}

LONG mZoomOut(Class *cl, Object *obj, Msg msg)
{
	struct MapData *data = INST_DATA(cl, obj);
	data->zoom = data->zoom * 100 / 120;
	if (data->zoom < 100) data->zoom = 100;
	data->zoom_center_set = TRUE;
	MUI_Redraw(obj, MADF_DRAWOBJECT);
	return 0;
}

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

LONG mPanNorth(Class *cl, Object *obj, Msg msg)
{
	struct MapData *data = INST_DATA(cl, obj);
	if (data->zoom_center_set)
	{
		data->zoom_center_lat += 500;
		if (data->zoom_center_lat > 9000) data->zoom_center_lat = 9000;
		MUI_Redraw(obj, MADF_DRAWOBJECT);
	}
	return 0;
}

LONG mPanSouth(Class *cl, Object *obj, Msg msg)
{
	struct MapData *data = INST_DATA(cl, obj);
	if (data->zoom_center_set)
	{
		data->zoom_center_lat -= 500;
		if (data->zoom_center_lat < -9000) data->zoom_center_lat = -9000;
		MUI_Redraw(obj, MADF_DRAWOBJECT);
	}
	return 0;
}

LONG mPanWest(Class *cl, Object *obj, Msg msg)
{
	struct MapData *data = INST_DATA(cl, obj);
	if (data->zoom_center_set)
	{
		data->zoom_center_lon -= 500;
		if (data->zoom_center_lon < -18000) data->zoom_center_lon = -18000;
		MUI_Redraw(obj, MADF_DRAWOBJECT);
	}
	return 0;
}

LONG mPanEast(Class *cl, Object *obj, Msg msg)
{
	struct MapData *data = INST_DATA(cl, obj);
	if (data->zoom_center_set)
	{
		data->zoom_center_lon += 500;
		if (data->zoom_center_lon > 18000) data->zoom_center_lon = 18000;
		MUI_Redraw(obj, MADF_DRAWOBJECT);
	}
	return 0;
}

ULONG mNew(Class *cl, Object *obj, struct opSet *msg)
{
	struct MapData *data;

	obj = (Object *)DoSuperMethodA(cl, obj, (Msg)msg);
	if (!obj) return 0;

	data = INST_DATA(cl, obj);

	// ...init data...

	return (ULONG)obj;
}

ULONG mDispose(Class *cl, Object *obj, Msg msg)
{
	return DoSuperMethodA(cl, obj, msg);
}

LONG WorldmapDispatcher(register __a0 Class *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	switch (msg->MethodID)
	{
		case OM_NEW:			return mNew(cl, obj, (struct opSet *)msg);
		case OM_DISPOSE:		return mDispose(cl, obj, msg);
		case MUIM_Setup:		return mSetup(cl, obj, msg);
		case MUIM_Cleanup:		return mCleanup(cl, obj, msg);
		case MUIM_AskMinMax:	return mAskMinMax(cl, obj, (struct MUIP_AskMinMax *)msg);
		case MUIM_Draw:			return mDraw(cl, obj, (struct MUIP_Draw *)msg);
		case MUIM_HandleEvent:	return mHandleEvent(cl, obj, (struct MUIP_HandleEvent *)msg);
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

BOOL __LibOpen(register __a6 struct LibBase *base)
{
	return TRUE;
}

void __LibClose(register __a6 struct LibBase *base)
{
}

struct MUI_CustomClass * MCC_Query(register __d0 LONG which, register __a6 struct LibBase *base)
{
	switch (which)
	{
		case 0:
			if (!WorldMapMCC)
			{
				MUIMasterBase = OpenLibrary(MUIMASTER_NAME, MUIMASTER_VMIN);
				if (!MUIMasterBase) return NULL;

				WorldMapMCC = MUI_CreateCustomClass(
					(struct Library *)base, MUIC_Area, NULL,
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
			return WorldMapMCC;

		case 3:
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
			return NULL;
	}
	return NULL;
}
