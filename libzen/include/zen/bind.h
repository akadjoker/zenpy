#ifndef ZEN_BIND_H
#define ZEN_BIND_H

/*
** bind.h — header-only C++ -> ZenPy binding layer.
**
** Hosts used to write one `static int f(VM*, Value*, int)` per method. This
** header generates that wrapper at compile time from a member-function
** pointer (Godot MethodBind / sol2 style), on top of the existing
** VM::ClassBuilder. It adds no runtime state beyond one ObjClass* per bound
** type, no heap, no exceptions, no virtuals: a generated thunk is "check
** nargs, convert args, call, convert result".
**
**     struct Body { Body(float x, float y); void move(float, float);
**                   float speed() const; int hits; };
**
**     zen::bind_class<Body>(vm, "Body")
**         .constructor<float, float>()
**         .method<&Body::move>("move")
**         .method<&Body::speed>("speed")
**         .property<&Body::hits>("hits")          // hits() and set_hits(v)
**         .end();
**
** Ownership rule: a class given constructor<...>() OWNS its instances
** (`new T(...)` on construction, `delete` when the GC frees them). A class
** without one is a view over host-owned memory: create instances with
** wrap<T>(vm, ptr) and the GC never touches the pointee.
**
** Only <type_traits>, <utility>, <cstdint>, <cstddef> are used.
*/

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "vm.h"
#include "memory.h"

namespace zen
{

    /* ObjClass* of each bound C++ type, filled by ClassBinder<T>::end().
    ** Process-global (one slot per C++ type, not per VM): with several VMs
    ** the most recently bound one wins. ref_klass is the hidden borrowed
    ** twin used by wrap() for classes that own their instances (see end()). */
    template <class T>
    struct TypeTag
    {
        static ObjClass *klass;
        static ObjClass *ref_klass;
    };
    template <class T>
    ObjClass *TypeTag<T>::klass = nullptr;
    template <class T>
    ObjClass *TypeTag<T>::ref_klass = nullptr;

    namespace bind_detail
    {
        template <class...>
        inline constexpr bool always_false = false;

        template <class... Ps>
        struct TypeList
        {
        };

        /* ---- small helpers (no <cstring>/<string>) ---- */

        inline int cstr_len(const char *s)
        {
            int n = 0;
            while (s[n])
                n++;
            return n;
        }

        inline const char *type_name_of(const Value &v)
        {
            if (is_nil(v))
                return "None";
            if (is_bool(v))
                return "bool";
            if (is_int(v))
                return "int";
            if (is_float(v))
                return "float";
            if (is_string(v))
                return "string";
            if (is_instance(v))
            {
                ObjClass *k = as_instance(v)->klass;
                return (k && k->name) ? k->name->chars : "instance";
            }
            return "object";
        }

        /* Instance of `k` or of a script subclass of it. */
        inline bool instance_is(const Value &v, const ObjClass *k)
        {
            if (!k || !is_instance(v))
                return false;
            for (const ObjClass *c = as_instance(v)->klass; c; c = c->parent)
                if (c == k)
                    return true;
            return false;
        }

        /* Does the script integer fit the target C++ integral type? Scripts
        ** hold int64; silently truncating into an int8 would be a garbage cast. */
        template <class T>
        constexpr bool int_fits(int64_t v)
        {
            if constexpr (sizeof(T) >= 8)
            {
                if constexpr (std::is_signed_v<T>)
                    return true;
                else
                    return v >= 0;
            }
            else if constexpr (std::is_signed_v<T>)
            {
                constexpr int64_t hi = (int64_t(1) << (sizeof(T) * 8 - 1)) - 1;
                return v >= -hi - 1 && v <= hi;
            }
            else
            {
                constexpr int64_t hi = (int64_t(1) << (sizeof(T) * 8)) - 1;
                return v >= 0 && v <= hi;
            }
        }

        template <class T>
        inline constexpr bool is_plain_scalar_v =
            std::is_arithmetic_v<T> || std::is_enum_v<T> || std::is_same_v<T, Value> ||
            std::is_same_v<T, const char *>;
    } /* namespace bind_detail */

    /* =========================================================
    ** Argument conversion.
    **
    ** get() converts or sets ok=false (it never reports and never casts a
    ** wrong-typed value). `Stored` is what lives between conversion and the
    ** call (a pointer for T&, a small buffer for strings); unwrap() turns it
    ** into the parameter. The thunk reports the error, because only it
    ** knows the parameter index.
    ** ========================================================= */
    template <class T, class = void>
    struct ArgConv
    {
        static_assert(bind_detail::always_false<T>,
                      "zen bind: unsupported parameter type (bind it as T*, T& or a scalar)");
    };

