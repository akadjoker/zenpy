/* =========================================================
** zen_bind.hpp — typed bindings for natives and native classes.
**
** Generates the NativeFn thunk from a C++ signature: argument conversion,
** type errors with the function name and argument position, arity from the
** signature, result conversion. Header-only, C++17, no STL, no exceptions.
**
**   double host_rand(double lo, double hi);
**   zen::bind::def_fn<&host_rand>(vm, "rand");           // arity 2, floats
**
**   struct Texture { int id; void draw(double x, double y); int width() const; };
**   zen::bind::def_class<Texture>(vm, "Texture")
**       .ctor<&load_texture>()                             // Texture *load_texture(const char *path)
**       .dtor()                                            // delete (Texture *)
**       .method<&Texture::draw>("draw", ZEN_NATIVE_GC_SAFE)
**       .method<&Texture::width>("width")
**       .end();
**
** Parameter types understood: bool, any integer type, float/double,
** const char *, Value, and C * / C & for a native class registered with
** def_class<C>. A first parameter of type VM * receives the VM. Results:
** void, the same scalar types, const char * (copied), Value.
** Everything else stays available through the raw NativeFn API.
** ========================================================= */
#ifndef ZEN_BIND_HPP
#define ZEN_BIND_HPP

#include "vm.h"
#include "module.h"
#include "object.h"
#include <cstring>

namespace zen
{
namespace bind
{
    /* ---- minimal traits (no <type_traits>) ---- */
    namespace detail
    {
        template <typename T> struct remove_cvref { using type = T; };
        template <typename T> struct remove_cvref<const T> { using type = T; };
        template <typename T> struct remove_cvref<T &> { using type = typename remove_cvref<T>::type; };
        template <typename T> struct remove_cvref<T &&> { using type = typename remove_cvref<T>::type; };

        template <bool B, typename T = void> struct enable_if {};
        template <typename T> struct enable_if<true, T> { using type = T; };

        template <typename A, typename B> struct is_same { static constexpr bool value = false; };
        template <typename A> struct is_same<A, A> { static constexpr bool value = true; };

        template <typename T> struct is_integral { static constexpr bool value = false; };
        template <> struct is_integral<char> { static constexpr bool value = true; };
        template <> struct is_integral<signed char> { static constexpr bool value = true; };
        template <> struct is_integral<unsigned char> { static constexpr bool value = true; };
        template <> struct is_integral<short> { static constexpr bool value = true; };
        template <> struct is_integral<unsigned short> { static constexpr bool value = true; };
        template <> struct is_integral<int> { static constexpr bool value = true; };
        template <> struct is_integral<unsigned> { static constexpr bool value = true; };
        template <> struct is_integral<long> { static constexpr bool value = true; };
        template <> struct is_integral<unsigned long> { static constexpr bool value = true; };
        template <> struct is_integral<long long> { static constexpr bool value = true; };
        template <> struct is_integral<unsigned long long> { static constexpr bool value = true; };

        template <typename T> struct is_floating { static constexpr bool value = false; };
        template <> struct is_floating<float> { static constexpr bool value = true; };
        template <> struct is_floating<double> { static constexpr bool value = true; };

        template <typename T> struct is_pointer { static constexpr bool value = false; };
        template <typename T> struct is_pointer<T *> { static constexpr bool value = true; };

        template <unsigned... I> struct Seq {};
        template <unsigned N, unsigned... I> struct MakeSeq : MakeSeq<N - 1, N - 1, I...> {};
        template <unsigned... I> struct MakeSeq<0, I...> { using type = Seq<I...>; };
    }

    /* ---- argument conversion: Arg<T>::check(Value) / get(VM*, Value) / what() ---- */
    template <typename T, typename Enable = void> struct Arg;

