#include "vm.h"
#include "compiler.h"
#include "debug.h"
#include "name_tables.h"
#include "zenconf.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

/* Dynamic loading */
#if defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace zen
{
    /* =========================================================
    ** Default callbacks — standard C stdio
    ** ========================================================= */

    static ZenFile default_io_open(const char *path, const char *mode, void * /*ud*/)
    {
        return (ZenFile)fopen(path, mode);
    }

    static long default_io_read(ZenFile file, void *buf, long size, void * /*ud*/)
    {
        return (long)fread(buf, 1, (size_t)size, (FILE *)file);
    }

    static long default_io_write(ZenFile file, const void *buf, long size, void * /*ud*/)
    {
        return (long)fwrite(buf, 1, (size_t)size, (FILE *)file);
    }

    static long default_io_seek(ZenFile file, long offset, int whence, void * /*ud*/)
    {
        if (fseek((FILE *)file, offset, whence) != 0)
            return -1;
        return ftell((FILE *)file);
    }

    static int default_io_close(ZenFile file, void * /*ud*/)
    {
        return fclose((FILE *)file);
    }

    static int default_io_exists(const char *path, void * /*ud*/)
    {
        FILE *f = fopen(path, "rb");
        if (!f)
            return 0;
        fclose(f);
        return 1;
    }

    static void default_print(const char *str, int len, void * /*ud*/)
    {
        zen_write(str, (size_t)len);
    }

    static void default_print_err(const char *str, int len, void * /*ud*/)
    {
        zen_writeerr(str, (size_t)len);
    }

    ZenCallbacks zen_default_callbacks()
    {
        ZenCallbacks cb;
        cb.io.open = default_io_open;
        cb.io.read = default_io_read;
        cb.io.write = default_io_write;
        cb.io.seek = default_io_seek;
        cb.io.close = default_io_close;
        cb.io.exists = default_io_exists;
        cb.print = default_print;
        cb.print_err = default_print_err;
        cb.userdata = nullptr;
        return cb;
    }

    /* =========================================================
    ** Construtor / Destrutor
    ** ========================================================= */

    VM::VM() : globals_(nullptr), global_names_(nullptr), num_globals_(0), globals_capacity_(0), main_fiber_(nullptr), current_fiber_(nullptr), fiber_depth_(0), run_depth_(0), external_call_stop_depth_(-1), had_error_(false), num_search_paths_(0), num_libs_(0), num_plugins_(0), selectors_(nullptr), num_selectors_(0), selectors_capacity_(0)
    {
        gc_init(&gc_);
        gc_.vm = this;

        callbacks_ = zen_default_callbacks();

        /* Globals: dynamic, grow on demand via def_global. */
        globals_capacity_ = kInitGlobalCapacity;
        globals_ = (Value *)calloc(globals_capacity_, sizeof(Value));
        global_names_ = (ObjString **)calloc(globals_capacity_, sizeof(ObjString *));

        memset(search_paths_, 0, sizeof(search_paths_));
        memset(plugin_handles_, 0, sizeof(plugin_handles_));

        /* Allocate initial selector array (dynamic, grows in intern_selector). */
        selectors_capacity_ = kInitSelectorCapacity;
        selectors_ = (ObjString **)calloc(selectors_capacity_, sizeof(ObjString *));

        num_selectors_ = 0;
        init_selector_ = -1;

        init_builtin_selectors();

        /* Criar main fiber */
        main_fiber_ = new_fiber(nullptr, kMaxFrames * 16);
        current_fiber_ = main_fiber_;
    }

    VM::~VM()
    {
        /* Close loaded plugins */
#if defined(__linux__) || defined(__APPLE__)
        for (int i = 0; i < num_plugins_; i++)
            if (plugin_handles_[i]) dlclose(plugin_handles_[i]);
#elif defined(_WIN32)
        for (int i = 0; i < num_plugins_; i++)
            if (plugin_handles_[i]) FreeLibrary((HMODULE)plugin_handles_[i]);
#endif

        /* Free search paths */
        for (int i = 0; i < num_search_paths_; i++)
            free(search_paths_[i]);

        /* Free selector table (the ObjString* it holds are GC-owned and
        ** will be released by gc_sweep_all below). */
        if (selectors_)
        {
            free(selectors_);
            selectors_ = nullptr;
            selectors_capacity_ = 0;
            num_selectors_ = 0;
        init_selector_ = -1;
        }

        /* Free the per-type builtin method tables (plain uint8_t arrays,
        ** not GC-owned). */
        free(str_method_); str_method_ = nullptr;
        free(arr_method_); arr_method_ = nullptr;
        free(map_method_); map_method_ = nullptr;
        free(set_method_); set_method_ = nullptr;
        free(buf_method_); buf_method_ = nullptr;
        num_builtin_selectors_ = 0;

        /* Free all objects via GC sweep */
        gc_sweep_all(&gc_);

        /* Destroy pool allocator (frees all chunks) */
        arena_destroy(&gc_.arena);

        free(globals_);
        free(global_names_);
        globals_ = nullptr;
        global_names_ = nullptr;
        globals_capacity_ = 0;
        num_globals_ = 0;

        /* Free gray list */
        if (gc_.gray_list)
            free(gc_.gray_list);
        /* Free intern table (allocated with calloc, not arena) */
        if (gc_.strings)
        {
            gc_.bytes_allocated -= gc_.string_capacity * sizeof(ObjString *);
            free(gc_.strings);
        }
    }

    /* =========================================================
    ** Fiber management
    ** ========================================================= */

    ObjFiber *VM::new_fiber(ObjClosure *closure, int stack_size, int max_frames)
    {
        /* Suppress GC during fiber construction.
        ** Problem: if zen_alloc triggers GC twice, the first cycle repaints
        ** our BLACK fiber to WHITE, and the second cycle sweeps it because
        ** it's not yet reachable from any root (not in pool_ yet).
        ** Fix: bump the GC threshold so no collection happens until we're done. */
        gc_pause(&gc_);

        ObjFiber *fiber = (ObjFiber *)zen_alloc(&gc_, sizeof(ObjFiber));
        fiber->obj.type = OBJ_FIBER;
        fiber->obj.color = GC_BLACK;
        fiber->obj.hash = 0;

        /* Initialize ALL fields BEFORE linking to GC list */
        fiber->state = FIBER_READY;
        fiber->stack_capacity = 0;
        fiber->stack = nullptr;
        fiber->stack_top = nullptr;
        fiber->stack_end = nullptr;
        fiber->frame_capacity = 0;
        fiber->frames = nullptr;
        fiber->frame_count = 0;
        fiber->open_upvalues = nullptr;
        fiber->caller = nullptr;
        fiber->transfer_value = val_nil();
        fiber->yield_dest = -1;
        fiber->error = nullptr;

        /* Link to GC list */
        fiber->obj.gc_next = gc_.objects;
        gc_.objects = (Obj *)fiber;

        /* Allocate stack and frames (GC suppressed — safe) */
        fiber->stack_capacity = stack_size;
        fiber->stack = (Value *)zen_alloc(&gc_, stack_size * sizeof(Value));
        fiber->stack_top = fiber->stack;
        fiber->stack_end = fiber->stack + stack_size;

        fiber->frame_capacity = max_frames;
        fiber->frames = (CallFrame *)zen_alloc(&gc_, max_frames * sizeof(CallFrame));

        gc_resume(&gc_);

        /* Se tiver closure, prepara o primeiro frame */
        if (closure)
        {
            fiber->frame_count = 1;
            CallFrame *frame = &fiber->frames[0];
            frame->closure = closure;
            frame->constants = closure->func->constants;
            frame->upvalues = closure->upvalues;
            frame->func = closure->func;
            frame->ip = closure->func->code;
            frame->base = fiber->stack;
            frame->ret_reg = 0;
            frame->ret_count = 0;
            fiber->stack_top = fiber->stack + closure->func->num_regs;
        }

        return fiber;
    }

    /* =========================================================
    ** Run
    ** ========================================================= */

    void VM::run(ObjFunc *func)
    {
        /* Wrap func num closure trivial (0 upvalues).
           Pause GC: zen_alloc below can trigger a collection which would reset
           func to WHITE (it is not yet a GC root) and sweep it. */
        gc_.pause_depth++;
        ObjClosure *cl = (ObjClosure *)zen_alloc(&gc_, sizeof(ObjClosure));
        gc_.pause_depth--;
        cl->obj.type = OBJ_CLOSURE;
        cl->obj.color = GC_BLACK;
        cl->obj.hash = 0;
        cl->obj.gc_next = gc_.objects;
        gc_.objects = (Obj *)cl;
        cl->func = func;
        cl->upvalues = nullptr;
        cl->upvalue_count = 0;

        run(cl);
    }

    /* Um script compilado e corrido de dentro de outro (um native que chama
    ** run(), como um instantiate(prefab) faria) não pode ir para o main_fiber_:
    ** o caminho normal abaixo rebobina-o, o que apagaria as frames de quem
    ** chamou. Mesmo padrão do import_script_module. */
    void VM::run_nested(ObjClosure *closure)
    {
        had_error_ = false;

        ObjFiber *fiber = new_fiber(closure, 256);
        fiber->frame_count = 1;
        CallFrame *frame = &fiber->frames[0];
        frame->closure = closure;
        frame->constants = closure->func->constants;
        frame->upvalues = closure->upvalues;
        frame->func = closure->func;
        frame->ip = closure->func->code;
        frame->base = fiber->stack;
        frame->ret_reg = 0;
        frame->ret_count = 0;
        fiber->stack_top = fiber->stack + closure->func->num_regs;
        fiber->state = FIBER_RUNNING;

        ObjFiber *saved_fiber = current_fiber_;
        current_fiber_ = fiber;
        run_depth_++;
        execute(fiber);
        run_depth_--;
        current_fiber_ = saved_fiber;
    }

    void VM::run(ObjClosure *closure)
    {
        if (run_depth_ > 0)
        {
            run_nested(closure);
            return;
        }

        had_error_ = false;
        /* Setup main fiber com o closure */
        main_fiber_->frame_count = 1;
        CallFrame *frame = &main_fiber_->frames[0];
        frame->closure = closure;
        frame->constants = closure->func->constants;
        frame->upvalues = closure->upvalues;
        frame->func = closure->func;
        frame->ip = closure->func->code;
        frame->base = main_fiber_->stack;
        frame->ret_reg = 0;
        frame->ret_count = 0;
        main_fiber_->stack_top = main_fiber_->stack + closure->func->num_regs;
        main_fiber_->state = FIBER_RUNNING;
        current_fiber_ = main_fiber_;

        run_depth_++;
        execute(main_fiber_);
        run_depth_--;

        /* Reset fiber state for subsequent C++ API calls */
        main_fiber_->stack_top = main_fiber_->stack;
        main_fiber_->state = FIBER_RUNNING;
    }

    Value *VM::root(Value v)
    {
        ObjFiber *f = current_fiber_;
        if (f->stack_top >= f->stack + f->stack_capacity)
        {
            runtime_error("native root overflow: fiber stack is full");
            return nullptr;
        }
        Value *slot = f->stack_top++;
        *slot = v;
        return slot;
    }

    Value VM::call_native_from_cpp(ObjNative *nat, Value receiver,
                                   Value *args, int nargs, bool has_receiver)
    {
        if (nargs < 0 || (nargs > 0 && !args))
        {
            runtime_error("native call received invalid arguments");
            return val_nil();
        }
        if (nat->generic_arity > 0)
        {
            runtime_error("'%s' is a generic native function/method and cannot be called this way",
                          nat->name ? nat->name->chars : "?");
            return val_nil();
        }

        /* ClassBuilder native methods count self in their registered arity,
        ** while NativeFn receives it at args[-1]. */
        const int expected = has_receiver ? nat->arity - 1 : nat->arity;
        if (nat->arity >= 0 && nargs != expected)
        {
            runtime_error("'%s' expected %d args but got %d",
                          nat->name ? nat->name->chars : "?", expected, nargs);
            return val_nil();
        }

        /* A zero-argument native still returns through args[0]. The public
        ** API commonly receives args=nullptr in that case, so reserve a
        ** writable result slot. Methods receive self at args[-1], matching
        ** bytecode invocation exactly. */
        const size_t offset = has_receiver ? 1u : 0u;
        const size_t value_slots = nargs > 0 ? (size_t)nargs : 1u;
        std::vector<Value> call_args(offset + value_slots);
        if (has_receiver)
            call_args[0] = receiver;
        for (int i = 0; i < nargs; i++)
            call_args[offset + (size_t)i] = args[i];

        /* This is C++ storage, not a VM root. Even a GC_SAFE native must run
        ** paused through an embedding entry point. */
        gc_pause(&gc_);
        int nret = nat->fn(this, call_args.data() + offset, nargs);
        gc_resume(&gc_);
        /* Preserve the long-standing free-function embedding convention:
        ** callers that supplied args also receive the first native result in
        ** args[0]. invoke() historically used a private copy, so methods do
        ** not write back into the host's argument array. */
        if (!has_receiver && nret > 0 && nargs > 0)
            args[0] = call_args[0];
        return nret > 0 ? call_args[offset] : val_nil();
    }

    /* Run a closure from an external C++ entry point. This cannot use
    ** call_closure(): that helper intentionally assumes an active caller
    ** frame in order to calculate ret_reg. A native called directly by C++
    ** may call back into Zen while main_fiber_ has no frames at all. */
    Value VM::call_closure_from_cpp(ObjClosure *cl, Value *args, int nargs)
    {
        ObjFunc *func = cl->func;
        if (func->arity >= 0)
        {
            int required = func->arity - func->default_count;
            if (nargs < required || nargs > func->arity)
            {
                if (func->default_count > 0)
                    runtime_error("expected %d to %d args but got %d", required, func->arity, nargs);
                else
                    runtime_error("expected %d args but got %d", func->arity, nargs);
                return val_nil();
            }
        }
        else if (nargs < (-func->arity) - 1)
        {
            runtime_error("expected at least %d args but got %d", (-func->arity) - 1, nargs);
            return val_nil();
        }

        ObjFiber *fiber = main_fiber_;
        Value *base = fiber->stack;
        if (nargs > fiber->stack_capacity || func->num_regs > fiber->stack_capacity)
        {
            runtime_error("stack overflow (data)");
            return val_nil();
        }
        for (int i = 0; i < nargs; i++)
            base[i] = args[i];
        for (int i = nargs; i < func->num_regs; i++)
            base[i] = val_nil();

        if (func->arity >= 0 && nargs < func->arity)
        {
            int required = func->arity - func->default_count;
            for (int i = nargs; i < func->arity; i++)
                base[i] = func->defaults[i - required];
        }
        else if (func->arity < 0)
        {
            int min_args = (-func->arity) - 1;
            gc_pause(&gc_);
            ObjArray *packed = new_array(&gc_);
            if (nargs > min_args)
                array_push_n(&gc_, packed, base + min_args, nargs - min_args);
            base[min_args] = val_obj((Obj *)packed);
            gc_resume(&gc_);
        }

        fiber->frame_count = 1;
        CallFrame *frame = &fiber->frames[0];
        frame->closure = cl;
        frame->constants = func->constants;
        frame->upvalues = cl->upvalues;
        frame->func = func;
        frame->ip = func->code;
        frame->base = base;
        frame->ret_reg = 0;
        frame->ret_count = 1;
        fiber->stack_top = base + func->num_regs;
        fiber->state = FIBER_RUNNING;
        current_fiber_ = fiber;

        execute(fiber);
        Value result = base[0];
        fiber->stack_top = base;
        fiber->state = FIBER_RUNNING;
        return result;
    }

    Value VM::call_global(int idx, Value *args, int nargs)
    {
        had_error_ = false;
        if (nargs < 0 || (nargs > 0 && !args))
        {
            runtime_error("call_global received invalid arguments");
            return val_nil();
        }
        if (idx < 0 || idx >= num_globals_)
        {
            runtime_error("global index %d is out of range", idx);
            return val_nil();
        }
        Value callee = globals_[idx];
        if (is_native(callee))
        {
            ObjNative *nat = as_native(callee);
            return call_native_from_cpp(nat, val_nil(), args, nargs, false);
        }
        if (is_closure(callee))
        {
            ObjClosure *cl = as_closure(callee);
            /* Same reasoning as the native branch above: a generic script
            ** function called through this C++ embedding entry point must
            ** not silently bind args[0] into the type-parameter register. */
            if (cl->func->generic_arity > 0)
            {
                runtime_error("'%s' is generic and must be called with <...> type arguments",
                              cl->func->name ? cl->func->name->chars : "?");
                return val_nil();
            }
            return call_closure_from_cpp(cl, args, nargs);
        }
        runtime_error("global %d is not callable", idx);
        return val_nil();
    }

    Value VM::call_global(const char *name, Value *args, int nargs)
    {
        int idx = find_global(name);
        if (idx < 0)
        {
            runtime_error("undefined global '%s'", name);
            return val_nil();
        }
        return call_global(idx, args, nargs);
    }

    /* Call any callable (closure or native) from within a native function */
    Value VM::call_fn(Value callee, Value *args, int nargs)
    {
        had_error_ = false;
        if (nargs < 0 || (nargs > 0 && !args))
        {
            runtime_error("call_fn received invalid arguments");
            return val_nil();
        }
        if (is_native(callee))
        {
            ObjNative *nat = as_native(callee);
            return call_native_from_cpp(nat, val_nil(), args, nargs, false);
        }
        if (is_closure(callee))
        {
            ObjClosure *cl = as_closure(callee);
            if (cl->func->generic_arity > 0)
            {
                runtime_error("'%s' is generic and must be called with <...> type arguments",
                              cl->func->name ? cl->func->name->chars : "?");
                return val_nil();
            }
            ObjFiber *fiber = current_fiber_;
            if (fiber->frame_count == 0)
                return call_closure_from_cpp(cl, args, nargs);
            Value *base = fiber->stack_top;
            int required_slots = nargs > cl->func->num_regs ? nargs : cl->func->num_regs;
            if (fiber->frame_count >= fiber->frame_capacity ||
                base + required_slots > fiber->stack + fiber->stack_capacity)
            {
                runtime_error("stack overflow while calling function from native");
                return val_nil();
            }
            for (int i = 0; i < nargs; i++)
                base[i] = args[i];
            /* call_closure owns the common script-call contract: arity,
            ** defaults, *args packing and clearing unused registers. */
            fiber->stack_top = base + nargs;
            if (!call_closure(fiber, cl, nargs, 1))
            {
                fiber->stack_top = base;
                return val_nil();
            }
            int prev = external_call_stop_depth_;
            external_call_stop_depth_ = fiber->frame_count - 1;
            execute(fiber);
            external_call_stop_depth_ = prev;
            Value result = base[0];
            fiber->stack_top = base;
            fiber->state = FIBER_RUNNING;
            return result;
        }
        runtime_error("value is not callable");
        return val_nil();
    }

    /* =========================================================
    ** Globals
    ** ========================================================= */

    int VM::find_global(const char *name) const
    {
        int len = (int)strlen(name);
        for (int i = 0; i < num_globals_; i++)
        {
            if (global_names_[i] &&
                global_names_[i]->length == len &&
                memcmp(global_names_[i]->chars, name, len) == 0)
            {
                return i;
            }
        }
        return -1;
    }

    int VM::def_global(const char *name, Value val)
    {
        int existing = find_global(name);
        if (existing >= 0)
        {
            globals_[existing] = val;
            return existing;
        }
        if (!grow_globals(num_globals_ + 1))
            return -1;
        int idx = num_globals_++;
        global_names_[idx] = intern_string(&gc_, name, (int)strlen(name),
                                           hash_string(name, (int)strlen(name)));
        globals_[idx] = val;
        return idx;
    }

    bool VM::grow_globals(int required)
    {
        if (required <= globals_capacity_)
            return true;
        if (required > kMaxGlobalsHard)
        {
            runtime_error("too many globals");
            return false;
        }

        int new_cap = globals_capacity_ > 0 ? globals_capacity_ : kInitGlobalCapacity;
        while (new_cap < required && new_cap < kMaxGlobalsHard)
            new_cap *= 2;
        if (new_cap < required)
            new_cap = kMaxGlobalsHard;

        Value *new_globals = (Value *)realloc(globals_, sizeof(Value) * (size_t)new_cap);
        if (!new_globals)
        {
            runtime_error("out of memory growing globals");
            return false;
        }
        ObjString **new_names = (ObjString **)realloc(global_names_, sizeof(ObjString *) * (size_t)new_cap);
        if (!new_names)
        {
            runtime_error("out of memory growing globals");
            globals_ = new_globals;
            return false;
        }

        for (int i = globals_capacity_; i < new_cap; i++)
        {
            new_globals[i] = val_nil();
            new_names[i] = nullptr;
        }
        globals_ = new_globals;
        global_names_ = new_names;
        globals_capacity_ = new_cap;
        return true;
    }

    Value VM::get_global(const char *name) const
    {
        int idx = find_global(name);
        if (idx < 0)
            return val_nil();
        Value v = globals_[idx];
        /* The embedder now holds a reference the VM cannot see. Mark string
        ** globals shared so the augmented-assign fast path never mutates a
        ** string a C++ caller kept — same rule OP_GETGLOBAL applies. */
        if (is_string(v) && is_obj(v))
            v.as.obj->flags |= OBJ_FLAG_SHARED;
        return v;
    }

    void VM::set_global(const char *name, Value val)
    {
        int idx = find_global(name);
        if (idx >= 0)
            globals_[idx] = val;
    }

    /* --- Method selector table (vtable slots) --- */

    int VM::find_selector(const char *name, int len) const
    {
        for (int i = 0; i < num_selectors_; i++)
        {
            if (selectors_[i]->length == len &&
                memcmp(selectors_[i]->chars, name, len) == 0)
                return i;
        }
        return -1;
    }

    int VM::intern_selector(const char *name, int len)
    {
        int idx = find_selector(name, len);
        if (idx >= 0) return idx;

        /* Grow if full. The pointer array may move; previously-returned
        ** slot indices are integers and remain valid (we never reorder). */
        if (num_selectors_ >= selectors_capacity_)
        {
            int new_cap = selectors_capacity_ ? selectors_capacity_ * 2
                                              : kInitSelectorCapacity;
            ObjString **grown = (ObjString **)realloc(selectors_,
                                  (size_t)new_cap * sizeof(ObjString *));
            if (!grown)
            {
                runtime_error("out of memory growing selector table");
                return -1;
            }
            /* Zero the new tail so any stray reads see nullptr, not garbage. */
            for (int i = selectors_capacity_; i < new_cap; i++)
                grown[i] = nullptr;
            selectors_ = grown;
            selectors_capacity_ = new_cap;
        }

        idx = num_selectors_++;
        selectors_[idx] = intern_string(&gc_, name, len, hash_string(name, len));
        if (len == 8 && memcmp(name, "__init__", 8) == 0)
            init_selector_ = idx; /* constructors are found through this slot */
        return idx;
    }

    /* One intern_selector() call per builtin method name, run once at
    ** construction. See BuiltinSelectors in vm.h and
    ** docs/plano-selector-dispatch-builtins.md. */
    void VM::init_builtin_selectors()
    {
#define SEL(field, name) bsel_.field = intern_selector(name, (int)(sizeof(name) - 1))
        SEL(str_len, "len");
        SEL(str_sub, "sub");
        SEL(str_find, "find");
        SEL(str_upper, "upper");
        SEL(str_lower, "lower");
        SEL(str_split, "split");
        SEL(str_trim, "trim");
        SEL(str_strip, "strip");
        SEL(str_replace, "replace");
        SEL(str_starts_with, "starts_with");
        SEL(str_startswith, "startswith");
        SEL(str_ends_with, "ends_with");
        SEL(str_endswith, "endswith");
        SEL(str_char_at, "char_at");
        SEL(str_byte_at, "byte_at");
        SEL(str_repeat, "repeat");
        SEL(str_count, "count");
        SEL(str_pad_left, "pad_left");
        SEL(str_pad_right, "pad_right");
        SEL(str_contains, "contains");
        SEL(str_reverse, "reverse");
        SEL(str_join, "join");
        SEL(str_lstrip, "lstrip");
        SEL(str_rstrip, "rstrip");
        SEL(str_title, "title");
        SEL(str_capitalize, "capitalize");
        SEL(str_swapcase, "swapcase");
        SEL(str_isalpha, "isalpha");
        SEL(str_isdigit, "isdigit");
        SEL(str_isalnum, "isalnum");
        SEL(str_isspace, "isspace");
        SEL(str_isupper, "isupper");
        SEL(str_islower, "islower");
        SEL(str_index, "index");
        SEL(str_rfind, "rfind");
        SEL(str_rindex, "rindex");
        SEL(str_center, "center");
        SEL(str_ljust, "ljust");
        SEL(str_rjust, "rjust");
        SEL(str_zfill, "zfill");
        SEL(str_splitlines, "splitlines");
        SEL(str_rsplit, "rsplit");
        SEL(str_partition, "partition");
        SEL(str_rpartition, "rpartition");

        SEL(arr_push, "push");
        SEL(arr_append, "append");
        SEL(arr_pop, "pop");
        SEL(arr_len, "len");
        SEL(arr_remove, "remove");
        SEL(arr_insert, "insert");
        SEL(arr_slice, "slice");
        SEL(arr_reverse, "reverse");
        SEL(arr_clear, "clear");
        SEL(arr_contains, "contains");
        SEL(arr_join, "join");
        SEL(arr_sort, "sort");
        SEL(arr_index_of, "index_of");
        SEL(arr_index, "index");
        SEL(arr_dump, "dump");
        SEL(arr_count, "count");
        SEL(arr_extend, "extend");
        SEL(arr_copy, "copy");

        SEL(map_set, "set");
        SEL(map_get, "get");
        SEL(map_has, "has");
        SEL(map_delete, "delete");
        SEL(map_keys, "keys");
        SEL(map_values, "values");
        SEL(map_items, "items");
        SEL(map_size, "size");
        SEL(map_clear, "clear");
        SEL(map_dump, "dump");
        SEL(map_update, "update");
        SEL(map_setdefault, "setdefault");
        SEL(map_pop, "pop");
        SEL(map_copy, "copy");

        SEL(set_add, "add");
        SEL(set_has, "has");
        SEL(set_delete, "delete");
        SEL(set_size, "size");
        SEL(set_clear, "clear");
        SEL(set_values, "values");
        SEL(set_dump, "dump");
        SEL(set_discard, "discard");
        SEL(set_remove, "remove");
        SEL(set_issubset, "issubset");
        SEL(set_issuperset, "issuperset");
        SEL(set_isdisjoint, "isdisjoint");
        SEL(set_union, "union");
        SEL(set_intersection, "intersection");
        SEL(set_difference, "difference");
        SEL(set_symmetric_difference, "symmetric_difference");
        SEL(set_copy, "copy");

        SEL(buf_len, "len");
        SEL(buf_fill, "fill");
        SEL(buf_byte_len, "byte_len");
        SEL(buf_tolist, "tolist");
        SEL(buf_copy, "copy");
        SEL(buf_slice, "slice");
        SEL(buf_type_name, "type_name");
#undef SEL

        init_builtin_method_tables();
    }

    /* Build the per-type sel_slot -> dense id tables.
    **
    ** Runs straight after init_builtin_selectors(), so num_selectors_ is
    ** exactly the builtin count: every slot below it names a builtin method
    ** of at least one receiver type, and every slot above it is a
    ** user-defined name interned later. One table per type because a shared
    ** name interns once — `len` is a single slot that must mean STR_LEN when
    ** the receiver is a string and ARR_LEN when it is an array. */
    void VM::init_builtin_method_tables()
    {
        num_builtin_selectors_ = num_selectors_;
        const size_t n = (size_t)num_builtin_selectors_;

        str_method_ = (uint8_t *)calloc(n, 1);
        arr_method_ = (uint8_t *)calloc(n, 1);
        map_method_ = (uint8_t *)calloc(n, 1);
        set_method_ = (uint8_t *)calloc(n, 1);
        buf_method_ = (uint8_t *)calloc(n, 1);
        if (!str_method_ || !arr_method_ || !map_method_ || !set_method_ || !buf_method_)
        {
            runtime_error("out of memory building builtin method tables");
            return;
        }

#define MAP_METHOD(table, field, id) table[bsel_.field] = (uint8_t)(id)
        MAP_METHOD(str_method_, str_len, STR_LEN);
        MAP_METHOD(str_method_, str_sub, STR_SUB);
        MAP_METHOD(str_method_, str_find, STR_FIND);
        MAP_METHOD(str_method_, str_upper, STR_UPPER);
        MAP_METHOD(str_method_, str_lower, STR_LOWER);
        MAP_METHOD(str_method_, str_split, STR_SPLIT);
        MAP_METHOD(str_method_, str_trim, STR_TRIM);
        MAP_METHOD(str_method_, str_strip, STR_STRIP);
        MAP_METHOD(str_method_, str_replace, STR_REPLACE);
        MAP_METHOD(str_method_, str_starts_with, STR_STARTS_WITH);
        MAP_METHOD(str_method_, str_startswith, STR_STARTSWITH);
        MAP_METHOD(str_method_, str_ends_with, STR_ENDS_WITH);
        MAP_METHOD(str_method_, str_endswith, STR_ENDSWITH);
        MAP_METHOD(str_method_, str_char_at, STR_CHAR_AT);
        MAP_METHOD(str_method_, str_byte_at, STR_BYTE_AT);
        MAP_METHOD(str_method_, str_repeat, STR_REPEAT);
        MAP_METHOD(str_method_, str_count, STR_COUNT);
        MAP_METHOD(str_method_, str_pad_left, STR_PAD_LEFT);
        MAP_METHOD(str_method_, str_pad_right, STR_PAD_RIGHT);
        MAP_METHOD(str_method_, str_contains, STR_CONTAINS);
        MAP_METHOD(str_method_, str_reverse, STR_REVERSE);
        MAP_METHOD(str_method_, str_join, STR_JOIN);
        MAP_METHOD(str_method_, str_lstrip, STR_LSTRIP);
        MAP_METHOD(str_method_, str_rstrip, STR_RSTRIP);
        MAP_METHOD(str_method_, str_title, STR_TITLE);
        MAP_METHOD(str_method_, str_capitalize, STR_CAPITALIZE);
        MAP_METHOD(str_method_, str_swapcase, STR_SWAPCASE);
        MAP_METHOD(str_method_, str_isalpha, STR_ISALPHA);
        MAP_METHOD(str_method_, str_isdigit, STR_ISDIGIT);
        MAP_METHOD(str_method_, str_isalnum, STR_ISALNUM);
        MAP_METHOD(str_method_, str_isspace, STR_ISSPACE);
        MAP_METHOD(str_method_, str_isupper, STR_ISUPPER);
        MAP_METHOD(str_method_, str_islower, STR_ISLOWER);
        MAP_METHOD(str_method_, str_index, STR_INDEX);
        MAP_METHOD(str_method_, str_rfind, STR_RFIND);
        MAP_METHOD(str_method_, str_rindex, STR_RINDEX);
        MAP_METHOD(str_method_, str_center, STR_CENTER);
        MAP_METHOD(str_method_, str_ljust, STR_LJUST);
        MAP_METHOD(str_method_, str_rjust, STR_RJUST);
        MAP_METHOD(str_method_, str_zfill, STR_ZFILL);
        MAP_METHOD(str_method_, str_splitlines, STR_SPLITLINES);
        MAP_METHOD(str_method_, str_rsplit, STR_RSPLIT);
        MAP_METHOD(str_method_, str_partition, STR_PARTITION);
        MAP_METHOD(str_method_, str_rpartition, STR_RPARTITION);

        MAP_METHOD(arr_method_, arr_push, ARR_PUSH);
        MAP_METHOD(arr_method_, arr_append, ARR_APPEND);
        MAP_METHOD(arr_method_, arr_pop, ARR_POP);
        MAP_METHOD(arr_method_, arr_len, ARR_LEN);
        MAP_METHOD(arr_method_, arr_remove, ARR_REMOVE);
        MAP_METHOD(arr_method_, arr_insert, ARR_INSERT);
        MAP_METHOD(arr_method_, arr_slice, ARR_SLICE);
        MAP_METHOD(arr_method_, arr_reverse, ARR_REVERSE);
        MAP_METHOD(arr_method_, arr_clear, ARR_CLEAR);
        MAP_METHOD(arr_method_, arr_contains, ARR_CONTAINS);
        MAP_METHOD(arr_method_, arr_join, ARR_JOIN);
        MAP_METHOD(arr_method_, arr_sort, ARR_SORT);
        MAP_METHOD(arr_method_, arr_index_of, ARR_INDEX_OF);
        MAP_METHOD(arr_method_, arr_index, ARR_INDEX);
        MAP_METHOD(arr_method_, arr_dump, ARR_DUMP);
        MAP_METHOD(arr_method_, arr_count, ARR_COUNT);
        MAP_METHOD(arr_method_, arr_extend, ARR_EXTEND);
        MAP_METHOD(arr_method_, arr_copy, ARR_COPY);

        MAP_METHOD(map_method_, map_set, MAP_SET);
        MAP_METHOD(map_method_, map_get, MAP_GET);
        MAP_METHOD(map_method_, map_has, MAP_HAS);
        MAP_METHOD(map_method_, map_delete, MAP_DELETE);
        MAP_METHOD(map_method_, map_keys, MAP_KEYS);
        MAP_METHOD(map_method_, map_values, MAP_VALUES);
        MAP_METHOD(map_method_, map_items, MAP_ITEMS);
        MAP_METHOD(map_method_, map_size, MAP_SIZE);
        MAP_METHOD(map_method_, map_clear, MAP_CLEAR);
        MAP_METHOD(map_method_, map_dump, MAP_DUMP);
        MAP_METHOD(map_method_, map_update, MAP_UPDATE);
        MAP_METHOD(map_method_, map_setdefault, MAP_SETDEFAULT);
        MAP_METHOD(map_method_, map_pop, MAP_POP);
        MAP_METHOD(map_method_, map_copy, MAP_COPY);

        MAP_METHOD(set_method_, set_add, SET_ADD);
        MAP_METHOD(set_method_, set_has, SET_HAS);
        MAP_METHOD(set_method_, set_delete, SET_DELETE);
        MAP_METHOD(set_method_, set_size, SET_SIZE);
        MAP_METHOD(set_method_, set_clear, SET_CLEAR);
        MAP_METHOD(set_method_, set_values, SET_VALUES);
        MAP_METHOD(set_method_, set_dump, SET_DUMP);
        MAP_METHOD(set_method_, set_discard, SET_DISCARD);
        MAP_METHOD(set_method_, set_remove, SET_REMOVE);
        MAP_METHOD(set_method_, set_issubset, SET_ISSUBSET);
        MAP_METHOD(set_method_, set_issuperset, SET_ISSUPERSET);
        MAP_METHOD(set_method_, set_isdisjoint, SET_ISDISJOINT);
        MAP_METHOD(set_method_, set_union, SET_UNION);
        MAP_METHOD(set_method_, set_intersection, SET_INTERSECTION);
        MAP_METHOD(set_method_, set_difference, SET_DIFFERENCE);
        MAP_METHOD(set_method_, set_symmetric_difference, SET_SYMMETRIC_DIFFERENCE);
        MAP_METHOD(set_method_, set_copy, SET_COPY);

        MAP_METHOD(buf_method_, buf_len, BUF_LEN);
        MAP_METHOD(buf_method_, buf_fill, BUF_FILL);
        MAP_METHOD(buf_method_, buf_byte_len, BUF_BYTE_LEN);
        MAP_METHOD(buf_method_, buf_tolist, BUF_TOLIST);
        MAP_METHOD(buf_method_, buf_copy, BUF_COPY);
        MAP_METHOD(buf_method_, buf_slice, BUF_SLICE);
        MAP_METHOD(buf_method_, buf_type_name, BUF_TYPE_NAME);
#undef MAP_METHOD
    }

    int VM::def_native(const char *name, NativeFn fn, int arity, int flags)
    {
        ObjString *s = intern_string(&gc_, name, (int)strlen(name),
                                     hash_string(name, (int)strlen(name)));
        ObjNative *nat = new_native(&gc_, fn, arity, s, flags);
        return def_global(name, val_obj((Obj *)nat));
    }

    /* =========================================================
    ** Module registry
    ** ========================================================= */

    void VM::register_lib(const NativeLib *lib)
    {
        if (num_libs_ < MAX_LIBS)
            libs_[num_libs_++] = lib;
    }

    const NativeLib *VM::find_lib(const char *name) const
    {
        for (int i = 0; i < num_libs_; i++)
            if (strcmp(libs_[i]->name, name) == 0)
                return libs_[i];
        return nullptr;
    }

    int VM::open_lib_globals(const NativeLib *lib, bool warn_shadows)
    {
        /* Pause GC during lib registration — multiple allocations happen
           (classes, maps, natives) and intermediate objects may not yet
           be reachable from any root. */
        gc_pause(&gc_);

        /* Run init_fn FIRST (it may register helper globals like classes).
           base is then set to the index where module functions start, so
           that compiler dot-access (module.fn → base + f) is correct. */
        if (lib->init_fn)
            lib->init_fn(this);
        int base = num_globals_;
        bool is_base = (strcmp(lib->name, "base") == 0);
        for (int i = 0; i < lib->num_functions; i++)
        {
            /* Always force-allocate a new slot, even if a global with this name
               already exists (e.g. a built-in called "error" or "warn").
               The compiler accesses module functions via explicit index (base + f),
               so each function MUST occupy exactly slot base + f. */
            const char *fname = lib->functions[i].name;

            /* For non-base modules, store qualified name ("math.sin") so that
               bytecode round-trip doesn't collide with homonymous globals. */
            char qbuf[256];
            const char *global_name = fname;
            int nlen;
            if (!is_base)
            {
                snprintf(qbuf, sizeof(qbuf), "%s.%s", lib->name, fname);
                global_name = qbuf;
            }
            nlen = (int)strlen(global_name);

            if (warn_shadows && find_global(global_name) >= 0)
                fprintf(stderr, "[zen warning] module '%s': function '%s' shadows an existing global\n",
                        lib->name, fname);
            if (!grow_globals(num_globals_ + 1))
                return base;
            int idx = num_globals_++;
            ObjString *s = intern_string(&gc_, global_name, nlen, hash_string(global_name, nlen));
            ObjNative *nat = new_native(&gc_, lib->functions[i].fn, lib->functions[i].arity, s,
                                        lib->functions[i].flags);
            global_names_[idx] = s;
            globals_[idx] = val_obj((Obj *)nat);
        }
        for (int i = 0; i < lib->num_constants; i++)
        {
            if (!is_base)
            {
                char qbuf[256];
                snprintf(qbuf, sizeof(qbuf), "%s.%s", lib->name, lib->constants[i].name);
                def_global(qbuf, lib->constants[i].value);
            }
            else
            {
                def_global(lib->constants[i].name, lib->constants[i].value);
            }
        }
        gc_resume(&gc_);
        return base;
    }

    void VM::open_lib(const NativeLib *lib)
    {
        register_lib(lib);
        open_lib_globals(lib);
    }

    void VM::resolve_native_globals()
    {
        /* For each global that is nil, try to find a matching native function
           or constant in the registered libraries and install it.
           This is needed after loading bytecode: imports are resolved at compile
           time (open_lib_globals), but bytecode doesn't serialize native values. */

        /* Ensure all lib init functions have run (they may lazily initialize
           constant tables, e.g. math_constants). */
        for (int li = 0; li < num_libs_; li++)
            if (libs_[li]->init_fn)
                libs_[li]->init_fn(this);

        for (int i = 0; i < num_globals_; i++)
        {
            if (!is_nil(globals_[i]))
                continue;
            if (!global_names_[i])
                continue;
            const char *name = global_names_[i]->chars;
            int name_len = global_names_[i]->length;

            /* Check for qualified name: "module.func" */
            const char *dot = strchr(name, '.');
            if (dot)
            {
                /* Qualified: search only the lib whose name matches the prefix */
                int lib_name_len = (int)(dot - name);
                const char *func_name = dot + 1;

                for (int li = 0; li < num_libs_; li++)
                {
                    const NativeLib *lib = libs_[li];
                    if ((int)strlen(lib->name) != lib_name_len ||
                        strncmp(lib->name, name, lib_name_len) != 0)
                        continue;

                    bool found = false;
                    for (int fi = 0; fi < lib->num_functions; fi++)
                    {
                        if (strcmp(lib->functions[fi].name, func_name) == 0)
                        {
                            ObjString *s = intern_string(&gc_, name, name_len,
                                                         hash_string(name, name_len));
                            ObjNative *nat = new_native(&gc_, lib->functions[fi].fn,
                                                        lib->functions[fi].arity, s,
                                                        lib->functions[fi].flags);
                            globals_[i] = val_obj((Obj *)nat);
                            found = true;
                            break;
                        }
                    }
                    if (found) break;

                    if (lib->constants)
                    {
                        for (int ci = 0; ci < lib->num_constants; ci++)
                        {
                            if (strcmp(lib->constants[ci].name, func_name) == 0)
                            {
                                globals_[i] = lib->constants[ci].value;
                                found = true;
                                break;
                            }
                        }
                    }
                    break; /* only one lib can match the prefix */
                }
            }
            else
            {
                /* Unqualified: linear search all libs (base lib behavior) */
                for (int li = 0; li < num_libs_; li++)
                {
                    const NativeLib *lib = libs_[li];
                    bool found = false;

                    for (int fi = 0; fi < lib->num_functions; fi++)
                    {
                        if (strcmp(lib->functions[fi].name, name) == 0)
                        {
                            ObjString *s = intern_string(&gc_, name, name_len,
                                                         hash_string(name, name_len));
                            ObjNative *nat = new_native(&gc_, lib->functions[fi].fn,
                                                        lib->functions[fi].arity, s,
                                                        lib->functions[fi].flags);
                            globals_[i] = val_obj((Obj *)nat);
                            found = true;
                            break;
                        }
                    }
                    if (found) break;

                    if (lib->constants)
                    {
                        for (int ci = 0; ci < lib->num_constants; ci++)
                        {
                            if (strcmp(lib->constants[ci].name, name) == 0)
                            {
                                globals_[i] = lib->constants[ci].value;
                                found = true;
                                break;
                            }
                        }
                    }
                    if (found) break;
                }
            }
        }
    }

    /* =========================================================
    ** try_load_plugin — fallback when find_lib fails.
    **
    ** Searches for <name>.so (Linux), <name>.dylib (macOS),
    ** or <name>.dll (Windows) in search paths and CWD.
    **
    ** The shared library must export:
    **   extern "C" const NativeLib* zen_open_<name>(void);
    **
    ** Returns the NativeLib* on success (already registered),
    ** or nullptr on failure (no error raised — caller handles it).
    ** ========================================================= */
    const NativeLib *VM::try_load_plugin(const char *name)
    {
#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
        if (num_plugins_ >= MAX_PLUGINS)
            return nullptr;

        /* Determine extension */
#if defined(__APPLE__)
        const char *ext = ".dylib";
#elif defined(_WIN32)
        const char *ext = ".dll";
#else
        const char *ext = ".so";
#endif

        /* Build symbol name: zen_open_<name> */
        char sym_name[80];
        snprintf(sym_name, sizeof(sym_name), "zen_open_%s", name);

        /* Try paths: CWD first, then search_paths_ */
        char path[512];
        void *handle = nullptr;

        /* Try: ./<name>.so */
        snprintf(path, sizeof(path), "./%s%s", name, ext);
#if defined(_WIN32)
        handle = (void *)LoadLibraryA(path);
#else
        handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif

        /* Try search paths */
        if (!handle)
        {
            for (int i = 0; i < num_search_paths_ && !handle; i++)
            {
                int dlen = (int)strlen(search_paths_[i]);
                if (dlen > 0 && search_paths_[i][dlen - 1] == '/')
                    snprintf(path, sizeof(path), "%s%s%s", search_paths_[i], name, ext);
                else
                    snprintf(path, sizeof(path), "%s/%s%s", search_paths_[i], name, ext);
#if defined(_WIN32)
                handle = (void *)LoadLibraryA(path);
#else
                handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
            }
        }

        if (!handle)
            return nullptr;

        /* Find the opener symbol */
        typedef const NativeLib *(*OpenFn)(void);
        OpenFn opener;
#if defined(_WIN32)
        opener = (OpenFn)GetProcAddress((HMODULE)handle, sym_name);
#else
        opener = (OpenFn)dlsym(handle, sym_name);
#endif

        if (!opener)
        {
            /* Symbol not found — close handle */
#if defined(_WIN32)
            FreeLibrary((HMODULE)handle);
#else
            dlclose(handle);
#endif
            return nullptr;
        }

        /* Call opener to get NativeLib */
        const NativeLib *lib = opener();
        if (!lib)
        {
#if defined(_WIN32)
            FreeLibrary((HMODULE)handle);
#else
            dlclose(handle);
#endif
            return nullptr;
        }

        /* Register plugin handle and library */
        plugin_handles_[num_plugins_++] = handle;
        register_lib(lib);
        return lib;

#else
        /* WASM/other — no dynamic loading */
        (void)name;
        return nullptr;
#endif
    }

    /* =========================================================
    ** Strings
    ** ========================================================= */

    ObjString *VM::make_string(const char *str, int length)
    {
        if (length < 0)
            length = (int)strlen(str);
        return new_string(&gc_, str, length);
    }

    /* =========================================================
    ** GC
    ** ========================================================= */

    void VM::collect()
    {
        gc_collect(this);
    }

    void VM::gc_mark_roots()
    {
        GC *gc = &gc_;

        /* 1. Mark globals (values and names) */
        for (int i = 0; i < num_globals_; i++)
        {
            gc_mark_value(gc, globals_[i]);
            if (global_names_[i])
                gc_mark_obj(gc, (Obj *)global_names_[i]);
        }

        /* 2. Mark all fibers (main + current + any others reachable from them) */
        if (main_fiber_)
            gc_mark_obj(gc, (Obj *)main_fiber_);
        if (current_fiber_ && current_fiber_ != main_fiber_)
            gc_mark_obj(gc, (Obj *)current_fiber_);
    }

    /* =========================================================
    ** Upvalues
    ** ========================================================= */

    ObjUpvalue *VM::capture_upvalue(ObjFiber *fiber, Value *local)
    {
        /* Procura upvalue aberto que já aponta para este slot */
        ObjUpvalue *prev = nullptr;
        ObjUpvalue *upval = fiber->open_upvalues;

        while (upval && upval->location > local)
        {
            prev = upval;
            upval = upval->next;
        }

        if (upval && upval->location == local)
        {
            return upval; /* Já existe — reutiliza */
        }

        /* Cria novo upvalue */
        ObjUpvalue *created = (ObjUpvalue *)zen_alloc_now(&gc_, sizeof(ObjUpvalue));
        created->obj.type = OBJ_UPVALUE;
        created->obj.color = GC_BLACK;
        created->obj.hash = 0;
        created->obj.gc_next = gc_.objects;
        gc_.objects = (Obj *)created;
        created->location = local;
        created->closed = val_nil();
        created->next = upval;

        /* Insere na lista ordenada por location (desc) */
        if (prev)
        {
            prev->next = created;
        }
        else
        {
            fiber->open_upvalues = created;
        }

        return created;
    }

    void VM::close_upvalues(ObjFiber *fiber, Value *last)
    {
        while (fiber->open_upvalues && fiber->open_upvalues->location >= last)
        {
            ObjUpvalue *upval = fiber->open_upvalues;
            upval->closed = *upval->location;
            upval->location = &upval->closed;
            fiber->open_upvalues = upval->next;
        }
    }

    /* =========================================================
    ** Call helpers
    ** ========================================================= */

    bool VM::call_closure(ObjFiber *fiber, ObjClosure *closure, int nargs, int nresults)
    {
        if (fiber->frame_count >= fiber->frame_capacity)
        {
            runtime_error("stack overflow (too many frames)");
            return false;
        }

        ObjFunc *func = closure->func;
        /* Verificar arity: negative = variadic def f(a, *args) -> arity=-(a+1) */
        if (func->arity >= 0)
        {
            int required = func->arity - func->default_count;
            if (nargs < required || nargs > func->arity)
            {
                if (func->default_count > 0)
                    runtime_error("expected %d to %d args but got %d", required, func->arity, nargs);
                else
                    runtime_error("expected %d args but got %d", func->arity, nargs);
                return false;
            }
        }
        else
        {
            /* variadic: minimum required = (-arity - 1) */
            int min_args = (-func->arity) - 1;
            if (nargs < min_args)
            {
                runtime_error("expected at least %d args but got %d", min_args, nargs);
                return false;
            }
        }

        CallFrame *frame = &fiber->frames[fiber->frame_count++];
        frame->closure = closure;
        frame->constants = closure->func->constants;
        frame->upvalues = closure->upvalues;
        frame->func = func;
        frame->ip = func->code;
        /* Args já estão no stack — base aponta para o início */
        frame->base = fiber->stack_top - nargs;
        frame->ret_reg = (int)(frame->base - fiber->frames[fiber->frame_count - 2].base);
        frame->ret_count = nresults;

        /* Expandir stack_top para cobrir registos da nova func */
        fiber->stack_top = frame->base + func->num_regs;

        /* Apply defaults for missing trailing args (stack already expanded above) */
        if (func->arity >= 0 && nargs < func->arity)
        {
            int required = func->arity - func->default_count;
            for (int di = nargs; di < func->arity; di++)
                frame->base[di] = func->defaults[di - required];
        }

        /* Pack *args: collect excess args into ObjArray at last param slot */
        if (func->arity < 0)
        {
            int min_args = (-func->arity) - 1; /* regular params before *args */
            int extra    = nargs - min_args;    /* number of values to pack */
            gc_pause(&gc_);
            ObjArray *arr = new_array(&gc_);
            if (extra > 0)
                array_push_n(&gc_, arr, frame->base + min_args, extra);
            frame->base[min_args] = val_obj((Obj *)arr);
            gc_resume(&gc_);
        }

        /* Clear unused regs so GC never sees stale Values */
        {
            int used = func->arity < 0 ? ((-func->arity - 1) + 1) : func->arity;
            for (int i = used; i < func->num_regs; i++)
                frame->base[i] = val_nil();
        }

        return true;
    }

    bool VM::call_value(ObjFiber *fiber, Value callee, int nargs, int nresults)
    {
        if (is_obj(callee))
        {
            switch (callee.as.obj->type)
            {
            case OBJ_CLOSURE:
                return call_closure(fiber, as_closure(callee), nargs, nresults);
            case OBJ_NATIVE:
            {
                ObjNative *nat = as_native(callee);
                if (nat->generic_arity > 0)
                {
                    runtime_error("'%s' is a generic native function and cannot be called this way",
                                  nat->name ? nat->name->chars : "?");
                    return false;
                }
                Value *args = fiber->stack_top - nargs;
                /* GC-safe natives run with the collector live — anything they
                ** vm->root() sits above stack_top and gets marked. Everyone
                ** else keeps the pause. Either way the top is restored, so
                ** roots die with the call. */
                Value *saved_top = fiber->stack_top;
                const bool paused = !(nat->flags & ZEN_NATIVE_GC_SAFE);
                if (paused)
                    gc_pause(&gc_);
                int nret = nat->fn(this, args, nargs);
                if (paused)
                    gc_resume(&gc_);
                fiber->stack_top = saved_top;
                /* Coloca resultado onde estava o callable */
                fiber->stack_top -= nargs;
                *(fiber->stack_top - 1) = (nret > 0) ? args[0] : val_nil();
                return true;
            }
            default:
                break;
            }
        }
        runtime_error("value is not callable");
        return false;
    }

    /* =========================================================
    ** Fiber resume/yield
    ** ========================================================= */

    Value VM::resume_fiber(ObjFiber *fiber, Value val)
    {
        had_error_ = false;
        if (fiber->state == FIBER_DONE || fiber->state == FIBER_ERROR)
        {
            runtime_error("cannot resume finished fiber");
            return val_nil();
        }

        ObjFiber *caller = current_fiber_;
        fiber->caller = caller;
        fiber->transfer_value = val;
        fiber->state = FIBER_RUNNING;
        if (caller)
            caller->state = FIBER_SUSPENDED;
        current_fiber_ = fiber;

        execute(fiber);

        /* Restore whichever fiber was running before this call — the same
        ** hand-back OP_RESUME/OP_AWAIT do for the in-script equivalent.
        ** Without it current_fiber_ is left pointing at a finished/errored
        ** fiber, so the next runtime_error() from the host's own fiber
        ** would attach its message and stack trace to the wrong one. */
        current_fiber_ = caller;
        if (caller)
            caller->state = FIBER_RUNNING;

        if (had_error_ || fiber->state == FIBER_ERROR)
        {
            /* The error was already reported — via runtime_error(), from
            ** inside execute() — at the point it happened. transfer_value
            ** was never produced by a normal return/yield, so handing it
            ** back would let the host read stale or garbage data instead
            ** of seeing the failure. */
            had_error_ = true;
            return val_nil();
        }

        return fiber->transfer_value;
    }

    /* =========================================================
    ** Error
    ** ========================================================= */

    void VM::runtime_error(const char *fmt, ...)
    {
        had_error_ = true;

        /* Format the error message */
        va_list args;
        va_start(args, fmt);
        char msg[512];
        vsnprintf(msg, sizeof(msg), fmt, args);
        va_end(args);

        /* Populate fiber->error so callers (e.g. await) can read the message.
           Pause GC to avoid a collection between new_string() and the store. */
        if (current_fiber_)
        {
            int mlen = (int)strlen(msg);
            gc_.pause_depth++;
            current_fiber_->error = new_string(&gc_, msg, mlen);
            gc_.pause_depth--;
        }

        /* Emit via callback if configured, otherwise fall back to stderr */
        auto emit = [&](const char *s, int len)
        {
            if (callbacks_.print_err)
                callbacks_.print_err(s, len, callbacks_.userdata);
            else
                fwrite(s, 1, (size_t)len, stderr);
        };

        char header[600];
        int hlen = snprintf(header, sizeof(header), "[zen runtime error] %s\n", msg);
        emit(header, hlen);

        /* Stack trace */
        if (current_fiber_)
        {
            for (int i = current_fiber_->frame_count - 1; i >= 0; i--)
            {
                CallFrame *frame = &current_fiber_->frames[i];
                ObjFunc *func = frame->func;
                int offset = (int)(frame->ip - func->code - 1);
                int line = (offset >= 0 && offset < func->code_count) ? func->lines[offset] : 0;
                const char *fname = func->name ? func->name->chars : "<script>";
                const char *src = func->source ? func->source->chars : "?";
                char trace[256];
                int tlen = snprintf(trace, sizeof(trace),
                                    "  File \"%s\", line %d, in %s\n", src, line, fname);
                emit(trace, tlen);
            }
        }

        if (current_fiber_ && current_fiber_ != main_fiber_)
        {
            current_fiber_->state = FIBER_ERROR;
        }
    }

    /* =========================================================
    ** Builders (stub — implementar quando tiver classes a funcionar)
    ** ========================================================= */

    VM::ClassBuilder VM::def_class(const char *name)
    {
        return ClassBuilder(this, name);
    }

    VM::ClassBuilder::ClassBuilder(VM *vm, const char *name) : vm_(vm)
    {
        gc_pause(&vm->gc_);
        ObjString *s = intern_string(&vm->gc_, name, (int)strlen(name),
                                     hash_string(name, (int)strlen(name)));
        klass_ = new_class(&vm->gc_, s, nullptr);
    }

    VM::ClassBuilder &VM::ClassBuilder::parent(const char *parent_name)
    {
        int idx = vm_->find_global(parent_name);
        if (idx >= 0 && is_class(vm_->globals_[idx]))
        {
            klass_->parent = as_class(vm_->globals_[idx]);
        }
        return *this;
    }

    VM::ClassBuilder &VM::ClassBuilder::field(const char *name)
    {
        int idx = klass_->num_fields++;
        /* Grow field_names array */
        klass_->field_names = (ObjString **)zen_realloc(
            &vm_->gc_, klass_->field_names,
            sizeof(ObjString *) * idx,
            sizeof(ObjString *) * (idx + 1));
        klass_->field_names[idx] = intern_string(&vm_->gc_, name, (int)strlen(name),
                                                 hash_string(name, (int)strlen(name)));
        return *this;
    }

    VM::ClassBuilder &VM::ClassBuilder::generic_method(const char *name, GenericNativeFn fn, int generic_arity, int arity)
    {
        ObjString *s = intern_string(&vm_->gc_, name, (int)strlen(name),
                                     hash_string(name, (int)strlen(name)));
        ObjNative *nat = new_native_generic(&vm_->gc_, fn, generic_arity, arity, s);
        map_set(&vm_->gc_, klass_->methods, val_obj((Obj *)s), val_obj((Obj *)nat));

        /* Generic methods skip operator-slot registration on purpose: an
        ** overload like __add__<T> has no calling convention (OP_ADD_OBJ
        ** never carries type arguments) — only OP_INVOKE_GENERIC's plain
        ** name+vtable path applies here. */
        int name_len = (int)strlen(name);
        int slot = vm_->intern_selector(name, name_len);
        if (slot >= klass_->vtable_size)
        {
            int new_size = slot + 1;
            Value *new_vt = (Value *)zen_alloc(&vm_->gc_, sizeof(Value) * new_size);
            for (int i = 0; i < klass_->vtable_size; i++)
                new_vt[i] = klass_->vtable[i];
            for (int i = klass_->vtable_size; i < new_size; i++)
                new_vt[i] = val_nil();
            if (klass_->vtable)
                zen_free(&vm_->gc_, klass_->vtable, sizeof(Value) * klass_->vtable_size);
            klass_->vtable = new_vt;
            klass_->vtable_size = new_size;
        }
        klass_->vtable[slot] = val_obj((Obj *)nat);
        return *this;
    }

    VM::ClassBuilder &VM::ClassBuilder::method(const char *name, NativeFn fn, int arity, int flags)
    {
        ObjString *s = intern_string(&vm_->gc_, name, (int)strlen(name),
                                     hash_string(name, (int)strlen(name)));
        ObjNative *nat = new_native(&vm_->gc_, fn, arity, s);
        nat->flags = flags;
        map_set(&vm_->gc_, klass_->methods, val_obj((Obj *)s), val_obj((Obj *)nat));

        int name_len = (int)strlen(name);
        int op_slot = operator_slot_for_name(name, name_len);
        if (op_slot >= 0)
        {
            klass_->operator_slots[op_slot] = val_obj((Obj *)nat);
            return *this;
        }

        /* Also register in vtable */
        int slot = vm_->intern_selector(name, name_len);
        if (slot >= klass_->vtable_size)
        {
            int new_size = slot + 1;
            Value *new_vt = (Value *)zen_alloc(&vm_->gc_, sizeof(Value) * new_size);
            for (int i = 0; i < klass_->vtable_size; i++)
                new_vt[i] = klass_->vtable[i];
            for (int i = klass_->vtable_size; i < new_size; i++)
                new_vt[i] = val_nil();
            if (klass_->vtable)
                zen_free(&vm_->gc_, klass_->vtable, sizeof(Value) * klass_->vtable_size);
            klass_->vtable = new_vt;
            klass_->vtable_size = new_size;
        }
        klass_->vtable[slot] = val_obj((Obj *)nat);
        return *this;
    }

    VM::ClassBuilder &VM::ClassBuilder::ctor(NativeClassCtor fn)
    {
        klass_->native_ctor = fn;
        return *this;
    }

    VM::ClassBuilder &VM::ClassBuilder::dtor(NativeClassDtor fn)
    {
        klass_->native_dtor = fn;
        return *this;
    }

    VM::ClassBuilder &VM::ClassBuilder::persistent(bool p)
    {
        klass_->persistent = p;
        return *this;
    }

    VM::ClassBuilder &VM::ClassBuilder::constructable(bool c)
    {
        klass_->constructable = c;
        return *this;
    }

    ObjClass *VM::ClassBuilder::end()
    {
        /* Flatten parent vtable if parent exists */
        if (klass_->parent && klass_->parent->vtable_size > 0)
        {
            ObjClass *p = klass_->parent;
            if (klass_->vtable_size < p->vtable_size)
            {
                int new_size = p->vtable_size;
                Value *new_vt = (Value *)zen_alloc(&vm_->gc_, sizeof(Value) * new_size);
                for (int i = 0; i < klass_->vtable_size; i++)
                    new_vt[i] = klass_->vtable[i];
                for (int i = klass_->vtable_size; i < new_size; i++)
                    new_vt[i] = val_nil();
                if (klass_->vtable)
                    zen_free(&vm_->gc_, klass_->vtable, sizeof(Value) * klass_->vtable_size);
                klass_->vtable = new_vt;
                klass_->vtable_size = new_size;
            }
            for (int i = 0; i < p->vtable_size; i++)
            {
                if (is_nil(klass_->vtable[i]))
                    klass_->vtable[i] = p->vtable[i];
            }
        }
        if (klass_->parent)
        {
            for (int i = 0; i < SLOT_OPERATOR_COUNT; i++)
            {
                if (is_nil(klass_->operator_slots[i]))
                    klass_->operator_slots[i] = klass_->parent->operator_slots[i];
            }
        }
        vm_->def_global(klass_->name->chars, val_obj((Obj *)klass_));
        klass_->sealed = true;
        gc_resume(&vm_->gc_);
        return klass_;
    }

    VM::StructBuilder VM::def_struct(const char *name)
    {
        return StructBuilder(this, name);
    }

    VM::StructBuilder::StructBuilder(VM *vm, const char *name) : vm_(vm)
    {
        def_ = (ObjStructDef *)zen_alloc(&vm_->gc_, sizeof(ObjStructDef));
        def_->obj.type = OBJ_STRUCT_DEF;
        def_->obj.color = GC_WHITE;
        def_->obj.interned = 0;
        def_->obj.flags = 0;
        def_->obj.hash = 0;
        def_->obj.gc_next = vm_->gc_.objects;
        vm_->gc_.objects = (Obj *)def_;
        def_->name = vm_->make_string(name);
        def_->num_fields = 0;
        def_->field_names = nullptr;
    }

    VM::StructBuilder &VM::StructBuilder::field(const char *name)
    {
        int n = def_->num_fields + 1;
        ObjString **new_names = (ObjString **)zen_realloc(
            &vm_->gc_, def_->field_names,
            sizeof(ObjString *) * def_->num_fields,
            sizeof(ObjString *) * n);
        new_names[def_->num_fields] = vm_->make_string(name);
        def_->field_names = new_names;
        def_->num_fields = n;
        return *this;
    }

    ObjStructDef *VM::StructBuilder::end()
    {
        vm_->def_global(def_->name->chars, val_obj((Obj *)def_));
        return def_;
    }

    /* =========================================================
    ** NativeStructBuilder — zero-copy C++ struct binding
    ** ========================================================= */

    VM::NativeStructBuilder VM::register_native_struct(const char *name, uint16_t size,
                                                       NativeStructCtor ctor, NativeStructDtor dtor)
    {
        return NativeStructBuilder(this, name, size, ctor, dtor);
    }

    VM::NativeStructBuilder::NativeStructBuilder(VM *vm, const char *name, uint16_t size,
                                                  NativeStructCtor ctor, NativeStructDtor dtor)
        : vm_(vm)
    {
        gc_pause(&vm->gc_);
        def_ = (NativeStructDef *)zen_alloc(&vm->gc_, sizeof(NativeStructDef));
        def_->obj.type = OBJ_NATIVE_STRUCT_DEF;
        def_->obj.color = GC_BLACK;
        def_->obj.hash = 0;
        def_->obj.interned = 0;
        def_->obj.flags = 0;
        def_->obj.gc_next = vm->gc_.objects;
        vm->gc_.objects = (Obj *)def_;
        def_->name = intern_string(&vm->gc_, name, (int)strlen(name),
                                   hash_string(name, (int)strlen(name)));
        def_->struct_size = size;
        def_->num_fields = 0;
        def_->fields = nullptr;
        def_->ctor = ctor;
        def_->dtor = dtor;
    }

    VM::NativeStructBuilder &VM::NativeStructBuilder::field(const char *name, uint16_t offset,
                                                             NativeFieldType type, bool read_only)
    {
        int idx = def_->num_fields++;
        def_->fields = (NativeFieldDef *)zen_realloc(
            &vm_->gc_, def_->fields,
            sizeof(NativeFieldDef) * idx,
            sizeof(NativeFieldDef) * (idx + 1));
        def_->fields[idx].name = intern_string(&vm_->gc_, name, (int)strlen(name),
                                               hash_string(name, (int)strlen(name)));
        def_->fields[idx].offset = offset;
        def_->fields[idx].type = type;
        def_->fields[idx].read_only = read_only;
        return *this;
    }

    NativeStructDef *VM::NativeStructBuilder::end()
    {
        vm_->def_global(def_->name->chars, val_obj((Obj *)def_));
        gc_resume(&vm_->gc_);
        return def_;
    }

    Value VM::make_native_struct(NativeStructDef *def, Value *args, int nargs)
    {
        ObjNativeStruct *ns = (ObjNativeStruct *)zen_alloc_now(&gc_, sizeof(ObjNativeStruct));
        ns->obj.type = OBJ_NATIVE_STRUCT;
        ns->obj.color = GC_WHITE;
        ns->obj.hash = 0;
        ns->obj.interned = 0;
        ns->obj.flags = 0;
        ns->obj.gc_next = gc_.objects;
        gc_.objects = (Obj *)ns;
        ns->def = def;
        ns->data = zen_alloc_now(&gc_, def->struct_size);
        memset(ns->data, 0, def->struct_size);
        if (def->ctor)
            def->ctor(this, ns->data, nargs, args);
        return val_obj((Obj *)ns);
    }

    /* --- Instance API (C++ embedding) --- */

    

    Value VM::make_instance(ObjClass *klass)
    {
        gc_pause(&gc_);
        ObjInstance *inst = new_instance(&gc_, klass);
        /* Call native constructor — walk parent chain */
        ObjClass *ctor_src = klass;
        while (ctor_src && !ctor_src->native_ctor)
            ctor_src = ctor_src->parent;
        if (ctor_src && ctor_src->native_ctor)
        {
            inst->native_data = ctor_src->native_ctor(this, 0, nullptr);
        }
        gc_resume(&gc_);
        return val_obj((Obj *)inst);
    }

    Value VM::make_instance(ObjClass *klass, Value *args, int nargs)
    {
        gc_pause(&gc_);
        ObjInstance *inst = new_instance(&gc_, klass);
        Value self = val_obj((Obj *)inst);

        /* Call native constructor — walk parent chain */
        ObjClass *ctor_src = klass;
        while (ctor_src && !ctor_src->native_ctor)
            ctor_src = ctor_src->parent;
        if (ctor_src && ctor_src->native_ctor)
        {
            inst->native_data = ctor_src->native_ctor(this, nargs, args);
        }

        /* Call init if exists */
        int init_slot = find_selector("init", 4);
        if (init_slot >= 0 && init_slot < klass->vtable_size && !is_nil(klass->vtable[init_slot]))
        {
            /* Set up args: [self, arg0, arg1, ...] on stack */
            Value call_args[17]; /* self + max 16 args */
            call_args[0] = self;
            for (int i = 0; i < nargs && i < 16; i++)
                call_args[i + 1] = args[i];

            Value method = klass->vtable[init_slot];
            if (is_native(method))
            {
                ObjNative *nat = as_native(method);
                if (nat->generic_arity > 0)
                {
                    runtime_error("'%s.init' is a generic native method and cannot be constructed this way",
                                  klass->name ? klass->name->chars : "?");
                    gc_resume(&gc_);
                    return self;
                }
                nat->fn(this, call_args, nargs + 1);
            }
            else if (is_closure(method))
            {
                ObjClosure *cl = as_closure(method);
                /* Same reasoning as every other closure-call site: a
                ** generic init<T> constructed via this C++ embedding entry
                ** point must not silently bind a value arg into T's slot. */
                if (cl->func->generic_arity > 0)
                {
                    runtime_error("'%s.init' is generic and must be constructed with <...> type arguments",
                                  klass->name ? klass->name->chars : "?");
                    gc_resume(&gc_);
                    return self;
                }
                /* Push args to fiber stack and call */
                ObjFiber *fiber = current_fiber_;
                Value *base = fiber->stack_top;
                for (int i = 0; i <= nargs; i++)
                    base[i] = call_args[i];
                fiber->stack_top = base + cl->func->num_regs;

                CallFrame *frame = &fiber->frames[fiber->frame_count++];
                frame->closure = cl;
                frame->constants = cl->func->constants;
                frame->upvalues = cl->upvalues;
                frame->func = cl->func;
                frame->ip = cl->func->code;
                frame->base = base;
                frame->ret_reg = 0;
                frame->ret_count = 0;
                int prev_stop_depth = external_call_stop_depth_;
                external_call_stop_depth_ = fiber->frame_count - 1;
                execute(fiber);
                external_call_stop_depth_ = prev_stop_depth;
                fiber->stack_top = base; /* restore stack */
                fiber->state = FIBER_RUNNING; /* reset after FIBER_DONE */
            }
        }
        gc_resume(&gc_);
        return self;
    }

    void VM::destroy_instance(Value instance)
    {
        if (!is_instance(instance)) return;
        ObjInstance *inst = as_instance(instance);
        zen::destroy_instance(&gc_, inst);
    }

    Value VM::invoke(Value instance, const char *method_name, Value *args, int nargs)
    {
        if (nargs < 0 || (nargs > 0 && !args))
        {
            runtime_error("invoke received invalid arguments");
            return val_nil();
        }
        if (!is_instance(instance))
        {
            runtime_error("cannot invoke a method on a non-instance");
            return val_nil();
        }
        if (!method_name)
        {
            runtime_error("method name is null");
            return val_nil();
        }
        int name_len = (int)strlen(method_name);
        int op_slot = operator_slot_for_name(method_name, name_len);
        if (op_slot >= 0)
            return invoke_operator(instance, op_slot, args, nargs);

        /* Try vtable slot first (fast path) */
        int slot = find_selector(method_name, name_len);
        if (slot >= 0)
            return invoke(instance, slot, args, nargs);

        /* Fallback: look up method by name in class methods map */
        if (is_instance(instance))
        {
            ObjInstance *inst = as_instance(instance);
            ObjClass *klass = inst->klass;
            ObjString *key = intern_string(&gc_, method_name, name_len,
                                           hash_string(method_name, name_len));
            bool found = false;
            Value method = map_get(klass->methods, val_obj((Obj *)key), &found);
            if (found)
            {
                /* Call the method directly */
                if (is_closure(method))
                {
                    ObjClosure *cl = as_closure(method);
                    if (cl->func->generic_arity > 0)
                    {
                        runtime_error("'%s' is generic and must be invoked with <...> type arguments",
                                      method_name);
                        return val_nil();
                    }
                    ObjFiber *fiber = current_fiber_;
                    if (fiber->frame_count >= fiber->frame_capacity)
                    {
                        runtime_error("stack overflow (too many frames)");
                        return val_nil();
                    }
                    Value *base = fiber->stack_top;
                    base[0] = instance; /* self */
                    for (int i = 0; i < nargs; i++)
                        base[i + 1] = args[i];

                    fiber->stack_top = base + cl->func->num_regs;

                    int saved_frame_count = fiber->frame_count; /* save before push */
                    CallFrame *frame = &fiber->frames[fiber->frame_count++];
                    frame->closure = cl;
                    frame->constants = cl->func->constants;
                    frame->upvalues = cl->upvalues;
                    frame->func = cl->func;
                    frame->ip = cl->func->code;
                    frame->base = base;
                    frame->ret_reg = (fiber->frame_count >= 2)
                        ? (int)(base - fiber->frames[fiber->frame_count - 2].base)
                        : 0;
                    frame->ret_count = 1;
                    int prev_stop_depth = external_call_stop_depth_;
                    external_call_stop_depth_ = fiber->frame_count - 1;

                    execute(fiber);

                    external_call_stop_depth_ = prev_stop_depth;
                    Value result = base[0];
                    fiber->frame_count = saved_frame_count; /* restore: fixes RT_ERROR frame leak */
                    fiber->stack_top = base;
                    fiber->state = FIBER_RUNNING;
                    return result;
                }
                if (is_native(method))
                {
                    ObjNative *nat = as_native(method);
                    return call_native_from_cpp(nat, instance, args, nargs, true);
                }
            }
        }

        runtime_error("method '%s' not found", method_name);
        return val_nil();
    }

    Value VM::invoke(Value instance, int slot, Value *args, int nargs)
    {
        if (!is_instance(instance))
        {
            runtime_error("cannot invoke a method on a non-instance");
            return val_nil();
        }
        ObjInstance *inst = as_instance(instance);
        ObjClass *klass = inst->klass;

        if (slot >= klass->vtable_size || is_nil(klass->vtable[slot]))
        {
            runtime_error("vtable slot %d is nil", slot);
            return val_nil();
        }

        Value method = klass->vtable[slot];

        /* Set up call args: [self, arg0, arg1, ...] */
        if (is_native(method))
        {
            ObjNative *nat = as_native(method);
            return call_native_from_cpp(nat, instance, args, nargs, true);
        }
        else if (is_closure(method))
        {
            ObjClosure *cl = as_closure(method);
            if (cl->func->generic_arity > 0)
            {
                runtime_error("'%s' is generic and must be invoked with <...> type arguments",
                              cl->func->name ? cl->func->name->chars : "?");
                return val_nil();
            }
            ObjFiber *fiber = current_fiber_;
            if (fiber->frame_count >= fiber->frame_capacity)
            {
                runtime_error("stack overflow (too many frames)");
                return val_nil();
            }
            Value *base = fiber->stack_top;
            base[0] = instance; /* self */
            for (int i = 0; i < nargs; i++)
                base[i + 1] = args[i];

            fiber->stack_top = base + cl->func->num_regs;

            int saved_frame_count = fiber->frame_count; /* save before push */
            CallFrame *frame = &fiber->frames[fiber->frame_count++];
            frame->closure = cl;
            frame->constants = cl->func->constants;
            frame->upvalues = cl->upvalues;
            frame->func = cl->func;
            frame->ip = cl->func->code;
            frame->base = base;
            frame->ret_reg = (fiber->frame_count >= 2)
                ? (int)(base - fiber->frames[fiber->frame_count - 2].base)
                : 0;
            frame->ret_count = 1;
            int prev_stop_depth = external_call_stop_depth_;
            external_call_stop_depth_ = fiber->frame_count - 1;
            execute(fiber);
            external_call_stop_depth_ = prev_stop_depth;

            Value result = base[0];
            fiber->frame_count = saved_frame_count; /* restore: fixes RT_ERROR frame leak */
            fiber->stack_top = base; /* restore stack */
            fiber->state = FIBER_RUNNING; /* reset after FIBER_DONE */
            return result;
        }
        return val_nil();
    }

    Value VM::invoke_operator(Value instance, int slot, Value *args, int nargs)
    {
        if (!is_instance(instance))
        {
            runtime_error("cannot invoke an operator on a non-instance");
            return val_nil();
        }
        ObjInstance *inst = as_instance(instance);
        ObjClass *klass = inst->klass;

        if (slot < 0 || slot >= SLOT_OPERATOR_COUNT || is_nil(klass->operator_slots[slot]))
        {
            runtime_error("operator slot %d is nil", slot);
            return val_nil();
        }

        Value method = klass->operator_slots[slot];

        if (is_native(method))
        {
            ObjNative *nat = as_native(method);
            return call_native_from_cpp(nat, instance, args, nargs, true);
        }

        if (is_closure(method))
        {
            ObjClosure *cl = as_closure(method);
            /* A generic dunder (def __add__<T>(self, other): ...) is
            ** reachable via plain infix syntax (a + b) with no <...>
            ** opt-in possible at the call site at all — must reject, not
            ** silently bind `other` into T's register. */
            if (cl->func->generic_arity > 0)
            {
                runtime_error("'%s' is a generic operator method and cannot be invoked this way",
                              cl->func->name ? cl->func->name->chars : "?");
                return val_nil();
            }
            ObjFiber *fiber = current_fiber_;
            Value *base = fiber->stack_top;

            if (fiber->frame_count >= kMaxFrames)
            {
                runtime_error("stack overflow");
                return val_nil();
            }
            if (base + cl->func->num_regs > fiber->stack + fiber->stack_capacity)
            {
                runtime_error("stack overflow (data)");
                return val_nil();
            }

            base[0] = instance;
            for (int i = 0; i < nargs; i++)
                base[i + 1] = args[i];

            fiber->stack_top = base + cl->func->num_regs;

            /* Clear unused regs so GC never sees stale Values */
            for (int i = 1 + nargs; i < cl->func->num_regs; i++)
                base[i] = val_nil();

            CallFrame *frame = &fiber->frames[fiber->frame_count++];
            frame->closure = cl;
            frame->constants = cl->func->constants;
            frame->upvalues = cl->upvalues;
            frame->func = cl->func;
            frame->ip = cl->func->code;
            frame->base = base;
            frame->ret_reg = (int)(base - fiber->frames[fiber->frame_count - 2].base);
            frame->ret_count = 1;
            int prev_stop_depth = external_call_stop_depth_;
            external_call_stop_depth_ = fiber->frame_count - 1;
            execute(fiber);
            external_call_stop_depth_ = prev_stop_depth;

            Value result = base[0];
            fiber->stack_top = base;
            fiber->state = FIBER_RUNNING;
            return result;
        }

        runtime_error("operator slot %d is not callable", slot);
        return val_nil();
    }
 

    /* =========================================================
    ** File I/O — used by compiler for include/import
    ** ========================================================= */

    void VM::add_search_path(const char *dir)
    {
        if (num_search_paths_ >= MAX_SEARCH_PATHS)
            return;
        int len = 0;
        while (dir[len])
            len++;
        /* Strip trailing slash */
        while (len > 1 && dir[len - 1] == '/')
            len--;
        char *copy = (char *)malloc((size_t)len + 1);
        memcpy(copy, dir, (size_t)len);
        copy[len] = '\0';
        search_paths_[num_search_paths_++] = copy;
    }

    /* Try to open and read a file via I/O callbacks. Returns malloc'd buffer or nullptr. */
    char *VM::try_read_cb(const char *path, long *out_size)
    {
        void *ud = callbacks_.userdata;
        ZenFile f = callbacks_.io.open(path, "rb", ud);
        if (!f)
            return nullptr;

        /* Get file size via seek */
        callbacks_.io.seek(f, 0, 2 /*SEEK_END*/, ud);
        long size = callbacks_.io.seek(f, 0, 1 /*SEEK_CUR*/, ud);
        callbacks_.io.seek(f, 0, 0 /*SEEK_SET*/, ud);

        char *buf = (char *)malloc((size_t)size + 1);
        if (!buf)
        {
            callbacks_.io.close(f, ud);
            return nullptr;
        }
        long actual = callbacks_.io.read(f, buf, size, ud);
        buf[actual] = '\0';
        callbacks_.io.close(f, ud);

        if (out_size)
            *out_size = actual;
        return buf;
    }

    char *VM::read_file(const char *path, const char *relative_to, long *out_size,
                        char *resolved_path, int resolved_max)
    {
        char fullpath[1024];

        /* Helper: copy resolved path if requested */
        auto store_resolved = [&](const char *p)
        {
            if (resolved_path && resolved_max > 0)
            {
                int len = 0;
                while (p[len])
                    len++;
                if (len >= resolved_max)
                    len = resolved_max - 1;
                memcpy(resolved_path, p, (size_t)len);
                resolved_path[len] = '\0';
            }
        };

        /* 1. If absolute path, try directly */
        if (path[0] == '/')
        {
            char *result = try_read_cb(path, out_size);
            if (result)
                store_resolved(path);
            return result;
        }

        /* 1b. Try relative path from current working directory */
        {
            char *result = try_read_cb(path, out_size);
            if (result)
            {
                store_resolved(path);
                return result;
            }
        }

        /* 2. Try relative to the requesting file's directory */
        if (relative_to)
        {
            int dir_len = 0;
            for (int i = 0; relative_to[i]; i++)
                if (relative_to[i] == '/')
                    dir_len = i + 1;
            int path_len = 0;
            while (path[path_len])
                path_len++;
            if (dir_len + path_len < (int)sizeof(fullpath))
            {
                memcpy(fullpath, relative_to, (size_t)dir_len);
                memcpy(fullpath + dir_len, path, (size_t)path_len);
                fullpath[dir_len + path_len] = '\0';
                char *result = try_read_cb(fullpath, out_size);
                if (result)
                {
                    store_resolved(fullpath);
                    return result;
                }
            }
        }

        /* 3. Try each search path */
        for (int i = 0; i < num_search_paths_; i++)
        {
            int dlen = 0;
            while (search_paths_[i][dlen])
                dlen++;
            int plen = 0;
            while (path[plen])
                plen++;
            if (dlen + 1 + plen < (int)sizeof(fullpath))
            {
                memcpy(fullpath, search_paths_[i], (size_t)dlen);
                fullpath[dlen] = '/';
                memcpy(fullpath + dlen + 1, path, (size_t)plen);
                fullpath[dlen + 1 + plen] = '\0';
                char *result = try_read_cb(fullpath, out_size);
                if (result)
                {
                    store_resolved(fullpath);
                    return result;
                }
            }
        }

        return nullptr;
    }

    /* =========================================================
    ** Script module import — load a .py file as a module.
    **
    ** 1. Convert dots to '/' and append ".py" (e.g. "foo.bar" → "foo/bar.py")
    ** 2. Find the file using read_file() (search paths, relative, etc.)
    ** 3. Compile the source
    ** 4. Execute in an isolated scope (save/restore globals)
    ** 5. Collect all new globals into an ObjMap
    ** 6. Return the map (caller caches it)
    ** ========================================================= */

    ObjMap *VM::import_script_module(const char *mod_name)
    {
        /* Build filename: dots → '/', append ".py" */
        char filename[512];
        int fi = 0;
        for (int i = 0; mod_name[i] && fi < 500; i++)
        {
            if (mod_name[i] == '.')
                filename[fi++] = '/';
            else
                filename[fi++] = mod_name[i];
        }
        filename[fi] = '\0';

        /* Try "modname.py" first, then "modname/__init__.py" */
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s.py", filename);

        char resolved[1024];
        long src_size = 0;
        const char *relative_to = nullptr;

        /* If we have a currently executing function, use its source for relative resolution */
        if (current_fiber_ && current_fiber_->frame_count > 0)
        {
            CallFrame *cf = &current_fiber_->frames[current_fiber_->frame_count - 1];
            if (cf->func && cf->func->source)
                relative_to = cf->func->source->chars;
        }

        char *source = read_file(filepath, relative_to, &src_size, resolved, sizeof(resolved));
        if (!source)
        {
            /* Try __init__.py (package) */
            snprintf(filepath, sizeof(filepath), "%s/__init__.py", filename);
            source = read_file(filepath, relative_to, &src_size, resolved, sizeof(resolved));
        }
        if (!source)
            return nullptr;

        /* Save global count BEFORE compilation (compile creates global slots).
        ** Also snapshot values of ALL existing globals so we can detect which ones
        ** the module wrote to (some slots may be reused from the caller). */
        int saved_globals = num_globals_;

        /* Compile the module source */
        Compiler compiler;
        ObjFunc *mod_func = compiler.compile(&gc_, this, source, resolved);
        free(source);

        if (!mod_func)
        {
            /* Compilation failed — rollback NEW globals only */
            num_globals_ = saved_globals;
            return nullptr;
        }

        /* Snapshot all global values AFTER compile but BEFORE execution.
        ** The compiler creates slots with val_nil() for module's defs. */
        int post_compile_globals = num_globals_;
        Value *snapshot = (Value *)malloc(post_compile_globals * sizeof(Value));
        for (int si = 0; si < post_compile_globals; si++)
            snapshot[si] = globals_[si];

        /* Execute module in a separate fiber so we don't clobber the caller's state.
        ** We wrap mod_func in a closure, create a new fiber, and execute it. */
        gc_.pause_depth++;
        ObjClosure *mod_cl = (ObjClosure *)zen_alloc(&gc_, sizeof(ObjClosure));
        gc_.pause_depth--;
        mod_cl->obj.type = OBJ_CLOSURE;
        mod_cl->obj.color = GC_BLACK;
        mod_cl->obj.hash = 0;
        mod_cl->obj.gc_next = gc_.objects;
        gc_.objects = (Obj *)mod_cl;
        mod_cl->func = mod_func;
        mod_cl->upvalues = nullptr;
        mod_cl->upvalue_count = 0;

        ObjFiber *mod_fiber = new_fiber(mod_cl, 256);
        mod_fiber->frame_count = 1;
        CallFrame *mf = &mod_fiber->frames[0];
        mf->closure = mod_cl;
        mf->constants = mod_cl->func->constants;
        mf->upvalues = mod_cl->upvalues;
        mf->func = mod_func;
        mf->ip = mod_func->code;
        mf->base = mod_fiber->stack;
        mf->ret_reg = 0;
        mf->ret_count = 0;
        mod_fiber->stack_top = mod_fiber->stack + mod_func->num_regs;
        mod_fiber->state = FIBER_RUNNING;

        /* Save current fiber and execute module */
        ObjFiber *saved_fiber = current_fiber_;
        current_fiber_ = mod_fiber;
        execute(mod_fiber);
        current_fiber_ = saved_fiber;

        if (had_error_)
        {
            /* Module had a runtime error — rollback globals */
            free(snapshot);
            num_globals_ = saved_globals;
            had_error_ = false;
            return nullptr;
        }

        /* Build ObjMap with module exports.
        ** Export any global that was:
        **   1. Newly created by the module compile (index >= saved_globals), OR
        **   2. Existed before but was MODIFIED by the module execution
        **      (was nil in snapshot, now has a value — i.e. the module wrote to
        **       a slot that was pre-allocated by the caller's compiler) */
        ObjMap *mod = new_map(&gc_);
        mod->is_module = true;
        for (int gi = 0; gi < num_globals_; gi++)
        {
            const char *gname = global_name(gi);
            if (!gname)
                continue;
            /* Skip private names (start with _) */
            if (gname[0] == '_')
                continue;

            bool is_new = (gi >= saved_globals);
            bool was_modified = (gi < post_compile_globals &&
                                 is_nil(snapshot[gi]) && !is_nil(globals_[gi]));

            if (!is_new && !was_modified)
                continue;

            int glen = (int)strlen(gname);
            ObjString *key = intern_string(&gc_, gname, glen, hash_string(gname, glen));
            map_set(&gc_, mod, val_obj((Obj *)key), globals_[gi]);
        }

        free(snapshot);
        return mod;
    }

} /* namespace zen */
