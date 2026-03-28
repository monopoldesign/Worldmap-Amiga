#ifndef WORLDMAP_H
#define WORLDMAP

#include <exec/libraries.h>
#include <libraries/mui.h>

struct WorldMapBase
{
	struct Library base;
	LONG dummy;
};

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

#endif
