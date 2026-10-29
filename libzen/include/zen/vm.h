#ifndef ZEN_VM_H
#define ZEN_VM_H

#include "memory.h"
#include "opcodes.h"
#include "module.h"

namespace zen
{

    /* Forward declarations */
    struct NativeStructDef;

    /* =========================================================
    ** ZenIO — platform-abstracted file I/O (like SDL_RWops).
    **
    ** Override to redirect all file I/O for embedded/SDL/WASM.
    ** Default implementations use standard C stdio.
    **
    ** Usage from C++ embedding:
    **   ZenCallbacks cb = zen_default_callbacks();
    **   cb.io.open = my_open;  // e.g. SDL_RWFromFile
    **   cb.io.read = my_read;  // e.g. SDL_RWread
    **   ...
    **   vm.set_callbacks(cb);
    ** ========================================================= */

    /* Opaque file handle returned by open(). */
    typedef void *ZenFile;

    struct ZenIO
    {
        /* Open a file. mode: "r", "rb", "w", "wb", "a", "ab".
        ** Returns opaque handle, or nullptr on failure. */
        ZenFile (*open)(const char *path, const char *mode, void *userdata);

        /* Read up to `size` bytes into `buf`. Returns bytes actually read. */
        long (*read)(ZenFile file, void *buf, long size, void *userdata);

        /* Write `size` bytes from `buf`. Returns bytes actually written. */
        long (*write)(ZenFile file, const void *buf, long size, void *userdata);

        /* Seek to offset. whence: 0=SET, 1=CUR, 2=END. Returns new position or -1. */
        long (*seek)(ZenFile file, long offset, int whence, void *userdata);

        /* Close handle. Returns 0 on success. */
        int (*close)(ZenFile file, void *userdata);

        /* Check if file exists (without opening). Returns non-zero if it does. */
        int (*exists)(const char *path, void *userdata);
    };

    /* =========================================================
    ** ZenCallbacks — all platform hooks in one struct.
    ** ========================================================= */
    struct ZenCallbacks
    {
        /* Granular file I/O (open/read/write/seek/close) */
        ZenIO io;

        /* Print output (used by the print statement). */
        void (*print)(const char *str, int len, void *userdata);

        /* Error output (used by runtime errors). */
        void (*print_err)(const char *str, int len, void *userdata);

        /* Opaque user pointer passed to all callbacks. */
        void *userdata;
    };

    /* Returns default callbacks using stdio (fopen/fread/fwrite/fclose). */
    ZenCallbacks zen_default_callbacks();

    /*
    ** CallFrame — estado de uma chamada activa.
    ** Igual ao conceito do toy_vm mas com multi-return.
    */
    struct CallFrame
    {
        ObjClosure *closure; /* closure a executar (nullptr = top-level script) */
        ObjFunc *func;       /* func shortcut (== closure->func) */
        Value *constants;    /* == func->constants, cached so LOAD_STATE reads only the frame */
        ObjUpvalue **upvalues; /* == closure->upvalues, same reason */
        Instruction *ip;     /* instruction pointer (ponteiro directo) */
        Value *base;         /* base dos registos deste frame */
        int ret_reg;         /* registo no caller onde começam os resultados */
        int ret_count;       /* quantos resultados o caller espera (-1=all) */
    };

    /*
    ** VM — estado completo do interpretador.
    **
    ** Classe em vez de struct+funções livres:
    **   - Encapsulamento natural (internals privados)
    **   - Métodos inline no hot path sem overhead
    **   - Permite herdar/mock para testes se necessário
    **   - this implícito em vez de passar vm* em tudo
    */
    class VM
    {
    public:
        VM();
        ~VM();

        /* Não copiável */
        VM(const VM &) = delete;
        VM &operator=(const VM &) = delete;

        /* --- Execução --- */
        void run(ObjFunc *func);
        void run(ObjClosure *closure);
        Value call_global(int idx, Value *args, int nargs);
        Value call_global(const char *name, Value *args, int nargs);
        Value call_fn(Value callee, Value *args, int nargs); /* call any callable from native */

