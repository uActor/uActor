#pragma once

#define __STDC_FORMAT_MACROS
#include <chrono>
#include <cinttypes>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string_view>

#define UACTOR_LOG_LEVEL_TRACE 6
#define UACTOR_LOG_LEVEL_DEBUG 5
#define UACTOR_LOG_LEVEL_INFO 4
#define UACTOR_LOG_LEVEL_WARNING 3
#define UACTOR_LOG_LEVEL_ERROR 2
#define UACTOR_LOG_LEVEL_FATAL 1

#define LOG_LEVEL UACTOR_LOG_LEVEL_INFO

namespace uActor::Support {
struct Logger {
  template <typename... Args>
  static void trace(std::string_view component, Args... args) {
#if LOG_LEVEL >= UACTOR_LOG_LEVEL_TRACE
    log("TRACE", component, args...);
#endif
  }

  template <typename... Args>
  static void debug(std::string_view component, Args... args) {
#if LOG_LEVEL >= UACTOR_LOG_LEVEL_DEBUG
    log("DEBUG", component, args...);
#endif
  }

  template <typename... Args>
  static void info(std::string_view component, Args... args) {
#if LOG_LEVEL >= UACTOR_LOG_LEVEL_INFO
    log("INFO", component, args...);
#endif
  }

  template <typename... Args>
  static void warning(std::string_view component, Args... args) {
#if LOG_LEVEL >= UACTOR_LOG_LEVEL_WARNING
    log("WARNING", component, args...);
#endif
  }

  template <typename... Args>
  static void error(std::string_view component, Args... args) {
#if LOG_LEVEL >= UACTOR_LOG_LEVEL_ERROR
    log("ERROR", component, args...);
#endif
  }

  template <typename... Args>
  static void fatal(std::string_view component, Args... args) {
#if LOG_LEVEL >= UACTOR_LOG_LEVEL_FATAL
    log("FATAL", component, args...);
#endif
  }

  static void log(std::string_view level, std::string_view component,
                  const char* fmt, ...) {
    printf("[%" PRIu64 "][%s][%s]",
           static_cast<uint64_t>(
               std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()),
           level.data(), component.data());

    va_list params;
    va_start(params, fmt);
    vprintf(fmt, params);
    va_end(params);
    printf("\n");
  }
};
}  // namespace uActor::Support
