// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// SDL_net-backed networking shim for linuxdoom.
// Currently supports local single-player initialization and
// defers real networking to future parity work.
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>

#include "d_event.h"
#include "d_net.h"
#include "doomstat.h"
#include "i_net.h"
#include "i_system.h"
#include "m_argv.h"

void I_InitNetwork(void)
{
    if (SDLNet_Init() < 0)
    {
        I_Error("SDL_net init failed: %s", SDLNet_GetError());
    }

    atexit(SDLNet_Quit);

    doomcom = malloc(sizeof(*doomcom));
    if (!doomcom)
    {
        I_Error("Failed to allocate doomcom for SDL_net backend");
    }

    memset(doomcom, 0, sizeof(*doomcom));

    doomcom->ticdup = 1;
    doomcom->extratics = 0;
    doomcom->id = DOOMCOM_ID;
    doomcom->numplayers = doomcom->numnodes = 1;
    doomcom->consoleplayer = 0;
    doomcom->deathmatch = false;

    netgame = false;
}

void I_NetCmd(void)
{
    /* Networking is not yet implemented with SDL_net; placeholder to keep
       the engine happy when the backend is selected. */
}
