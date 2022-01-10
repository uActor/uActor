#if CONFIG_UACTOR_V8
#include "actor_runtime/v8/publication_wrapper.hpp"

#include "actor_runtime/v8/gc_wrapper.hpp"
#include "actor_runtime/v8/utils.hpp"
#include "support/logger.hpp"

namespace uActor::ActorRuntime::V8Runtime {

void PublicationBaseWrapper::add_value_to_map(
    v8::Isolate* isolate, PubSub::Publication::Map* publication,
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
  } else if (value->IsObject()) {
    v8::Local<v8::Object> input_object = v8::Local<v8::Object>::Cast(value);
    auto new_map = std::make_shared<PubSub::Publication::Map>();
    parse_map_object(isolate, input_object, &*new_map);
    publication->set_attr(key, new_map);
  }
}

void PublicationBaseWrapper::parse_map_object(
    v8::Isolate* isolate, v8::Local<v8::Object> input_object,
    PubSub::Publication::Map* publication) {
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
          add_value_to_map(isolate, publication, c_key, value);
        } else {
          Support::Logger::info("PUBLICATION-WRAPPER",
                                "Initialize skipped key.");
        }
      } else {
        Support::Logger::info("PUBLICATION-WRAPPER", "Initialize skipped key.");
      }
    }
  } else {
    Support::Logger::info("PUBLICATION-WRAPPER",
                          "Initialize called with wrong argument.");
  }
}

v8::Local<v8::Value> PublicationBaseWrapper::get_base(
    v8::Isolate* isolate, PubSub::Publication::Map* item,
    std::string_view key) {
  v8::EscapableHandleScope handle_scope(isolate);

  if (!item->has_attr(key)) {
    return handle_scope.Escape(v8::Null(isolate));
  }
  auto value = item->get_attr(key);
  if (std::holds_alternative<std::monostate>(value)) {
    return handle_scope.Escape(v8::Null(isolate));
  } else if (std::holds_alternative<std::string_view>(value)) {
    auto str_val = std::get<std::string_view>(value);
    v8::Local<v8::String> v8_string;
    if (!v8::String::NewFromUtf8(isolate, str_val.data(),
                                 v8::NewStringType::kNormal, str_val.size())
             .ToLocal(&v8_string)) {
      Support::Logger::warning("PUBLICATION-WRAPPER",
                               "Failed creating a V8 string (length).");
      return handle_scope.Escape(v8::Null(isolate));
    }

    return handle_scope.Escape(v8_string);
    // TODO(raphaelhetzel) Check if we can use a better type here
  } else if (std::holds_alternative<int32_t>(value)) {
    return handle_scope.Escape(v8::Number::New(
        isolate, static_cast<double>(std::get<int32_t>(value))));
  } else if (std::holds_alternative<float>(value)) {
    return handle_scope.Escape(
        v8::Number::New(isolate, static_cast<double>(std::get<float>(value))));
  } else if (std::holds_alternative<std::shared_ptr<PubSub::Publication::Map>>(
                 value)) {
    auto sp = std::get<std::shared_ptr<PubSub::Publication::Map>>(value);
    auto ret = PublicationMapWrapper::wrap_map_handle(isolate, std::move(sp));
    return handle_scope.Escape(ret);
  }
  return handle_scope.Escape(v8::Null(isolate));
}

v8::Local<v8::Array> PublicationBaseWrapper::enumerate_base(
    v8::Isolate* isolate, PubSub::Publication::Map* data) {
  v8::EscapableHandleScope handle_scope(isolate);

  auto arr = v8::Array::New(isolate, data->size());
  size_t index = 0;
  for (const auto& [key, _value] : *data) {
    v8::Local<v8::String> v8_string;
    if (!v8::String::NewFromUtf8(isolate, key.c_str(),
                                 v8::NewStringType::kNormal, key.size())
             .ToLocal(&v8_string)) {
      Support::Logger::warning("PUBLICATION-WRAPPER",
                               "Failed creating a V8 string (length).");
      continue;
    }
    if (arr->Set(isolate->GetCurrentContext(), index++, v8_string)
            .IsNothing()) {
      Support::Logger::warning("PUBLICATION-WRAPPER",
                               "Failed adding key to enumeration array.");
      continue;
    }
  }
  return handle_scope.Escape(arr);
}