    template <>
    struct ArgConv<bool, void>
    {
        using Stored = bool;
        static const char *expected() { return "bool"; }
        static bool get(VM *, const Value &v, bool &ok)
        {
            if (is_bool(v))
                return v.as.boolean;
            if (is_int(v))
                return v.as.integer != 0;
            ok = false;
            return false;
        }
        static bool unwrap(Stored &s) { return s; }
    };

    template <class T>
    struct ArgConv<T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>>
    {
        using Stored = T;
        static const char *expected() { return "int"; }
        static T get(VM *, const Value &v, bool &ok)
        {
            if (!is_int(v) || !bind_detail::int_fits<T>(v.as.integer))
            {
                ok = false;
                return T();
            }
            return static_cast<T>(v.as.integer);
        }
        static T unwrap(Stored &s) { return s; }
    };

    template <class T>
    struct ArgConv<T, std::enable_if_t<std::is_floating_point_v<T>>>
    {
        using Stored = T;
        static const char *expected() { return "float"; }
        static T get(VM *, const Value &v, bool &ok)
        {
            /* ints promote, as they do everywhere else in the language */
            if (is_float(v))
                return static_cast<T>(v.as.number);
            if (is_int(v))
                return static_cast<T>(v.as.integer);
            ok = false;
            return T();
        }
        static T unwrap(Stored &s) { return s; }
    };

    template <class T>
    struct ArgConv<T, std::enable_if_t<std::is_enum_v<T>>>
    {
        using Stored = T;
        static const char *expected() { return "int"; }
        static T get(VM *, const Value &v, bool &ok)
        {
            if (!is_int(v))
            {
                ok = false;
                return T();
            }
            return static_cast<T>(v.as.integer);
        }
        static T unwrap(Stored &s) { return s; }
    };

    template <>
    struct ArgConv<Value, void>
    {
        using Stored = Value;
        static const char *expected() { return "any"; }
        static Value get(VM *, const Value &v, bool &) { return v; }
        static Value unwrap(Stored &s) { return s; }
    };

    /* const char*: a script string is a heap ObjString (NUL-terminated). The
    ** pointer is only valid during the call; copy it if the callee keeps it. */
    template <>
    struct ArgConv<const char *, void>
    {
        using Stored = const char *;
        static const char *expected() { return "string"; }
        static const char *get(VM *, const Value &v, bool &ok)
        {
            if (is_string(v))
                return safe_string_chars(v);
            ok = false;
            return "";
        }
        static const char *unwrap(Stored &s) { return s; }
    };

    /* Pointer to a bound class: native_data of an instance of that class (or a
    ** script subclass). Anything else, or an instance without native data, is
    ** a type error — never a cast. */
    template <class T>
    struct ArgConv<T *, void>
    {
        static_assert(!std::is_same_v<std::remove_cv_t<T>, VM>,
                      "zen bind: VM* is only allowed as the FIRST parameter");
        using Stored = T *;
        static const char *expected()
        {
            ObjClass *k = TypeTag<std::remove_cv_t<T>>::klass;
            return (k && k->name) ? k->name->chars : "<unbound class>";
        }
        static T *get(VM *, const Value &v, bool &ok)
        {
            if (!bind_detail::instance_is(v, TypeTag<std::remove_cv_t<T>>::klass))
            {
                ok = false;
                return nullptr;
            }
            void *p = as_instance(v)->native_data;
            if (!p)
            {
                ok = false;
                return nullptr;
            }
            return static_cast<T *>(p);
        }
        static T *unwrap(Stored &s) { return s; }
    };

    /* T& / const T&: scalars are converted by value (a `const float&`
    ** parameter is just a float); bound classes go through the pointer path. */
    namespace bind_detail
    {
        /* Lazy pick: ArgConv<U> must not be instantiated for a bound class. */
        template <bool ByValue, class T, class U>
        struct RefStored
        {
            using type = T *;
        };
        template <class T, class U>
        struct RefStored<true, T, U>
        {
            using type = typename ArgConv<U>::Stored;
        };
    } /* namespace bind_detail */

