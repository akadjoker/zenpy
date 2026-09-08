/* Shared raylib host for the cross-language "texmark".
**
** Unlike the bunnymark, the sprite texture is NOT owned by the host: the
** script loads it (`Texture("wabbit_alpha.png")`), keeps it in its own
** objects and draws through it (`tex.draw(x, y)`). So each binary measures
** "script object update + a native method call on a native object per
** sprite", i.e. a script driving engine-owned resources.
**
** Each language binary embeds its VM, exposes the same natives
** (Texture class with draw/width/height, draw_texture(tex, x, y),
** rand, screen_width, screen_height) and hands the host add_sprites(n) /
** update_all(dt). The host owns the window, the texture table, the frame
** loop, FPS measurement and the auto-add policy.
*/
#pragma once

struct TexHost
{
    void *ud;
    /* Both return false on a script error (the host then quits).
    ** add_sprites: x, y < 0 means "random positions", otherwise spawn at (x, y). */
    bool (*add_sprites)(void *ud, int n, double x, double y);
    bool (*update_all)(void *ud, double dt);
};

/* Texture table: ids are >= 1, 0 means failure. Looks next to the
** executable as well as in the cwd; falls back to a generated sprite. */
int host_load_texture(const char *path);
void host_unload_texture(int id);
int host_texture_width(int id);
int host_texture_height(int id);
void host_draw_texture(int id, double x, double y);

double host_rand(double lo, double hi);
int host_screen_width();
int host_screen_height();

int host_run(int argc, char **argv, const char *lang, const TexHost *host);
char *host_read_script(const char *argv0, const char *filename);
