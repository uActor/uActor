#pragma once

#include <utility>
#include <memory>

#include <v8.h>

#include "pubsub/publication.hpp"

namespace uActor::ActorRuntime::V8Runtime {

class PublicationBaseWrapper {
 protected:
  static void add_value_to_map(v8::Isolate* isolate,
                               PubSub::Publication::Map* publication,
                               std::string_view key,
                               v8::Local<v8::Value> value);

  static void parse_map_object(v8::Isolate* isolate,
                               v8::Local<v8::Object> input_object,
                               PubSub::Publication::Map* publication);

  static v8::Local<v8::Value> get_base(v8::Isolate* isolate,
                                       PubSub::Publication::Map* item,
                                       std::string_view key);

  static v8::Local<v8::Array> enumerate_base(v8::Isolate* isolate,
                                             PubSub::Publication::Map* data);
};

class PublicationWrapper : public PublicationBaseWrapper {
 public:
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

 private:
  template <typename T>
  static PubSub::Publication* publication_from_context(
      const v8::PropertyCallbackInfo<T>& info) {
    v8::Local<v8::External> ext =
        v8::Local<v8::External>::Cast(info.Holder()->GetInternalField(0));
    void* ptr = ext->Value();
    return reinterpret_cast<PubSub::Publication*>(ptr);
  }
};

class PublicationMapWrapper : public PublicationBaseWrapper {
 public:
  std::shared_ptr<PubSub::Publication::Map> data;

  PublicationMapWrapper() = delete;

  explicit PublicationMapWrapper(std::shared_ptr<PubSub::Publication::Map> data)
      : data(std::move(data)) {}

  ~PublicationMapWrapper() = default;

  static void get(v8::Local<v8::Name> key,
                  const v8::PropertyCallbackInfo<v8::Value>& info);

  static void set(v8::Local<v8::Name> key, v8::Local<v8::Value> value,
                  const v8::PropertyCallbackInfo<v8::Value>& info);

  static void del(v8::Local<v8::Name> key,
                  const v8::PropertyCallbackInfo<v8::Boolean>& info);

  static void enumerate(const v8::PropertyCallbackInfo<v8::Array>& info);

  static v8::Local<v8::Object> wrap_map_handle(
      v8::Isolate* isolate,
      std::shared_ptr<PubSub::Publication::Map>&& wrapper);

 private:
  template <typename T>
  static PublicationMapWrapper* publication_map_from_context(
      const v8::PropertyCallbackInfo<T>& info) {
    v8::Local<v8::External> ext =
        v8::Local<v8::External>::Cast(info.Holder()->GetInternalField(0));
    void* ptr = ext->Value();
    return reinterpret_cast<PublicationMapWrapper*>(ptr);
  }
};

}  // namespace uActor::ActorRuntime::V8Runtime
