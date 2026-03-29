#ifndef MCP_H
#define MCP_H

#include <exec/libraries.h>

/******************************************************************************
* Definitions
*******************************************************************************/
#define MUICFG_Worldmap_CoastColour		0x80420201UL
#define MUICFG_Worldmap_OceanColour		0x80420202UL
#define MUICFG_Worldmap_CrossColour		0x80420203UL
#define MUICFG_Worldmap_ZoomStep		0x80420204UL
#define MUICFG_Worldmap_PanStep			0x80420205UL
#define MUICFG_Worldmap_ShowCross		0x80420206UL

#define DEFAULT_COAST_COLOUR	1
#define DEFAULT_OCEAN_COLOUR	0
#define DEFAULT_CROSS_COLOUR	2
#define DEFAULT_ZOOM_STEP		20
#define DEFAULT_PAN_STEP		500
#define DEFAULT_SHOW_CROSS		1

/******************************************************************************
* Prototypes
*******************************************************************************/
struct MUI_CustomClass * MCP_Query(register __d0 LONG which);

LONG WorldmapDispatcher(register __a0 Class *cl, register __a2 Object *obj, register __a1 Msg msg);

ULONG xget(Object *obj, ULONG attr);
void DebugWrite(char *msg);

#endif
