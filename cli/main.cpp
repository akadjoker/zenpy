/*
** zen_main.cpp — CLI entry point for the Zen language.
** Usage:
**   ./zen              → REPL (interactive)
**   ./zen file.zen     → compile and run file
**   ./zen -e "code"    → compile and run inline code
*/

#include "vm.h"
#include "compiler.h"
#include "module.h"
#include "memory.h"
#include "debug.h"
#include "bytecode.h"

 
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace zen;

/* =========================================================
** Read file into malloc'd buffer
** ========================================================= */

static char *read_file(const char *path, long *out_size = nullptr)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        fprintf(stderr, "zen: cannot open '%s'\n", path);
        return nullptr;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = (char *)malloc(size + 1);
    if (!buf)
    {
        fclose(f);
        fprintf(stderr, "zen: out of memory\n");
        return nullptr;
    }

    size_t read = fread(buf, 1, size, f);
    buf[read] = '\0';
    fclose(f);
    if (out_size)
        *out_size = (long)read;
    return buf;
}

/* =========================================================
** Run source code (compile + execute)
** ========================================================= */

static bool g_disassemble = false;
static bool g_dis_only = false;
static bool g_verbose = false;
static const char *g_dump_path = nullptr;
static bool g_strip_debug = false;
static const char *g_search_paths[16];
static int g_num_search_paths = 0;

static void register_default_libs(VM &vm)
{
    vm.open_lib_globals(&zen_lib_base); /* always available */

    /* Core modules — available via 'import <name>' */
    vm.register_lib(&zen_lib_math);
    vm.register_lib(&zen_lib_time);
    vm.register_lib(&zen_lib_struct);
    vm.register_lib(&zen_lib_numpy);
    vm.register_lib(&zen_lib_io);
    vm.register_lib(&zen_lib_os);
    vm.register_lib(&zen_lib_path);
    vm.register_lib(&zen_lib_json);
    vm.register_lib(&zen_lib_net);
    vm.register_lib(&zen_lib_http);

    for (int i = 0; i < g_num_search_paths; i++)
        vm.add_search_path(g_search_paths[i]);
}

static void install_args(VM &vm, int script_argc, char **script_argv)
{
    ObjArray *args_arr = new_array(&vm.get_gc());
    for (int i = 0; i < script_argc; i++)
    {
        ObjString *s = vm.make_string(script_argv[i]);
        array_push(&vm.get_gc(), args_arr, val_obj((Obj *)s));
    }
    vm.def_global("args", val_obj((Obj *)args_arr));
}

static void disassemble_nested(ObjFunc *fn, VM *vm)
{
    for (int i = 0; i < fn->const_count; i++)
    {
        Value v = fn->constants[i];
        if (v.type == VAL_OBJ && v.as.obj && v.as.obj->type == OBJ_FUNC)
        {
            ObjFunc *nested = (ObjFunc *)v.as.obj;
            const char *n = nested->name ? nested->name->chars : "<anon>";
            disassemble_func(nested, n, vm);
            disassemble_nested(nested, vm);
        }
    }
}

static int run_source(const char *source, const char *filename,
                      int script_argc = 0, char **script_argv = nullptr)
{
    VM vm;
    register_default_libs(vm);
    install_args(vm, script_argc, script_argv);

    Compiler compiler;
    ObjFunc *fn = compiler.compile(&vm.get_gc(), &vm, source, filename);

    if (!fn)
    {
        fprintf(stderr, "zen: compilation failed.\n");
        return 1;
    }

    if (g_disassemble)
    {
        disassemble_func(fn, filename, &vm);
        disassemble_nested(fn, &vm);
        dump_constants(fn);
        printf("--- execution ---\n");
    }

    if (g_dump_path)
    {
        char err[256] = {0};
        BytecodeStats stats;
        if (!dump_bytecode_file(&vm, fn, g_dump_path, g_strip_debug,
                                g_verbose ? &stats : nullptr, err, sizeof(err)))
        {
            fprintf(stderr, "zen: bytecode dump failed: %s\n", err[0] ? err : "unknown error");
            return 1;
        }
        if (g_verbose)
        {
            printf("zen: dumped bytecode '%s'\n", g_dump_path);
            printf("  bytes:        %zu\n", stats.bytes);
            printf("  globals:      %u\n", stats.globals);
            printf("  selectors:    %u\n", stats.selectors);
            printf("  functions:    %u\n", stats.functions);

            printf("  classes:      %u\n", stats.classes);
            printf("  closures:     %u\n", stats.closures);
            printf("  strings:      %u\n", stats.strings);
            printf("  constants:    %u\n", stats.constants);
            printf("  instructions: %u\n", stats.instructions);
        }
        if (g_dis_only)
            return 0;
        return 0;
    }

    if (g_dis_only) return 0;

    vm.run(fn);
    return vm.had_error() ? 1 : 0;
}