    template <class T>
    struct ArgConv<T &, void>
    {
        using U = std::remove_cv_t<T>;
        static constexpr bool by_value = bind_detail::is_plain_scalar_v<U>;
        using Stored = typename bind_detail::RefStored<by_value, T, U>::type;
        static const char *expected()
        {
            if constexpr (by_value)
                return ArgConv<U>::expected();
            else
                return ArgConv<T *>::expected();
        }
        static Stored get(VM *vm, const Value &v, bool &ok)
        {
            if constexpr (by_value)
                return ArgConv<U>::get(vm, v, ok);
            else
                return ArgConv<T *>::get(vm, v, ok);
        }
        static T &unwrap(Stored &s)
        {
            if constexpr (by_value)
                return s;
            else
                return *s;
        }
    };

    /* =========================================================
    ** Wrapping host-owned memory.
    ** ========================================================= */

    /* Wrap a host-owned pointer as a script instance of its bound class
    ** (None for nullptr). The instance is built with new_instance(), NOT
    ** make_instance(): the latter would run the native constructor with zero
    ** arguments. For a class that owns its instances the wrapper is made from
    ** the hidden borrowed twin class, whose no-op destructor stops the GC from
    ** `delete`-ing memory the host owns. */
    template <class T>
    Value wrap(VM &vm, T *ptr)
    {
        using U = std::remove_cv_t<T>;
        if (!ptr)
            return val_nil();
        ObjClass *k = TypeTag<U>::ref_klass ? TypeTag<U>::ref_klass : TypeTag<U>::klass;
        if (!k)
        {
            vm.runtime_error("wrap: C++ type is not bound to a script class");
            return val_nil();
        }
        ObjInstance *inst = new_instance(&vm.get_gc(), k);
        inst->native_data = const_cast<U *>(ptr);
        return val_obj((Obj *)inst);
    }

    /* =========================================================
    ** Result conversion.
    ** ========================================================= */
    template <class T, class = void>
    struct RetConv
    {
        static_assert(bind_detail::always_false<T>,
                      "zen bind: unsupported return type (return a scalar, const char*, Value or a bound T*)");
    };

    template <>
    struct RetConv<void, void>
    {
        /* callers special-case void: nothing is written and 0 is returned */
    };

    template <>
    struct RetConv<bool, void>
    {
        static void put(VM *, Value *out, bool v) { *out = val_bool(v); }
    };

    template <class T>
    struct RetConv<T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>>
    {
        static void put(VM *, Value *out, T v) { *out = val_int(static_cast<int64_t>(v)); }
    };

    template <class T>
    struct RetConv<T, std::enable_if_t<std::is_enum_v<T>>>
    {
        static void put(VM *, Value *out, T v) { *out = val_int(static_cast<int64_t>(v)); }
    };

    template <class T>
    struct RetConv<T, std::enable_if_t<std::is_floating_point_v<T>>>
    {
        static void put(VM *, Value *out, T v) { *out = val_float(static_cast<double>(v)); }
    };

    template <>
    struct RetConv<Value, void>
    {
        static void put(VM *, Value *out, Value v) { *out = v; }
    };

    template <>
    struct RetConv<const char *, void>
    {
        static void put(VM *vm, Value *out, const char *s)
        {
            *out = s ? val_obj((Obj *)vm->make_string(s, bind_detail::cstr_len(s))) : val_nil();
        }
    };

    template <class T>
    struct RetConv<T *, void>
    {
        static void put(VM *vm, Value *out, T *p) { *out = wrap<T>(*vm, p); }
    };

    template <class T>
    struct RetConv<T &, void>
    {
        using U = std::remove_cv_t<T>;
        static void put(VM *vm, Value *out, T &r)
        {
            if constexpr (bind_detail::is_plain_scalar_v<U>)
                RetConv<U>::put(vm, out, r);
            else
                *out = wrap<T>(*vm, &r);
        }
    };

    namespace bind_detail
    {
        /* ---- signature introspection ---- */

        template <class C, class R, class... Ps>
        struct MfInfo
        {
            using Class = C;
            using Ret = R;
            using Params = TypeList<Ps...>;
        };
        template <class MF>
        struct MfTraits;
        template <class C, class R, class... Ps>
        struct MfTraits<R (C::*)(Ps...)> : MfInfo<C, R, Ps...>
        {
        };
        template <class C, class R, class... Ps>
        struct MfTraits<R (C::*)(Ps...) const> : MfInfo<C, R, Ps...>
        {
        };
        template <class C, class R, class... Ps>
        struct MfTraits<R (C::*)(Ps...) noexcept> : MfInfo<C, R, Ps...>
        {
        };
        template <class C, class R, class... Ps>
        struct MfTraits<R (C::*)(Ps...) const noexcept> : MfInfo<C, R, Ps...>
        {
        };

