/* Shared raylib host for the cross-language quadtree demo.
**
** The whole quadtree lives in the script: points, nodes, insert, queries.
** Every frame the script moves its points, rebuilds the tree, runs a
** broad-phase query per point (neighbours within a small box), a
** rectangle query around the mouse, and draws through the natives below.
** The host owns the window, input, the frame loop, fps and the HUD.
*/
#pragma once

struct QtHost
{
    void *ud;
    /* x, y < 0 means "random positions", otherwise spawn at (x, y). */
    bool (*add_points)(void *ud, int n, double x, double y);
    bool (*update_all)(void *ud, double dt);
};

/* Natives the script calls back into. Colours: 0 white, 1 red (has a
** neighbour), 2 yellow (inside the mouse rectangle), 3 grey (node bounds). */
void host_draw_point(double x, double y, int color);
void host_draw_rect(double x, double y, double w, double h, int color);
void host_set_stats(int nodes, int neighbours, int mouse_hits);
double host_mouse_x();
double host_mouse_y();
double host_rand(double lo, double hi);
int host_screen_width();
int host_screen_height();

int host_run(int argc, char **argv, const char *lang, const QtHost *host);
char *host_read_script(const char *argv0, const char *filename);