static int run_bytecode(const uint8_t *data, size_t size, const char *filename,
                        int script_argc = 0, char **script_argv = nullptr)
{
    VM vm;
    register_default_libs(vm);
    install_args(vm, script_argc, script_argv);

    char err[256] = {0};
    ObjFunc *fn = load_bytecode_buffer(&vm, data, size, err, sizeof(err));
    if (!fn)
    {
        fprintf(stderr, "zen: bytecode load failed: %s\n", err[0] ? err : "unknown error");
        return 1;
    }

    if (g_disassemble)
    {
        disassemble_func(fn, filename, &vm);
        disassemble_nested(fn, &vm);
        dump_constants(fn);
        printf("--- execution ---\n");
    }

    if (g_dis_only)
        return 0;

    vm.run(fn);
    return vm.had_error() ? 1 : 0;
}

/* =========================================================
** REPL
** ========================================================= */

static void repl()
{
    printf("zen %s  [type 'exit' to quit]\n", "0.1.0");

    VM vm;
    register_default_libs(vm);
    Compiler compiler;

    char line[4096];
    for (;;)
    {
        printf(">> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin))
        {
            printf("\n");
            break;
        }

        /* Strip newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';

        /* Exit command */
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0)
            break;

        /* Skip empty lines */
        if (line[0] == '\0')
            continue;

        ObjFunc *fn = compiler.compile(&vm.get_gc(), &vm, line, "<repl>");
        if (fn)
        {
            vm.run(fn);
        }
    }
}

/* =========================================================
** Main
** ========================================================= */

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        /* No arguments: REPL */
        repl();
        return 0;
    }

    /* Parse flags */
    const char *source_code = nullptr;
    const char *file_path = nullptr;
    int script_arg_start = 0; /* index in argv where script args begin */

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--dis") == 0 || strcmp(argv[i], "-d") == 0)
        {
            g_disassemble = true;
        }
        else if (strcmp(argv[i], "--dis-only") == 0)
        {
            g_disassemble = true;
            g_dis_only = true;
        }
        else if (strcmp(argv[i], "--dump") == 0 && i + 1 < argc)
        {
            g_dump_path = argv[++i];
        }
        else if (strcmp(argv[i], "--strip-debug") == 0)
        {
            g_strip_debug = true;
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
        {
            g_verbose = true;
        }
        else if (strcmp(argv[i], "-e") == 0 && i + 1 < argc)
        {
            source_code = argv[++i];
        }
        else if ((strcmp(argv[i], "-I") == 0 || strcmp(argv[i], "--include") == 0) && i + 1 < argc)
        {
            if (g_num_search_paths < 16)
                g_search_paths[g_num_search_paths++] = argv[++i];
            else
                i++; /* skip silently */
        }
        else if (argv[i][0] != '-')
        {
            file_path = argv[i];
            script_arg_start = i + 1; /* everything after is script args */
            break;
        }
        else
        {
            fprintf(stderr, "zen: unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    if (source_code)
    {
        return run_source(source_code, "<cmdline>");
    }

    if (file_path)
    {
        long size = 0;
        char *data = read_file(file_path, &size);
        if (!data)
            return 1;
        int sa = script_arg_start;
        int result = is_bytecode_buffer((const uint8_t *)data, (size_t)size)
                         ? run_bytecode((const uint8_t *)data, (size_t)size, file_path, argc - sa, argv + sa)
                         : run_source(data, file_path, argc - sa, argv + sa);
        free(data);
        return result;
    }

    fprintf(stderr, "zen: no input specified\n");
    return 1;
}