        /* --- Globals (by index — O(1), used at runtime) --- */
        int def_global(const char *name, Value val); /* returns index */
        Value get_global(int idx) const { return globals_[idx]; }
        void set_global(int idx, Value val) { globals_[idx] = val; }

        /* --- Globals (by name — O(n), used at compile/embed time only) --- */
        int find_global(const char *name) const;
        Value get_global(const char *name) const;
        void set_global(const char *name, Value val);
        int num_globals() const { return num_globals_; }
        /* Trim global table back to `n` — used to undo speculative compilation */
        void shrink_globals(int n) { if (n < num_globals_) num_globals_ = n; }
        const char *global_name(int idx) const { return global_names_[idx] ? global_names_[idx]->chars : nullptr; }

        /* --- Natives --- */
        int def_native(const char *name, NativeFn fn, int arity, int flags = 0);
        /* Keyword arguments of the native call in progress (nullptr when
        ** none): the compiler passes `f(x, key=v)` to a native as a trailing
        ** map, the VM parks it here around the call. */
        ObjMap *kwargs() const { return kwargs_; }
        ObjMap *kwargs_ = nullptr;

        /* --- Module registry --- */
        void register_lib(const NativeLib *lib);  /* make available for import */
        void open_lib(const NativeLib *lib);       /* register + put in globals */
        const NativeLib *find_lib(const char *name) const;
        int open_lib_globals(const NativeLib *lib, bool warn_shadows = false); /* returns base gidx */
        const NativeLib *try_load_plugin(const char *name); /* dlopen fallback */
        void resolve_native_globals(); /* fill nil globals from registered libs */

        /* --- Script module import (.py files) --- */
        /* Try to import a .py file as a module. Returns ObjMap* or nullptr. */
        ObjMap *import_script_module(const char *mod_name);

        /* --- Class builder --- */
        struct ClassBuilder
        {
            ClassBuilder(VM *vm, const char *name);
            ClassBuilder &parent(const char *parent_name);
            ClassBuilder &field(const char *name);
            ClassBuilder &method(const char *name, NativeFn fn, int arity, int flags = 0); /* flags: ZEN_NATIVE_GC_SAFE */
            /* entity.get_component<Transform>(): a native method taking
            ** `generic_arity` type arguments ahead of `arity` value
            ** arguments. Variadic value arity (-1, like plain method()) is
            ** not supported for generics yet — arity must be >= 0. */
            ClassBuilder &generic_method(const char *name, GenericNativeFn fn, int generic_arity, int arity);
            /* Native property: `obj.name` calls getter, `obj.name = v` calls setter
            ** (nullptr = read-only: assigning is a runtime error; a null getter
            ** makes it write-only). Registering a name again completes or
            ** replaces it. Same native
            ** convention as method(): self at args[-1]; getter takes no
            ** arguments, leaves the value in args[0] and returns 1; setter gets
            ** the value in args[0] and returns 0. A script subclass inherits it.
            ** Lookup order for `inst.name`: instance field, property, method. */
            ClassBuilder &property(const char *name, NativeFn getter, NativeFn setter = nullptr, int flags = 0);
            ClassBuilder &ctor(NativeClassCtor fn);       /* native constructor (returns void*) */
            ClassBuilder &dtor(NativeClassDtor fn);       /* native destructor */
            ClassBuilder &persistent(bool p = true);      /* instances NOT managed by GC */
            ClassBuilder &constructable(bool c = true);   /* false = script cannot instantiate */
            ObjClass *end();

        private:
            VM *vm_;
            ObjClass *klass_;
        };
        ClassBuilder def_class(const char *name);

        /* --- Struct builder (lightweight named tuples) --- */
        struct StructBuilder
        {
            StructBuilder(VM *vm, const char *name);
            StructBuilder &field(const char *name);
            ObjStructDef *end();

        private:
            VM *vm_;
            ObjStructDef *def_;
        };
        StructBuilder def_struct(const char *name);

