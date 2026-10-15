#pragma once

#include <cstdio>
#include <format>
#include <print>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

#include <systemd/sd-daemon.h>

namespace cfc {

// Two configured levels. Errors and normal messages both print at level norm;
// level debug adds the per-decision control trace.
enum class Level { Norm, Debug };

namespace detail {

inline Level& level_ref() {
    static Level lv = Level::Norm;
    return lv;
}

// True only when this process's stderr really is journald's stream socket.
// Checking just that $JOURNAL_STREAM exists is not enough: systemd.exec(5)
// warns that a parent can leave the variable set while the fd was replaced,
// which would make us emit "<6>" into a terminal.
inline bool stderr_is_journal() {
    const char* js = ::getenv("JOURNAL_STREAM");
    if (js == nullptr)
        return false;

    unsigned long long dev = 0;
    unsigned long long ino = 0;
    if (std::sscanf(js, "%llu:%llu", &dev, &ino) != 2)
        return false;

    struct stat st {};
    if (::fstat(STDERR_FILENO, &st) != 0)
        return false;

    return static_cast<unsigned long long>(st.st_dev) == dev &&
           static_cast<unsigned long long>(st.st_ino) == ino;
}

inline bool journal() {
    static const bool j = stderr_is_journal();
    return j;
}

// Writes one line. journald strips the leading "<N>" and stores it as
// PRIORITY; a terminal would show the prefix, so it is only added when the
// output actually goes to the journal.
inline void emit(const char* prefix, std::string_view msg) {
    if (journal())
        std::println(stderr, "{}[cfc] {}", prefix, msg);
    else
        std::println(stderr, "[cfc] {}", msg);
}

template <class... Args>
inline void emitf(const char* prefix, std::format_string<Args...> fmt, Args&&... args) {
    emit(prefix, std::format(fmt, std::forward<Args>(args)...));
}

} // namespace detail

inline void set_level(Level lv) { detail::level_ref() = lv; }
inline Level level() { return detail::level_ref(); }

inline bool debug_enabled() { return detail::level_ref() == Level::Debug; }

inline void init_journal_detection() { (void)detail::journal(); }

// Errors always print, at every configured level.
inline void error(std::string_view msg) { detail::emit(SD_ERR, msg); }

template <class... Args>
inline void errorf(std::format_string<Args...> fmt, Args&&... args) {
    detail::emitf(SD_ERR, fmt, std::forward<Args>(args)...);
}

inline void log(std::string_view msg) { detail::emit(SD_INFO, msg); }

template <class... Args>
inline void logf(std::format_string<Args...> fmt, Args&&... args) {
    detail::emitf(SD_INFO, fmt, std::forward<Args>(args)...);
}

inline void debug(std::string_view msg) {
    if (debug_enabled())
        detail::emit(SD_DEBUG, msg);
}

template <class... Args>
inline void debugf(std::format_string<Args...> fmt, Args&&... args) {
    if (debug_enabled())
        detail::emitf(SD_DEBUG, fmt, std::forward<Args>(args)...);
}

} // namespace cfc
