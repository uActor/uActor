#include "subscription.hpp"

namespace uactor {
void Subscription::insert(const char* key, const char* value) {
  Entry e{Type::STRING, key, value};
  Publication::insert(key, e);
}
}  // namespace uactor