        /* --- Native struct builder (zero-copy C++ struct binding) --- */
        struct NativeStructBuilder
        {
            NativeStructBuilder(VM *vm, const char *name, uint16_t size,
                                NativeStructCtor ctor, NativeStructDtor dtor);
            NativeStructBuilder &field(const char *name, uint16_t offset,
                                       NativeFieldType type, bool read_only = false);
            /* Convenience shortcuts */
            NativeStructBuilder &i32(const char *name, uint16_t offset, bool ro = false)  { return field(name, offset, FIELD_INT, ro); }
            NativeStructBuilder &u32(const char *name, uint16_t offset, bool ro = false)  { return field(name, offset, FIELD_UINT, ro); }
            NativeStructBuilder &f32(const char *name, uint16_t offset, bool ro = false)  { return field(name, offset, FIELD_FLOAT, ro); }
            NativeStructBuilder &f64(const char *name, uint16_t offset, bool ro = false)  { return field(name, offset, FIELD_DOUBLE, ro); }
            NativeStructBuilder &byte(const char *name, uint16_t offset, bool ro = false) { return field(name, offset, FIELD_BYTE, ro); }
            NativeStructBuilder &boolean(const char *name, uint16_t offset, bool ro = false) { return field(name, offset, FIELD_BOOL, ro); }
            NativeStructBuilder &ptr(const char *name, uint16_t offset, bool ro = false)  { return field(name, offset, FIELD_POINTER, ro); }
            NativeStructDef *end();

        private:
            VM *vm_;
            NativeStructDef *def_;
        };
        NativeStructBuilder register_native_struct(const char *name, uint16_t size,
                                                   NativeStructCtor ctor = nullptr,
                                                   NativeStructDtor dtor = nullptr);

        /* Create a native struct instance from C++ (e.g. inside a NativeFn) */
        Value make_native_struct(NativeStructDef *def, Value *args = nullptr, int nargs = 0);

        /* --- Instance API (for C++ embedding) --- */
        Value make_instance(ObjClass *klass);                    /* creates instance, no init */
        Value make_instance(ObjClass *klass, Value *args, int nargs); /* creates + calls init */
        /* Wraps a host-owned object: new instance of `klass` (or of a script
        ** subclass) with native_data = ptr, WITHOUT running the native
        ** constructor or init. The instance is marked borrowed: neither the GC
        ** nor destroy_instance() ever calls the class's native_dtor on ptr. */
        Value make_instance_borrowed(ObjClass *klass, void *native_data);
        void destroy_instance(Value instance);                   /* destroy persistent instance */
        /* Create a native struct instance from C++ (e.g. inside a NativeFn). */
        Value invoke(Value instance, const char *method, Value *args, int nargs);
        Value invoke(Value instance, int slot, Value *args, int nargs); /* vtable slot, O(1) */
        Value invoke_operator(Value instance, int slot, Value *args, int nargs);

        enum OperatorSlot : int
        {
            SLOT_ADD = 0,
            SLOT_RADD,
            SLOT_SUB,
            SLOT_RSUB,
            SLOT_MUL,
            SLOT_RMUL,
            SLOT_DIV,
            SLOT_RDIV,
            SLOT_MOD,
            SLOT_RMOD,
            SLOT_NEG,
            SLOT_EQ,
            SLOT_LT,
            SLOT_LE,
            SLOT_GT, /* reflected partner of LT: `a < b` may run b.__gt__(a) */
            SLOT_GE, /* reflected partner of LE */
            SLOT_STR,
            SLOT_LEN,
            SLOT_OPERATOR_COUNT = kOperatorSlotCount
        };

              /* --- Fiber API --- */
        ObjFiber *new_fiber(ObjClosure *closure, int stack_size = 256, int max_frames = kMaxFrames);
        Value resume_fiber(ObjFiber *fiber, Value val);

        /* --- File I/O (for import) --- */
        void add_search_path(const char *dir);
        char *read_file(const char *path, const char *relative_to, long *out_size,
                        char *resolved_path = nullptr, int resolved_max = 0);

