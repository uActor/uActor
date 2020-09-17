#include "support/logger.hpp"

#include <chrono>
#include <cstdio>
#include <mutex>

namespace uActor::Support {

std::mutex Logger::mtx;
}  // namespace uActor::Support
