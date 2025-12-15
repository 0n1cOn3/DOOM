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
// $Log:$
//
// DESCRIPTION:
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: m_bbox.c,v 1.1 1997/02/03 22:45:10 b1 Exp $";

#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#ifndef DOOM_USE_LEGACY_NETWORKING
#include <SDL_net.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#endif

#include "i_system.h"
#include "d_event.h"
#include "d_net.h"
#include "m_argv.h"

#include "doomstat.h"

#ifdef __GNUG__
#pragma implementation "i_net.h"
#endif
#include "i_net.h"

#define DOOM_DEFAULT_PORT (IPPORT_USERRESERVED + 0x1d)

void    NetSend (void);
boolean NetListen (void);

static void NetListenThunk(void);

void    (*netget) (void);
void    (*netsend) (void);

typedef uint16_t doom_port_t;

static int net_latency_ms = 0;
static int net_packet_loss = 0;

/*
 * Parse a positive integer from a command-line argument string.
 *
 * text:       String to parse
 * upperBound: Maximum allowed value (if > 0). Values <= 0 mean no upper limit.
 * fallback:   Value to return if parsing fails or result is non-positive
 *
 * Returns the parsed positive integer, clamped to upperBound if specified,
 * or fallback if the input is invalid.
 */
static int ParsePositiveIntArg(const char *text, int upperBound, int fallback)
{
    char *end = NULL;
    long value;

    if (!text || !*text)
        return fallback;

    value = strtol(text, &end, 10);
    if (end == text || value <= 0)
        return fallback;

    if (upperBound > 0 && value > upperBound)
        value = upperBound;

    return (int)value;
}

static void InitNetworkSimulation(void)
{
    static boolean seeded = false;
    int p;

    if (!seeded)
    {
        srand((unsigned int)time(NULL));
        seeded = true;
    }

    p = M_CheckParm("-netdelay");
    if (p && p < myargc - 1)
        net_latency_ms = ParsePositiveIntArg(myargv[p + 1], 2000, 0);

    p = M_CheckParm("-packetloss");
    if (p && p < myargc - 1)
        net_packet_loss = ParsePositiveIntArg(myargv[p + 1], 99, 0);
}

static boolean ShouldDropPacket(void)
{
    if (net_packet_loss <= 0)
        return false;

    return (rand() % 100) < net_packet_loss;
}

static void ApplyNetworkLatency(void)
{
    if (net_latency_ms <= 0)
        return;

    usleep((unsigned long)net_latency_ms * 1000);
}

#ifndef DOOM_USE_LEGACY_NETWORKING
typedef Uint16 netorder_16;
typedef Uint32 netorder_32;
static doom_port_t doomport = DOOM_DEFAULT_PORT;

static netorder_32 NetWrite32(uint32_t value)
{
    netorder_32 encoded;
    SDLNet_Write32(value, &encoded);
    return encoded;
}

static netorder_16 NetWrite16(uint16_t value)
{
    netorder_16 encoded;
    SDLNet_Write16(value, &encoded);
    return encoded;
}

static uint32_t NetRead32(const netorder_32 *value)
{
    return SDLNet_Read32(value);
}

static uint16_t NetRead16(const netorder_16 *value)
{
    return SDLNet_Read16(value);
}
#else
typedef uint16_t netorder_16;
typedef uint32_t netorder_32;
static doom_port_t doomport = DOOM_DEFAULT_PORT;

static netorder_32 NetWrite32(uint32_t value)
{
    return htonl(value);
}

static netorder_16 NetWrite16(uint16_t value)
{
    return htons(value);
}

static uint32_t NetRead32(const netorder_32 *value)
{
    return ntohl(*value);
}

static uint16_t NetRead16(const netorder_16 *value)
{
    return ntohs(*value);
}
#endif

void I_NetPackBuffer(const doomdata_t *src, doomdata_t *dest)
{
    int c;

    *dest = *src;
    dest->checksum = NetWrite32(src->checksum);

    for (c = 0; c < src->numtics; ++c)
    {
        dest->cmds[c].angleturn = NetWrite16(src->cmds[c].angleturn);
        dest->cmds[c].consistancy = NetWrite16(src->cmds[c].consistancy);
    }
}

