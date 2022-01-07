#if CONFIG_UACTOR_V8
#include "actor_runtime/v8/managed_actor.hpp"

#include "support/logger.hpp"

namespace uActor::ActorRuntime::V8Runtime {

ManagedV8Actor::~ManagedV8Actor() {
  if (isolate != nullptr) {
    actor_state.Reset();
    receive_function.Reset();
    actor_context.Reset();
    isolate->Dispose();
  }
  isolate = nullptr;
}

ManagedActor::RuntimeReturnValue ManagedV8Actor::receive(
    PubSub::Publication&& m) {
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, actor_context);
  v8::Context::Scope context_scope(context);
  v8::TryCatch try_catch(isolate);

  auto pub_obj = PublicationWrapper::wrap_publication(isolate, std::move(m));

  const size_t argc = 2;
  v8::Local<v8::Value> argv[argc] = {
      pub_obj, v8::Local<v8::Object>::New(isolate, actor_state)};

  // Like in the the process.cc example, we store a reference to the receive
  // function instead of querying it. This is different from the Lua version in
  // which we query the "receive" object on each invocation. Dynamic querying of
  // the name would enable users to overwrite this (assuming no hibernation).
  v8::Local<v8::Function> rec_fun =
      v8::Local<v8::Function>::New(isolate, receive_function);

  if (rec_fun->Call(context, context->Global(), argc, argv).IsEmpty()) {
    v8::String::Utf8Value error(isolate, try_catch.Exception());
    Support::Logger::info("MANAGED-V8-ACTOR", "Execution Error: %s\n", *error);
  }

  // https://stackoverflow.com/questions/50115031/does-v8-have-an-event-loop
  // https://stackoverflow.com/questions/49811043/relationship-between-event-loop-libuv-and-v8-engine
  while (v8::platform::PumpMessageLoop(platform, isolate)) {
    continue;
  }

  // Force GC: https://stackoverflow.com/a/62542952
  // isolate->LowMemoryNotification();
  // Alternative: RequestGarbageCollectionForTesting + Flag

  return RuntimeReturnValue::OK;
}

ManagedActor::RuntimeReturnValue ManagedV8Actor::early_internal_initialize() {
  return attempt_direct_init_or_fetch_code();
}

ManagedActor::RuntimeReturnValue ManagedV8Actor::late_internal_initialize(
    std::string&& code) {
  return createActorEnvironment(std::move(code));
}

ManagedActor::RuntimeReturnValue
ManagedV8Actor::attempt_direct_init_or_fetch_code() {
  auto direct_fetch =
      ActorRuntime::CodeStore::get_instance().retrieve(CodeIdentifier(
          actor_type(), actor_version(), std::string_view("javascript")));
  if (direct_fetch) {
    return createActorEnvironment(direct_fetch->code);
  } else {
    trigger_code_fetch();
    return RuntimeReturnValue::NOT_READY;
  }
}

ManagedActor::RuntimeReturnValue ManagedV8Actor::createActorEnvironment(
    std::string_view code) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  isolate = v8::Isolate::New(create_params);
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context =
      v8::Context::New(isolate, NULL, actor_context_template());
  context->Global()->SetInternalField(0, v8::External::New(isolate, this));
  v8::Context::Scope context_scope(context);

  actor_context.Reset(isolate, context);

  load_code(context, code);

  actor_state.Reset(isolate, v8::Object::New(isolate));

  return RuntimeReturnValue::OK;
}

