#ifndef MCP_H
#define MCP_H

#include <exec/libraries.h>

/******************************************************************************
* Definitions
*******************************************************************************/
struct WorldmapBase
{
	struct Library base;
};

#define MUICFG_Worldmap_Resolution		0x80420201UL
#define MUICFG_Worldmap_CoastPen		0x80420202UL
#define MUICFG_Worldmap_CrossPen		0x80420203UL
#define MUICFG_Worldmap_ZoomStep		0x80420204UL
#define MUICFG_Worldmap_PanStep			0x80420205UL
#define MUICFG_Worldmap_CrossSize		0x80420206UL

#define DEFAULT_RESOLUTION		0
#define DEFAULT_COAST_PEN		"m1"
#define DEFAULT_CROSS_PEN		"m2"
#define DEFAULT_ZOOM_STEP		20
#define DEFAULT_PAN_STEP		500
#define DEFAULT_CROSS_SIZE		5

#define STR(x)  STR2(x)
#define STR2(x) #x

#define LIB_VERSION    11
#define LIB_REVISION   0
#define LIB_DATE       "31.03.2026"
#define LIB_COPYRIGHT  "Copyright (C) 2026 M.Volkel"
#define LIB_REV_STRING STR(LIB_VERSION) "." STR(LIB_REVISION)

/******************************************************************************
* Prototypes
*******************************************************************************/
struct MUI_CustomClass * MCP_Query(register __d0 LONG which, register __a6 struct WorldmapBase *base);

LONG WorldmapDispatcher(register __a0 Class *cl, register __a2 Object *obj, register __a1 Msg msg);
ULONG xget(Object *obj, ULONG attr);

#endif