void I_NetUnpackBuffer(const doomdata_t *src, doomdata_t *dest)
{
    int c;

    *dest = *src;
    dest->checksum = NetRead32((const netorder_32 *)&src->checksum);

    for (c = 0; c < dest->numtics; ++c)
    {
        dest->cmds[c].angleturn = NetRead16((const netorder_16 *)&src->cmds[c].angleturn);
        dest->cmds[c].consistancy = NetRead16((const netorder_16 *)&src->cmds[c].consistancy);
    }
}

#ifndef DOOM_USE_LEGACY_NETWORKING

static UDPsocket             udpsocket;
static UDPpacket            *recvpacket;
static UDPpacket            *sendpacket;
static IPaddress             sendaddress[MAXNETNODES];

static doom_port_t ParsePort(const char *text, doom_port_t fallback)
{
    char *end = NULL;
    long value;

    if (!text || !*text)
        return fallback;

    value = strtol(text, &end, 10);
    if (end == text || value <= 0 || value > 65535)
        return fallback;

    return (Uint16)value;
}

static boolean ResolveAddressSpec(const char *spec,
                                  Uint16 defaultPort,
                                  IPaddress *out)
{
    Uint16 port = defaultPort;
    char address[256];
    const char *portText = NULL;
    const char *endBracket;
    const char *colon;

    memset(address, 0, sizeof(address));

    if (!spec || !*spec)
        return false;

    if (spec[0] == '[')
    {
        size_t len;
        endBracket = strchr(spec, ']');
        if (!endBracket)
            return false;
        len = endBracket - spec - 1;
        if (len >= sizeof(address))
            len = sizeof(address) - 1;
        strncpy(address, spec + 1, len);
        address[len] = '\0';
        if (endBracket[1] == ':' && endBracket[2] != '\0')
            portText = endBracket + 2;
    }
    else
    {
        size_t len;
        colon = strrchr(spec, ':');
        if (colon && strchr(colon + 1, ':') == NULL)
        {
            portText = colon + 1;
            len = colon - spec;
            if (len >= sizeof(address))
                len = sizeof(address) - 1;
            strncpy(address, spec, len);
            address[len] = '\0';
        }
        else
        {
            strncpy(address, spec, sizeof(address) - 1);
            address[sizeof(address) - 1] = '\0';
        }
    }

    if (portText)
        port = ParsePort(portText, defaultPort);

    if (SDLNet_ResolveHost(out, address[0] ? address : spec, port) == -1)
        return false;

    return true;
}

static void EnsurePacketCapacity(int length)
{
    if (!recvpacket || recvpacket->maxlen < length)
    {
        if (recvpacket)
            SDLNet_FreePacket(recvpacket);
        recvpacket = SDLNet_AllocPacket(length);
    }

    if (!sendpacket || sendpacket->maxlen < length)
    {
        if (sendpacket)
            SDLNet_FreePacket(sendpacket);
        sendpacket = SDLNet_AllocPacket(length);
    }

    if (!recvpacket || !sendpacket)
        I_Error("Failed to allocate UDP packets");
}

void NetSend (void)
{
    doomdata_t wire;
    int length = doomcom->datalength;

    if (ShouldDropPacket())
        return;

    ApplyNetworkLatency();

    EnsurePacketCapacity(length);
    I_NetPackBuffer(netbuffer, &wire);

    memcpy(sendpacket->data, &wire, length);
    sendpacket->len = length;
    sendpacket->address = sendaddress[doomcom->remotenode];

    if (SDLNet_UDP_Send(udpsocket, -1, sendpacket) == 0)
        I_Error("SendPacket error: %s", SDLNet_GetError());
}

boolean NetListen (void)
{
    doomdata_t wire;
    int i;

    doomcom->remotenode = -1;

    if (!recvpacket)
        return false;

    if (SDLNet_UDP_Recv(udpsocket, recvpacket) <= 0)
        return false;

    if (ShouldDropPacket())
        return false;

    ApplyNetworkLatency();

    for (i = 0; i < doomcom->numnodes; ++i)
    {
        if (SDLNet_CompareAddresses(&recvpacket->address, &sendaddress[i]))
        {
            doomcom->remotenode = i;
            break;
        }
    }

    if (doomcom->remotenode == -1)
        return false;

    doomcom->datalength = recvpacket->len;
    memcpy(&wire, recvpacket->data, sizeof(wire));
    I_NetUnpackBuffer(&wire, netbuffer);

    return true;
}