    template <> struct Arg<bool>
    {
        static bool check(Value) { return true; } /* Python truthiness */
        static bool get(VM *, Value v) { return is_truthy_full(v); }
        static const char *what() { return "a bool"; }
    };
    template <typename T> struct Arg<T, typename detail::enable_if<detail::is_integral<T>::value>::type>
    {
        static bool check(Value v) { return is_int(v) || is_bool(v); }
        static T get(VM *, Value v) { return (T)(is_int(v) ? v.as.integer : (v.as.boolean ? 1 : 0)); }
        static const char *what() { return "an int"; }
    };
    template <typename T> struct Arg<T, typename detail::enable_if<detail::is_floating<T>::value>::type>
    {
        static bool check(Value v) { return is_numeric_like(v); }
        static T get(VM *, Value v) { return (T)to_number(v); }
        static const char *what() { return "a number"; }
    };
    template <> struct Arg<const char *>
    {
        static bool check(Value v) { return is_string(v); }
        static const char *get(VM *, Value v) { return as_string(v)->chars; }
        static const char *what() { return "a string"; }
    };
    template <> struct Arg<Value>
    {
        static bool check(Value) { return true; }
        static Value get(VM *, Value v) { return v; }
        static const char *what() { return "a value"; }
    };
    /* C * : a native-class instance registered with def_class<C> */
    template <typename C> struct Arg<C *, typename detail::enable_if<!detail::is_same<C, const char>::value && !detail::is_same<C, char>::value>::type>
    {
        static bool check(Value v) { return is_instance(v) && as_instance(v)->native_data != nullptr; }
        static C *get(VM *, Value v) { return zen_instance_data<C>(v); }
        static const char *what() { return "a native object"; }
    };
    template <typename C> struct Arg<C &, typename detail::enable_if<!detail::is_same<typename detail::remove_cvref<C>::type, Value>::value>::type>
    {
        static bool check(Value v) { return Arg<C *>::check(v); }
        static C &get(VM *vm, Value v) { return *Arg<C *>::get(vm, v); }
        static const char *what() { return Arg<C *>::what(); }
    };
    template <> struct Arg<const Value &> : Arg<Value> {};

    /* ---- result conversion: Ret<R>::put(VM*, Value *slot, R) ---- */
    template <typename R, typename Enable = void> struct Ret;
    template <> struct Ret<bool> { static void put(VM *, Value *s, bool r) { *s = val_bool(r); } };
    template <typename T> struct Ret<T, typename detail::enable_if<detail::is_integral<T>::value>::type>
    {
        static void put(VM *, Value *s, T r) { *s = val_int((int64_t)r); }
    };
    template <typename T> struct Ret<T, typename detail::enable_if<detail::is_floating<T>::value>::type>
    {
        static void put(VM *, Value *s, T r) { *s = val_float((double)r); }
    };
    template <> struct Ret<const char *>
    {
        static void put(VM *vm, Value *s, const char *r) { *s = val_obj((Obj *)vm->make_string(r ? r : "", r ? (int)strlen(r) : 0)); }
    };
    template <> struct Ret<Value> { static void put(VM *, Value *s, Value r) { *s = r; } };

    /* ---- signature traits ---- */
    template <typename F> struct Sig;
    template <typename R, typename... A> struct Sig<R (*)(A...)>
    {
        using Ret = R;
        using Class = void;
        static constexpr bool wants_vm = false;
        static constexpr int arity = (int)sizeof...(A);
        template <typename Fn, typename Self, unsigned... I>
        static R call(Fn fn, VM *vm, Self *, Value *args, detail::Seq<I...>) { (void)vm; (void)args; return fn(Arg<A>::get(vm, args[I])...); }
        static bool check(VM *vm, Value *args, const char *name)
        {
            const char *bad = nullptr;
            int idx = 0;
            (void)vm; (void)args; (void)idx;
            ((bad == nullptr && !Arg<A>::check(args[idx]) ? (bad = Arg<A>::what(), (void)0) : (void)0, idx++), ...);
            if (bad)
            {
                vm->runtime_error("%s: argument %d must be %s", name, idx, bad);
                return false;
            }
            return true;
        }
    };
    template <typename R, typename... A> struct Sig<R (*)(VM *, A...)> : Sig<R (*)(A...)>
    {
        static constexpr bool wants_vm = true;
        template <typename Fn, typename Self, unsigned... I>
        static R call(Fn fn, VM *vm, Self *, Value *args, detail::Seq<I...>) { (void)args; return fn(vm, Arg<A>::get(vm, args[I])...); }
    };
    template <typename C, typename R, typename... A> struct Sig<R (C::*)(A...)> : Sig<R (*)(A...)>
    {
        using Class = C;
        template <typename Fn, unsigned... I>
        static R call(Fn fn, VM *vm, C *self, Value *args, detail::Seq<I...>) { (void)vm; (void)args; return (self->*fn)(Arg<A>::get(vm, args[I])...); }
    };
    template <typename C, typename R, typename... A> struct Sig<R (C::*)(A...) const> : Sig<R (C::*)(A...)> {};
    template <typename C, typename R, typename... A> struct Sig<R (C::*)(VM *, A...)> : Sig<R (*)(A...)>
    {
        using Class = C;
        static constexpr bool wants_vm = true;
        template <typename Fn, unsigned... I>
        static R call(Fn fn, VM *vm, C *self, Value *args, detail::Seq<I...>) { (void)args; return (self->*fn)(vm, Arg<A>::get(vm, args[I])...); }
    };
    template <typename C, typename R, typename... A> struct Sig<R (C::*)(VM *, A...) const> : Sig<R (C::*)(VM *, A...)> {};