        /* --- Callbacks (platform I/O) --- */
        void set_callbacks(const ZenCallbacks &cb) { callbacks_ = cb; }
        const ZenCallbacks &callbacks() const { return callbacks_; }

        /* --- Strings (para embedding — internadas) --- */
        ObjString *make_string(const char *str, int length = -1);

        /* --- GC --- */
        GC &get_gc() { return gc_; }
        ObjFiber *current_fiber() const { return current_fiber_; }

        /* --- Debugging ---
        ** With a hook set, the VM calls it before an instruction whose source
        ** line, function or call depth differs from the last one it reported:
        ** once a line, so a host can stop at a breakpoint or a step and read
        ** the frames meanwhile. The hook is not called again while it runs.
        ** Without one the dispatch is as it always was. Frame 0 is the
        ** innermost; a local is one of the frame's named ones in scope at its
        ** instruction.
        ** The dispatch only has the hook in a build with ZEN_DEBUG_HOOK=1; in
        ** any other set_debug_hook returns false and the VM runs exactly as it
        ** would with no debugger. */
        typedef void (*DebugHook)(VM *vm, void *user);
        bool set_debug_hook(DebugHook hook, void *user);
        bool debugging() const { return debug_hook_ != nullptr; }
        int debug_frame_count() const;
        ObjFunc *debug_frame_function(int depth) const;
        int debug_frame_line(int depth) const;
        int debug_local_count(int depth) const;
        const char *debug_local_name(int depth, int index) const;
        Value debug_local_value(int depth, int index) const;
        /* The value as repr() writes it, cut to fit capacity (with its
        ** terminator); the length written. */
        int debug_value_text(Value value, char *out, int capacity);

        /* --- GC root for natives (Lua-style) ---
        ** Copies v into the slot at the current fiber's stack_top and bumps
        ** it. The GC marks the fiber stack up to stack_top, so the value
        ** survives any collection a later allocation triggers. The VM resets
        ** stack_top when the native returns — roots live exactly as long as
        ** the call, no cleanup in the native. Only meaningful inside a
        ** ZEN_NATIVE_GC_SAFE native; under the default paused-GC convention
        ** it is harmless and does nothing useful. Returns the slot, or NULL
        ** (with a runtime error raised) if the fiber stack is full. */
        Value *root(Value v);
        void collect();

        /* --- Collector tuning ---
        ** min_threshold_bytes: the heap size below which no collection runs
        ** (the first cycle fires there). grow_factor: after a cycle the next
        ** one is scheduled at live_bytes * grow_factor (never below the
        ** minimum). Out-of-range values are clamped, not rejected: the
        ** minimum to >= 64 KB, the factor to >= 1.0. Raising the minimum
        ** above the current heap size moves the pending threshold up at
        ** once; while the GC is paused the saved threshold is updated. */
        void set_gc_tuning(size_t min_threshold_bytes, float grow_factor);
        void gc_tuning(size_t *min_out, float *factor_out) const;

        void gc_mark_roots(); /* mark all VM roots for GC */

        /* --- Host references: GC roots owned by C++ ---
        ** A Value kept only in C++ memory (a stored callback, a script
        ** component instance, a cached handle) is invisible to the
        ** collector, which runs at any script call or loop back edge: it
        ** would be freed under the host. ref() keeps v alive until unref().
        **
        **   int h = vm.ref(callback);   // handle, valid until unref(h)
        **   ... any amount of script execution / collections ...
        **   Value cb = vm.deref(h);     // the same object, still alive
        **   vm.call_fn(cb, args, n);
        **   vm.unref(h);
        **
        ** Handles are indices into a table with a free list, so they are
        ** cheap, reusable and stable (never invalidated by other refs).
        ** unref() of an already-released (or never-issued) handle is
        ** ignored -- a double release cannot corrupt the free list; deref()
        ** of such a handle returns nil. Prefer the zen::Ref RAII wrapper
        ** (below) so the release cannot be forgotten. */
        int ref(Value v);
        Value deref(int h) const;
        void unref(int h);
        int ref_count() const; /* live references (for tests / leak checks) */

