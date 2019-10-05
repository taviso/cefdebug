#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#if defined(_WIN32)
# include <winsock2.h>
# include <windows.h>
# include <iphlpapi.h>
#elif defined(__linux__)
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <arpa/inet.h>
#endif

#include "libwebsockets.h"
#include "cefscan.h"

#if defined(_WIN32)
int32_t get_listening_ports(const char *localaddr, uint16_t **portlist)
{
    PMIB_TCPTABLE tcptable;
    uint32_t tablesize;
    int32_t result;

    *portlist = NULL;
    tablesize = 0;
    result    = -1;

    if (GetTcpTable(NULL, &tablesize, FALSE) != ERROR_INSUFFICIENT_BUFFER) {
        goto error;
    }

    tcptable = _alloca(tablesize);

    if (GetTcpTable(tcptable, &tablesize, FALSE) != ERROR_SUCCESS) {
        goto error;
    }

    *portlist = calloc(tcptable->dwNumEntries + 1, sizeof **portlist);

    for (int i = result = 0; i < tcptable->dwNumEntries; i++) {
        if (tcptable->table[i].dwLocalAddr != inet_addr(localaddr))
            continue;

        if (tcptable->table[i].dwState != MIB_TCP_STATE_LISTEN)
            continue;

        (*portlist)[result] = tcptable->table[i].dwLocalPort;
        (*portlist)[result] = ntohs((*portlist)[result]);
        result++;
    }

error:
    return result;
}
#endif

#if defined(__linux__)
int32_t get_listening_ports(const char *localaddr, uint16_t **portlist)
{
    FILE *tcptable;
    char *line;
    int32_t result;
    size_t len;

    line      = NULL;
    len       = 0;
    result    = 0;
    *portlist = calloc(sizeof(uint16_t), 1);
    tcptable  = fopen("/proc/net/tcp", "r");

    // Skip the column headers.
    if (getline(&line, &len, tcptable) < 0) {
        goto error;
    }

    while (getline(&line, &len, tcptable) != -1) {
        uint32_t addr;
        uint16_t port;
        uint32_t state;
        int match;

        match = sscanf(line, "%*hhx: %x:%hx %*x:%*hx %02x",
                             &addr,
                             &port,
                             &state);

        if (match != 3)
            continue;

        if (state != TCP_LISTEN)
            continue;

        if (addr != inet_addr(localaddr))
            continue;

        *portlist = realloc(*portlist, (result + 2) * sizeof(uint16_t));
        (*portlist)[result+0] = port;
        (*portlist)[result+1] = 0;
        result++;
    }

error:

    if (tcptable)
        fclose(tcptable);
    if (result <= 0)
        free(*portlist);

    free(line);

    return result;
}
#endif
