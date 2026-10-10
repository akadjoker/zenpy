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
**   struct Texture { int id; int hits; void draw(double x, double y); int width() const; };
**   zen::bind::def_class<Texture>(vm, "Texture")
**       .ctor<&load_texture>()                             // Texture *load_texture(const char *path)
**       .dtor()                                            // delete (Texture *)
**       .method<&Texture::draw>("draw", ZEN_NATIVE_GC_SAFE)
**       .method<&Texture::width>("width")
**       .property<&Texture::hits>("hits")                  // tex.hits, tex.hits = v, hits(), set_hits(v)
**       .end();
**
** Parameter types understood: bool, any integer type (out-of-range script
** ints are an error, never truncated), enums (as ints), float/double,
** const char *, Value, Vec2f / Vec3f (also as const &; script Vec2 / Vec3
** values, a wrong type is an error naming Vec2 / Vec3), and C * / C & for a native class registered with
** def_class<C> (the argument must be an instance of C or of a script
** subclass of it; None is an error, take a Value if it may be absent).
** A first parameter of type VM * receives the VM. Results: void, the same
** scalar types, const char * (copied), Value, Vec2f / Vec3f, and C * / C & of a bound class
** (wrapped, nullptr -> None). Everything else stays available through the raw
** NativeFn API.
**
** Properties: .property<&T::member>("x") makes `obj.x`, `obj.x = v` and
** `obj.x.y += 1` reach the C++ member (Vec2 / Vec3 members arrive as value
** types; assigning a wrong type is an error) and also keeps the methods x() /
** set_x(v). A const char * member is read-only. For engines that use accessor
** methods: .property<&T::getPos, &T::setPos>("pos") (getter with no
** parameters, setter with one; free functions taking T * first work too) and
** .getter<&T::getFov>("fov") for a read-only property. getter<> / setter<> on
** a data member register the method AND the matching half of the property
** (call both for a read-write property, which is what property<> does).
** Lookup order for `obj.x`: script field, then property, then method.
**
** Ownership: a class given .dtor() OWNS the instances the script creates (the
** GC deletes them). A host-owned object is exposed with wrap<C>(vm, ptr): the
** script instance is "borrowed" (VM::make_instance_borrowed), so neither the
** GC nor destroy_instance() ever runs the class's dtor on it -- it works for
** classes with and without .dtor(), and for engine classes whose objects the
** engine frees itself.
**
** The class registry (ClassOf<C>) is one slot per C++ type, not per VM: with
** several VMs alive, the most recently bound one wins. Bind a type once per VM
** before running code that passes or returns it.
** ========================================================= */
#ifndef ZEN_BIND_HPP
#define ZEN_BIND_HPP

