/* Bunnymark-style embedding benchmark for ZenPy.
**
** Simulates the real-world shape this VM is meant to serve: N script
** objects (a class with x/y/vx/vy), each updated once per "frame" via a
** script method, with one native C++ call per object per frame (standing
** in for a draw call) — the actual thing that dominates a bunnymark, not
** pure interpreter loops like fib()/for_loop().
**
** No graphics dependency (no Raylib) so this runs anywhere and isolates
** the VM+embedding cost from GPU/windowing variance.
*/
#include "vm.h"
#include "compiler.h"
#include "module.h"
#include "memory.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>

using namespace zen;

static long g_draw_calls = 0;
static double g_draw_accum = 0.0; /* prevent the optimizer from eliding the call */

static int native_draw(VM *vm, Value *args, int nargs)
{
    (void)vm; (void)nargs;
    /* args[0]=x, args[1]=y — stand-in for draw_rect(x, y, w, h) */
    double x = args[0].type == VAL_FLOAT ? args[0].as.number : (double)args[0].as.integer;
    double y = args[1].type == VAL_FLOAT ? args[1].as.number : (double)args[1].as.integer;
    g_draw_accum += x + y;
    g_draw_calls++;
    return 0;
}

int main(int argc, char **argv)
{
    int n = argc > 1 ? atoi(argv[1]) : 20000;
    int frames = argc > 2 ? atoi(argv[2]) : 300;

    VM vm;
    vm.open_lib_globals(&zen_lib_base);
    /* The benchmark callback neither allocates nor retains Values. */
    vm.def_native("native_draw", native_draw, 2, ZEN_NATIVE_GC_SAFE);

    Compiler compiler;
    const char *source = R"ZEN(
class Bunny:
    def __init__(self, x, y, vx, vy):
        self.x = x
        self.y = y
        self.vx = vx
        self.vy = vy

    def update(self, dt):
        self.x = self.x + self.vx * dt
        self.y = self.y + self.vy * dt
        if self.x > 800 or self.x < 0:
            self.vx = 0 - self.vx
        if self.y > 600 or self.y < 0:
            self.vy = 0 - self.vy
        native_draw(self.x, self.y)

def make_bunnies(n):
    bunnies = []
    i = 0
    while i < n:
        b = Bunny(i % 800, i % 600, 1.5, 2.0)
        bunnies.append(b)
        i = i + 1
    return bunnies

def run_frames(bunnies, frames):
    n = len(bunnies)
    f = 0
    while f < frames:
        i = 0
        while i < n:
            bunnies[i].update(0.016)
            i = i + 1
        f = f + 1
)ZEN";

    ObjFunc *fn = compiler.compile(&vm.get_gc(), &vm, source, "<bunnymark>");
    if (!fn)
    {
        fprintf(stderr, "compile failed\n");
        return 1;
    }
    vm.run(fn);

    Value n_val = val_int(n);
    /* call_fn() is for callbacks issued by a native while a script frame is
       active. These are top-level embedding calls, so create a root frame. */
    Value bunnies = vm.call_global("make_bunnies", &n_val, 1);

    auto start = std::chrono::high_resolution_clock::now();
    Value args[2] = { bunnies, val_int(frames) };
    vm.call_global("run_frames", args, 2);
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed = std::chrono::duration<double>(end - start).count();
    printf("bunnies=%d frames=%d draw_calls=%ld elapsed=%.4f fps=%.1f\n",
           n, frames, g_draw_calls, elapsed, frames / elapsed);
    return 0;
}
