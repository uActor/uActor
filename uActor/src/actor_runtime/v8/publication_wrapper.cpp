#if CONFIG_UACTOR_V8
#include "actor_runtime/v8/publication_wrapper.hpp"

#include "actor_runtime/v8/gc_wrapper.hpp"
#include "actor_runtime/v8/utils.hpp"
#include "support/logger.hpp"

namespace uActor::ActorRuntime::V8Runtime {
void PublicationWrapper::add_value_to_publication(
    v8::Isolate* isolate, PubSub::Publication* publication,
    std::string_view key, v8::Local<v8::Value> value) {
  auto parsed = V8Runtime::TypeUtils::parse_number(isolate, value);
  if (!std::holds_alternative<std::monostate>(parsed)) {
    if (std::holds_alternative<float>(parsed)) {
      publication->set_attr(key, std::get<float>(parsed));
    } else if (std::holds_alternative<double>(parsed)) {
      Support::Logger::warning("PUBLICATION-WRAPPER",
                               "Loss of precission: double");
      publication->set_attr(key, static_cast<float>(std::get<double>(parsed)));
    } else if (std::holds_alternative<int32_t>(parsed)) {
      publication->set_attr(key, std::get<int32_t>(parsed));
    } else if (std::holds_alternative<uint32_t>(parsed)) {
      Support::Logger::warning("PUBLICATION-WRAPPER",
                               "Loss of precission: uint32_t");
      publication->set_attr(key,
                            static_cast<int32_t>(std::get<uint32_t>(parsed)));
    } else if (std::holds_alternative<int64_t>(parsed)) {
      Support::Logger::warning("PUBLICATION-WRAPPER",
                               "Loss of precission: int64_t");
      publication->set_attr(key,
                            static_cast<int32_t>(std::get<int64_t>(parsed)));
    } else if (std::holds_alternative<uint64_t>(parsed)) {
      Support::Logger::warning("PUBLICATION-WRAPPER",
                               "Loss of precission: uint64_t");
      publication->set_attr(key,
                            static_cast<int32_t>(std::get<uint64_t>(parsed)));
    }
    // TODO(raphaelhetzel) there might be types that can be safely converted to
    // the String type
  } else if (value->IsString()) {
    std::string c_value = *v8::String::Utf8Value(isolate, value);
    publication->set_attr(key, std::string_view(c_value));
  }
}

void PublicationWrapper::get(v8::Local<v8::Name> key,
                             const v8::PropertyCallbackInfo<v8::Value>& info) {
  std::string c_key = *v8::String::Utf8Value(info.GetIsolate(), key);
  auto pub_ptr = publication_from_context(info);
  if (!pub_ptr->has_attr(c_key)) {
    return;
  }
  auto value = pub_ptr->get_attr(c_key);
  if (std::holds_alternative<std::monostate>(value)) {
    return;
  } else if (std::holds_alternative<std::string_view>(value)) {
    auto str_val = std::get<std::string_view>(value);
    v8::Local<v8::String> v8_string;
    if (!v8::String::NewFromUtf8(info.GetIsolate(), str_val.data(),
                                 v8::NewStringType::kNormal, str_val.size())
             .ToLocal(&v8_string)) {
      Support::Logger::warning("PUBLICATION-WRAPPER",
                               "Failed creating a V8 string (length).");
      return;
    }
    info.GetReturnValue().Set(v8_string);
    // TODO(raphaelhetzel) Check if we can use a better type here
  } else if (std::holds_alternative<int32_t>(value)) {
    info.GetReturnValue().Set(v8::Number::New(
        info.GetIsolate(), static_cast<double>(std::get<int32_t>(value))));
  } else if (std::holds_alternative<float>(value)) {
    info.GetReturnValue().Set(v8::Number::New(
        info.GetIsolate(), static_cast<double>(std::get<float>(value))));
  }
}

void PublicationWrapper::set(v8::Local<v8::Name> key,
                             v8::Local<v8::Value> value,
                             const v8::PropertyCallbackInfo<v8::Value>& info) {
  std::string c_key = *v8::String::Utf8Value(info.GetIsolate(), key);
  auto pub_ptr = publication_from_context(info);
  add_value_to_publication(info.GetIsolate(), pub_ptr, c_key, value);
  info.GetReturnValue().Set(value);
}

void PublicationWrapper::del(
    v8::Local<v8::Name> key,
    const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  auto pub_ptr = publication_from_context(info);
  std::string c_key = *v8::String::Utf8Value(info.GetIsolate(), key);
  pub_ptr->erase_attr(c_key);
  v8::Local<v8::Boolean> v8_true = v8::Boolean::New(info.GetIsolate(), true);
  info.GetReturnValue().Set(v8_true);
}

void PublicationWrapper::enumerate(
    const v8::PropertyCallbackInfo<v8::Array>& info) {
  auto pub_ptr = publication_from_context(info);
  auto arr = v8::Array::New(info.GetIsolate(), pub_ptr->size());
  size_t index = 0;
  for (const auto& [key, _value] : *pub_ptr) {
    v8::Local<v8::String> v8_string;
    if (!v8::String::NewFromUtf8(info.GetIsolate(), key.c_str(),
                                 v8::NewStringType::kNormal, key.size())
             .ToLocal(&v8_string)) {
      Support::Logger::warning("PUBLICATION-WRAPPER",
                               "Failed creating a V8 string (length).");
      continue;
    }
    if (arr->Set(info.GetIsolate()->GetCurrentContext(), index++, v8_string)
            .IsNothing()) {
      Support::Logger::warning("PUBLICATION-WRAPPER",
                               "Failed adding key to enumeration array.");
      continue;
    }
  }
  info.GetReturnValue().Set(arr);
}

void PublicationWrapper::initialize(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope scope(isolate);

  PubSub::Publication pub;

  if (info.Length() == 1 && info[0]->IsObject()) {
    v8::Local<v8::Object> input_object = v8::Local<v8::Object>::Cast(info[0]);
    // This appears to be the way to enumerate an Object (but might be slow):
    // https://groups.google.com/g/v8-users/c/MYBWMsUS5F0
    v8::Local<v8::Array> keys;
    if (input_object
            ->GetOwnPropertyNames(isolate->GetCurrentContext(),
                                  v8::PropertyFilter::ALL_PROPERTIES,
                                  v8::KeyConversionMode::kNoNumbers)
            .ToLocal(&keys)) {
      for (size_t i = 0; i < keys->Length(); i++) {
        v8::Local<v8::Value> key;
        if (keys->Get(isolate->GetCurrentContext(), i).ToLocal(&key)) {
          auto key_str = v8::Local<v8::String>::Cast(key);
          v8::Local<v8::Value> value;
          if (input_object->Get(isolate->GetCurrentContext(), key_str)
                  .ToLocal(&value)) {
            std::string c_key = *v8::String::Utf8Value(isolate, key_str);
            add_value_to_publication(isolate, &pub, c_key, value);
          } else {
            Support::Logger::info("PUBLICATION-WRAPPER",
                                  "Initialize skipped key.");
          }
        } else {
          Support::Logger::info("PUBLICATION-WRAPPER",
                                "Initialize skipped key.");
        }
      }
    } else {
      Support::Logger::info("PUBLICATION-WRAPPER",
                            "Initialize called with wrong argument.");
    }
  } else {
    Support::Logger::info("PUBLICATION-WRAPPER",
                          "Initialize called with wrong argument.");
  }

  auto pub_obj = wrap_publication(isolate, std::move(pub));
  info.GetReturnValue().Set(pub_obj);
}

v8::Local<v8::Object> PublicationWrapper::wrap_publication(
    v8::Isolate* isolate, PubSub::Publication&& publication) {
  v8::EscapableHandleScope handle_scope(isolate);

  v8::Local<v8::ObjectTemplate> publication_template =
      v8::ObjectTemplate::New(isolate);
  publication_template->SetInternalFieldCount(1);
  publication_template->SetHandler(
      v8::NamedPropertyHandlerConfiguration(get, set, nullptr, del, enumerate));

  return handle_scope.Escape(V8Runtime::GCWrapper<PubSub::Publication>::gc_wrap(
      isolate, std::move(publication), publication_template));
}
}  // namespace uActor::ActorRuntime::V8Runtime
#endif
