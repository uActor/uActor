#pragma once

#define __STDC_FORMAT_MACROS
#include <chrono>
#include <cinttypes>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string_view>

#define LOG_LEVEL_TRACE 6
#define LOG_LEVEL_DEBUG 5
#define LOG_LEVEL_INFO 4
#define LOG_LEVEL_WARNING 3
#define LOG_LEVEL_ERROR 2
#define LOG_LEVEL_FATAL 1

#define LOG_LEVEL LOG_LEVEL_INFO

namespace uActor::Support {
struct Logger {
  template <typename... Args>
  static void trace(std::string_view component, std::string_view sub_component,
                    Args... args) {
#if LOG_LEVEL >= LOG_LEVEL_TRACE
    log("TRACE", component, sub_component, args...);
#endif
  }

  template <typename... Args>
  static void debug(std::string_view component, std::string_view sub_component,
                    Args... args) {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    log("DEBUG", component, sub_component, args...);
#endif
  }

  template <typename... Args>
  static void info(std::string_view component, std::string_view sub_component,
                   Args... args) {
#if LOG_LEVEL >= LOG_LEVEL_INFO
    log("INFO", component, sub_component, args...);
#endif
  }

  template <typename... Args>
  static void warning(std::string_view component,
                      std::string_view sub_component, Args... args) {
#if LOG_LEVEL >= LOG_LEVEL_WARNING
    log("WARNING", component, sub_component, args...);
#endif
  }

  template <typename... Args>
  static void error(std::string_view component, std::string_view sub_component,
                    Args... args) {
#if LOG_LEVEL >= LOG_LEVEL_ERROR
    log("ERROR", component, sub_component, args...);
#endif
  }

  template <typename... Args>
  static void fatal(std::string_view component, std::string_view sub_component,
                    Args... args) {
#if LOG_LEVEL >= LOG_LEVEL_FATAL
    log("FATAL", component, sub_component, args...);
#endif
  }

  static void log(std::string_view level, std::string_view component,
                  std::string_view sub_component, const char* fmt, ...) {
    printf("[%" PRIu64 "][%s][%s][%s]",
           static_cast<uint64_t>(
               std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()),
           level.data(), component.data(), sub_component.data());

    va_list params;
    va_start(params, fmt);
    vprintf(fmt, params);
    va_end(params);
    printf("\n");
  }

  static std::mutex mtx;
};
}  // namespace uActor::Support
