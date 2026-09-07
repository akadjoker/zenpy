#include "host.h"

#include "raylib.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static const int kMaxTextures = 64;
static Texture2D g_textures[kMaxTextures];
static bool g_tex_used[kMaxTextures];
static long g_frame_draws = 0;
static int g_screen_w = 1280;
static int g_screen_h = 720;
static std::string g_exe_dir = ".";

static std::string dir_of(const char *argv0)
{
    std::string dir(argv0);
    size_t slash = dir.find_last_of('/');
    return slash == std::string::npos ? std::string(".") : dir.substr(0, slash);
}

int host_load_texture(const char *path)
{
    int id = -1;
    for (int i = 1; i < kMaxTextures; i++)
        if (!g_tex_used[i]) { id = i; break; }
    if (id < 0)
        return 0;
    Texture2D t = LoadTexture(path);
    if (t.id == 0)
        t = LoadTexture((g_exe_dir + "/" + path).c_str());
    if (t.id == 0)
    {
        Image img = GenImageColor(26, 37, BLANK);
        ImageDrawCircle(&img, 13, 20, 12, PINK);
        ImageDrawCircle(&img, 8, 8, 4, PINK);
        ImageDrawCircle(&img, 18, 8, 4, PINK);
        t = LoadTextureFromImage(img);
        UnloadImage(img);
    }
    g_textures[id] = t;
    g_tex_used[id] = true;
    return id;
}

void host_unload_texture(int id)
{
    if (id <= 0 || id >= kMaxTextures || !g_tex_used[id])
        return;
    UnloadTexture(g_textures[id]);
    g_tex_used[id] = false;
}

int host_texture_width(int id) { return (id > 0 && id < kMaxTextures && g_tex_used[id]) ? g_textures[id].width : 0; }
int host_texture_height(int id) { return (id > 0 && id < kMaxTextures && g_tex_used[id]) ? g_textures[id].height : 0; }

void host_draw_texture(int id, double x, double y)
{
    if (id <= 0 || id >= kMaxTextures || !g_tex_used[id])
        return;
    DrawTexture(g_textures[id], (int)x, (int)y, WHITE);
    g_frame_draws++;
}

double host_rand(double lo, double hi)
{
    return lo + (hi - lo) * ((double)rand() / (double)RAND_MAX);
}

int host_screen_width() { return g_screen_w; }
int host_screen_height() { return g_screen_h; }

char *host_read_script(const char *argv0, const char *filename)
{
    std::string dir = dir_of(argv0);
    const std::string candidates[2] = { dir + "/" + filename, std::string(filename) };
    for (const std::string &path : candidates)
    {
        FILE *f = fopen(path.c_str(), "rb");
        if (!f)
            continue;
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *buf = (char *)malloc((size_t)n + 1);
        size_t got = fread(buf, 1, (size_t)n, f);
        fclose(f);
        buf[got] = '\0';
        return buf;
    }
    fprintf(stderr, "cannot find %s next to %s or in the current directory\n", filename, argv0);
    return nullptr;
}

static void usage(const char *prog)
{
    printf("usage: %s [options]\n"
           "  --start N     sprites at start (default 1000)\n"
           "  --step N      sprites added per frame while fps >= target (default 200)\n"
           "  --target FPS  auto-add stops once the rolling fps drops below this (default 60)\n"
           "  --fixed N     run with exactly N sprites, no auto-add\n"
           "  --seconds S   quit after S seconds (default: run until ESC/close)\n"
           "  --size WxH    window size (default 1280x720)\n"
           "  --vsync       enable vsync (default off)\n",
           prog);
}