        /* --- Error reporting (for native functions) --- */
        void runtime_error(const char *fmt, ...);

    private:
/*
** Dispatch mode — seleccionado em compile-time:
**   -DZEN_COMPUTED_GOTO   → computed goto (GCC/Clang, ~45% mais rápido)
**   default               → switch (portável, MSVC)
**
** Detecção automática se não definido explicitamente:
*/
#ifndef ZEN_DISPATCH_MODE
#if defined(__GNUC__) || defined(__clang__)
#define ZEN_COMPUTED_GOTO
#endif
#endif

        /* --- Dispatch (hot path) — implementado em vm_dispatch.cpp --- */
        void execute(ObjFiber *fiber);

        /* --- Helpers internos --- */
        ObjUpvalue *capture_upvalue(ObjFiber *fiber, Value *local);
        void close_upvalues(ObjFiber *fiber, Value *last);
        bool call_value(ObjFiber *fiber, Value callee, int nargs, int nresults);
        /* self_slots = 1: the receiver already sits on the stack just below
        ** the nargs arguments (a bound-method call); arity counts the
        ** arguments only, as for OP_INVOKE. */
        bool call_closure(ObjFiber *fiber, ObjClosure *closure, int nargs, int nresults,
                          int self_slots = 0);
        /* C++ entry points do not put their arguments on a GC-marked VM
        ** stack. Keep the public native-call convention in one place. */
        Value call_native_from_cpp(ObjNative *native, Value receiver,
                                   Value *args, int nargs, bool has_receiver,
                                   bool check_arity = true);
        /* `self` != nullptr: a bound-method call, *self becomes parameter 0. */
        Value call_closure_from_cpp(ObjClosure *closure, Value *args, int nargs,
                                    const Value *self = nullptr);
        void run_nested(ObjClosure *closure);
        char *try_read_cb(const char *path, long *out_size); /* read file via callbacks */

        /* find_global moved to public */

        /* --- Estado --- */
        GC gc_;

        /* Host reference table (ref/deref/unref). Opaque here so this public
        ** header does not pull in the ct containers; defined in vm.cpp. */
        struct HostRefs;
        HostRefs *host_refs_;

        /* Globals — dynamic. Grows on demand via def_global up to
        ** kMaxGlobalsHard (65536, dictated by OP_GETGLOBAL Bx encoding).
        ** Same safety argument as selectors_: callers only hold integer
        ** indices, never raw pointers into the array, so realloc is safe. */
        Value *globals_;
        ObjString **global_names_;
        int num_globals_;
        int globals_capacity_;
        bool grow_globals(int required); /* internal — returns false on hard cap */

        /* Main fiber (script top-level corre aqui) */
        ObjFiber *main_fiber_;
        ObjFiber *current_fiber_; /* fiber actualmente a executar */
        int fiber_depth_;         /* current nested execute() depth */
        int run_depth_;           /* nested VM::run() depth — see run_nested */
        int external_call_stop_depth_; /* return to C++ when nested script call unwinds here */
        bool had_error_;          /* runtime error occurred */

        DebugHook debug_hook_ = nullptr;
        void *debug_user_ = nullptr;
        bool in_debug_hook_ = false;
        ObjFunc *debug_func_ = nullptr;
        int debug_line_ = -1;
        int debug_depth_ = -1;
        void debug_step(ObjFiber *fiber);
        const CallFrame *debug_frame(int depth) const;
        const LocVar *debug_local(int depth, int index) const;

        /* Search paths for include/import */
        static const int MAX_SEARCH_PATHS = 16;
        char *search_paths_[MAX_SEARCH_PATHS];
        int num_search_paths_;

        /* Platform I/O callbacks */
        ZenCallbacks callbacks_;

