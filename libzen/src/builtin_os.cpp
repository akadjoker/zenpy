/* =========================================================
** builtin_os.cpp — os module for Zen
**
** OS interaction: getcwd, listdir, mkdir, remove, rename,
**                 getenv, setenv, exit, system, clock.
**
** Usage:
**   import os
**   print(os.getcwd())
**   files = os.listdir(".")
**   os.mkdir("newdir")
**   os.remove("file.txt")
**   os.rename("old.txt", "new.txt")
**   print(os.getenv("HOME"))
** ========================================================= */

#include "module.h"
#include "vm.h"
#include "memory.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
#define getcwd _getcwd
#define chdir _chdir
#else
#include <unistd.h>
#include <dirent.h>
#endif

namespace zen
{
    /* getcwd() → string */
    static int nat_getcwd(VM *vm, Value *args, int nargs)
    {
        (void)nargs;
        char buf[4096];
        if (!getcwd(buf, sizeof(buf)))
        {
            vm->runtime_error("os.getcwd: failed");
            return -1;
        }
        args[0] = val_obj((Obj *)vm->make_string(buf));
        return 1;
    }

    /* chdir(path) → nil */
    static int nat_chdir(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1 || !is_string(args[0]))
        {
            vm->runtime_error("os.chdir: expected string path");
            return -1;
        }
        if (chdir(safe_string_chars(args[0])) != 0)
        {
            vm->runtime_error("os.chdir: cannot change to '%s'", safe_string_chars(args[0]));
            return -1;
        }
        return 0;
    }

    /* listdir(path) → list of strings */
    static int nat_listdir(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1 || !is_string(args[0]))
        {
            vm->runtime_error("os.listdir: expected string path");
            return -1;
        }
        const char *path = as_cstring(args[0]);
#if defined(_WIN32)
        char pattern[4096];
        snprintf(pattern, sizeof(pattern), "%s\\*", path);
        WIN32_FIND_DATAA find_data;
        HANDLE handle = FindFirstFileA(pattern, &find_data);
        if (handle == INVALID_HANDLE_VALUE)
        {
            vm->runtime_error("os.listdir: cannot open '%s'", path);
            return -1;
        }
#else
        DIR *d = opendir(path);
        if (!d)
        {
            vm->runtime_error("os.listdir: cannot open '%s'", path);
            return -1;
        }
#endif
        /* GC_SAFE: root the array through the args window right away (path
        ** was consumed above), and give the string in flight one reusable
        ** root slot — array_push may allocate before storing it. */
        GC *gc = &vm->get_gc();
        ObjArray *arr = new_array(gc);
        args[0] = val_obj((Obj *)arr);
        Value *name_root = vm->root(val_nil());
        if (!name_root)
        {
#if defined(_WIN32)
            FindClose(handle);
#else
            closedir(d);
#endif
            return -1;
        }
#if defined(_WIN32)
        do
        {
            const char *name = find_data.cFileName;
            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
                continue;
            *name_root = val_obj((Obj *)create_string(gc, name, (int)strlen(name)));
            array_push(gc, arr, *name_root);
        } while (FindNextFileA(handle, &find_data));
        FindClose(handle);
#else
        struct dirent *ent;
        while ((ent = readdir(d)) != nullptr)
        {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;
            *name_root = val_obj((Obj *)create_string(gc, ent->d_name, (int)strlen(ent->d_name)));
            array_push(gc, arr, *name_root);
        }
        closedir(d);
#endif
        return 1;
    }

    /* mkdir(path) → nil */
    static int nat_mkdir(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1 || !is_string(args[0]))
        {
            vm->runtime_error("os.mkdir: expected string path");
            return -1;
        }
        const char *path = safe_string_chars(args[0]);
#if defined(_WIN32)
        if (mkdir(path) != 0)
#else
        if (mkdir(path, 0755) != 0)