v8::Local<v8::ObjectTemplate> ManagedV8Actor::actor_context_template() {
  // this is an internal method and we expect that the caller sets all other
  // scopes
  v8::EscapableHandleScope handle_scope(isolate);

  v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
  global->SetInternalFieldCount(1);

  global->Set(isolate, "log",
              v8::FunctionTemplate::New(isolate, ActorClosures::log));

  global->Set(isolate, "publish",
              v8::FunctionTemplate::New(isolate, ActorClosures::publish));
  global->Set(
      isolate, "enqueue_wakeup",
      v8::FunctionTemplate::New(isolate, ActorClosures::enqueue_wakeup));
  global->Set(isolate, "forward",
              v8::FunctionTemplate::New(isolate, ActorClosures::fwd));

  global->Set(isolate, "reply",
              v8::FunctionTemplate::New(isolate, ActorClosures::reply));

  global->Set(
      isolate, "Message",
      v8::FunctionTemplate::New(isolate, PublicationWrapper::initialize));

  global->Set(isolate, "subscribe",
              v8::FunctionTemplate::New(isolate, ActorClosures::subscribe));
  global->Set(isolate, "unsubscribe",
              v8::FunctionTemplate::New(isolate, ActorClosures::unsubscribe));
  global->Set(isolate, "deferred_block_for",
              v8::FunctionTemplate::New(isolate, ActorClosures::block_for));

  global->Set(isolate, "encode_base64",
              v8::FunctionTemplate::New(
                  isolate, V8Runtime::ActorFunctions::encode_base64));
  global->Set(isolate, "decode_base64",
              v8::FunctionTemplate::New(
                  isolate, V8Runtime::ActorFunctions::decode_base64));
  global->Set(
      isolate, "code_hash",
      v8::FunctionTemplate::New(isolate, V8Runtime::ActorFunctions::code_hash));

  global->Set(isolate, "node_id",
              v8::FunctionTemplate::New(isolate, ActorClosures::node_id));
  global->Set(isolate, "actor_type",
              v8::FunctionTemplate::New(isolate, ActorClosures::actor_type));
  global->Set(isolate, "instance_id",
              v8::FunctionTemplate::New(isolate, ActorClosures::instance_id));

  v8::Local<v8::ObjectTemplate> constraint_predicates =
      v8::ObjectTemplate::New(isolate);
  for (uint32_t i = 1; i <= PubSub::ConstraintPredicates::MAX_INDEX; i++) {
    constraint_predicates->Set(isolate, PubSub::ConstraintPredicates::name(i),
                               v8::Uint32::NewFromUnsigned(isolate, i));
  }
  global->Set(isolate, "ConstraintPredicate", constraint_predicates);

  v8::Local<v8::ObjectTemplate> log_levels = v8::ObjectTemplate::New(isolate);
  for (auto valid_level : Support::Logger::valid_levels) {
    log_levels->Set(
        isolate, valid_level.data(),
        v8::String::NewFromUtf8(isolate, valid_level.data()).ToLocalChecked());
  }
  global->Set(isolate, "LogLevel", log_levels);

  return handle_scope.Escape(global);
}

ManagedActor::RuntimeReturnValue ManagedV8Actor::load_code(
    const v8::Local<v8::Context>& context, std::string_view code) {
  // this is an internal method and we expect that the caller sets all other
  // scopes
  v8::HandleScope handle_scope(isolate);
  v8::TryCatch try_catch(isolate);

  v8::Local<v8::String> source;
  if (!v8::String::NewFromUtf8(isolate, code.data(), v8::NewStringType::kNormal,
                               code.size())
           .ToLocal(&source)) {
    Support::Logger::warning(
        "MANAGED-V8-ACTOR",
        "Load failed: Code's length exceeds V8's maximum length.\n");
    return RuntimeReturnValue::INITIALIZATION_ERROR;
  }

  v8::Local<v8::Script> compiled_script;
  if (!v8::Script::Compile(context, source).ToLocal(&compiled_script)) {
    v8::String::Utf8Value error(isolate, try_catch.Exception());
    Support::Logger::warning("MANAGED-V8-ACTOR",
                             "Load failed: Compiler Error: %s\n", *error);
    return RuntimeReturnValue::INITIALIZATION_ERROR;
  }

  if (compiled_script->Run(context).IsEmpty()) {
    v8::String::Utf8Value error(isolate, try_catch.Exception());
    Support::Logger::warning("MANAGED-V8-ACTOR",
                             "Load failed: Execution Error: %s\n", *error);
    return RuntimeReturnValue::INITIALIZATION_ERROR;
  }

  // TODO(raphaelhetzel) We could use the return value, an export, or
  // EventListeners (used by Cloudflare Workers) instead of the default
  // function.
  v8::Local<v8::Value> local_receive_function;
  if (!context->Global()
           ->Get(context, v8::String::NewFromUtf8Literal(isolate, "receive"))
           .ToLocal(&local_receive_function) ||
      !local_receive_function->IsFunction()) {
    Support::Logger::warning("MANAGED-V8-ACTOR",
                             "Load failed: no receive function\n");
    return RuntimeReturnValue::INITIALIZATION_ERROR;
  }

  v8::Local<v8::Function> rec_fun = local_receive_function.As<v8::Function>();
  receive_function.Reset(isolate, rec_fun);

  return RuntimeReturnValue::OK;
}

}  // namespace uActor::ActorRuntime::V8Runtime
#endif
