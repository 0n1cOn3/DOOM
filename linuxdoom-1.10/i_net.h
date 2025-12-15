// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//	System specific network interface stuff.
//
//-----------------------------------------------------------------------------


#ifndef __I_NET__
#define __I_NET__


#ifdef __GNUG__
#pragma interface
#endif

#include "d_net.h"

// Called by D_DoomMain.


void I_InitNetwork (void);
void I_ShutdownNetwork(void);
void I_NetCmd (void);

// Shared helpers for packing and unpacking network payloads.
void I_NetPackBuffer(const doomdata_t *src, doomdata_t *dest);
void I_NetUnpackBuffer(const doomdata_t *src, doomdata_t *dest);

// Simple regression harness entry point.
int I_RunNetworkHarness(int argc, char **argv);


#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
