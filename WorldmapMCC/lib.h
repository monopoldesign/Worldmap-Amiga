#ifndef LIB_H
#define LIB_H

#include <exec/libraries.h>

/*
   Dies ist die Basisstruktur f�r die neue Bibliothek.
   
   Das erste Element ist immer ein struct Library, dann folgen die �ffentlich
   zug�nglichen Elemente. Private Elemente der Bibliothek sollte man in
   normalen Variablen deklarieren, nicht in dieser Struktur.
*/

struct LibBase
{
	struct Library base;
	int last_result;
};

#endif
