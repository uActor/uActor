#include "support/logger.hpp"

#include <chrono>
#include <cstdio>
#include <mutex>

namespace uActor::Support {

std::mutex
    Logger::mtx;  // NOLINT (cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace uActor::Support
