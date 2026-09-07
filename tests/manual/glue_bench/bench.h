/* Shared parameters and the RESULT printer. */
#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>

struct BenchArgs { int n; int frames; };

inline BenchArgs bench_args(int argc, char **argv)
{
    BenchArgs a = { 10000, 30 };
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--n") && i + 1 < argc) a.n = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) a.frames = atoi(argv[++i]);
    }
    return a;
}

inline void bench_result(const char *lang, const char *mode, const BenchArgs &a, double seconds, long checksum)
{
    printf("RESULT lang=%s mode=%s n=%d frames=%d ms_per_frame=%.2f checksum=%ld\n",
           lang, mode, a.n, a.frames, seconds * 1000.0 / a.frames, checksum);
}

inline char *read_script(const char *argv0, const char *filename)
{
    char path[1024];
    const char *slash = strrchr(argv0, '/');
    if (slash) snprintf(path, sizeof path, "%.*s/%s", (int)(slash - argv0), argv0, filename);
    else snprintf(path, sizeof path, "%s", filename);
    FILE *f = fopen(path, "rb");
    if (!f) f = fopen(filename, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", filename); return nullptr; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)n + 1);
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}
