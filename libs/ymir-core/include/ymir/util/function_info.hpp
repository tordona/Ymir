#pragma once

#include <tuple>
#include <type_traits>

namespace fninfo {

// Information about function types.
// Functions types describe the shape of a function.
//
// Matches TReturn(TArgs...).
template <bool isVariadic, typename TReturn, typename... TArgs>
struct FunctionType {
    using pointer_type = TReturn (*)(TArgs...);
    using function_type = TReturn(TArgs...);
    using return_type = TReturn;
    using arg_types = std::tuple<TArgs...>;
    static constexpr bool is_member_function_pointer = false;
    static constexpr bool is_variadic = isVariadic;
};

// ----------------------------------------------------------------------------

// Information about freestanding functions.
// Freestanding functions are either regular C-like functions, coerced lambdas without capture, or static class member
// functions.
//
// Matches TReturn (*)(TArgs...).
template <bool isVariadic, typename TReturn, typename... TArgs>
struct FreestandingFunction {
    using pointer_type = TReturn (*)(TArgs...);
    using function_type = TReturn(TArgs...);
    using return_type = TReturn;
    using arg_types = std::tuple<TArgs...>;
    static constexpr bool is_member_function_pointer = false;
    static constexpr bool is_variadic = isVariadic;
};

// ----------------------------------------------------------------------------

// Information about member functions.
// Member functions belong to a class and must be invoked with a particular instance of that class, that is, they are
// non-static methods of a class.
template <bool isVariadic, typename T>
struct MemberFunction {};

// Matches TReturn (TClass::*)(TArgs...) without qualifiers.
template <bool isVariadic, typename TReturn, typename TClass, typename... TArgs>
struct MemberFunction<isVariadic, TReturn (TClass::*)(TArgs...)> {
    using pointer_type = TReturn (TClass::*)(TArgs...);
    using function_type = TReturn(TArgs...);
    using return_type = TReturn;
    using arg_types = std::tuple<TArgs...>;
    static constexpr bool is_member_function_pointer = true;
    static constexpr bool is_variadic = isVariadic;
};

// Matches TReturn (TClass::*)(TArgs...) const.
template <bool isVariadic, typename TReturn, typename TClass, typename... TArgs>
struct MemberFunction<isVariadic, TReturn (TClass::*)(TArgs...) const> {
    using pointer_type = TReturn (TClass::*)(TArgs...) const;
    using function_type = TReturn(TArgs...);
    using return_type = TReturn;
    using arg_types = std::tuple<TArgs...>;
    static constexpr bool is_member_function_pointer = true;
    static constexpr bool is_variadic = isVariadic;
};

// Matches TReturn (TClass::*)(TArgs...) volatile.
template <bool isVariadic, typename TReturn, typename TClass, typename... TArgs>
struct MemberFunction<isVariadic, TReturn (TClass::*)(TArgs...) volatile> {
    using pointer_type = TReturn (TClass::*)(TArgs...);
    using function_type = TReturn(TArgs...);
    using return_type = TReturn;
    using arg_types = std::tuple<TArgs...>;
    static constexpr bool is_member_function_pointer = true;
    static constexpr bool is_variadic = isVariadic;
};

// Matches TReturn (TClass::*)(TArgs...) const volatile.
template <bool isVariadic, typename TReturn, typename TClass, typename... TArgs>
struct MemberFunction<isVariadic, TReturn (TClass::*)(TArgs...) const volatile> {
    using pointer_type = TReturn (TClass::*)(TArgs...) const volatile;
    using function_type = TReturn(TArgs...);
    using return_type = TReturn;
    using arg_types = std::tuple<TArgs...>;
    static constexpr bool is_member_function_pointer = true;
    static constexpr bool is_variadic = isVariadic;
};

// ----------------------------------------------------------------------------

// Information about functors.
// Functors are structs/classes containing the callable operator(). This includes lambdas, both with and without
// capture.
template <typename T>
struct Functor {};

// Matches TReturn TClass::operator()(TArgs...) without qualifiers.
template <typename TClass, typename TReturn, typename... TArgs>
struct Functor<TReturn (TClass::*)(TArgs...)> {
    using pointer_type = const TClass *;
    using function_type = TReturn(TArgs...);
    using return_type = TReturn;
    using arg_types = std::tuple<TArgs...>;
    static constexpr bool is_member_function_pointer = false;
    static constexpr bool is_variadic = false;
};

// Matches TReturn TClass::operator()(TArgs..., ...) without qualifiers.
template <typename TClass, typename TReturn, typename... TArgs>
struct Functor<TReturn (TClass::*)(TArgs..., ...)> {
    using pointer_type = const TClass *;
    using function_type = TReturn(TArgs...);
    using return_type = TReturn;
    using arg_types = std::tuple<TArgs...>;
    static constexpr bool is_member_function_pointer = false;
    static constexpr bool is_variadic = true;
};

// Matches TReturn TClass::operator()(TArgs...) const.
template <typename TClass, typename TReturn, typename... TArgs>
struct Functor<TReturn (TClass::*)(TArgs...) const> {
    using pointer_type = TClass *;
    using function_type = TReturn(TArgs...);
    using return_type = TReturn;
    using arg_types = std::tuple<TArgs...>;
    static constexpr bool is_variadic = false;
    static constexpr bool is_member_function_pointer = false;
};

// Matches TReturn TClass::operator()(TArgs..., ...) const.
template <typename TClass, typename TReturn, typename... TArgs>
struct Functor<TReturn (TClass::*)(TArgs..., ...) const> {
    using pointer_type = TClass *;
    using function_type = TReturn(TArgs...);
    using return_type = TReturn;
    using arg_types = std::tuple<TArgs...>;
    static constexpr bool is_member_function_pointer = false;
    static constexpr bool is_variadic = true;
};

// Matches TReturn TClass::operator()(TArgs...) volatile.
template <typename TClass, typename TReturn, typename... TArgs>
struct Functor<TReturn (TClass::*)(TArgs...) volatile> {
    using pointer_type = const TClass *;
    using function_type = TReturn(TArgs...);
    using return_type = TReturn;
    using arg_types = std::tuple<TArgs...>;
    static constexpr bool is_member_function_pointer = false;
    static constexpr bool is_variadic = false;
};

// Matches TReturn TClass::operator()(TArgs...) volatile.
template <typename TClass, typename TReturn, typename... TArgs>
struct Functor<TReturn (TClass::*)(TArgs..., ...) volatile> {
    using pointer_type = const TClass *;
    using function_type = TReturn(TArgs...);
    using return_type = TReturn;
    using arg_types = std::tuple<TArgs...>;
    static constexpr bool is_member_function_pointer = false;
    static constexpr bool is_variadic = true;
};

// Matches TReturn TClass::operator()(TArgs...) const volatile.
template <typename TClass, typename TReturn, typename... TArgs>
struct Functor<TReturn (TClass::*)(TArgs...) const volatile> {
    using pointer_type = TClass *;
    using function_type = TReturn(TArgs...);
    using return_type = TReturn;
    using arg_types = std::tuple<TArgs...>;
    static constexpr bool is_member_function_pointer = false;
    static constexpr bool is_variadic = false;
};

// Matches TReturn TClass::operator()(TArgs..., ...) const volatile.
template <typename TClass, typename TReturn, typename... TArgs>
struct Functor<TReturn (TClass::*)(TArgs..., ...) const volatile> {
    using pointer_type = TClass *;
    using function_type = TReturn(TArgs...);
    using return_type = TReturn;
    using arg_types = std::tuple<TArgs...>;
    static constexpr bool is_member_function_pointer = false;
    static constexpr bool is_variadic = true;
};

// ----------------------------------------------------------------------------

namespace detail {
    // Template magic that automatically identifies function types from the raw type.
    template <typename TFunc>
    struct FunctionIdentifier {
        // TFunc is not a callable function, functor or member function pointer
        // type is intentionally left undefined to enable SFINAE
    };

