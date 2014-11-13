#include <kernel.h>

/*
statename - devuelve un string que describe el estado de una tarea, longitud
			máxima 9 caracteres.
*/

const char *
statename(unsigned state)
{
	static const char *states[] =
	{
		"suspended",
		"ready", 
		"current", 
		"delay", 
		"wait", 
		"send", 
		"receive", 
		"join",
		"zombie",
		"finished" 
	};
	static const unsigned nstates = sizeof states / sizeof(char *);

	return state >= nstates ? "???" : states[state];
}