        /* Module registry */
        static const int MAX_LIBS = 32;
        const NativeLib *libs_[MAX_LIBS];
        int num_libs_;

        /* Loaded plugin handles (for cleanup) */
        static const int MAX_PLUGINS = 16;
        void *plugin_handles_[MAX_PLUGINS];
        int num_plugins_;

        /* Method selector table (for vtable dispatch).
        ** Dynamic: grows on demand via intern_selector().
        ** The array of pointers may move on realloc, but:
        **   - Slot indices (int) returned to callers stay valid forever
        **     (we only append, never reorder).
        **   - The ObjString* objects themselves are GC-allocated and never
        **     move when this array reallocates.
        ** So no caller is at risk of dangling references after a grow. */
        static const int kInitSelectorCapacity = 32;
        ObjString **selectors_;       /* interned method names (heap-allocated) */
        int num_selectors_;
        int selectors_capacity_;
        int init_selector_; /* vtable slot of "__init__": -1 until that name is interned */

        /* Builtin-method selector slots — resolved once at construction so
        ** invoke_string.inl (and friends) can dispatch via switch(sel_slot)
        ** instead of the memcmp chain OP_INVOKE otherwise falls back to for
        ** non-instance receivers. See docs/plano-selector-dispatch-builtins.md. */
        struct BuiltinSelectors
        {
            int str_len, str_sub, str_find, str_upper, str_lower, str_split,
                str_trim, str_strip, str_replace, str_starts_with, str_startswith,
                str_ends_with, str_endswith, str_char_at, str_byte_at, str_repeat,
                str_count, str_pad_left, str_pad_right, str_contains, str_reverse,
                str_join, str_lstrip, str_rstrip, str_title, str_capitalize,
                str_swapcase, str_isalpha, str_isdigit, str_isalnum, str_isspace,
                str_isupper, str_islower, str_index, str_rfind, str_rindex,
                str_center, str_ljust, str_rjust, str_zfill, str_splitlines,
                str_rsplit, str_partition, str_rpartition;

            int arr_push, arr_append, arr_pop, arr_len, arr_remove, arr_insert,
                arr_slice, arr_reverse, arr_clear, arr_contains, arr_join,
                arr_sort, arr_index_of, arr_index, arr_dump, arr_count,
                arr_extend, arr_copy;

            int map_set, map_get, map_has, map_delete, map_keys, map_values,
                map_items, map_size, map_clear, map_dump, map_update,
                map_setdefault, map_pop, map_copy;

            int set_add, set_has, set_delete, set_size, set_clear, set_values,
                set_dump, set_discard, set_remove, set_issubset, set_issuperset,
                set_isdisjoint, set_union, set_intersection, set_difference,
                set_symmetric_difference, set_copy;

            int buf_len, buf_fill, buf_byte_len, buf_tolist, buf_copy,
                buf_slice, buf_type_name;

            int vec_length, vec_length_sq, vec_normalized, vec_dot, vec_cross,
                vec_distance, vec_distance_sq, vec_lerp, vec_abs, vec_with_x,
                vec_with_y, vec_with_z, vec_to_list;
        };
        BuiltinSelectors bsel_;
        void init_builtin_selectors();

        /* Rare branches pulled out of execute(). Keeping them inline made the
        ** interpreter body large enough that ip/R/K/frame could not all stay
        ** in machine registers, so every opcode paid reload traffic. */
        bool call_make_struct(Value *R, int a, int nargs, Value callee);
        void call_make_native_struct(Value *R, int a, int nargs, Value callee);
        ObjFunc *compile_eval_source(const char *source);

