#pragma once

#include <v8.h>

#include <optional>
#include <variant>

#include "pubsub/constraint.hpp"
#include "pubsub/filter.hpp"
#include "pubsub/subscription_arguments.hpp"

namespace uActor::ActorRuntime::V8Runtime {
struct TypeUtils {
  using NumberVariant = std::variant<std::monostate, uint32_t, int32_t,
                                     uint64_t, int64_t, float, double>;

  static NumberVariant parse_number(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value);

  static std::optional<PubSub::Constraint> parse_constraint(
      v8::Isolate* isolate, v8::Local<v8::Object> constraint_object);

  static std::optional<PubSub::Filter> parse_filter(
      v8::Isolate* isolate, v8::Local<v8::Array> constraints);

  static std::optional<std::vector<PubSub::Constraint>> parse_filter_list(
      v8::Isolate* isolate, v8::Local<v8::Array> constraints);

  static std::optional<PubSub::SubscriptionArguments> parse_arguments(
      v8::Isolate* isolate, v8::Local<v8::Object> args);

  static std::optional<v8::Local<v8::Value>> get_value_from_obj(
      v8::Isolate* isolate, const v8::Local<v8::Object>& obj,
      std::string_view key);
};

}  // namespace uActor::ActorRuntime::V8Runtime
