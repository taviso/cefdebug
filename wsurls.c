#include <stdio.h>
#if defined(_WIN32)
#include <winsock2.h>
#include <windows.h>
#endif

#include "libwebsockets.h"
#include "cefscan.h"

// The JSON path for the websocket URL
// https://chromedevtools.github.io/devtools-protocol/
static const char kDebuggerPath[] = "pages[].webSocketDebuggerUrl";

// libwebsockets callback for HTTP events.
static int callback_http(struct lws *wsi,
                         enum lws_callback_reasons reason,
                         void *user,
                         void *in,
                         size_t len);

static const struct lws_protocols protocols[] = {
        { "http", callback_http, 0, 0 },
        { 0 }
};

static uint32_t ActiveConnect;

static signed char callback_json(struct lejp_ctx *ctx, char reason)
{
    char ***wsurls = ctx->user;
    uint32_t urlcount;

    // The only event I'm interested in is when a string value has been seen.
    if (reason != LEJPCB_VAL_STR_END)
        return 0;

    lwsl_info("inspecting key %s\n", ctx->path);

    if (strcmp(ctx->path, kDebuggerPath) != 0)
        return 0;

    lwsl_info("key matches, value is %s\n", ctx->buf);

    // This is a websocket url. AFAIK, this can only ever be called from one
    // thread, so no mutex needed.
    for (urlcount = 0; wsurls[urlcount]; urlcount++)
        ;

    // Append to the list of known URLs.
    *wsurls = realloc(*wsurls, (urlcount + 2) * sizeof(char *));
    (*wsurls)[urlcount++] = strdup(ctx->buf);
    (*wsurls)[urlcount++] = NULL;

    return 0;
}

static int callback_http(struct lws *wsi,
                         enum lws_callback_reasons reason,
                         void *user,
                         void *in,
                         size_t len)
{
    lwsl_debug("callback_http() ctx %p, reason %u\n", wsi, reason);

    switch (reason) {
        case LWS_CALLBACK_WSI_DESTROY:
            // Update number of active connections.
            ActiveConnect--;
            free(user);
            break;
        case LWS_CALLBACK_WSI_CREATE:
            // Update number of active connections.
            ActiveConnect++;
            break;
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            lwsl_info("ctx %p connection error: %s\n", wsi, in);
            break;
        case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:
            lwsl_info("ctx %p http session establised, status %d\n",
                      wsi,
                      lws_http_client_http_response(wsi));

            if (lws_http_client_http_response(wsi) != HTTP_STATUS_OK) {
                return -1;
            }

            // Add JSON Preamble, or the libwebsocket parser will reject it.
            lejp_parse(user, "{ \"pages\": ", 11);
            break;

        case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
            // Stream data to JSON parser.
            if (lejp_parse(user, in, len) < 0) {
                return -1;
            }

            break;
        case LWS_CALLBACK_RECEIVE_CLIENT_HTTP: {
            // This is a quirk of libwebsocket, you must guarantee buffers you
            // provide have LWS_PRE bytes behind them (?!?).
            // https://libwebsockets.org/lws-api-doc-master/html/group__sending-data.html#gafd5fdd285a0e25ba7e3e1051deec1001
            char buffer[LWS_PRE + 1024];
            char *px = &buffer[LWS_PRE];
            int lenx = sizeof(buffer) - LWS_PRE;

            if (lws_http_client_read(wsi, &px, &lenx) < 0) {
                return -1;
            }

            return 0;
        }

        case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:
            // Cleanup JSON parser, I don't care if it fails.
            lejp_parse(user, "}", 1);
            lejp_destruct(user);
            break;
    }

    return lws_callback_http_dummy(wsi, reason, user, in, len);
}

int32_t get_websocket_urls(char *hostname, uint16_t *portlist, char **wsurls[])
{
    int32_t result;
    struct lws_context *context;
    struct lws_context_creation_info info = {
        .port       = CONTEXT_PORT_NO_LISTEN,
        .protocols  = protocols,
    };

    lwsl_info("get_websocket_urls(\"%s\", %p, %p);\n", hostname, portlist, wsurls);

    result  = -1;
    context = lws_create_context(&info);
    *wsurls = NULL;

    for (; *portlist; portlist++) {
        struct lws *ctx;
        struct lejp_ctx *jsctx;
        struct lws_client_connect_info conn = {
            .context    = context,
            .port       = *portlist,
            .address    = hostname,
            .alpn       = "http/1.1",
            .path       = "/json/list",
            .method     = "GET",
            .protocol   = "http",
        };

        jsctx           = malloc(sizeof *jsctx);
        conn.userdata   = jsctx;
        conn.host       = calloc(strlen(hostname) + 1 + 5 + 1, 1);

        #pragma warning(suppress: 4090)
        sprintf(conn.host, "%s:%u", hostname, conn.port);

        lejp_construct(jsctx, callback_json, wsurls, NULL, 0);

        ctx = lws_client_connect_via_info(&conn);

        lwsl_info("connect %s, ctx %p\n", conn.host, ctx);

        #pragma warning(suppress: 4090)
        free(conn.host);
    }

    while (lws_service(context, 0) >= 0 && ActiveConnect)
        lwsl_info("waiting for %u jobs to complete\n", ActiveConnect);

    for (result = 0; *wsurls && (*wsurls)[result]; result++) {
        lwsl_info("discovered ws url %s\n", (*wsurls)[result]);
    }

    lwsl_info("total ws urls discovered %d\n", result);

error:
    lws_context_destroy(context);
    return result;
}