        /* Dense method ids, per receiver type.
        **
        ** bsel_ turned each builtin method name into a selector slot, which
        ** removed the memcmp chain but left a linear scan of int comparisons
        ** (44 of them for the last string method). These tables collapse that
        ** scan into one array read: sel_slot indexes straight into a dense
        ** id, and the invoke_*.inl files switch() on that id — case labels
        ** are enum constants, so the compiler builds a real jump table on
        ** every target, MSVC's switch-dispatch mode included.
        **
        ** Per type, not global: `len`, `copy`, `count` and 13 other names are
        ** shared between receivers and intern to the SAME slot (101 fields,
        ** 79 distinct slots), so one global table could not tell
        ** string.len from array.len. */
        enum StrMethod : uint8_t { STR_NONE = 0, STR_LEN, STR_SUB, STR_FIND,
            STR_UPPER, STR_LOWER, STR_SPLIT, STR_TRIM, STR_STRIP, STR_REPLACE,
            STR_STARTS_WITH, STR_STARTSWITH, STR_ENDS_WITH, STR_ENDSWITH,
            STR_CHAR_AT, STR_BYTE_AT, STR_REPEAT, STR_COUNT, STR_PAD_LEFT,
            STR_PAD_RIGHT, STR_CONTAINS, STR_REVERSE, STR_JOIN, STR_LSTRIP,
            STR_RSTRIP, STR_TITLE, STR_CAPITALIZE, STR_SWAPCASE, STR_ISALPHA,
            STR_ISDIGIT, STR_ISALNUM, STR_ISSPACE, STR_ISUPPER, STR_ISLOWER,
            STR_INDEX, STR_RFIND, STR_RINDEX, STR_CENTER, STR_LJUST, STR_RJUST,
            STR_ZFILL, STR_SPLITLINES, STR_RSPLIT, STR_PARTITION,
            STR_RPARTITION };
        enum ArrMethod : uint8_t { ARR_NONE = 0, ARR_PUSH, ARR_APPEND, ARR_POP,
            ARR_LEN, ARR_REMOVE, ARR_INSERT, ARR_SLICE, ARR_REVERSE, ARR_CLEAR,
            ARR_CONTAINS, ARR_JOIN, ARR_SORT, ARR_INDEX_OF, ARR_INDEX, ARR_DUMP,
            ARR_COUNT, ARR_EXTEND, ARR_COPY };
        enum MapMethod : uint8_t { MAP_NONE = 0, MAP_SET, MAP_GET, MAP_HAS,
            MAP_DELETE, MAP_KEYS, MAP_VALUES, MAP_ITEMS, MAP_SIZE, MAP_CLEAR,
            MAP_DUMP, MAP_UPDATE, MAP_SETDEFAULT, MAP_POP, MAP_COPY };
        enum SetMethod : uint8_t { SET_NONE = 0, SET_ADD, SET_HAS, SET_DELETE,
            SET_SIZE, SET_CLEAR, SET_VALUES, SET_DUMP, SET_DISCARD, SET_REMOVE,
            SET_ISSUBSET, SET_ISSUPERSET, SET_ISDISJOINT, SET_UNION,
            SET_INTERSECTION, SET_DIFFERENCE, SET_SYMMETRIC_DIFFERENCE,
            SET_COPY };
        enum BufMethod : uint8_t { BUF_NONE = 0, BUF_LEN, BUF_FILL,
            BUF_BYTE_LEN, BUF_TOLIST, BUF_COPY, BUF_SLICE, BUF_TYPE_NAME };
        enum VecMethod : uint8_t { VEC_NONE = 0, VEC_LENGTH, VEC_LENGTH_SQ,
            VEC_NORMALIZED, VEC_DOT, VEC_CROSS, VEC_DISTANCE, VEC_DISTANCE_SQ,
            VEC_LERP, VEC_ABS, VEC_WITH_X, VEC_WITH_Y, VEC_WITH_Z, VEC_TO_LIST };

        /* Sized to the builtin selectors, which are interned first and so
        ** occupy slots [0, num_builtin_selectors_). A sel_slot at or past
        ** that bound is a user-defined method name and cannot name a builtin
        ** — the bound check below is what keeps the lookup in range. */
        int num_builtin_selectors_ = 0;
        uint8_t *str_method_ = nullptr;
        uint8_t *arr_method_ = nullptr;
        uint8_t *map_method_ = nullptr;
        uint8_t *set_method_ = nullptr;
        uint8_t *buf_method_ = nullptr;
        uint8_t *vec_method_ = nullptr;
        void init_builtin_method_tables();

