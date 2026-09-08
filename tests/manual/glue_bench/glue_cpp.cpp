/* The same workload with no script at all: the baseline. */
#include "qtree.h"
#include "bench.h"
#include <chrono>
#include <vector>

static const double W = 1280, H = 720, DT = 1.0 / 60.0, R = 6;
static long g_seed = 42;
static double rnd() { g_seed = (g_seed * 16807) % 2147483647; return (double)g_seed; }
static double rnd_range(double lo, double hi) { return lo + (hi - lo) * (rnd() / 2147483647.0); }

struct P { double x, y, vx, vy; };

int main(int argc, char **argv)
{
    BenchArgs a = bench_args(argc, argv);
    std::vector<P> pts;
    for (int i = 0; i < a.n; i++)
        pts.push_back({rnd_range(0, W), rnd_range(0, H), rnd_range(-60, 60), rnd_range(-60, 60)});
    QuadTree tree(W, H);
    long checksum = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (int f = 0; f < a.frames; f++)
    {
        tree.clear();
        for (P &p : pts)
        {
            p.x += p.vx * DT;
            p.y += p.vy * DT;
            if (p.x < 0) { p.x = 0; p.vx = -p.vx; } else if (p.x >= W) { p.x = W - 1; p.vx = -p.vx; }
            if (p.y < 0) { p.y = 0; p.vy = -p.vy; } else if (p.y >= H) { p.y = H - 1; p.vy = -p.vy; }
            tree.insert(p.x, p.y);
        }
        for (const P &p : pts)
            if (tree.count(p.x - R, p.y - R, p.x + R, p.y + R) > 1)
                checksum++;
    }
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    bench_result("cpp", "cpp", a, secs, checksum);
    return 0;
}
