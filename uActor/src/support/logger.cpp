#include "support/logger.hpp"

namespace uActor::Support {

const std::set<std::string_view> Logger::valid_levels = {
    "TRACE", "DEBUG", "INFO", "WARNING", "ERROR", "FATAL"};

}  // namespace uActor::Support
