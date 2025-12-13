#ifndef SDL_NET_STUB_H
#define SDL_NET_STUB_H

#include <stdint.h>

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;

typedef struct SDLNet_version {
    Uint8 major;
    Uint8 minor;
    Uint8 patch;
} SDLNet_version;

typedef struct IPaddress {
    Uint16 port;
    Uint8 host[16];
    int family;
    Uint8 _padding[8];
    unsigned char sockaddr[128];
    unsigned long sockaddr_len;
} IPaddress;

typedef struct _UDPpacket {
    int channel;
    Uint8 *data;
    int len;
    int maxlen;
    int status;
    IPaddress address;
} UDPpacket;

typedef struct _UDPsocket *UDPsocket;

typedef struct _SDLNet_SocketSet *SDLNet_SocketSet;

typedef struct _TCPsocket *TCPsocket;

typedef struct _SDLNet_GenericSocket *SDLNet_GenericSocket;

int SDLNet_Init(void);
void SDLNet_Quit(void);

const SDLNet_version *SDLNet_Linked_Version(void);

UDPsocket SDLNet_UDP_Open(Uint16 port);
void SDLNet_UDP_Close(UDPsocket sock);
int SDLNet_UDP_Send(UDPsocket sock, int channel, UDPpacket *packet);
int SDLNet_UDP_Recv(UDPsocket sock, UDPpacket *packet);

UDPpacket *SDLNet_AllocPacket(int size);
void SDLNet_FreePacket(UDPpacket *packet);

int SDLNet_ResolveHost(IPaddress *address, const char *host, Uint16 port);
int SDLNet_CompareAddresses(const IPaddress *a, const IPaddress *b);

const char *SDLNet_GetError(void);

void SDLNet_Write16(Uint16 value, void *buf);
void SDLNet_Write32(Uint32 value, void *buf);
Uint16 SDLNet_Read16(const void *buf);
Uint32 SDLNet_Read32(const void *buf);

void SDL_Delay(Uint32 ms);

#endif