    /* A free function used as a method: the first parameter (T * or T &, after
    ** an optional VM *) receives the script receiver, the rest are the script
    ** arguments. SelfSig<F, T> reports whether decltype(fn) has that shape. */
    template <typename F, typename T> struct SelfSig { static constexpr bool value = false; };
    template <typename R, typename T, typename... A> struct SelfSig<R (*)(T *, A...), T> { static constexpr bool value = true; };
    template <typename R, typename T, typename... A> struct SelfSig<R (*)(T &, A...), T> { static constexpr bool value = true; };
    template <typename R, typename T, typename... A> struct SelfSig<R (*)(const T &, A...), T> { static constexpr bool value = true; };
    template <typename R, typename T, typename... A> struct SelfSig<R (*)(VM *, T *, A...), T> { static constexpr bool value = true; };
    template <typename R, typename T, typename... A> struct SelfSig<R (*)(VM *, T &, A...), T> { static constexpr bool value = true; };
    template <typename R, typename T, typename... A> struct SelfSig<R (*)(VM *, const T &, A...), T> { static constexpr bool value = true; };

    /* ---- the thunk ---- */
    template <auto fn> struct Thunk
    {
        using S = Sig<decltype(fn)>;
        using R = typename S::Ret;
        using C = typename S::Class;
        static inline const char *name = "native";

        template <typename Rr> static int finish(VM *vm, Value *args, Rr r) { Ret<Rr>::put(vm, args, r); return 1; }

        template <typename Self> static int run(VM *vm, Self *self, Value *args, int nargs)
        {
            if (nargs < S::arity)
            {
                vm->runtime_error("%s: expected %d argument(s), got %d", name, S::arity, nargs);
                return -1;
            }
            if (!S::check(vm, args, name))
                return -1;
            if constexpr (detail::is_same<R, void>::value)
            {
                S::call(fn, vm, self, args, typename detail::MakeSeq<(unsigned)S::arity>::type{});
                return 0;
            }
            else
            {
                return finish<R>(vm, args, S::call(fn, vm, self, args, typename detail::MakeSeq<(unsigned)S::arity>::type{}));
            }
        }
        static int native(VM *vm, Value *args, int nargs)
        {
            if constexpr (detail::is_same<C, void>::value)
            {
                return run<void>(vm, (void *)nullptr, args, nargs);
            }
            else
            {
                C *self = is_instance(args[-1]) ? zen_instance_data<C>(args[-1]) : nullptr;
                if (!self)
                {
                    vm->runtime_error("%s: receiver is not a native object", name);
                    return -1;
                }
                return run<C>(vm, self, args, nargs);
            }
        }
    };

    /* Thunk for a free function bound as a method: receiver -> first parameter. */
    template <auto fn, typename T> struct SelfThunk
    {
        using S = Sig<decltype(fn)>;
        using R = typename S::Ret;
        static constexpr int arity = S::arity - 1; /* the receiver is not a script argument */
        static inline const char *name = "method";

        template <typename Self, typename... A> struct Call;
        static int native(VM *vm, Value *args, int nargs)
        {
            if (!is_instance(args[-1]) || !as_instance(args[-1])->native_data)
            {
                vm->runtime_error("%s: receiver is not a native object", name);
                return -1;
            }
            if (nargs < arity)
            {
                vm->runtime_error("%s: expected %d argument(s), got %d", name, arity, nargs);
                return -1;
            }
            /* args[-1] is the receiver: pass args - 1 so index 0 is the receiver
            ** and the declared parameter list lines up with the C++ signature. */
            if (!S::check(vm, args - 1, name))
                return -1;
            if constexpr (detail::is_same<R, void>::value)
            {
                S::call(fn, vm, (void *)nullptr, args - 1, typename detail::MakeSeq<(unsigned)S::arity>::type{});
                return 0;
            }
            else
            {
                Ret<R>::put(vm, args, S::call(fn, vm, (void *)nullptr, args - 1, typename detail::MakeSeq<(unsigned)S::arity>::type{}));
                return 1;
            }
        }
    };

