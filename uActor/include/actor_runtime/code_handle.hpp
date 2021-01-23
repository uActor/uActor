#pragma once

#include <mutex>
#include <string_view>
#include <utility>

namespace uActor::ActorRuntime {
struct CodeHandle {
  CodeHandle(std::string_view code, std::unique_lock<std::mutex>&& lck)
      : lck(std::move(lck)), code(code) {}
  std::unique_lock<std::mutex> lck;
  std::string_view code;
};
}  // namespace uActor::ActorRuntime
