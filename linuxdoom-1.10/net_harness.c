#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "doomdef.h"
#include "doomstat.h"
#include "m_argv.h"
#include "i_net.h"

// Minimal global definitions required by the network layer.
doomcom_t* doomcom;
doomdata_t* netbuffer;
boolean netgame;

void I_Error(char *error, ...)
{
    va_list argptr;

    fprintf(stderr, "I_Error: ");
    va_start(argptr, error);
    vfprintf(stderr, error, argptr);
    va_end(argptr);
    fprintf(stderr, "\n");
    exit(1);
}

int main(int argc, char **argv)
{
    myargc = argc;
    myargv = argv;

    return I_RunNetworkHarness(argc, argv);
}