    /* def_fn<&f>(vm, "name"[, flags]) — a global native from a free function. */
    template <auto fn> int def_fn(VM &vm, const char *name, int flags = 0)
    {
        Thunk<fn>::name = name;
        return vm.def_native(name, &Thunk<fn>::native, Sig<decltype(fn)>::arity, flags);
    }

    /* ---- constructors ---- */
    template <typename T, typename... A> struct CtorThunk
    {
        static inline const char *name = "ctor";
        template <unsigned... I> static T *make(VM *vm, Value *args, detail::Seq<I...>) { (void)vm; (void)args; return new T(Arg<A>::get(vm, args[I])...); }
        static void *create(VM *vm, int argc, Value *args)
        {
            if (argc < (int)sizeof...(A))
            {
                vm->runtime_error("%s: expected %d argument(s), got %d", name, (int)sizeof...(A), argc);
                return nullptr;
            }
            if (!Sig<void (*)(A...)>::check(vm, args, name))
                return nullptr;
            return make(vm, args, typename detail::MakeSeq<(unsigned)sizeof...(A)>::type{});
        }
    };
    template <auto factory> struct FactoryThunk
    {
        using S = Sig<decltype(factory)>;
        static inline const char *name = "ctor";
        static void *create(VM *vm, int argc, Value *args)
        {
            if (argc < S::arity)
            {
                vm->runtime_error("%s: expected %d argument(s), got %d", name, S::arity, argc);
                return nullptr;
            }
            if (!S::check(vm, args, name))
                return nullptr;
            return (void *)S::call(factory, vm, (void *)nullptr, args, typename detail::MakeSeq<(unsigned)S::arity>::type{});
        }
    };

    /* ---- classes ---- */
    template <typename T> class ClassDef
    {
    public:
        ClassDef(VM &vm, const char *name) : vm_(vm), builder_(vm.def_class(name)), name_(name) {}

        /* Cls(args...) -> new T(converted args...) */
        template <typename... A> ClassDef &ctor()
        {
            CtorThunk<T, A...>::name = name_;
            builder_.ctor(&CtorThunk<T, A...>::create);
            return *this;
        }
        /* Cls(args...) -> T *factory(converted args...) (nullptr = error already reported) */
        template <auto factory> ClassDef &ctor()
        {
            FactoryThunk<factory>::name = name_;
            builder_.ctor(&FactoryThunk<factory>::create);
            return *this;
        }
        /* delete (T *) when the instance is collected */
        ClassDef &dtor()
        {
            builder_.dtor([](VM *, void *data) { delete (T *)data; });
            return *this;
        }
        /* a member function (const or not, optionally VM* first) or a free
        ** function whose first parameter is T* (handled as a method thunk) */
        template <auto m> ClassDef &method(const char *name, int flags = 0)
        {
            if constexpr (SelfSig<decltype(m), T>::value)
            {
                SelfThunk<m, T>::name = name;
                builder_.method(name, &SelfThunk<m, T>::native, SelfThunk<m, T>::arity, flags);
            }
            else
            {
                Thunk<m>::name = name;
                builder_.method(name, &Thunk<m>::native, Sig<decltype(m)>::arity, flags);
            }
            return *this;
        }
        ClassDef &field(const char *name) { builder_.field(name); return *this; }
        ClassDef &persistent(bool p = true) { builder_.persistent(p); return *this; }
        ClassDef &constructable(bool c = true) { builder_.constructable(c); return *this; }
        ObjClass *end() { return builder_.end(); }

    private:
        VM &vm_;
        VM::ClassBuilder builder_;
        const char *name_;
    };

    template <typename T> ClassDef<T> def_class(VM &vm, const char *name) { return ClassDef<T>(vm, name); }

} /* namespace bind */
} /* namespace zen */

#endif /* ZEN_BIND_HPP */
