#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#if defined(_WIN32)
# include <winsock2.h>
# include <windows.h>
#endif

#include "libwebsockets.h"

#include "cefscan.h"

static const char kRuntimeEnable[] = "{"
    "\"id\": %d,"
    "\"method\": \"Runtime.enable\""
"}";

static const char kRuntimeEvaluate[] = "{"
    "\"id\": %d,"
    "\"method\": \"Runtime.evaluate\","
    "\"params\": {"
        "\"expression\": \"eval(unescape('%s'))\"}"
"}";

static int callback_default_ws(struct lws *wsi,
                               enum lws_callback_reasons reason,
                               void *user,
                               void *in,
                               size_t len);

static const struct lws_protocols wsprotocols[] = {
        { "default", callback_default_ws, 0, 0 },
        { 0 }
};

struct dbg_context_handle {
    int msgid;
    int size;
    int pending;
    struct lws *ctx;
    char pre_buf[LWS_PRE];
    char message[1024];
    char *result;
    char *description;
};


static signed char callback_json_ws(struct lejp_ctx *ctx, char reason)
{
    struct dbg_context_handle *handle = ctx->user;

    if (reason != LEJPCB_VAL_STR_END && reason != LEJPCB_VAL_NUM_INT)
        return 0;

    lwsl_info("ws inspecting key %s\n", ctx->path);

    if (strcmp(ctx->path, "result.result.value") == 0) {
        handle->result = strdup(ctx->buf);
    }

    if (strcmp(ctx->path, "result.result.description") == 0) {
        handle->description = strdup(ctx->buf);
    }

    if (strcmp(ctx->path, "id") == 0) {
        lwsl_info("id %s\n", ctx->buf);

        if (strtoul(ctx->buf, NULL, 0) == handle->msgid) {
            handle->pending = false;
            handle->msgid++;
        }
    }

    return 0;
}

static char *url_escape_string(const char *str)
{
    char *result = calloc(strlen(str) * 3 + 1, 1);

    for (char *p = result; *str; str++) {
        if (isalnum(*str)) {
            *p++ = *str;
        } else {
            p += sprintf(p, "%%%02hhX", *str);
        }
    }

    return result;
}


static uint32_t ActiveConnect;

static int callback_default_ws(struct lws *wsi,
                               enum lws_callback_reasons reason,
                               void *user,
                               void *in,
                               size_t len)
{
    struct dbg_context_handle *handle = user;

    switch (reason) {
        // Update number of active connections.
        case LWS_CALLBACK_WSI_DESTROY:
            ActiveConnect--;
            break;
        case LWS_CALLBACK_WSI_CREATE:
            ActiveConnect++;
            break;
        case LWS_CALLBACK_CLIENT_RECEIVE: {
            struct lejp_ctx jsctx;

            lwsl_info("client %p received message \"%s\"\n", wsi, in);

            lejp_construct(&jsctx, callback_json_ws, user, NULL, 0);
            lejp_parse(&jsctx, in, len);
            lejp_destruct(&jsctx);
            break;
        }
        case LWS_CALLBACK_CLIENT_WRITEABLE: {
            lwsl_info("client %p is writeable, %d pending\n", wsi, handle->size);

            if (handle->size > 0) {
                lwsl_info("write \"%s\" to client %p\n", handle->message, wsi);
                lws_write(wsi, handle->message, handle->size, LWS_WRITE_TEXT);
                handle->size = 0;
            }

            if (handle->size < 0)
                return -1;
            break;
        }
    }

    return lws_callback_http_dummy(wsi, reason, user, in, len);
}

