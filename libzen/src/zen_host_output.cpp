/* =========================================================
** zen_host_output.cpp — see include/zen/zen_host_output.h
**
** Compiled only under -DZEN_HOST_OUTPUT. Without the option a host that calls
** zen_host_set_writer gets a link error rather than a writer that is silently
** never consulted, because zenconf.h would still be routing to stdout.
** ========================================================= */

#if defined(ZEN_HOST_OUTPUT)

#include "zen/zen_host_output.h"

#include <cstdio>
#include <cstring>

namespace
{
    ZenHostWriteFn g_writer = nullptr;
    void *g_writer_user = nullptr;
}

extern "C" void zen_host_set_writer(ZenHostWriteFn fn, void *user)
{
    g_writer = fn;
    g_writer_user = user;
}

extern "C" void zen_host_write(const char *text, size_t length)
{
    if (g_writer)
        g_writer(text, length, 0, g_writer_user);
    else
        std::fwrite(text, 1, length, stdout);
}

extern "C" void zen_host_writes(const char *text)
{
    zen_host_write(text, std::strlen(text));
}

extern "C" void zen_host_writeln(void)
{
    zen_host_write("\n", 1);
    if (!g_writer)
        std::fflush(stdout);
}

extern "C" void zen_host_writeerr(const char *text, size_t length)
{
    if (g_writer)
        g_writer(text, length, 1, g_writer_user);
    else
        std::fwrite(text, 1, length, stderr);
}

#endif /* ZEN_HOST_OUTPUT */