        /* One array read, bounds-checked. Returns *_NONE for anything that is
        ** not a builtin method of that receiver type. */
        inline uint8_t str_method(int sel) const
        { return (unsigned)sel < (unsigned)num_builtin_selectors_ ? str_method_[sel] : 0; }
        inline uint8_t arr_method(int sel) const
        { return (unsigned)sel < (unsigned)num_builtin_selectors_ ? arr_method_[sel] : 0; }
        inline uint8_t map_method(int sel) const
        { return (unsigned)sel < (unsigned)num_builtin_selectors_ ? map_method_[sel] : 0; }
        inline uint8_t set_method(int sel) const
        { return (unsigned)sel < (unsigned)num_builtin_selectors_ ? set_method_[sel] : 0; }
        inline uint8_t buf_method(int sel) const
        { return (unsigned)sel < (unsigned)num_builtin_selectors_ ? buf_method_[sel] : 0; }
        inline uint8_t vec_method(int sel) const
        { return (unsigned)sel < (unsigned)num_builtin_selectors_ ? vec_method_[sel] : 0; }

    public:
        bool had_error() const { return had_error_; }

        /* Intern a method name → slot index (0..255). Same name → same slot. */
        int intern_selector(const char *name, int len);
        int find_selector(const char *name, int len) const;
        int num_selectors() const { return num_selectors_; }
        const char *selector_name(int idx) const { return (idx >= 0 && idx < num_selectors_ && selectors_[idx]) ? selectors_[idx]->chars : nullptr; }
        /* The interned ObjString behind a selector slot, hash already
        ** computed — module dispatch needs the key, not just the chars. */
        ObjString *selector_obj(int idx) const { return (idx >= 0 && idx < num_selectors_) ? selectors_[idx] : nullptr; }
    };

    /* =========================================================
    ** Ref -- RAII owner of one VM::ref() handle.
    **
    ** Hosts hold Values across frames (button callbacks, signal listeners,
    ** script component instances). The collector runs at any call or loop
    ** back edge, so a Value only a C++ member refers to is not safe; this
    ** keeps it rooted for exactly as long as the Ref lives. Move-only:
    ** moving transfers the single handle, the moved-from Ref is empty, and
    ** the handle is released once. Must not outlive the VM.
    **
    **   zen::Ref cb(vm, args[0]);          // in a native: on_click(fn)
    **   ...
    **   vm.call_fn(cb.get(), a, 1);        // later, any number of frames on
    ** ========================================================= */
    struct Ref
    {
        VM *vm = nullptr;
        int h = -1;

        Ref() = default;
        Ref(VM &v, Value value) : vm(&v), h(v.ref(value)) {}
        ~Ref() { reset(); }
        Ref(Ref &&o) noexcept : vm(o.vm), h(o.h) { o.vm = nullptr; o.h = -1; }
        Ref &operator=(Ref &&o) noexcept
        {
            if (this != &o)
            {
                reset();
                vm = o.vm;
                h = o.h;
                o.vm = nullptr;
                o.h = -1;
            }
            return *this;
        }
        Ref(const Ref &) = delete;
        Ref &operator=(const Ref &) = delete;

        Value get() const { return vm && h >= 0 ? vm->deref(h) : val_nil(); }
        void reset()
        {
            if (vm && h >= 0)
                vm->unref(h);
            vm = nullptr;
            h = -1;
        }
        explicit operator bool() const { return vm != nullptr && h >= 0; }
    };

    /* builtin_base.cpp: printf-style "fmt" % rhs (rhs: one value or an array of them) */
    Value zen_percent_format(VM *vm, Value fmt, Value rhs);
    /* builtin_base.cpp: ordering with __lt__ on instances, values_compare() otherwise */
    int zen_compare_vm(VM *vm, Value a, Value b);

} /* namespace zen */

#endif /* ZEN_VM_H */
