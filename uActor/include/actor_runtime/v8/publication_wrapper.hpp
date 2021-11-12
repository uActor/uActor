#pragma once

#include <v8.h>

#include "pubsub/publication.hpp"

namespace uActor::ActorRuntime::V8Runtime {

struct PublicationWrapper {
  template <typename T>
  static PubSub::Publication* publication_from_context(
      const v8::PropertyCallbackInfo<T>& info) {
    v8::Local<v8::External> ext =
        v8::Local<v8::External>::Cast(info.Holder()->GetInternalField(0));
    void* ptr = ext->Value();
    return reinterpret_cast<PubSub::Publication*>(ptr);
  }

  static void add_value_to_publication(v8::Isolate* isolate,
                                       PubSub::Publication* publication,
                                       std::string_view key,
                                       v8::Local<v8::Value> value);

  static void get(v8::Local<v8::Name> key,
                  const v8::PropertyCallbackInfo<v8::Value>& info);

  static void set(v8::Local<v8::Name> key, v8::Local<v8::Value> value,
                  const v8::PropertyCallbackInfo<v8::Value>& info);

  static void del(v8::Local<v8::Name> key,
                  const v8::PropertyCallbackInfo<v8::Boolean>& info);

  static void enumerate(const v8::PropertyCallbackInfo<v8::Array>& info);

  static void initialize(const v8::FunctionCallbackInfo<v8::Value>& info);

  static v8::Local<v8::Object> wrap_publication(
      v8::Isolate* isolate, PubSub::Publication&& publication);
};
}  // namespace uActor::ActorRuntime::V8Runtime