    // Identifies function types.
    template <typename TReturn, typename... TArgs>
    struct FunctionIdentifier<TReturn(TArgs...)> {
        using type = FunctionType<false, TReturn, TArgs...>;
    };

    // Identifies variadic function types.
    template <typename TReturn, typename... TArgs>
    struct FunctionIdentifier<TReturn(TArgs..., ...)> {
        using type = FunctionType<true, TReturn, TArgs...>;
    };

    // Identifies freestanding functions passed by reference.
    template <typename TReturn, typename... TArgs>
    struct FunctionIdentifier<TReturn (&)(TArgs...)> {
        using type = FreestandingFunction<false, TReturn, TArgs...>;
    };

    // Identifies variadic freestanding functions passed by reference.
    template <typename TReturn, typename... TArgs>
    struct FunctionIdentifier<TReturn (&)(TArgs..., ...)> {
        using type = FreestandingFunction<true, TReturn, TArgs...>;
    };

    // Identifies freestanding functions passed by pointer.
    template <typename TReturn, typename... TArgs>
    struct FunctionIdentifier<TReturn (*)(TArgs...)> {
        using type = FreestandingFunction<false, TReturn, TArgs...>;
    };

    // Identifies variadic freestanding functions passed by pointer.
    template <typename TReturn, typename... TArgs>
    struct FunctionIdentifier<TReturn (*)(TArgs..., ...)> {
        using type = FreestandingFunction<true, TReturn, TArgs...>;
    };

