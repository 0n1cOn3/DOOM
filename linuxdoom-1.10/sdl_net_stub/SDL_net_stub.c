#include "SDL_net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

struct _UDPsocket {
    int fd;
    int family;
};

static char error_buffer[256];
static SDLNet_version linked_version = {2, 0, 0};

static void set_error(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vsnprintf(error_buffer, sizeof(error_buffer), fmt, args);
    va_end(args);
}

const SDLNet_version *SDLNet_Linked_Version(void)
{
    return &linked_version;
}

const char *SDLNet_GetError(void)
{
    if (error_buffer[0])
        return error_buffer;
    return "SDL_net stub";
}

int SDLNet_Init(void)
{
    error_buffer[0] = '\0';
    return 0;
}

void SDLNet_Quit(void)
{
}

static void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

UDPsocket SDLNet_UDP_Open(Uint16 port)
{
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    char service[16];
    int fd = -1;
    int yes = 1;
    int no = 0;
    UDPsocket sock = NULL;

    snprintf(service, sizeof(service), "%u", (unsigned)port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, service, &hints, &result) != 0)
    {
        set_error("SDLNet_UDP_Open getaddrinfo failed: %s", strerror(errno));
        return NULL;
    }

    fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (fd < 0)
    {
        set_error("SDLNet_UDP_Open socket failed: %s", strerror(errno));
        freeaddrinfo(result);
        return NULL;
    }

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));

    if (bind(fd, result->ai_addr, result->ai_addrlen) < 0)
    {
        set_error("SDLNet_UDP_Open bind failed: %s", strerror(errno));
        freeaddrinfo(result);
        close(fd);
        return NULL;
    }

    freeaddrinfo(result);

    set_nonblocking(fd);

    sock = (UDPsocket)malloc(sizeof(struct _UDPsocket));
    if (!sock)
    {
        close(fd);
        set_error("SDLNet_UDP_Open out of memory");
        return NULL;
    }

    sock->fd = fd;
    sock->family = AF_INET6;
    return sock;
}

void SDLNet_UDP_Close(UDPsocket sock)
{
    if (!sock)
        return;
    close(sock->fd);
    free(sock);
}

static void fill_ipaddress(IPaddress *dest, const struct sockaddr *addr, socklen_t len)
{
    const struct sockaddr_in *ipv4;
    const struct sockaddr_in6 *ipv6;

    memset(dest, 0, sizeof(*dest));

    if (addr->sa_family == AF_INET)
    {
        ipv4 = (const struct sockaddr_in *)addr;
        dest->family = AF_INET;
        memcpy(dest->host, &ipv4->sin_addr, 4);
        dest->port = ntohs(ipv4->sin_port);
    }
    else
    {
        ipv6 = (const struct sockaddr_in6 *)addr;
        dest->family = AF_INET6;
        memcpy(dest->host, &ipv6->sin6_addr, 16);
        dest->port = ntohs(ipv6->sin6_port);
    }

    /* Prevent buffer overflow by copying at most sizeof(dest->sockaddr) bytes */
    size_t copy_len = len < sizeof(dest->sockaddr) ? len : sizeof(dest->sockaddr);
    memcpy(dest->sockaddr, addr, copy_len);
    dest->sockaddr_len = copy_len;
}

int SDLNet_ResolveHost(IPaddress *address, const char *host, Uint16 port)
{
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    char service[16];
    int rc;

    snprintf(service, sizeof(service), "%u", (unsigned)port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    rc = getaddrinfo(host, service, &hints, &result);
    if (rc != 0)
    {
        set_error("SDLNet_ResolveHost: %s", gai_strerror(rc));
        return -1;
    }

    fill_ipaddress(address, result->ai_addr, result->ai_addrlen);
    freeaddrinfo(result);
    return 0;
}

int SDLNet_CompareAddresses(const IPaddress *a, const IPaddress *b)
{
    if (!a || !b)
        return 0;

    if (a->family != b->family || a->port != b->port)
        return 0;

    if (a->family == AF_INET)
        return memcmp(a->host, b->host, 4) == 0;

    return memcmp(a->host, b->host, 16) == 0;
}

int SDLNet_UDP_Send(UDPsocket sock, int channel, UDPpacket *packet)
{
    (void)channel;

    if (!sock || !packet)
        return -1;

    if (packet->address.sockaddr_len == 0)
    {
        set_error("SDLNet_UDP_Send missing address");
        return -1;
    }

    if (sendto(sock->fd, packet->data, packet->len, 0,
               (const struct sockaddr *)packet->address.sockaddr,
               (socklen_t)packet->address.sockaddr_len) < 0)
    {
        set_error("SDLNet_UDP_Send failed: %s", strerror(errno));
        return -1;
    }

    return 1;
}

int SDLNet_UDP_Recv(UDPsocket sock, UDPpacket *packet)
{
    struct sockaddr_storage from;
    socklen_t fromlen = sizeof(from);
    ssize_t result;

    if (!sock || !packet)
        return -1;

    result = recvfrom(sock->fd, packet->data, packet->maxlen, 0,
                      (struct sockaddr *)&from, &fromlen);

    if (result < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;

        set_error("SDLNet_UDP_Recv failed: %s", strerror(errno));
        return -1;
    }

    fill_ipaddress(&packet->address, (struct sockaddr *)&from, fromlen);
    packet->len = (int)result;
    return 1;
}

UDPpacket *SDLNet_AllocPacket(int size)
{
    UDPpacket *packet = (UDPpacket *)malloc(sizeof(UDPpacket));
    if (!packet)
        return NULL;

    packet->data = (Uint8 *)malloc(size);
    if (!packet->data)
    {
        free(packet);
        return NULL;
    }

    packet->maxlen = size;
    packet->len = 0;
    packet->channel = -1;
    packet->status = 0;
    memset(&packet->address, 0, sizeof(packet->address));

    return packet;
}

void SDLNet_FreePacket(UDPpacket *packet)
{
    if (!packet)
        return;
    free(packet->data);
    free(packet);
}

void SDLNet_Write16(Uint16 value, void *buf)
{
    Uint16 v = htons(value);
    memcpy(buf, &v, sizeof(v));
}

void SDLNet_Write32(Uint32 value, void *buf)
{
    Uint32 v = htonl(value);
    memcpy(buf, &v, sizeof(v));
}

Uint16 SDLNet_Read16(const void *buf)
{
    Uint16 v;
    memcpy(&v, buf, sizeof(v));
    return ntohs(v);
}

Uint32 SDLNet_Read32(const void *buf)
{
    Uint32 v;
    memcpy(&v, buf, sizeof(v));
    return ntohl(v);
}

void SDL_Delay(Uint32 ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000ULL;
    nanosleep(&ts, NULL);
}
