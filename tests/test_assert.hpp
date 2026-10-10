#pragma once

#include <stdexcept>
#include <string>

inline void expect(bool cond, const std::string& msg) {
    if (!cond) {
        throw std::runtime_error(msg);
    }
}

template <typename T>
inline void expectEq(const T& a, const T& b, const std::string& msg) {
    if (!(a == b)) {
        throw std::runtime_error(msg);
    }
}
