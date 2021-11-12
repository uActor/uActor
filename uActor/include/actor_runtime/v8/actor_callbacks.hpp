#pragma once

#include <v8.h>

#include <tuple>

namespace uActor::ActorRuntime::V8Runtime {

class ManagedV8Actor;

class ActorFunctions {
 public:
  static void encode_base64(const v8::FunctionCallbackInfo<v8::Value>& info);
  static void decode_base64(const v8::FunctionCallbackInfo<v8::Value>& info);

  static void code_hash(const v8::FunctionCallbackInfo<v8::Value>& info);
};

class ActorClosures {
 public:
  static void node_id(const v8::FunctionCallbackInfo<v8::Value>& info);

  static void actor_type(const v8::FunctionCallbackInfo<v8::Value>& info);

  static void instance_id(const v8::FunctionCallbackInfo<v8::Value>& info);

  static void publish(const v8::FunctionCallbackInfo<v8::Value>& info);

  static void fwd(const v8::FunctionCallbackInfo<v8::Value>& info);

  static void subscribe(const v8::FunctionCallbackInfo<v8::Value>& info);

  static void block_for(const v8::FunctionCallbackInfo<v8::Value>& info);

  static void unsubscribe(const v8::FunctionCallbackInfo<v8::Value>& info);

  static void enqueue_wakeup(const v8::FunctionCallbackInfo<v8::Value>& info);

  static void log(const v8::FunctionCallbackInfo<v8::Value>& info);

 private:
  static std::tuple<v8::Isolate*, ManagedV8Actor*> closure_preamble(
      const v8::FunctionCallbackInfo<v8::Value>& info);
};

}  // namespace uActor::ActorRuntime::V8Runtime