        template <class R, class... Ps>
        struct FnInfo
        {
            using Ret = R;
            using Params = TypeList<Ps...>;
        };
        template <class F>
        struct FnTraits;
        template <class R, class... Ps>
        struct FnTraits<R (*)(Ps...)> : FnInfo<R, Ps...>
        {
        };
        template <class R, class... Ps>
        struct FnTraits<R (*)(Ps...) noexcept> : FnInfo<R, Ps...>
        {
        };

        /* A leading VM* is injected by the thunk and is not a script argument. */
        template <class L>
        struct SplitVM;
        template <class... Ps>
        struct SplitVM<TypeList<Ps...>>
        {
            static constexpr bool has_vm = false;
            using Script = TypeList<Ps...>;
            static constexpr int arity = (int)sizeof...(Ps);
        };
        template <class... Ps>
        struct SplitVM<TypeList<VM *, Ps...>>
        {
            static constexpr bool has_vm = true;
            using Script = TypeList<Ps...>;
            static constexpr int arity = (int)sizeof...(Ps);
        };

        template <class P>
        struct DataMember;
        template <class C, class T>
        struct DataMember<T C::*>
        {
            using Class = C;
            using Type = T;
        };

        /* ---- argument pack ----
        ** Every argument is converted before the call so a bad one is reported
        ** instead of being passed on. Slots are bases of an aggregate because
        ** braced-init is the one place C++ guarantees left-to-right evaluation. */
        template <size_t I, class S>
        struct Slot
        {
            S v;
        };
        template <class Seq, class... Ps>
        struct Pack;
        template <size_t... Is, class... Ps>
        struct Pack<std::index_sequence<Is...>, Ps...> : Slot<Is, typename ArgConv<Ps>::Stored>...
        {
        };

        struct ArgStatus
        {
            int bad = -1; /* first offending parameter, -1 = all good */
            const char *want = nullptr;
        };

        template <class P>
        inline typename ArgConv<P>::Stored convert(VM *vm, const Value &v, int idx, ArgStatus &st)
        {
            bool ok = true;
            typename ArgConv<P>::Stored s = ArgConv<P>::get(vm, v, ok);
            if (!ok && st.bad < 0)
            {
                st.bad = idx;
                st.want = ArgConv<P>::expected();
            }
            return s;
        }

        /* Convert args[0..N-1], call, write the result to args[0]. `call` is a
        ** callable taking the unwrapped parameters. The one shared engine for
        ** methods, free functions and constructors. */
        template <class R, class Call, class... Ps, size_t... Is>
        inline int run(VM *vm, Value *args, Call &&call, TypeList<Ps...>, std::index_sequence<Is...>)
        {
            ArgStatus st;
            Pack<std::index_sequence<Is...>, Ps...> pk{
                Slot<Is, typename ArgConv<Ps>::Stored>{convert<Ps>(vm, args[Is], (int)Is, st)}...};
            if (st.bad >= 0)
            {
                vm->runtime_error("parameter %d: expected %s, got %s", st.bad, st.want,
                                  type_name_of(args[st.bad]));
                return -1;
            }
            (void)pk;
            if constexpr (std::is_void_v<R>)
            {
                call(ArgConv<Ps>::unwrap(static_cast<Slot<Is, typename ArgConv<Ps>::Stored> &>(pk).v)...);
                return 0;
            }
            else
            {
                RetConv<R>::put(vm, &args[0],
                                call(ArgConv<Ps>::unwrap(static_cast<Slot<Is, typename ArgConv<Ps>::Stored> &>(pk).v)...));
                return 1;
            }
        }

        template <class R, class... Ps, class Call>
        inline int run(VM *vm, Value *args, Call &&call, TypeList<Ps...> tl)
        {
            return run<R>(vm, args, static_cast<Call &&>(call), tl, std::make_index_sequence<sizeof...(Ps)>{});
        }

        inline int arity_error(VM *vm, int want, int got)
        {
            vm->runtime_error("expected %d argument(s), got %d", want, got);
            return -1;
        }
    } /* namespace bind_detail */

    /* =========================================================
    ** Generated wrappers.
    ** ========================================================= */

