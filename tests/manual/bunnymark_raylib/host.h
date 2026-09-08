/* Shared raylib host for the cross-language bunnymark.
**
** Each language binary embeds its VM, exposes the same four natives to the
** script (draw_bunny, rand, screen_width, screen_height) and hands the host
** two entry points: add_bunnies(n) and update_all(dt). The host owns the
** window, the sprite, the frame loop, FPS measurement and the auto-add
** policy, so the four binaries measure only their scripting layer.
*/
#pragma once

struct BunnyHost
{
    void *ud;
    /* Both return false on a script error (the host then quits). */
    bool (*add_bunnies)(void *ud, int n);
    bool (*update_all)(void *ud, double dt);
};

/* Natives the script calls back into. */
void host_draw_bunny(double x, double y);
double host_rand(double lo, double hi);
int host_screen_width();
int host_screen_height();

/* Runs the whole benchmark; returns the process exit code. */
int host_run(int argc, char **argv, const char *lang, const BunnyHost *host);

/* Reads the script file next to the executable (or the cwd) into a
** malloc'd, NUL-terminated buffer. Returns nullptr on failure. */
char *host_read_script(const char *argv0, const char *filename);
