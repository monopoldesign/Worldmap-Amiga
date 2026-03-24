/* This file contains empty template routines that
 * the IDCMP handler will call uppon. Fill out these
 * routines with your code or use them as a reference
 * to create your program.
 */

#include <exec/types.h>
#include <intuition/intuition.h>
#include <proto/graphics.h>

#include "gui.h"

#define PAN_STEP	500
#define ZOOM_STEP	120

extern long zoom;
extern BOOL zoom_center_set;
extern short zoom_center_lon;
extern short zoom_center_lat;

extern void project_points(struct Window *win);
extern void draw(struct Window *win);
extern void update_zoom_display(void);

int Gadget00Clicked( void )
{
	/* routine when gadget "N" is clicked. */
	zoom_center_lat += PAN_STEP;
	if (zoom_center_lat > 9000) zoom_center_lat = 9000;
	project_points(WorldmapWnd);
	draw(WorldmapWnd);
	return TRUE;
}

int Gadget10Clicked( void )
{
	/* routine when gadget "S" is clicked. */
	zoom_center_lat -= PAN_STEP;
	if (zoom_center_lat < -9000) zoom_center_lat = -9000;
	project_points(WorldmapWnd);
	draw(WorldmapWnd);
	return TRUE;
}

int Gadget20Clicked( void )
{
	/* routine when gadget "W" is clicked. */
	zoom_center_lon -= PAN_STEP;
	if (zoom_center_lon < -18000) zoom_center_lon = -18000;
	project_points(WorldmapWnd);
	draw(WorldmapWnd);
	return TRUE;
}

int Gadget30Clicked( void )
{
	/* routine when gadget "E" is clicked. */
	zoom_center_lon += PAN_STEP;
	if (zoom_center_lon > 18000) zoom_center_lon = 18000;
	project_points(WorldmapWnd);
	draw(WorldmapWnd);
	return TRUE;
}

int Gadget40Clicked( void )
{
	/* routine when gadget "+" is clicked. */
	zoom = zoom * ZOOM_STEP / 100;
	if (zoom > 1600) zoom = 1600;
	if (!zoom_center_set) zoom_center_set = TRUE;
	update_zoom_display();
	project_points(WorldmapWnd);
	draw(WorldmapWnd);
	return TRUE;
}

int Gadget50Clicked( void )
{
	/* routine when gadget "-" is clicked. */
	zoom = zoom * 100 / ZOOM_STEP;
	if (zoom < 100) zoom = 100;
	update_zoom_display();
	project_points(WorldmapWnd);
	draw(WorldmapWnd);
	return TRUE;
}

int Gadget60Clicked( void )
{
	/* routine when gadget "Reset" is clicked. */
	zoom = 100;
	zoom_center_set = FALSE;
	zoom_center_lon = 0;
	zoom_center_lat = 0;
	update_zoom_display();
	project_points(WorldmapWnd);
	draw(WorldmapWnd);
	return TRUE;
}

int WorldmapCloseWindow( void )
{
	/* routine for "IDCMP_CLOSEWINDOW". */
	return FALSE;
}

int WorldmapNewSize( void )
{
	/* routine for "IDCMP_NEWSIZE". */
	project_points(WorldmapWnd);
	draw(WorldmapWnd);
	return TRUE;
}

int WorldmapRawKey( void )
{
	/* routine for "IDCMP_RAWKEY". */
	if (!(WorldmapMsg.Code & 0x80))
	{
		switch (WorldmapMsg.Code)
		{
			case 0x4c:
				return Gadget00Clicked();
			case 0x4d:
				return Gadget10Clicked();
			case 0x4f:
				return Gadget20Clicked();
			case 0x4e:
				return Gadget30Clicked();
		}
	}
	return TRUE;
}