    /* Member function. self = native_data of args[-1] (the ClassBuilder method
    ** convention); script arguments start at args[0]. `Self` is the bound
    ** class: it defaults to the class the member belongs to, ClassBinder<T>
    ** passes T so a base-class member can be bound on a derived class. */
    template <auto MF, class Self = typename bind_detail::MfTraits<decltype(MF)>::Class>
    int method_thunk(VM *vm, Value *args, int nargs)
    {
        using Tr = bind_detail::MfTraits<decltype(MF)>;
        using Sp = bind_detail::SplitVM<typename Tr::Params>;
        if (nargs != Sp::arity)
            return bind_detail::arity_error(vm, Sp::arity, nargs);
        if (!is_instance(args[-1]) || !as_instance(args[-1])->native_data)
        {
            vm->runtime_error("method called on an object without native data");
            return -1;
        }
        Self *self = zen_instance_data<Self>(args[-1]);
        return bind_detail::run<typename Tr::Ret>(
            vm, args,
            [=](auto &&...a) -> typename Tr::Ret
            {
                if constexpr (Sp::has_vm)
                    return (self->*MF)(vm, static_cast<decltype(a) &&>(a)...);
                else
                    return (self->*MF)(static_cast<decltype(a) &&>(a)...);
            },
            typename Sp::Script{});
    }

    /* Free function: arguments at args[0..], result in args[0]. */
    template <auto F>
    int function_thunk(VM *vm, Value *args, int nargs)
    {
        using Tr = bind_detail::FnTraits<decltype(F)>;
        using Sp = bind_detail::SplitVM<typename Tr::Params>;
        if (nargs != Sp::arity)
            return bind_detail::arity_error(vm, Sp::arity, nargs);
        return bind_detail::run<typename Tr::Ret>(
            vm, args,
            [=](auto &&...a) -> typename Tr::Ret
            {
                if constexpr (Sp::has_vm)
                    return F(vm, static_cast<decltype(a) &&>(a)...);
                else
                    return F(static_cast<decltype(a) &&>(a)...);
            },
            typename Sp::Script{});
    }

    namespace bind_detail
    {
        /* Data member -> `name()` */
        template <auto PM, class Self>
        int getter_thunk(VM *vm, Value *args, int nargs)
        {
            using Ty = typename DataMember<decltype(PM)>::Type;
            if (nargs != 0)
                return arity_error(vm, 0, nargs);
            if (!is_instance(args[-1]) || !as_instance(args[-1])->native_data)
            {
                vm->runtime_error("getter called on an object without native data");
                return -1;
            }
            Self *self = zen_instance_data<Self>(args[-1]);
            RetConv<Ty>::put(vm, &args[0], self->*PM);
            return 1;
        }

        /* Data member -> `set_name(v)` */
        template <auto PM, class Self>
        int setter_thunk(VM *vm, Value *args, int nargs)
        {
            using Ty = typename DataMember<decltype(PM)>::Type;
            if (nargs != 1)
                return arity_error(vm, 1, nargs);
            if (!is_instance(args[-1]) || !as_instance(args[-1])->native_data)
            {
                vm->runtime_error("setter called on an object without native data");
                return -1;
            }
            Self *self = zen_instance_data<Self>(args[-1]);
            return run<void>(
                vm, args, [self](Ty v) { self->*PM = v; }, TypeList<Ty>{});
        }

        /* NativeClassCtor: `new T(args...)` from the script arguments. A
        ** subclass in script that adds its own __init__ still gets this called
        ** first, with the same arguments (see VM OP_CALL). */
        template <class T, class... Args>
        void *ctor_thunk(VM *vm, int argc, Value *args)
        {
            using Sp = SplitVM<TypeList<Args...>>;
            if (argc != Sp::arity)
            {
                arity_error(vm, Sp::arity, argc);
                return nullptr;
            }
            T *obj = nullptr;
            int r = run<void>(
                vm, args,
                [&](auto &&...a)
                {
                    if constexpr (Sp::has_vm)
                        obj = new T(vm, static_cast<decltype(a) &&>(a)...);
                    else
                        obj = new T(static_cast<decltype(a) &&>(a)...);
                },
                typename Sp::Script{});
            return r < 0 ? nullptr : obj;
        }

        template <class T>
        void dtor_thunk(VM *, void *p)
        {
            delete static_cast<T *>(p);
        }

        /* The borrowed twin must have *a* destructor, because the GC walks up
        ** the parent chain only until it finds one — this stops it at the twin. */
        inline void noop_dtor(VM *, void *) {}

