/* =========================================================
** zen_host_output.h — route script output through the embedder
**
** An editor or game host usually wants print() and runtime errors in its own
** console, not on stdout. Register a writer and every zen_write* goes there:
**
**   zen_host_set_writer(my_sink, my_console);
**
** Build libzen with -DZEN_HOST_OUTPUT=ON to enable it; zenconf.h then picks
** this up instead of its platform defaults. With no writer registered the
** functions fall back to stdout/stderr, so a host build behaves like a plain
** one until it opts in.
**
** This header is included by zenconf.h under the option — include it directly
** only to call zen_host_set_writer from host code.
** ========================================================= */

#ifndef ZEN_HOST_OUTPUT_H
#define ZEN_HOST_OUTPUT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* is_error is 0 for print(), 1 for runtime errors and diagnostics. */
    typedef void (*ZenHostWriteFn)(const char *text, size_t length, int is_error, void *user);

    /* Pass fn = nullptr to go back to stdout/stderr. */
    void zen_host_set_writer(ZenHostWriteFn fn, void *user);

    void zen_host_write(const char *text, size_t length);
    void zen_host_writes(const char *text);
    void zen_host_writeln(void);
    void zen_host_writeerr(const char *text, size_t length);

#ifdef __cplusplus
}
#endif

#define zen_write(s, l) zen_host_write((s), (l))
#define zen_writes(s) zen_host_writes(s)
#define zen_writeln() zen_host_writeln()
#define zen_writeerr(s, l) zen_host_writeerr((s), (l))

#endif /* ZEN_HOST_OUTPUT_H */
