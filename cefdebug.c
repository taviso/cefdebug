#include <stdio.h>

#if defined(_WIN32)
# include <winsock2.h>
# include <windows.h>
# include <iphlpapi.h>
# include "wineditline/src/editline/readline.h"
#elif defined(__linux__)
# include <readline.h>
# include <readline/history.h>
#endif

#include "libwebsockets.h"

#include "cefscan.h"

void print_usage(const char *name)
{
    lwsl_err("usage: %s [--code=CODE] [--url=URL]\n", name);
}

int main(int argc, const char **argv)
{
    uint16_t *portlist;
    char **wsurls;
    char *result;
    char *line;
    void *handle;
    const char *url, *code;

    //lws_set_log_level(LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO, NULL);
    lws_set_log_level(LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE, NULL);

    url  = lws_cmdline_option(argc, argv, "--url");
    code = lws_cmdline_option(argc, argv, "--code");

    if (lws_cmdline_option(argc, argv, "-h")
     || lws_cmdline_option(argc, argv, "-u")
     || lws_cmdline_option(argc, argv, "-v")) {
        print_usage(*argv);
        return 1;
    }

    // If no URL or code specified, scan for debuggers.
    if (!url && !code) {
        int32_t count;

        count = get_listening_ports("127.0.0.1", &portlist);

        lwsl_user("There are %d tcp sockets in state listen.\n", count);

        count = get_websocket_urls("127.0.0.1", portlist, &wsurls);

        lwsl_user("There were %d servers that appear to be CEF debuggers.\n", count);

        for (; wsurls && *wsurls; wsurls++) {
            lwsl_user("%s\n", *wsurls);
        }
    }

    // If URL, but no code, then enter interactive mode.
    if (url && !code) {
        handle = dbg_open_handle(url);

        while ((line = readline(">>> "))) {

            if (strcmp(line, "quit") == 0)
                break;

            result = dbg_eval_expression(handle, line);

            lwsl_user("<<< %s\n", result);

            add_history(line);

            rl_free(line);
        }

        dbg_close_handle(handle);
    }

    // If URL and code, enter evaluation mode.
    if (url && code) {
        handle = dbg_open_handle(url);

        result = dbg_eval_expression(handle, code);

        dbg_close_handle(handle);

        lwsl_user(">>> %s\n", code);
        lwsl_user("<<< %s\n", result);

        free(result);
    }

    return 0;
}
