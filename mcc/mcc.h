#ifndef MCC_H
#define MCC_H

#include <exec/libraries.h>
#include <libraries/mui.h>

/******************************************************************************
* Definitions
*******************************************************************************/
#define MYATTR_Base			0x80420000

#define MYATTR_Zoom			(MYATTR_Base + 1)
#define MYATTR_CenterLon	(MYATTR_Base + 2)
#define MYATTR_CenterLat	(MYATTR_Base + 3)
#define MYATTR_CenterSet	(MYATTR_Base + 4)
#define MYATTR_DrawTime		(MYATTR_Base + 5)

#define MYMETH_Base			0x80420100

#define MYMETH_ZoomIn		(MYMETH_Base + 1)
#define MYMETH_ZoomOut		(MYMETH_Base + 2)
#define MYMETH_Reset		(MYMETH_Base + 3)
#define MYMETH_PanNorth		(MYMETH_Base + 4)
#define MYMETH_PanSouth		(MYMETH_Base + 5)
#define MYMETH_PanWest		(MYMETH_Base + 6)
#define MYMETH_PanEast		(MYMETH_Base + 7)

/******************************************************************************
* Prototypes
*******************************************************************************/
LONG WorldmapDispatcher(register __a0 Class *cl, register __a2 Object *obj, register __a1 Msg msg);

void read_prefs(Class *cl, Object *obj);

void get_map_coords(Object *obj, struct MapData *data, short *x0, short *y0, short *cx, short *cy);
void project_points(Object *obj, struct MapData *data);
void project_dataset(Object *obj, struct MapData *data, const short *points, int total_points, short *proj_x, short *proj_y);
void project_triangles(Object *obj, struct MapData *data);
void screen_to_lonlat(Object *obj, struct MapData *data, short mx, short my, short *lon, short *lat);

void clear_map(Object *obj, struct MapData *data);
void draw_cross(struct RastPort *rp, Object *obj, struct MapData *data);
void draw_coastline(struct RastPort *rp, Object *obj, struct MapData *data, const short *proj_x, const short *proj_y, const short *lengths, short linecount);
void draw_fill(struct RastPort *rp, Object *obj, struct MapData *data);
void draw_map(Object *obj, struct MapData *data);

#endif