static void NetListenThunk(void)
{
    if (!NetListen())
        doomcom->remotenode = -1;
}

void I_InitNetwork (void)
{
    int                 i;
    int                 p;

    doomcom = malloc (sizeof (*doomcom) );
    memset (doomcom, 0, sizeof(*doomcom) );

    if (SDLNet_Init() == -1)
        I_Error("SDLNet_Init failed: %s", SDLNet_GetError());

    InitNetworkSimulation();

    i = M_CheckParm ("-dup");
    if (i && i< myargc-1)
    {
        doomcom->ticdup = myargv[i+1][0]-'0';
        if (doomcom->ticdup < 1)
            doomcom->ticdup = 1;
        if (doomcom->ticdup > 9)
            doomcom->ticdup = 9;
    }
    else
        doomcom-> ticdup = 1;

    doomcom-> extratics = M_CheckParm ("-extratic") ? 1 : 0;

    p = M_CheckParm ("-port");
    if (p && p<myargc-1)
    {
        doomport = ParsePort(myargv[p+1], doomport);
        printf ("using alternate port %u\n", doomport);
    }

    i = M_CheckParm ("-net");
    if (!i)
    {
        netgame = false;
        doomcom->id = DOOMCOM_ID;
        doomcom->numplayers = doomcom->numnodes = 1;
        doomcom->deathmatch = false;
        doomcom->consoleplayer = 0;
        netbuffer = &doomcom->data;
        return;
    }

    netsend = NetSend;
    netget = NetListenThunk;
    netgame = true;

    doomcom->consoleplayer = myargv[i+1][0]-'1';

    doomcom->numnodes = 1;

    i++;
    while (++i < myargc && myargv[i][0] != '-')
    {
        if (!ResolveAddressSpec(myargv[i], doomport, &sendaddress[doomcom->numnodes]))
            I_Error("Couldn't resolve %s", myargv[i]);

        doomcom->numnodes++;
    }

    doomcom->id = DOOMCOM_ID;
    doomcom->numplayers = doomcom->numnodes;

    udpsocket = SDLNet_UDP_Open(doomport);
    if (!udpsocket)
        I_Error("BindToPort: %s", SDLNet_GetError());

    EnsurePacketCapacity(sizeof(doomdata_t));

    netbuffer = &doomcom->data;
}

void I_ShutdownNetwork(void)
{
    if (recvpacket)
    {
        SDLNet_FreePacket(recvpacket);
        recvpacket = NULL;
    }

    if (sendpacket)
    {
        SDLNet_FreePacket(sendpacket);
        sendpacket = NULL;
    }

    if (udpsocket)
    {
        SDLNet_UDP_Close(udpsocket);
        udpsocket = NULL;
    }

    SDLNet_Quit();

    if (doomcom)
    {
        free(doomcom);
        doomcom = NULL;
    }
    netbuffer = NULL;
}

void I_NetCmd (void)
{
    if (doomcom->command == CMD_SEND)
    {
        netsend ();
    }
    else if (doomcom->command == CMD_GET)
    {
        netget ();
    }
    else
        I_Error ("Bad net cmd: %i\n",doomcom->command);
}

int I_RunNetworkHarness(int argc, char **argv)
{
    doomdata_t expected;
    int attempts;
    static char *defaultArgs[] = { "net_harness", "-net", "1", "[::1]" };

    if (argc < 4)
    {
        myargc = 4;
        myargv = defaultArgs;
    }
    else
    {
        myargc = argc;
        myargv = argv;
    }

    I_InitNetwork();
    netbuffer = &doomcom->data;

    memset(&expected, 0, sizeof(expected));
    expected.player = 1;
    expected.numtics = 1;
    expected.cmds[0].forwardmove = 10;
    expected.cmds[0].sidemove = -4;
    expected.cmds[0].angleturn = 0x3456;
    expected.cmds[0].consistancy = 0x1234;
    expected.cmds[0].buttons = 0xAA;
    doomcom->datalength = (int)&(((doomdata_t *)0)->cmds[expected.numtics]);

    *netbuffer = expected;
    doomcom->remotenode = 1;
    NetSend();

    for (attempts = 0; attempts < 64; ++attempts)
    {
        if (NetListen())
            break;
        SDL_Delay(1);
    }

    if (doomcom->remotenode != 1)
    {
        fprintf(stderr, "Failed to receive loopback packet\n");
        I_ShutdownNetwork();
        return 1;
    }

    if (memcmp(netbuffer, &expected, doomcom->datalength) != 0)
    {
        fprintf(stderr, "Loopback packet mismatch\n");
        I_ShutdownNetwork();
        return 2;
    }

    printf("Network harness succeeded with %d attempts\n", attempts + 1);
    I_ShutdownNetwork();
    return 0;
}