void PublicationWrapper::get(v8::Local<v8::Name> key,
                             const v8::PropertyCallbackInfo<v8::Value>& info) {
  std::string c_key = *v8::String::Utf8Value(info.GetIsolate(), key);
  auto pub_ptr = publication_from_context(info);
  info.GetReturnValue().Set(
      get_base(info.GetIsolate(), pub_ptr->root_ptr(), c_key));
}

void PublicationWrapper::set(v8::Local<v8::Name> key,
                             v8::Local<v8::Value> value,
                             const v8::PropertyCallbackInfo<v8::Value>& info) {
  std::string c_key = *v8::String::Utf8Value(info.GetIsolate(), key);
  auto pub_ptr = publication_from_context(info);
  add_value_to_map(info.GetIsolate(), pub_ptr->root_ptr(), c_key, value);
  info.GetReturnValue().Set(value);
}

void PublicationWrapper::del(
    v8::Local<v8::Name> key,
    const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  auto pub_ptr = publication_from_context(info);
  std::string c_key = *v8::String::Utf8Value(info.GetIsolate(), key);
  pub_ptr->erase_attr(c_key);
  info.GetReturnValue().Set(v8::Boolean::New(info.GetIsolate(), true));
}

void PublicationWrapper::enumerate(
    const v8::PropertyCallbackInfo<v8::Array>& info) {
  auto pub_ptr = publication_from_context(info);
  auto keys = enumerate_base(info.GetIsolate(), pub_ptr->root_ptr());
  info.GetReturnValue().Set(keys);
}

void PublicationWrapper::initialize(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope scope(isolate);

  PubSub::Publication pub;

  if (info.Length() == 1 && info[0]->IsObject()) {
    v8::Local<v8::Object> input_object = v8::Local<v8::Object>::Cast(info[0]);
    parse_map_object(isolate, input_object, pub.root_ptr());
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

void PublicationMapWrapper::get(
    v8::Local<v8::Name> key, const v8::PropertyCallbackInfo<v8::Value>& info) {
  std::string c_key = *v8::String::Utf8Value(info.GetIsolate(), key);
  auto pub_ptr = publication_map_from_context(info);
  info.GetReturnValue().Set(PublicationWrapper::get_base(
      info.GetIsolate(), pub_ptr->data.get(), c_key));
}

void PublicationMapWrapper::set(
    v8::Local<v8::Name> key, v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  std::string c_key = *v8::String::Utf8Value(info.GetIsolate(), key);
  auto map_ptr = publication_map_from_context(info);
  add_value_to_map(info.GetIsolate(), map_ptr->data.get(), c_key, value);
  info.GetReturnValue().Set(value);
}

void PublicationMapWrapper::del(
    v8::Local<v8::Name> key,
    const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  auto pub_ptr = publication_map_from_context(info);
  std::string c_key = *v8::String::Utf8Value(info.GetIsolate(), key);
  pub_ptr->data->erase_attr(c_key);
  info.GetReturnValue().Set(v8::Boolean::New(info.GetIsolate(), true));
}

void PublicationMapWrapper::enumerate(
    const v8::PropertyCallbackInfo<v8::Array>& info) {
  auto pub_map = publication_map_from_context(info);
  auto keys = PublicationWrapper::enumerate_base(info.GetIsolate(),
                                                 pub_map->data.get());
  info.GetReturnValue().Set(keys);
}

v8::Local<v8::Object> PublicationMapWrapper::wrap_map_handle(
    v8::Isolate* isolate, std::shared_ptr<PubSub::Publication::Map>&& wrapper) {
  v8::EscapableHandleScope handle_scope(isolate);

  v8::Local<v8::ObjectTemplate> publication_map_wrapper_template =
      v8::ObjectTemplate::New(isolate);
  publication_map_wrapper_template->SetInternalFieldCount(1);
  publication_map_wrapper_template->SetHandler(
      v8::NamedPropertyHandlerConfiguration(get, set, nullptr, del, enumerate));

  return handle_scope.Escape(
      V8Runtime::GCWrapper<PublicationMapWrapper>::gc_wrap(
          isolate, PublicationMapWrapper(std::move(wrapper)),
          publication_map_wrapper_template));
}

}  // namespace uActor::ActorRuntime::V8Runtime
#endif
