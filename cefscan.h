#ifndef __CEFSCAN_H
#define __CEFSCAN_H

// Discover listening debuggers.
int32_t get_listening_ports(const char *localaddr, uint16_t **portlist);
int32_t get_websocket_urls(char *hostname, uint16_t *portlist, char **wsurls[]);

// Open and interact with debuggers.
void *dbg_open_handle(const char *wsurl);
void dbg_close_handle(void *handle);
char *dbg_eval_expression(void *handle, const char *expression);

#endif