#else

int     DOOMPORT =      (IPPORT_USERRESERVED +0x1d );

int                     sendsocket;
int                     insocket;

struct  sockaddr_in     sendaddress[MAXNETNODES];

int UDPsocket (void)
{
    int s;

    s = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s<0)
        I_Error ("can't create socket: %s",strerror(errno));

    return s;
}

void
BindToLocalPort
( int   s,
  int   port )
{
    int                 v;
    struct sockaddr_in  address;

    memset (&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = port;

    v = bind (s, (void *)&address, sizeof(address));
    if (v == -1)
        I_Error ("BindToPort: bind: %s", strerror(errno));
}

void PacketSend (void)
{
    int         c;
    doomdata_t  sw;

    if (ShouldDropPacket())
        return;

    ApplyNetworkLatency();

    I_NetPackBuffer(netbuffer, &sw);

    c = sendto (sendsocket , &sw, doomcom->datalength
                ,0,(void *)&sendaddress[doomcom->remotenode]
                ,sizeof(sendaddress[doomcom->remotenode]));

    (void)c;
}

void PacketGet (void)
{
    int                 i;
    int                 c;
    struct sockaddr_in  fromaddress;
    int                 fromlen;
    doomdata_t          sw;

    fromlen = sizeof(fromaddress);
    c = recvfrom (insocket, &sw, sizeof(sw), 0
                  , (struct sockaddr *)&fromaddress, &fromlen );
    if (c == -1 )
    {
        if (errno != EWOULDBLOCK)
            I_Error ("GetPacket: %s",strerror(errno));
        doomcom->remotenode = -1;
        return;
    }

    for (i=0 ; i<doomcom->numnodes ; i++)
        if ( fromaddress.sin_addr.s_addr == sendaddress[i].sin_addr.s_addr )
            break;

    if (i == doomcom->numnodes)
    {
        doomcom->remotenode = -1;
        return;
    }

    if (ShouldDropPacket())
    {
        doomcom->remotenode = -1;
        return;
    }

    ApplyNetworkLatency();

    doomcom->remotenode = i;
    doomcom->datalength = c;

    I_NetUnpackBuffer(&sw, netbuffer);
}

void NetSend(void)
{
    PacketSend();
}

boolean NetListen(void)
{
    PacketGet();
    return doomcom->remotenode != -1;
}

int GetLocalAddress (void)
{
    char                hostname[1024];
    struct hostent*     hostentry;
    int                 v;

    v = gethostname (hostname, sizeof(hostname));
    if (v == -1)
        I_Error ("GetLocalAddress : gethostname: errno %d",errno);

    hostentry = gethostbyname (hostname);
    if (!hostentry)
        I_Error ("GetLocalAddress : gethostbyname: couldn't get local host");

    return *(int *)hostentry->h_addr_list[0];
}

void I_InitNetwork (void)
{
    boolean             trueval = true;
    int                 i;
    int                 p;
    struct hostent*     hostentry;

    doomcom = malloc (sizeof (*doomcom) );
    memset (doomcom, 0, sizeof(*doomcom) );

    InitNetworkSimulation();

    i = M_CheckParm ("-dup");
    if (i && i< myargc-1)
    {
        doomcom->ticdup = myargv[i+1][0]-'0';
        if (doomcom->ticdup < 1)
            doomcom->ticdup = 1;
        if (doomcom->ticdup > 9)
            doomcom->ticdup = 9;
    }
    else
        doomcom-> ticdup = 1;

    if (M_CheckParm ("-extratic"))
        doomcom-> extratics = 1;
    else
        doomcom-> extratics = 0;

    p = M_CheckParm ("-port");
    if (p && p<myargc-1)
    {
        DOOMPORT = atoi (myargv[p+1]);
        printf ("using alternate port %i\n",DOOMPORT);
    }

    i = M_CheckParm ("-net");
    if (!i)
    {
        netgame = false;
        doomcom->id = DOOMCOM_ID;
        doomcom->numplayers = doomcom->numnodes = 1;
        doomcom->deathmatch = false;
        doomcom->consoleplayer = 0;
        netbuffer = &doomcom->data;
        return;
    }

    netsend = PacketSend;
    netget = PacketGet;
    netgame = true;

    doomcom->consoleplayer = myargv[i+1][0]-'1';

    doomcom->numnodes = 1;

    i++;
    while (++i < myargc && myargv[i][0] != '-')
    {
        sendaddress[doomcom->numnodes].sin_family = AF_INET;
        sendaddress[doomcom->numnodes].sin_port = htons(DOOMPORT);
        if (myargv[i][0] == '.')
        {
            sendaddress[doomcom->numnodes].sin_addr.s_addr
                = inet_addr (myargv[i]+1);
        }
        else
        {
            hostentry = gethostbyname (myargv[i]);
            if (!hostentry)
                I_Error ("gethostbyname: couldn't find %s", myargv[i]);
            sendaddress[doomcom->numnodes].sin_addr.s_addr
                = *(int *)hostentry->h_addr_list[0];
        }
        doomcom->numnodes++;
    }

    doomcom->id = DOOMCOM_ID;
    doomcom->numplayers = doomcom->numnodes;

    insocket = UDPsocket ();
    BindToLocalPort (insocket,htons(DOOMPORT));
    ioctl (insocket, FIONBIO, &trueval);

    sendsocket = UDPsocket ();

    netbuffer = &doomcom->data;
}

void I_ShutdownNetwork(void)
{
    if (insocket)
    {
        close(insocket);
        insocket = 0;
    }

    if (sendsocket)
    {
        close(sendsocket);
        sendsocket = 0;
    }

    if (doomcom != NULL)
    {
        free(doomcom);
        doomcom = NULL;
        netbuffer = NULL;
    }
}

void I_NetCmd (void)
{
    if (doomcom->command == CMD_SEND)
    {
        netsend ();
    }
    else if (doomcom->command == CMD_GET)
    {
        netget ();
    }
    else
        I_Error ("Bad net cmd: %i\n",doomcom->command);
}

int I_RunNetworkHarness(int argc, char **argv)
{
    doomdata_t expected;
    int attempts;
    static char *defaultArgs[] = { "net_harness", "-net", "1", "127.0.0.1" };

    if (argc < 4)
    {
        myargc = 4;
        myargv = defaultArgs;
    }
    else
    {
        myargc = argc;
        myargv = argv;
    }

    I_InitNetwork();
    netbuffer = &doomcom->data;

    memset(&expected, 0, sizeof(expected));
    expected.player = 1;
    expected.numtics = 1;
    expected.cmds[0].forwardmove = 10;
    expected.cmds[0].sidemove = -4;
    expected.cmds[0].angleturn = 0x3456;
    expected.cmds[0].consistancy = 0x1234;
    expected.cmds[0].buttons = 0xAA;
    doomcom->datalength = (int)&(((doomdata_t *)0)->cmds[expected.numtics]);

    *netbuffer = expected;
    doomcom->remotenode = 1;
    NetSend();

    for (attempts = 0; attempts < 64; ++attempts)
    {
        if (NetListen())
            break;
        usleep(1000);
    }

    if (doomcom->remotenode != 1)
    {
        fprintf(stderr, "Failed to receive loopback packet\n");
        I_ShutdownNetwork();
        return 1;
    }

    if (memcmp(netbuffer, &expected, doomcom->datalength) != 0)
    {
        fprintf(stderr, "Loopback packet mismatch\n");
        I_ShutdownNetwork();
        return 2;
    }

    printf("Network harness succeeded with %d attempts\n", attempts + 1);
    I_ShutdownNetwork();
    return 0;
}

#endif
