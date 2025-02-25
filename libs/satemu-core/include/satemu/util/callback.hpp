#pragma once

#include <cassert>
#include <functional>
#include <type_traits>
#include <utility>

namespace util {

namespace detail {

    template <typename T>
    constexpr bool alwaysFalse = false;

    template <bool skipNullCheck, typename TReturn, typename... TArgs>
    struct FuncClass {
        using ReturnType = TReturn;
        using FnType = ReturnType (*)(TArgs... args, void *context);

        FuncClass()
            : m_context(nullptr)
            , m_fn(nullptr) {}

        FuncClass(void *context, FnType fn)
            : m_context(context)
            , m_fn(fn) {}

        FuncClass(const FuncClass &) = default;
        FuncClass(FuncClass &&) = default;

        FuncClass &operator=(const FuncClass &) = default;
        FuncClass &operator=(FuncClass &&) = default;

        void Rebind(FnType fn) {
            Rebind(nullptr, fn);
        }

        void Rebind(void *context, FnType fn) {
            m_context = context;
            m_fn = fn;
        }

        ReturnType operator()(TArgs... args) {
            if constexpr (!skipNullCheck) {
                if (m_fn == nullptr) [[unlikely]] {
                    if constexpr (std::is_void_v<ReturnType>) {
                        return;
                    } else {
                        return {};
                    }
                }
            } else {
                assert(m_fn != nullptr);
            }
            return m_fn(std::forward<TArgs>(args)..., m_context);
        }

    private:
        void *m_context;
        FnType m_fn;
    };

    template <bool skipNullCheck, typename TFunc>
    struct FuncImpl {
        static_assert(alwaysFalse<TFunc>, "Callback requires a function argument");
    };

    template <bool skipNullCheck, typename TReturn, typename... TArgs>
    struct FuncImpl<skipNullCheck, TReturn(TArgs...)> {
        using type = FuncClass<skipNullCheck, TReturn, TArgs...>;
    };

    template <typename TFunc, bool skipNullCheck>
    class Callback : public detail::FuncImpl<skipNullCheck, TFunc>::type {
        using FnType = typename detail::FuncImpl<skipNullCheck, TFunc>::type::FnType;

    public:
        Callback() = default;

        Callback(FnType fn)
            : detail::FuncImpl<skipNullCheck, TFunc>::type(nullptr, fn) {}

        Callback(void *context, FnType fn)
            : detail::FuncImpl<skipNullCheck, TFunc>::type(context, fn) {}

        Callback(const Callback &) = default;
        Callback(Callback &&) = default;

        Callback &operator=(const Callback &) = default;
        Callback &operator=(Callback &&) = default;
    };

} // namespace detail

template <typename TFunc>
using RequiredCallback = detail::Callback<TFunc, true>;

template <typename TFunc>
using OptionalCallback = detail::Callback<TFunc, false>;

// -------------------------------------------------------------------------------------------------

namespace detail {

    template <typename>
    struct MFPCallbackMaker;

    template <typename Return, typename Object, typename... Args>
    struct MFPCallbackMaker<Return (Object::*)(Args...)> {
        using class_type = Object;

        template <Return (Object::*mfp)(Args...), bool skipNullCheck>
        static auto GetCallback(Object *context) {
            return Callback<Return(Args...), skipNullCheck>{context, [](Args... args, void *context) {
                                                                auto &obj = *static_cast<Object *>(context);
                                                                return (obj.*mfp)(std::forward<Args>(args)...);
                                                            }};
        }
    };

    template <typename Return, typename Object, typename... Args>
    struct MFPCallbackMaker<Return (Object::*)(Args...) const> {
        using class_type = Object;

        template <Return (Object::*mfp)(Args...), bool skipNullCheck>
        static auto GetCallback(Object *context) {
            return Callback<Return(Args...), skipNullCheck>{context, [](Args... args, void *context) {
                                                                auto &obj = *static_cast<Object *>(context);
                                                                return (obj.*mfp)(std::forward<Args>(args)...);
                                                            }};
        }
    };

} // namespace detail

template <auto mfp>
    requires std::is_member_function_pointer_v<decltype(mfp)>
auto MakeClassMemberRequiredCallback(typename detail::MFPCallbackMaker<decltype(mfp)>::class_type *context) {
    return detail::MFPCallbackMaker<decltype(mfp)>::template GetCallback<mfp, true>(context);
}

template <auto mfp>
    requires std::is_member_function_pointer_v<decltype(mfp)>
auto MakeClassMemberOptionalCallback(typename detail::MFPCallbackMaker<decltype(mfp)>::class_type *context) {
    return detail::MFPCallbackMaker<decltype(mfp)>::template GetCallback<mfp, false>(context);
}

} // namespace util
