#pragma once
#include <cstdarg>
#include <cstdio>
namespace Base {
struct StandaloneConsole {
    void Log(const char *fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        std::vfprintf(stderr, fmt, args);
        va_end(args);
    }
    void Warning(const char *fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        std::vfprintf(stderr, fmt, args);
        va_end(args);
    }
};
inline StandaloneConsole Console() {
    return {};
}
} // namespace Base