int host_run(int argc, char **argv, const char *lang, const TexHost *host)
{
    int start = 1000, step = 200, fixed = -1;
    double target = 60.0, seconds = 0.0;
    bool vsync = false;
    for (int i = 1; i < argc; i++)
    {
        auto next = [&](int &i) -> const char * { return i + 1 < argc ? argv[++i] : ""; };
        if (!strcmp(argv[i], "--start")) start = atoi(next(i));
        else if (!strcmp(argv[i], "--step")) step = atoi(next(i));
        else if (!strcmp(argv[i], "--target")) target = atof(next(i));
        else if (!strcmp(argv[i], "--fixed")) fixed = atoi(next(i));
        else if (!strcmp(argv[i], "--seconds")) seconds = atof(next(i));
        else if (!strcmp(argv[i], "--size")) sscanf(next(i), "%dx%d", &g_screen_w, &g_screen_h);
        else if (!strcmp(argv[i], "--vsync")) vsync = true;
        else { usage(argv[0]); return 2; }
    }
    srand(12345); /* same spawn sequence in every language */
    g_exe_dir = dir_of(argv[0]);

    SetTraceLogLevel(LOG_WARNING);
    if (vsync)
        SetConfigFlags(FLAG_VSYNC_HINT);
    char title[128];
    snprintf(title, sizeof title, "texmark — %s", lang);
    InitWindow(g_screen_w, g_screen_h, title);

    long sprites = 0;
    int initial = fixed >= 0 ? fixed : start;
    if (!host->add_sprites(host->ud, initial))
    {
        fprintf(stderr, "%s: add_sprites failed\n", lang);
        CloseWindow();
        return 1;
    }
    sprites = initial;

    const int kWindow = 30;
    double frame_times[kWindow] = {0};
    int ft_idx = 0, ft_count = 0;
    double ft_sum = 0.0;

    bool auto_add = fixed < 0;
    bool stopped = false;
    int below = 0;
    long max_at_target = 0;
    double fps_at_stop = 0.0;
    double elapsed = 0.0;
    int frames = 0;
    double last_report = 0.0;
    double avg_fps = 0.0;
    int exit_code = 0;

    while (!WindowShouldClose())
    {
        double dt = GetFrameTime();
        if (dt > 1.0 / 30.0) dt = 1.0 / 30.0;
        if (dt <= 0.0) dt = 1.0 / 60.0;

        BeginDrawing();
        ClearBackground(RAYWHITE);
        g_frame_draws = 0;
        if (!host->update_all(host->ud, dt))
        {
            fprintf(stderr, "%s: update_all failed\n", lang);
            exit_code = 1;
            EndDrawing();
            break;
        }

        DrawRectangle(0, 0, 440, 78, Fade(BLACK, 0.7f));
        DrawText(TextFormat("%s   sprites: %ld   draws: %ld", lang, sprites, g_frame_draws), 10, 8, 20, RAYWHITE);
        DrawText(TextFormat("fps: %.1f (rolling)   %d (raylib)", avg_fps, GetFPS()), 10, 30, 20, RAYWHITE);
        DrawText(auto_add ? (stopped ? TextFormat("stopped: %ld sprites at >= %.0f fps", max_at_target, target)
                                     : TextFormat("adding %d/frame while fps >= %.0f", step, target))
                          : "fixed count",
                 10, 52, 20, stopped ? GREEN : YELLOW);
        EndDrawing();

        double real_dt = GetFrameTime();
        if (real_dt <= 0.0) real_dt = dt;
        ft_sum -= frame_times[ft_idx];
        frame_times[ft_idx] = real_dt;
        ft_sum += real_dt;
        ft_idx = (ft_idx + 1) % kWindow;
        if (ft_count < kWindow) ft_count++;
        avg_fps = ft_count > 0 ? ft_count / ft_sum : 0.0;
        elapsed += real_dt;
        frames++;

        if (auto_add && !stopped && frames > kWindow)
        {
            if (avg_fps >= target)
            {
                below = 0;
                if (!host->add_sprites(host->ud, step))
                {
                    exit_code = 1;
                    break;
                }
                sprites += step;
                max_at_target = sprites;
            }
            else if (++below >= 10)
            {
                stopped = true;
                fps_at_stop = avg_fps;
                printf("%s: auto-add stopped at %ld sprites (fps %.1f < %.0f)\n", lang, sprites, avg_fps, target);
                fflush(stdout);
            }
        }
        if (elapsed - last_report >= 1.0)
        {
            last_report = elapsed;
            printf("%s: t=%.0fs sprites=%ld fps=%.1f\n", lang, elapsed, sprites, avg_fps);
            fflush(stdout);
        }
        if (seconds > 0.0 && elapsed >= seconds)
            break;
    }

    printf("RESULT lang=%s sprites=%ld draws=%ld fps=%.1f frames=%d seconds=%.1f", lang, sprites, g_frame_draws, avg_fps, frames, elapsed);
    if (auto_add)
        printf(" max_sprites_at_%.0ffps=%ld%s", target, max_at_target, stopped ? "" : " (still adding)");
    if (stopped)
        printf(" fps_at_stop=%.1f", fps_at_stop);
    printf("\n");

    for (int i = 1; i < kMaxTextures; i++)
        host_unload_texture(i);
    CloseWindow();
    return exit_code;
}
