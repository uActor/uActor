#if CONFIG_UACTOR_V8
#include "actor_runtime/v8/utils.hpp"

#include <cmath>

#include "support/logger.hpp"

namespace uActor::ActorRuntime::V8Runtime {

TypeUtils::NumberVariant TypeUtils::parse_number(v8::Isolate* isolate,
                                                 v8::Local<v8::Value> value) {
  if (value->IsInt32()) {
    int32_t number;
    if (value->Int32Value(isolate->GetCurrentContext()).To(&number)) {
      return number;
    }
    // We give priority to signed numbers here, this would also match unsigned
  } else if (value->IsUint32()) {
    uint32_t number;
    if (value->Uint32Value(isolate->GetCurrentContext()).To(&number)) {
      return number;
    }
  } else if (value->IsNumber()) {
    double base_val;
    if (!value->NumberValue(isolate->GetCurrentContext()).To(&base_val)) {
      NumberVariant();
    }
    // This appears to be the only way (ignoring BigInt) of V8 storing an us
    // retrieving an Integer that exceeds the range of an INT32.
    // https://stackoverflow.com/a/25223135 +
    // https://stackoverflow.com/a/1521682
    double integral_part;
    if (std::modf(base_val, &integral_part) == 0.0) {
      // Assume this does not fit into an int32 and doesn't have to be
      // unsigned (see links above)
      return static_cast<int64_t>(base_val);
    } else {
      return base_val;
    }
  } else if (value->IsBigInt()) {
    auto bi = v8::Local<v8::BigInt>::Cast(value);
    bool loss_i, loss_ui;
    int64_t val_i = bi->Int64Value(&loss_i);
    uint64_t val_ui = bi->Uint64Value(&loss_ui);
    if (loss_i && !loss_ui) {
      return val_ui;
    } else {
      Support::Logger::warning("V8-TYPE-UTILS",
                               "BigInt was large than (U)INT64.");
      return val_i;
    }
  }

  return NumberVariant();
}

std::optional<PubSub::Constraint> TypeUtils::parse_constraint(
    v8::Isolate* isolate, v8::Local<v8::Object> constraint_object) {
  std::string attribute;
  PubSub::ConstraintPredicates::Predicate predicate =
      PubSub::ConstraintPredicates::Predicate::EQ;
  bool optional = false;

  // required
  auto attribute_value =
      get_value_from_obj(isolate, constraint_object, "attribute");
  if (attribute_value && (*attribute_value)->IsString()) {
    attribute = *v8::String::Utf8Value(isolate, *attribute_value);
  } else {
    Support::Logger::warning("V8-TYPE-UTILS", "bad attribute");
    return std::nullopt;
  }

  // optional
  auto optional_value =
      get_value_from_obj(isolate, constraint_object, "optional");
  if (optional_value) {
    if ((*optional_value)->IsBoolean()) {
      optional = v8::Local<v8::Boolean>::Cast(*optional_value)->Value();
    } else {
      Support::Logger::warning("V8-TYPE-UTILS", "bad optional");
      return std::nullopt;
    }
  }

  // optional
  auto predicate_value =
      get_value_from_obj(isolate, constraint_object, "predicate");
  if (predicate_value) {
    uint32_t predicate_id;
    if ((*predicate_value)->IsUint32() &&
        (*predicate_value)
            ->Uint32Value(isolate->GetCurrentContext())
            .To(&predicate_id)) {
      static_cast<PubSub::ConstraintPredicates::Predicate>(predicate_id);
    } else {
      Support::Logger::warning("V8-TYPE-UTILS", "bad optional");
      return std::nullopt;
    }
  }

  // required
  auto operand_value =
      get_value_from_obj(isolate, constraint_object, "operand");
  if (operand_value) {
    auto number_parsed = parse_number(isolate, *operand_value);
    if (std::holds_alternative<uint32_t>(number_parsed)) {
      return PubSub::Constraint(
          attribute, static_cast<int32_t>(std::get<uint32_t>(number_parsed)),
          predicate, optional);
    } else if (std::holds_alternative<int32_t>(number_parsed)) {
      return PubSub::Constraint(attribute, std::get<int32_t>(number_parsed),
                                predicate, optional);
    } else if (std::holds_alternative<uint64_t>(number_parsed)) {
      return PubSub::Constraint(
          attribute, static_cast<int32_t>(std::get<uint64_t>(number_parsed)),
          predicate, optional);
    } else if (std::holds_alternative<int64_t>(number_parsed)) {
      return PubSub::Constraint(
          attribute, static_cast<int32_t>(std::get<int64_t>(number_parsed)),
          predicate, optional);
    } else if (std::holds_alternative<float>(number_parsed)) {
      return PubSub::Constraint(attribute, std::get<float>(number_parsed),
                                predicate, optional);
    } else if (std::holds_alternative<double>(number_parsed)) {
      return PubSub::Constraint(
          attribute, static_cast<float>(std::get<double>(number_parsed)),
          predicate, optional);
    } else if ((*operand_value)->IsString()) {
      std::string operand = *v8::String::Utf8Value(isolate, *operand_value);
      return PubSub::Constraint(attribute, operand, predicate, optional);
    }
  }

  Support::Logger::warning("V8-TYPE-UTILS", "fall through");

  return std::nullopt;
}

std::optional<PubSub::Filter> TypeUtils::parse_filter(
    v8::Isolate* isolate, v8::Local<v8::Array> constraints) {
  std::vector<PubSub::Constraint> parsed_constraints;

  for (size_t i = 0; i < constraints->Length(); i++) {
    v8::Local<v8::Value> val;
    if (!constraints->Get(isolate->GetCurrentContext(), i).ToLocal(&val) ||
        !val->IsObject()) {
      return std::nullopt;
    }
    auto parsed_constraint =
        parse_constraint(isolate, v8::Local<v8::Object>::Cast(val));
    if (!parsed_constraint) {
      return std::nullopt;
    }
    parsed_constraints.push_back(*parsed_constraint);
  }

  return PubSub::Filter(std::move(parsed_constraints));
}

std::optional<v8::Local<v8::Value>> TypeUtils::get_value_from_obj(
    v8::Isolate* isolate, const v8::Local<v8::Object>& obj,
    std::string_view key) {
  v8::EscapableHandleScope handle_scope(isolate);
  bool has_attribute = false;
  v8::Local<v8::String> attribute_key;
  if (!v8::String::NewFromUtf8(isolate, key.data(), v8::NewStringType::kNormal,
                               key.size())
           .ToLocal(&attribute_key)) {
    return std::nullopt;
  }

  if (obj->HasOwnProperty(isolate->GetCurrentContext(), attribute_key)
          .To(&has_attribute) &&
      has_attribute) {
    v8::Local<v8::Value> attribute_obj;
    if (obj->Get(isolate->GetCurrentContext(), attribute_key)
            .ToLocal(&attribute_obj)) {
      return handle_scope.Escape(attribute_obj);
    }
  }
  return std::nullopt;
}

}  // namespace uActor::ActorRuntime::V8Runtime
#endif