#endif
        {
            vm->runtime_error("os.mkdir: cannot create '%s'", path);
            return -1;
        }
        return 0;
    }

    /* remove(path) → nil */
    static int nat_remove(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1 || !is_string(args[0]))
        {
            vm->runtime_error("os.remove: expected string path");
            return -1;
        }
        const char *path = as_cstring(args[0]);
#if defined(_WIN32)
        /* glibc's remove() unlinks files and rmdir's directories; MSVCRT's
        ** refuses directories outright. Keep both platforms behaving alike. */
        const DWORD attributes = GetFileAttributesA(path);
        const bool is_dir = attributes != INVALID_FILE_ATTRIBUTES &&
                            (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        const bool ok = is_dir ? RemoveDirectoryA(path) != 0 : ::remove(path) == 0;
#else
        const bool ok = ::remove(path) == 0;
#endif
        if (!ok)
        {
            vm->runtime_error("os.remove: cannot remove '%s'", path);
            return -1;
        }
        return 0;
    }

    /* rename(old, new) → nil */
    static int nat_rename(VM *vm, Value *args, int nargs)
    {
        if (nargs != 2 || !is_string(args[0]) || !is_string(args[1]))
        {
            vm->runtime_error("os.rename: expected (string old, string new)");
            return -1;
        }
        if (::rename(as_cstring(args[0]), as_cstring(args[1])) != 0)
        {
            vm->runtime_error("os.rename: failed");
            return -1;
        }
        return 0;
    }

    /* getenv(name) → string or nil */
    static int nat_getenv(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1 || !is_string(args[0]))
        {
            vm->runtime_error("os.getenv: expected string name");
            return -1;
        }
        const char *val = getenv(as_cstring(args[0]));
        if (!val)
        {
            args[0] = val_nil();
        }
        else
        {
            args[0] = val_obj((Obj *)vm->make_string(val));
        }
        return 1;
    }

    /* exit(code?) → does not return */
    static int nat_exit(VM *vm, Value *args, int nargs)
    {
        (void)vm;
        int code = 0;
        if (nargs >= 1 && is_int(args[0]))
            code = (int)args[0].as.integer;
        exit(code);
        return 0; /* unreachable */
    }

    /* isdir(path) → bool */
    static int nat_isdir(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1 || !is_string(args[0]))
        {
            vm->runtime_error("os.isdir: expected string path");
            return -1;
        }
#if defined(_WIN32)
        const DWORD attributes = GetFileAttributesA(as_cstring(args[0]));
        bool result = attributes != INVALID_FILE_ATTRIBUTES &&
                      (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
        struct stat st;
        bool result = (stat(as_cstring(args[0]), &st) == 0 && S_ISDIR(st.st_mode));
#endif
        args[0] = val_bool(result);
        return 1;
    }

    /* isfile(path) → bool */
    static int nat_isfile(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1 || !is_string(args[0]))
        {
            vm->runtime_error("os.isfile: expected string path");
            return -1;
        }
#if defined(_WIN32)
        const DWORD attributes = GetFileAttributesA(as_cstring(args[0]));
        bool result = attributes != INVALID_FILE_ATTRIBUTES &&
                      (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
        struct stat st;
        bool result = (stat(as_cstring(args[0]), &st) == 0 && S_ISREG(st.st_mode));
#endif
        args[0] = val_bool(result);
        return 1;
    }

    static const NativeReg os_functions[] = {
        {"getcwd",  nat_getcwd,  0},
        {"chdir",   nat_chdir,   1},
        {"listdir", nat_listdir, 1, ZEN_NATIVE_GC_SAFE},
        {"mkdir",   nat_mkdir,   1},
        {"remove",  nat_remove,  1},
        {"rename",  nat_rename,  2},
        {"getenv",  nat_getenv,  1},
        {"exit",    nat_exit,   -1},
        {"isdir",   nat_isdir,   1},
        {"isfile",  nat_isfile,  1},
        {nullptr, nullptr, 0}
    };

    extern const NativeLib zen_lib_os = {
        "os",
        os_functions,
        10,
        nullptr, 0, nullptr
    };

} /* namespace zen */