void *dbg_open_handle(const char *wsurl)
{
    struct lws_context *wscontext = NULL;
    struct lws_client_connect_info conn = {0};
    struct dbg_context_handle *handle = NULL;
    struct lws_context_creation_info wsinfo = {
        .port       = CONTEXT_PORT_NO_LISTEN,
        .protocols  = wsprotocols,
    };

    lwsl_info("dbg_open_handle(\"%s\");\n", wsurl);

    if (wsurl == NULL) {
        goto error;
    }

    if (strncmp(wsurl, "ws://127.0.0.1:", 15) != 0) {
        lwsl_info("does not appear to be a debugger url\n");
        goto error;
    }

    wscontext = lws_create_context(&wsinfo);
    handle    = calloc(sizeof(*handle), 1);

    // Skip to port
    wsurl += strlen("ws://127.0.0.1:");

    #pragma warning(suppress: 4090)
    conn.port     = strtoul(wsurl, &wsurl, 10);
    conn.context  = wscontext;
    conn.address  = "127.0.0.1";
    conn.host     = "127.0.0.1";
    conn.path     = wsurl;
    conn.alpn     = "http/1.1";
    conn.protocol = "http";

    conn.userdata = handle;
    handle->ctx   = lws_client_connect_via_info(&conn);

    lwsl_info("port %d, path %s, ctx %p\n", conn.port, conn.path, handle->ctx);

    handle->pending = true;
    handle->size    = lws_snprintf(handle->message,
                                   sizeof (handle->message),
                                   kRuntimeEnable,
                                   handle->msgid);

    lws_callback_on_writable(handle->ctx);
    lws_service(wscontext, 0);

    return handle;

error:
    if (wscontext)
        lws_context_destroy(wscontext);

    free(handle);
    return NULL;
}

void dbg_close_handle(void *handle)
{
    struct dbg_context_handle *dbg = handle;

    lwsl_info("dbg_close_handle(%p);\n", dbg);

    if (handle == NULL)
        goto error;

    // If there is a message still waiting to be sent, or 
    // a response hasn't been received, just keep calling
    // lws_service().
    while (dbg->size || dbg->pending)
        lws_service(lws_get_context(dbg->ctx), 0);

    dbg->size = -1;

    do {
        lws_callback_on_writable(dbg->ctx);
    } while (lws_service(lws_get_context(dbg->ctx), 0) >= 0 && ActiveConnect);

    lws_context_destroy(lws_get_context(dbg->ctx));

error:

    return;
}


char *dbg_eval_expression(void *handle, const char *expression)
{
    struct dbg_context_handle *dbg = handle;
    char *escaped = NULL; 
    char *result = NULL;

    lwsl_info("dbg_eval_expression(%p, \"%s\");\n", dbg, expression);

    if (handle == NULL)
        goto error;

    if (expression == NULL)
        goto error;

    lwsl_info("dbg->size %d, dbg->pending %d, dbg->msgid %d\n",
               dbg->size,
               dbg->pending,
               dbg->msgid);

    escaped = url_escape_string(expression);

    // If there is a message still waiting to be sent, or 
    // a response hasn't been received, just keep calling
    // lws_service().
    while (dbg->size || dbg->pending) {
        lws_callback_on_writable(dbg->ctx);
        lws_service(lws_get_context(dbg->ctx), 0);
    }

    lwsl_info("ready to send message\n");

    // OK, queue is clear.
    dbg->size = lws_snprintf(dbg->message,
                             sizeof (dbg->message),
                             kRuntimeEvaluate,
                             dbg->msgid,
                             escaped);
    dbg->pending = true;

    lwsl_info("final buffer \"%s\"\n", dbg->message);

    // If there is a message still waiting to be sent, or 
    // a response hasn't been received, just keep calling
    // lws_service().
    while (dbg->size || dbg->pending) {
        lws_callback_on_writable(dbg->ctx);
        lws_service(lws_get_context(dbg->ctx), 0);
    }

    result = dbg->description
        ? strdup(dbg->description)
        : strdup(dbg->result);

    free(dbg->result);
    free(dbg->description);
    dbg->result = NULL;
    dbg->description = NULL;

error:
    free(escaped);

    return result;
}
