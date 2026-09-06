#include "host.h"

#include "raylib.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static Texture2D g_tex;
static long g_frame_draws = 0;
static int g_screen_w = 1280;
static int g_screen_h = 720;

void host_draw_bunny(double x, double y)
{
    DrawTexture(g_tex, (int)x, (int)y, WHITE);
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
    std::string dir(argv0);
    size_t slash = dir.find_last_of('/');
    dir = slash == std::string::npos ? std::string(".") : dir.substr(0, slash);
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
           "  --start N     bunnies at start (default 1000)\n"
           "  --step N      bunnies added per frame while fps >= target (default 200)\n"
           "  --target FPS  auto-add stops once the rolling fps drops below this (default 60)\n"
           "  --fixed N     run with exactly N bunnies, no auto-add\n"
           "  --seconds S   quit after S seconds (default: run until ESC/close)\n"
           "  --size WxH    window size (default 1280x720)\n"
           "  --vsync       enable vsync (default off, so fps can exceed the refresh rate)\n",
           prog);
}

int host_run(int argc, char **argv, const char *lang, const BunnyHost *host)
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

    SetTraceLogLevel(LOG_WARNING);
    if (vsync)
        SetConfigFlags(FLAG_VSYNC_HINT);
    char title[128];
    snprintf(title, sizeof title, "bunnymark — %s", lang);
    InitWindow(g_screen_w, g_screen_h, title);

    g_tex = LoadTexture("wabbit_alpha.png");
    if (g_tex.id == 0)
    {
        std::string dir(argv[0]);
        size_t slash = dir.find_last_of('/');
        if (slash != std::string::npos)
            g_tex = LoadTexture((dir.substr(0, slash) + "/wabbit_alpha.png").c_str());
    }
    if (g_tex.id == 0)
    {
        Image img = GenImageColor(26, 37, BLANK);
        ImageDrawCircle(&img, 13, 20, 12, PINK);
        ImageDrawCircle(&img, 8, 8, 4, PINK);
        ImageDrawCircle(&img, 18, 8, 4, PINK);
        g_tex = LoadTextureFromImage(img);
        UnloadImage(img);
    }

    long bunnies = 0;
    int initial = fixed >= 0 ? fixed : start;
    if (!host->add_bunnies(host->ud, initial))
    {
        fprintf(stderr, "%s: add_bunnies failed\n", lang);
        CloseWindow();
        return 1;
    }
    bunnies = initial;

    /* Rolling fps over the last 30 frames. */
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
        if (dt > 1.0 / 30.0) dt = 1.0 / 30.0; /* no teleporting after a stall */
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

        DrawRectangle(0, 0, 420, 78, Fade(BLACK, 0.7f));
        DrawText(TextFormat("%s   bunnies: %ld", lang, bunnies), 10, 8, 20, RAYWHITE);
        DrawText(TextFormat("fps: %.1f (rolling)   %d (raylib)", avg_fps, GetFPS()), 10, 30, 20, RAYWHITE);
        DrawText(auto_add ? (stopped ? TextFormat("stopped: %ld bunnies at >= %.0f fps", max_at_target, target)
                                     : TextFormat("adding %d/frame while fps >= %.0f", step, target))
                          : "fixed count",
                 10, 52, 20, stopped ? GREEN : YELLOW);
        EndDrawing();

        /* Measure with the real frame time (GetFrameTime includes the
        ** previous EndDrawing), so a frame's cost is attributed as raylib
        ** reports it. */
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
                if (!host->add_bunnies(host->ud, step))
                {
                    exit_code = 1;
                    break;
                }
                bunnies += step;
                max_at_target = bunnies;
            }
            else if (++below >= 10)
            {
                stopped = true;
                fps_at_stop = avg_fps;
                printf("%s: auto-add stopped at %ld bunnies (fps %.1f < %.0f)\n", lang, bunnies, avg_fps, target);
                fflush(stdout);
            }
        }
        if (elapsed - last_report >= 1.0)
        {
            last_report = elapsed;
            printf("%s: t=%.0fs bunnies=%ld fps=%.1f\n", lang, elapsed, bunnies, avg_fps);
            fflush(stdout);
        }
        if (seconds > 0.0 && elapsed >= seconds)
            break;
    }

    printf("RESULT lang=%s bunnies=%ld fps=%.1f frames=%d seconds=%.1f", lang, bunnies, avg_fps, frames, elapsed);
    if (auto_add)
        printf(" max_bunnies_at_%.0ffps=%ld%s", target, max_at_target, stopped ? "" : " (still adding)");
    if (stopped)
        printf(" fps_at_stop=%.1f", fps_at_stop);
    printf("\n");

    UnloadTexture(g_tex);
    CloseWindow();
    return exit_code;
}
