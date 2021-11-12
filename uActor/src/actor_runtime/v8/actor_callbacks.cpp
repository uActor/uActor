#if CONFIG_UACTOR_V8
#include "actor_runtime/v8/actor_callbacks.hpp"

#include <base64.h>
#include <blake2.h>

#include "actor_runtime/v8/managed_actor.hpp"
#include "actor_runtime/v8/utils.hpp"

namespace uActor::ActorRuntime::V8Runtime {

// Functions

void ActorFunctions::encode_base64(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope scope(isolate);
  if (info.Holder()->InternalFieldCount() != 1) {
    return;
  }

  if (info.Length() != 1 || !info[0]->IsString()) {
    return;
  }

  // TODO(raphaelhetzel) check if the Utf8 string covers all relevant cases
  auto raw = v8::String::Utf8Value(isolate, info[0]);

  std::string encoded = base64_encode(std::string(*raw, raw.length()), false);

  v8::Local<v8::String> v8_return;
  if (v8::String::NewFromUtf8(isolate, encoded.c_str()).ToLocal(&v8_return)) {
    info.GetReturnValue().Set(v8_return);
  }
}

void ActorFunctions::decode_base64(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope scope(isolate);
  if (info.Holder()->InternalFieldCount() != 1) {
    return;
  }

  if (info.Length() != 1 || !info[0]->IsString()) {
    return;
  }

  auto raw = v8::String::Utf8Value(isolate, info[0]);
  std::string decoded = base64_decode(std::string(*raw, raw.length()));

  v8::Local<v8::String> v8_return;
  if (v8::String::NewFromUtf8(isolate, decoded.c_str()).ToLocal(&v8_return)) {
    info.GetReturnValue().Set(v8_return);
  }
}

void ActorFunctions::code_hash(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope scope(isolate);
  if (info.Holder()->InternalFieldCount() != 1) {
    return;
  }

  if (info.Length() != 1 || !info[0]->IsString()) {
    return;
  }

  std::string code = *v8::String::Utf8Value(isolate, info[0]);
  std::array<char, 32> out_buffer;

  blake2s(out_buffer.data(), 32, code.data(), code.length(), code.data(), 0);

  std::string out =
      base64_encode(std::string_view(out_buffer.data(), out_buffer.size()))
          .data();

  v8::Local<v8::String> v8_return;
  if (v8::String::NewFromUtf8(isolate, out.c_str()).ToLocal(&v8_return)) {
    info.GetReturnValue().Set(v8_return);
  }
}

// Closures

std::tuple<v8::Isolate*, ManagedV8Actor*> ActorClosures::closure_preamble(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope scope(isolate);
  if (info.Holder()->InternalFieldCount() != 1) {
    exit(1);
  }
  ManagedV8Actor* actor = reinterpret_cast<ManagedV8Actor*>(
      v8::Local<v8::External>::Cast(info.Holder()->GetInternalField(0))
          ->Value());
  return std::make_tuple(isolate, actor);
}

void ActorClosures::node_id(const v8::FunctionCallbackInfo<v8::Value>& info) {
  const auto [isolate, actor] = closure_preamble(info);
  v8::HandleScope scope(isolate);

  v8::Local<v8::String> v8_return;
  if (v8::String::NewFromUtf8(isolate, actor->node_id()).ToLocal(&v8_return)) {
    info.GetReturnValue().Set(v8_return);
  }
}

void ActorClosures::actor_type(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  const auto [isolate, actor] = closure_preamble(info);
  v8::HandleScope scope(isolate);

  v8::Local<v8::String> v8_return;
  if (v8::String::NewFromUtf8(isolate, actor->actor_type())
          .ToLocal(&v8_return)) {
    info.GetReturnValue().Set(v8_return);
  }
}

void ActorClosures::instance_id(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  const auto [isolate, actor] = closure_preamble(info);
  v8::HandleScope scope(isolate);

  v8::Local<v8::String> v8_return;
  if (v8::String::NewFromUtf8(isolate, actor->instance_id())
          .ToLocal(&v8_return)) {
    info.GetReturnValue().Set(v8_return);
  }
}

void ActorClosures::publish(const v8::FunctionCallbackInfo<v8::Value>& info) {
  const auto [isolate, actor] = closure_preamble(info);
  v8::HandleScope scope(isolate);

  if (info.Length() != 1 || !info[0]->IsObject()) {
    return;
  }
  auto message_object = v8::Local<v8::Object>::Cast(info[0]);
  if (message_object->InternalFieldCount() != 1) {
    return;
  }
  // TODO(raphaelhetzel) Ensure that the external is of the expected type.
  v8::Local<v8::External> ext_publication =
      v8::Local<v8::External>::Cast(message_object->GetInternalField(0));

  actor->publish(std::move(
      *reinterpret_cast<PubSub::Publication*>(ext_publication->Value())));
}

void ActorClosures::fwd(const v8::FunctionCallbackInfo<v8::Value>& info) {
  const auto [isolate, actor] = closure_preamble(info);
  v8::HandleScope scope(isolate);

  if (info.Length() != 1 || !info[0]->IsObject()) {
    return;
  }
  auto message_object = v8::Local<v8::Object>::Cast(info[0]);
  if (message_object->InternalFieldCount() != 1) {
    return;
  }
  v8::Local<v8::External> ext_publication =
      v8::Local<v8::External>::Cast(message_object->GetInternalField(0));

  actor->republish(std::move(
      *reinterpret_cast<PubSub::Publication*>(ext_publication->Value())));
}

void ActorClosures::subscribe(const v8::FunctionCallbackInfo<v8::Value>& info) {
  const auto [isolate, actor] = closure_preamble(info);
  v8::HandleScope scope(isolate);

  if (info.Length() == 1 && info[0]->IsArray()) {
    auto f = V8Runtime::TypeUtils::parse_filter(
        info.GetIsolate(), v8::Local<v8::Array>::Cast(info[0]));
    if (!f) {
      info.GetReturnValue().Set(v8::Number::New(info.GetIsolate(), 0));
      return;
    }
    auto sid = actor->subscribe(std::move(*f), 0);
    info.GetReturnValue().Set(
        v8::Number::New(info.GetIsolate(), static_cast<double>(sid)));
  } else {
    info.GetReturnValue().Set(v8::Number::New(info.GetIsolate(), 0));
  }
}

void ActorClosures::block_for(const v8::FunctionCallbackInfo<v8::Value>& info) {
  const auto [isolate, actor] = closure_preamble(info);
  v8::HandleScope scope(isolate);

  if (info.Length() != 2 || !info[0]->IsArray() || !info[1]->IsUint32()) {
    return;
  }

  auto filter = V8Runtime::TypeUtils::parse_filter(
      info.GetIsolate(), v8::Local<v8::Array>::Cast(info[0]));
  if (!filter) {
    return;
  }
  uint32_t timeout;
  if (info[1]->Uint32Value(isolate->GetCurrentContext()).To(&timeout)) {
    actor->deffered_block_for(std::move(*filter), timeout);
  }
}

void ActorClosures::unsubscribe(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  const auto [isolate, actor] = closure_preamble(info);
  v8::HandleScope scope(isolate);

  if (info.Length() != 1 || !info[0]->IsUint32()) {
    return;
  }

  uint32_t sub_id;
  if (info[0]->Uint32Value(isolate->GetCurrentContext()).To(&sub_id)) {
    actor->unsubscribe(sub_id);
  }
}

void ActorClosures::enqueue_wakeup(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  const auto [isolate, actor] = closure_preamble(info);
  v8::HandleScope scope(isolate);

  if (info.Length() != 2 || !info[0]->IsUint32() || !info[1]->IsString()) {
    return;
  }

  uint32_t delay;
  if (info[0]->Uint32Value(isolate->GetCurrentContext()).To(&delay)) {
    std::string wakeup_id = *v8::String::Utf8Value(isolate, info[1]);
    actor->enqueue_wakeup(delay, wakeup_id);
  }
}

void ActorClosures::log(const v8::FunctionCallbackInfo<v8::Value>& info) {
  const auto [isolate, actor] = closure_preamble(info);
  v8::HandleScope scope(isolate);

  if (info.Length() < 1) {
    return;
  }

  std::string level = "INFO";
  std::string data = "";

  if (info.Length() == 2) {
    // TODO(raphaelhetzel) there might be types that can be safely converted to
    // the String type
    if (!info[0]->IsString() || !info[1]->IsString()) {
      return;
    }
    std::string level_string(*v8::String::Utf8Value(isolate, info[0]));
    if (Support::Logger::valid_levels.find(level_string) !=
        Support::Logger::valid_levels.end()) {
      level = level_string;
    }
    data = *v8::String::Utf8Value(isolate, info[1]);
  } else {
    if (!info[0]->IsString()) {
      return;
    }
    data = *v8::String::Utf8Value(isolate, info[0]);
  }
  std::string actor_identifier =
      std::string(actor->actor_type()) + "." + actor->instance_id();

  // The message is user defined, this needs to ensure safety:
  // https://owasp.org/www-community/attacks/Format_string_attack
  Support::Logger::log(level, actor_identifier, "%s", data.c_str());
}

}  // namespace uActor::ActorRuntime::V8Runtime
#endif