#include "vm.h"
#include "module.h"
#include "object.h"
#include "memory.h"
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

        /* compiler intrinsics (GCC, Clang and MSVC all provide them) */
        template <typename T> struct is_enum { static constexpr bool value = __is_enum(T); };

        template <typename T> constexpr bool is_signed_int() { return (T)-1 < (T)0; }

        /* Does the script integer (int64) fit the target integral type? */
        template <typename T> constexpr bool int_fits(int64_t v)
        {
            if constexpr (sizeof(T) >= 8)
                return is_signed_int<T>() || v >= 0;
            else if constexpr (is_signed_int<T>())
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

        /* Types converted by value (as opposed to bound classes, which travel
        ** as pointers into native_data). */
        template <typename T> struct is_scalar
        {
            static constexpr bool value = is_same<T, bool>::value || is_integral<T>::value ||
                                          is_floating<T>::value || is_enum<T>::value ||
                                          is_same<T, Value>::value || is_same<T, const char *>::value ||
                                          is_same<T, Vec2f>::value || is_same<T, Vec3f>::value;
        };

        template <unsigned... I> struct Seq {};
        template <unsigned N, unsigned... I> struct MakeSeq : MakeSeq<N - 1, N - 1, I...> {};
        template <unsigned... I> struct MakeSeq<0, I...> { using type = Seq<I...>; };

        template <typename...> struct always_false { static constexpr bool value = false; };

        /* name of a script value's type, for error messages */
        inline const char *type_name_of(const Value &v)
        {
            if (is_nil(v)) return "None";
            if (is_bool(v)) return "bool";
            if (is_int(v)) return "int";
            if (is_float(v)) return "float";
            if (is_string(v)) return "string";
            if (is_vec2(v)) return "Vec2";
            if (is_vec3(v)) return "Vec3";
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

        /* out = prefix + name, truncated to cap-1 */
        inline void concat(char *out, size_t cap, const char *a, const char *b)
        {
            size_t n = 0;
            for (; *a && n + 1 < cap; a++) out[n++] = *a;
            for (; *b && n + 1 < cap; b++) out[n++] = *b;
            out[n] = 0;
        }
    }

    /* ---- class registry: the ObjClass of each bound C++ type, filled by
    ** ClassDef<T>::end(). ---- */
    template <typename T> struct ClassOf
    {
        static inline ObjClass *klass = nullptr;
    };

    /* ---- argument conversion: Arg<T>::check(Value) / get(VM*, Value) / what() ---- */
    template <typename T, typename Enable = void> struct Arg
    {
        static_assert(detail::always_false<T>::value,
                      "zen_bind: unsupported parameter type (use a scalar, const char *, Value, Vec2f / Vec3f, or a bound C * / C &)");
    };

    template <> struct Arg<bool>
    {
        static bool check(Value) { return true; } /* Python truthiness */
        static bool get(VM *, Value v) { return is_truthy_full(v); }
        static const char *what() { return "a bool"; }
    };
    template <typename T> struct Arg<T, typename detail::enable_if<detail::is_integral<T>::value>::type>
    {
        static bool check(Value v)
        {
            if (is_bool(v)) return true;
            return is_int(v) && detail::int_fits<T>(v.as.integer);
        }
        static T get(VM *, Value v) { return (T)(is_int(v) ? v.as.integer : (v.as.boolean ? 1 : 0)); }
        static const char *what() { return "an int in range"; }
    };
    template <typename T> struct Arg<T, typename detail::enable_if<detail::is_enum<T>::value>::type>
    {
        using U = __underlying_type(T);
        static bool check(Value v) { return is_int(v) && detail::int_fits<U>(v.as.integer); }
        static T get(VM *, Value v) { return (T)v.as.integer; }
        static const char *what() { return "an int (enum)"; }
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
    /* Vec2 / Vec3 are value types inside Value (no heap object): they travel
    ** by value as the plain Vec2f / Vec3f structs; `const Vec2f &` etc. reach
    ** these through the is_scalar rule below, never the bound-class Arg<C &>. */
    template <> struct Arg<Vec2f>
    {
        static bool check(Value v) { return is_vec2(v); }
        static Vec2f get(VM *, Value v) { return as_vec2(v); }
        static const char *what() { return "Vec2"; }
    };
    template <> struct Arg<Vec3f>
    {
        static bool check(Value v) { return is_vec3(v); }
        static Vec3f get(VM *, Value v) { return as_vec3(v); }
        static const char *what() { return "Vec3"; }
    };
    /* C * : an instance of the class registered with def_class<C> (or of a
    ** script subclass of it) that carries native data. */
    template <typename C> struct Arg<C *, typename detail::enable_if<!detail::is_same<C, const char>::value && !detail::is_same<C, char>::value>::type>
    {
        static_assert(!detail::is_same<typename detail::remove_cvref<C>::type, VM>::value,
                      "zen_bind: VM * is only allowed as the FIRST parameter");
        using U = typename detail::remove_cvref<C>::type;
        static bool check(Value v)
        {
            return detail::instance_is(v, ClassOf<U>::klass) && as_instance(v)->native_data != nullptr;
        }
        static C *get(VM *, Value v) { return zen_instance_data<C>(v); }
        static const char *what()
        {
            ObjClass *k = ClassOf<U>::klass;
            return (k && k->name) ? k->name->chars : "a bound native object";
        }
    };
    /* C & : the same through the pointer; scalars as const T & are by value */
    template <typename C> struct Arg<C &, typename detail::enable_if<!detail::is_scalar<typename detail::remove_cvref<C>::type>::value>::type>
    {
        static bool check(Value v) { return Arg<C *>::check(v); }
        static C &get(VM *vm, Value v) { return *Arg<C *>::get(vm, v); }
        static const char *what() { return Arg<C *>::what(); }
    };
    template <typename T> struct Arg<const T &, typename detail::enable_if<detail::is_scalar<T>::value>::type> : Arg<T> {};

    /* ---- wrapping host-owned memory ---- */

    /* A script instance of T's bound class holding `ptr` (None for nullptr).
    ** Built with make_instance_borrowed(): the native constructor does not run
    ** (it would allocate a second object) and the instance is marked
    ** borrowed, so the GC never calls the class's dtor on `ptr`. */
    template <typename T> Value wrap(VM &vm, T *ptr)
    {
        using U = typename detail::remove_cvref<T>::type;
        if (!ptr)
            return val_nil();
        ObjClass *k = ClassOf<U>::klass;
        if (!k)
        {
            vm.runtime_error("wrap: C++ type is not bound to a script class");
            return val_nil();
        }
        return vm.make_instance_borrowed(k, (void *)const_cast<U *>(ptr));
    }

    /* ---- result conversion: Ret<R>::put(VM*, Value *slot, R) ---- */
    template <typename R, typename Enable = void> struct Ret
    {
        static_assert(detail::always_false<R>::value,
                      "zen_bind: unsupported return type (return a scalar, const char *, Value, Vec2f / Vec3f, or a bound C * / C &)");
    };
    template <> struct Ret<bool> { static void put(VM *, Value *s, bool r) { *s = val_bool(r); } };
    template <typename T> struct Ret<T, typename detail::enable_if<detail::is_integral<T>::value>::type>
    {
        static void put(VM *, Value *s, T r) { *s = val_int((int64_t)r); }
    };
    template <typename T> struct Ret<T, typename detail::enable_if<detail::is_enum<T>::value>::type>
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
    template <> struct Ret<Vec2f> { static void put(VM *, Value *s, Vec2f r) { *s = val_vec2(r); } };
    template <> struct Ret<Vec3f> { static void put(VM *, Value *s, Vec3f r) { *s = val_vec3(r); } };
    /* C * : the object of a bound class, wrapped (nullptr -> None). The GC
    ** does not own it; see wrap(). */
    template <typename C> struct Ret<C *, typename detail::enable_if<!detail::is_same<C, const char>::value && !detail::is_same<C, char>::value>::type>
    {
        static void put(VM *vm, Value *s, C *r) { *s = wrap<C>(*vm, r); }
    };
    template <typename T> struct Ret<T &, void>
    {
        using U = typename detail::remove_cvref<T>::type;
        static void put(VM *vm, Value *s, T &r)
        {
            if constexpr (detail::is_scalar<U>::value)
                Ret<U>::put(vm, s, r);
            else
                *s = wrap<T>(*vm, &r);
        }
    };

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
        /* Checks args[0..arity-1]. `base` is the position reported for args[0]
        ** (1 for ordinary arguments; 0 when args[0] is the method receiver). */
        static bool check(VM *vm, Value *args, const char *name, int base = 1)
        {
            int bad = -1, idx = 0;
            const char *want = nullptr;
            (void)args; (void)idx; (void)want;
            ((bad < 0 && !Arg<A>::check(args[idx]) ? (bad = idx, want = Arg<A>::what(), (void)0) : (void)0, idx++), ...);
            if (bad >= 0)
            {
                if (base == 0 && bad == 0)
                    vm->runtime_error("%s: receiver must be %s, got %s", name, want, detail::type_name_of(args[bad]));
                else
                    vm->runtime_error("%s: argument %d must be %s, got %s", name, bad + base, want, detail::type_name_of(args[bad]));
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
    /* noexcept is part of the type in C++17: same traits as without it (the
    ** VM * forms resolve through the inner lookup) */
    template <typename R, typename... A> struct Sig<R (*)(A...) noexcept> : Sig<R (*)(A...)> {};
    template <typename C, typename R, typename... A> struct Sig<R (C::*)(A...) noexcept> : Sig<R (C::*)(A...)> {};
    template <typename C, typename R, typename... A> struct Sig<R (C::*)(A...) const noexcept> : Sig<R (C::*)(A...) const> {};

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
    template <typename R, typename T, typename... A> struct SelfSig<R (*)(A...) noexcept, T> : SelfSig<R (*)(A...), T> {};

    /* data-member pointers: T C::* (the member's type; member functions have
    ** function types and are rejected by is_data) */
    template <typename P> struct DataMember { static constexpr bool is_data = false; };
    template <typename C, typename T> struct DataMember<T C::*>
    {
        template <typename X> struct IsFn { static constexpr bool value = false; };
        template <typename R, typename... A> struct IsFn<R(A...)> { static constexpr bool value = true; };
        template <typename R, typename... A> struct IsFn<R(A...) const> { static constexpr bool value = true; };
        template <typename R, typename... A> struct IsFn<R(A...) noexcept> { static constexpr bool value = true; };
        template <typename R, typename... A> struct IsFn<R(A...) const noexcept> { static constexpr bool value = true; };
        using Class = C;
        using Type = T;
        static constexpr bool is_data = !IsFn<T>::value;
    };

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
            if (!S::check(vm, args - 1, name, 0))
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

    /* Data member as accessors: get -> name(), set -> set_name(v). */
    template <auto PM, typename T> struct MemberThunk
    {
        using DM = DataMember<decltype(PM)>;
        static_assert(DM::is_data, "zen_bind: getter/setter/property need a data member pointer (use method<> for functions)");
        using V = typename detail::remove_cvref<typename DM::Type>::type;
        static inline const char *get_name = "getter";
        static inline const char *set_name = "setter"; /* the property name; errors print set_<name> */

        static T *self_of(VM *vm, Value *args, const char *name)
        {
            if (!is_instance(args[-1]) || !as_instance(args[-1])->native_data)
            {
                vm->runtime_error("%s: receiver is not a native object", name);
                return nullptr;
            }
            return zen_instance_data<T>(args[-1]);
        }
        static int get(VM *vm, Value *args, int)
        {
            T *self = self_of(vm, args, get_name);
            if (!self)
                return -1;
            Ret<V>::put(vm, args, self->*PM);
            return 1;
        }
        static int set(VM *vm, Value *args, int nargs)
        {
            static_assert(!detail::is_same<V, const char *>::value,
                          "zen_bind: a const char * member cannot be set (the script string is not owned by the object)");
            char fname[96];
            detail::concat(fname, sizeof(fname), "set_", set_name);
            T *self = self_of(vm, args, fname);
            if (!self)
                return -1;
            if (nargs < 1)
            {
                vm->runtime_error("%s: expected 1 argument(s), got %d", fname, nargs);
                return -1;
            }
            if (!Sig<void (*)(V)>::check(vm, args, fname))
                return -1;
            self->*PM = Arg<V>::get(vm, args[0]);
            return 0;
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
    /* a leading VM * is injected: new T(vm, converted args...) */
    template <typename T, typename... A> struct CtorThunk<T, VM *, A...>
    {
        static inline const char *name = "ctor";
        template <unsigned... I> static T *make(VM *vm, Value *args, detail::Seq<I...>) { (void)args; return new T(vm, Arg<A>::get(vm, args[I])...); }
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
        ClassDef(VM &vm, const char *name) : builder_(vm.def_class(name)), name_(name) {}

        /* script subclass of an already registered class */
        ClassDef &parent(const char *parent_name) { builder_.parent(parent_name); return *this; }

        /* Cls(args...) -> new T(converted args...) (a leading VM * is injected) */
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
        /* delete (T *) when a script-created instance is collected: the class
        ** owns those. Host objects exposed with wrap<T>() / make_instance_borrowed
        ** are never deleted. */
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
        /* Read side of `x`: the script method x() AND the property `x` (read-only
        ** until a setter is registered under the same name). PM is either a
        ** data member `T::x` or a function with no script parameters (member
        ** function, or a free function taking T * first). */
        template <auto PM> ClassDef &getter(const char *name)
        {
            if constexpr (DataMember<decltype(PM)>::is_data)
            {
                MemberThunk<PM, T>::get_name = name;
                builder_.method(name, &MemberThunk<PM, T>::get, 0);
                builder_.property(name, &MemberThunk<PM, T>::get, nullptr);
            }
            else
            {
                static_assert(fn_arity<PM>() == 0, "zen_bind: a getter function takes no script parameters");
                static_assert(!detail::is_same<typename Sig<decltype(PM)>::Ret, void>::value,
                              "zen_bind: a getter function must return the value");
                NativeFn fn = bind_fn<PM>(name);
                builder_.method(name, fn, 0);
                builder_.property(name, fn, nullptr);
            }
            return *this;
        }
        /* Write side of `x`: the script method set_x(v) (range / type checked)
        ** AND the setter half of the property `x`. PM as for getter<>, with
        ** exactly one script parameter for a function. */
        template <auto PM> ClassDef &setter(const char *name)
        {
            char buf[96];
            detail::concat(buf, sizeof(buf), "set_", name);
            if constexpr (DataMember<decltype(PM)>::is_data)
            {
                MemberThunk<PM, T>::set_name = name;
                builder_.method(buf, &MemberThunk<PM, T>::set, 1);
                builder_.property(name, nullptr, &MemberThunk<PM, T>::set);
            }
            else
            {
                static_assert(fn_arity<PM>() == 1, "zen_bind: a setter function takes exactly one script parameter");
                NativeFn fn = bind_fn<PM>(FnName<PM>::make(buf));
                builder_.method(buf, fn, 1);
                builder_.property(name, nullptr, fn);
            }
            return *this;
        }
        /* Both sides: data member `T::x` (a const char * member is read-only). */
        template <auto PM> ClassDef &property(const char *name)
        {
            getter<PM>(name);
            if constexpr (!detail::is_same<typename MemberThunk<PM, T>::V, const char *>::value)
                setter<PM>(name);
            return *this;
        }
        /* Accessor pair: T::getPos() / T::setPos(v) (or free functions taking T *
        ** first) -> pos, pos(), set_pos(v). The property's type is the getter's
        ** return type. */
        template <auto G, auto S> ClassDef &property(const char *name)
        {
            return getter<G>(name).template setter<S>(name);
        }

        ClassDef &field(const char *name) { builder_.field(name); return *this; }
        ClassDef &persistent(bool p = true) { builder_.persistent(p); return *this; }
        ClassDef &constructable(bool c = true) { builder_.constructable(c); return *this; }

        /* Seals the class and records it in ClassOf<T>, which the class check
        ** on C * / C & parameters and wrap() read. */
        ObjClass *end()
        {
            ObjClass *k = builder_.end();
            ClassOf<T>::klass = k;
            return k;
        }

    private:
        /* script-visible parameter count of a method-like function */
        template <auto F> static constexpr int fn_arity()
        {
            if constexpr (SelfSig<decltype(F), T>::value)
                return SelfThunk<F, T>::arity;
            else
                return Sig<decltype(F)>::arity;
        }
        /* storage for the "set_<name>" a Thunk reports in its errors */
        template <auto F> struct FnName
        {
            static inline char buf[96];
            static const char *make(const char *n)
            {
                detail::concat(buf, sizeof(buf), n, "");
                return buf;
            }
        };
        /* names the function's thunk (for error messages) and returns it */
        template <auto F> static NativeFn bind_fn(const char *name)
        {
            if constexpr (SelfSig<decltype(F), T>::value)
            {
                SelfThunk<F, T>::name = name;
                return &SelfThunk<F, T>::native;
            }
            else
            {
                Thunk<F>::name = name;
                return &Thunk<F>::native;
            }
        }

        VM::ClassBuilder builder_;
        const char *name_;
    };

    template <typename T> ClassDef<T> def_class(VM &vm, const char *name) { return ClassDef<T>(vm, name); }

} /* namespace bind */
} /* namespace zen */

#endif /* ZEN_BIND_HPP */
