#pragma once
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) +      \
                                     ": " #x);                                                     \
    } while (false)
inline void near(double actual, double expected, double tolerance = 1e-6) {
    CHECK(std::isfinite(actual));
    if (std::abs(actual - expected) > tolerance)
        throw std::runtime_error("expected " + std::to_string(expected) + ", observed " +
                                 std::to_string(actual) + ", tolerance " +
                                 std::to_string(tolerance));
}
template <class F> void rejects(F &&function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::exception &) {
        rejected = true;
    }
    CHECK(rejected);
}
template <class F> int run(F &&function) {
    try {
        function();
        std::cout << "PASS\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
