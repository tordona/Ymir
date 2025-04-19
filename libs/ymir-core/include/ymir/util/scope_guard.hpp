#pragma once

#include <type_traits>

namespace util {

template <typename Fn>
class ScopeGuard {
public:
    ScopeGuard(Fn &&fn)
        : fn(std::move(fn)) {}

    ~ScopeGuard() {
        if (!cancelled) {
            fn();
        }
    }

    void Cancel() {
        cancelled = true;
    }

private:
    Fn fn;
    bool cancelled = false;
};

} // namespace util
