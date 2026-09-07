#include "host.h"

#include "raylib.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static int g_screen_w = 1280;
static int g_screen_h = 720;
static long g_frame_draws = 0;
static int g_nodes = 0, g_neighbours = 0, g_mouse_hits = 0;
static bool g_draw_nodes = true;

static const Color kColors[4] = { RAYWHITE, RED, YELLOW, { 90, 90, 90, 255 } };

void host_draw_point(double x, double y, int color)
{
    DrawRectangle((int)x - 2, (int)y - 2, 4, 4, kColors[color & 3]);
    g_frame_draws++;
}

void host_draw_rect(double x, double y, double w, double h, int color)
{
    if (color == 3 && !g_draw_nodes)
        return;
    DrawRectangleLines((int)x, (int)y, (int)w, (int)h, kColors[color & 3]);
}

void host_set_stats(int nodes, int neighbours, int mouse_hits)
{
    g_nodes = nodes;
    g_neighbours = neighbours;
    g_mouse_hits = mouse_hits;
}

double host_mouse_x() { return GetMousePosition().x; }
double host_mouse_y() { return GetMousePosition().y; }
double host_rand(double lo, double hi) { return lo + (hi - lo) * ((double)rand() / (double)RAND_MAX); }
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
           "  (default)     interactive: left click adds --click points at the cursor, N toggles node bounds, ESC quits\n"
           "  --click N     points per click (default 500); hold the button to keep adding\n"
           "  --burst X,Y   add --click points at (X,Y) on the first frame (a scripted click)\n"
           "  --auto        auto-add: +step points per frame while fps >= target\n"
           "  --start N     points at start (default 1000)\n"
           "  --step N      points added per frame in --auto mode (default 100)\n"
           "  --target FPS  auto-add stops once the rolling fps drops below this (default 60)\n"
           "  --fixed N     run with exactly N points, no adding\n"
           "  --seconds S   quit after S seconds (default: run until ESC/close)\n"
           "  --size WxH    window size (default 1280x720)\n"
           "  --no-nodes    do not draw node bounds\n"
           "  --vsync       enable vsync (default off)\n",
           prog);
}

int host_run(int argc, char **argv, const char *lang, const QtHost *host)
{
    int start = 1000, step = 100, fixed = -1, click = 500;
    double target = 60.0, seconds = 0.0;
    bool vsync = false, auto_mode = false;
    double burst_x = -1, burst_y = -1;
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
        else if (!strcmp(argv[i], "--auto")) auto_mode = true;
        else if (!strcmp(argv[i], "--no-nodes")) g_draw_nodes = false;
        else if (!strcmp(argv[i], "--click")) click = atoi(next(i));
        else if (!strcmp(argv[i], "--burst")) sscanf(next(i), "%lf,%lf", &burst_x, &burst_y);
        else { usage(argv[0]); return 2; }
    }
    srand(12345);

    SetTraceLogLevel(LOG_WARNING);
    if (vsync)
        SetConfigFlags(FLAG_VSYNC_HINT);
    char title[128];
    snprintf(title, sizeof title, "quadtree — %s", lang);
    InitWindow(g_screen_w, g_screen_h, title);

    long points = 0;
    int initial = fixed >= 0 ? fixed : start;
    if (!host->add_points(host->ud, initial, -1, -1))
    {
        fprintf(stderr, "%s: add_points failed\n", lang);
        CloseWindow();
        return 1;
    }
    points = initial;

    const int kWindow = 30;
    double frame_times[kWindow] = {0};
    int ft_idx = 0, ft_count = 0;
    double ft_sum = 0.0;

    bool auto_add = fixed < 0 && auto_mode;
    bool interactive = fixed < 0 && !auto_mode;
    bool stopped = false;
    int below = 0;
    long max_at_target = 0;
    double fps_at_stop = 0.0;
    double elapsed = 0.0;
    int frames = 0;
    double last_report = 0.0;
    double avg_fps = 0.0;
    double script_ms = 0.0;
    int exit_code = 0;

    while (!WindowShouldClose())
    {
        double dt = GetFrameTime();
        if (dt > 1.0 / 30.0) dt = 1.0 / 30.0;
        if (dt <= 0.0) dt = 1.0 / 60.0;
        if (IsKeyPressed(KEY_N))
            g_draw_nodes = !g_draw_nodes;

        BeginDrawing();
        ClearBackground(BLACK);
        g_frame_draws = 0;
        double t0 = GetTime();
        if (!host->update_all(host->ud, dt))
        {
            fprintf(stderr, "%s: update_all failed\n", lang);
            exit_code = 1;
            EndDrawing();
            break;
        }
        script_ms = (GetTime() - t0) * 1000.0;

        DrawRectangle(0, 0, 560, 100, Fade(BLACK, 0.7f));
        DrawText(TextFormat("%s   points: %ld   nodes: %d   neighbours: %d   mouse hits: %d", lang, points, g_nodes, g_neighbours, g_mouse_hits), 10, 8, 20, RAYWHITE);
        DrawText(TextFormat("fps: %.1f (rolling)   %d (raylib)   script: %.1f ms", avg_fps, GetFPS(), script_ms), 10, 30, 20, RAYWHITE);
        DrawText(interactive ? TextFormat("left click: +%d points at the cursor   N: node bounds   ESC: quit", click)
                 : auto_add ? (stopped ? TextFormat("stopped: %ld points at >= %.0f fps", max_at_target, target)
                                       : TextFormat("adding %d/frame while fps >= %.0f", step, target))
                            : "fixed count",
                 10, 52, 20, stopped ? GREEN : YELLOW);
        DrawText("red: has a neighbour within 6 px   yellow: inside the mouse rectangle", 10, 74, 20, GRAY);
        EndDrawing();

        if (burst_x >= 0)
        {
            if (!host->add_points(host->ud, click, burst_x, burst_y))
            {
                exit_code = 1;
                break;
            }
            points += click;
            burst_x = -1;
        }
        if (interactive && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            int n = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ? click : click / 10;
            if (n > 0)
            {
                Vector2 m = GetMousePosition();
                if (!host->add_points(host->ud, n, m.x, m.y))
                {
                    exit_code = 1;
                    break;
                }
                points += n;
            }
        }

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
                if (!host->add_points(host->ud, step, -1, -1))
                {
                    exit_code = 1;
                    break;
                }
                points += step;
                max_at_target = points;
            }
            else if (++below >= 10)
            {
                stopped = true;
                fps_at_stop = avg_fps;
                printf("%s: auto-add stopped at %ld points (fps %.1f < %.0f)\n", lang, points, avg_fps, target);
                fflush(stdout);
            }
        }
        if (elapsed - last_report >= 1.0)
        {
            last_report = elapsed;
            printf("%s: t=%.0fs points=%ld nodes=%d fps=%.1f script=%.1fms\n", lang, elapsed, points, g_nodes, avg_fps, script_ms);
            fflush(stdout);
        }
        if (seconds > 0.0 && elapsed >= seconds)
            break;
    }

    printf("RESULT lang=%s points=%ld draws=%ld nodes=%d neighbours=%d fps=%.1f script_ms=%.1f frames=%d seconds=%.1f",
           lang, points, g_frame_draws, g_nodes, g_neighbours, avg_fps, script_ms, frames, elapsed);
    if (auto_add)
        printf(" max_points_at_%.0ffps=%ld%s", target, max_at_target, stopped ? "" : " (still adding)");
    if (stopped)
        printf(" fps_at_stop=%.1f", fps_at_stop);
    printf("\n");

    CloseWindow();
    return exit_code;
}