    // Identifies member function pointers.
    template <typename TClass, typename TReturn, typename... TArgs>
    struct FunctionIdentifier<TReturn (TClass::*)(TArgs...)> {
        using type = MemberFunction<false, TReturn (TClass::*)(TArgs...)>;
    };

    // Identifies variadic member function pointers.
    template <typename TClass, typename TReturn, typename... TArgs>
    struct FunctionIdentifier<TReturn (TClass::*)(TArgs..., ...)> {
        using type = MemberFunction<true, TReturn (TClass::*)(TArgs...)>;
    };

    // Identifies const-qualified member function pointers.
    template <typename TClass, typename TReturn, typename... TArgs>
    struct FunctionIdentifier<TReturn (TClass::*)(TArgs...) const> {
        using type = MemberFunction<false, TReturn (TClass::*)(TArgs...) const>;
    };

    // Identifies variadic const-qualified member function pointers.
    template <typename TClass, typename TReturn, typename... TArgs>
    struct FunctionIdentifier<TReturn (TClass::*)(TArgs..., ...) const> {
        using type = MemberFunction<true, TReturn (TClass::*)(TArgs...) const>;
    };

    // Identifies volatile-qualified member function pointers.
    template <typename TClass, typename TReturn, typename... TArgs>
    struct FunctionIdentifier<TReturn (TClass::*)(TArgs...) volatile> {
        using type = MemberFunction<false, TReturn (TClass::*)(TArgs...) volatile>;
    };

    // Identifies variadic volatile-qualified member function pointers.
    template <typename TClass, typename TReturn, typename... TArgs>
    struct FunctionIdentifier<TReturn (TClass::*)(TArgs..., ...) volatile> {
        using type = MemberFunction<true, TReturn (TClass::*)(TArgs...) volatile>;
    };

    // Identifies const- and volatile-qualified member function pointers.
    template <typename TClass, typename TReturn, typename... TArgs>
    struct FunctionIdentifier<TReturn (TClass::*)(TArgs...) const volatile> {
        using type = MemberFunction<false, TReturn (TClass::*)(TArgs...) const volatile>;
    };

    // Identifies variadic const- and volatile-qualified member function pointers.
    template <typename TClass, typename TReturn, typename... TArgs>
    struct FunctionIdentifier<TReturn (TClass::*)(TArgs..., ...) const volatile> {
        using type = MemberFunction<true, TReturn (TClass::*)(TArgs...) const volatile>;
    };

    // Identifies functors.
    template <typename TFunc>
        requires requires() { &TFunc::operator(); }
    struct FunctionIdentifier<TFunc> {
        using type = Functor<decltype(&TFunc::operator())>;
    };

} // namespace detail

// ----------------------------------------------------------------------------

// Exposes information about a function type.
// If TFunc is a valid callable function, functor or member function pointer, the following fields and types are
// defined:
// - type pointer_type: the function pointer type: TReturn ([TClass::]*)(TArgs...[, ...])
// - type function_type: the function type: TReturn(TArgs...[, ...])
// - type return_type: the return type of the function: TReturn
// - type arg_types: a tuple of the function's argument types
// - static bool is_member_function_pointer: indicates if the function is a member function pointer
// - static bool is_variadic: indicates if the function is variadic (...)
template <typename TFunc>
using FunctionInfo = typename detail::FunctionIdentifier<std::remove_cvref_t<std::decay_t<TFunc>>>::type;

// Determines if function T2 can be assigned to a variable of type T1.
// Both functions must have the same signature and either be both member function pointers or both not.
template <typename T1, typename T2>
inline constexpr bool IsAssignable =
    std::is_same_v<typename fninfo::FunctionInfo<T1>::function_type,
                   typename fninfo::FunctionInfo<T2>::function_type> &&
    fninfo::FunctionInfo<T1>::is_member_function_pointer == fninfo::FunctionInfo<T2>::is_member_function_pointer;

} // namespace fninfo