        /* "set_" + name, without <cstring>. */
        inline void prefixed(char *out, size_t cap, const char *prefix, const char *name)
        {
            size_t n = 0;
            for (; *prefix && n + 1 < cap; prefix++)
                out[n++] = *prefix;
            for (; *name && n + 1 < cap; name++)
                out[n++] = *name;
            out[n] = 0;
        }
    } /* namespace bind_detail */

    /* =========================================================
    ** ClassBinder
    ** ========================================================= */
    template <class T>
    class ClassBinder
    {
    public:
        ClassBinder(VM &vm, const char *name) : vm_(vm), b_(vm.def_class(name))
        {
            bind_detail::prefixed(name_, sizeof(name_), "", name);
        }

        ClassBinder &parent(const char *name)
        {
            b_.parent(name);
            return *this;
        }

        ClassBinder &field(const char *name)
        {
            b_.field(name);
            return *this;
        }

        /* Script `T(a, b)` runs `new T(a, b)`; the GC `delete`s it. */
        template <class... Args>
        ClassBinder &constructor()
        {
            static_assert(std::is_constructible_v<T, std::remove_reference_t<Args>...> ||
                              bind_detail::SplitVM<bind_detail::TypeList<Args...>>::has_vm,
                          "zen bind: T is not constructible from these argument types");
            b_.ctor(&bind_detail::ctor_thunk<T, Args...>);
            b_.dtor(&bind_detail::dtor_thunk<T>);
            owning_ = true;
            return *this;
        }

        /* Arity comes from the member's parameter list (a leading VM* is not
        ** counted). const and non-const members both work. */
        template <auto MF>
        ClassBinder &method(const char *name, int flags = 0)
        {
            static_assert(std::is_member_function_pointer_v<decltype(MF)>,
                          "zen bind: method<> needs a member function pointer");
            using Sp = bind_detail::SplitVM<typename bind_detail::MfTraits<decltype(MF)>::Params>;
            b_.method(name, &method_thunk<MF, T>, Sp::arity, flags);
            return *this;
        }

        /* Data member -> `name()` */
        template <auto PM>
        ClassBinder &getter(const char *name)
        {
            static_assert(std::is_member_object_pointer_v<decltype(PM)>,
                          "zen bind: getter<> needs a data member pointer");
            b_.method(name, &bind_detail::getter_thunk<PM, T>, 0);
            return *this;
        }

        /* Data member -> `set_<name>(v)` */
        template <auto PM>
        ClassBinder &setter(const char *name)
        {
            static_assert(std::is_member_object_pointer_v<decltype(PM)>,
                          "zen bind: setter<> needs a data member pointer");
            char buf[96];
            bind_detail::prefixed(buf, sizeof(buf), "set_", name);
            b_.method(buf, &bind_detail::setter_thunk<PM, T>, 1);
            return *this;
        }

        template <auto PM>
        ClassBinder &property(const char *name)
        {
            return getter<PM>(name).template setter<PM>(name);
        }

        ClassBinder &persistent(bool p = true)
        {
            b_.persistent(p);
            return *this;
        }

        ClassBinder &constructable(bool c = true)
        {
            b_.constructable(c);
            return *this;
        }

        ObjClass *end()
        {
            ObjClass *k = b_.end();
            TypeTag<T>::klass = k;
            TypeTag<T>::ref_klass = nullptr;
            if (owning_)
            {
                /* wrap() must be able to hand out instances that the GC will
                ** NOT delete. Destructors are found by walking the parent
                ** chain, so a hidden subclass with a no-op destructor shields
                ** host memory. The name is not a valid identifier: scripts
                ** cannot spell it, they only see it through type(). */
                char refname[112];
                bind_detail::prefixed(refname, sizeof(refname), name_, "$ref");
                TypeTag<T>::ref_klass = vm_.def_class(refname)
                                            .parent(name_)
                                            .dtor(&bind_detail::noop_dtor)
                                            .constructable(false)
                                            .end();
            }
            return k;
        }

    private:
        VM &vm_;
        VM::ClassBuilder b_;
        bool owning_ = false;
        char name_[96];
    };

    template <class T>
    ClassBinder<T> bind_class(VM &vm, const char *name)
    {
        return ClassBinder<T>(vm, name);
    }

    /* Free function as a global; arity deduced (leading VM* not counted). */
    template <auto F>
    void bind_function(VM &vm, const char *name, int flags = 0)
    {
        using Sp = bind_detail::SplitVM<typename bind_detail::FnTraits<decltype(F)>::Params>;
        vm.def_native(name, &function_thunk<F>, Sp::arity, flags);
    }

} /* namespace zen */

#endif /* ZEN_BIND_H */
