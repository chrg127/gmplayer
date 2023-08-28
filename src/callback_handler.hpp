#pragma once

#include <vector>
#include <functional>

template <typename T> class CallbackHandler;

template <typename R, typename... P>
class CallbackHandler<R(P...)> {
    std::vector<std::function<R(P...)>> callbacks;
public:
    void add(auto &&fn) { callbacks.push_back(fn); }
    void call(auto&&... args)
    {
        for (auto &f : callbacks)
            f(std::forward<decltype(args)>(args)...);
    }
    void operator()(auto&&... args) { call(std::forward<decltype(args)>(args)...); }
};
