#pragma once

#include <v8.h>

#include <cassert>
#include <cmath>
#include <utility>

namespace uActor::ActorRuntime::V8Runtime {

// We need to let V8 decide on the lifetime of certain C++ data structures, e.g.
// Publications. According to https://stackoverflow.com/a/176380, this is done
// using weak v8::Persistent references. v8 Externals do not support move
// semantics, so this helper needs to do some manual memory management. This
// helper is based on
// https://groups.google.com/g/v8-users/c/nVJF0SsdvhM/m/7BE5ddw1BQAJ.
template <typename T>
struct GCWrapper {
  static v8::Local<v8::Object> gc_wrap(
      v8::Isolate* isolate, T&& to_wrap,
      const v8::Local<v8::ObjectTemplate>& object_template) {
    v8::EscapableHandleScope handle_scope(isolate);
    assert(object_template->InternalFieldCount() == 1);

    // This pointer is freed by the collect callback
    auto container = new GCWrapper();
    container->item = std::move(to_wrap);

    v8::Local<v8::Object> local_handle =
        object_template->NewInstance(isolate->GetCurrentContext())
            .ToLocalChecked();
    local_handle->SetInternalField(
        0, v8::External::New(isolate, &(container->item)));

    container->handle.Reset(isolate,
                            v8::Global<v8::Object>(isolate, local_handle));
    container->handle.SetWeak(container, collect,
                              v8::WeakCallbackType::kParameter);

    return handle_scope.Escape(local_handle);
  }

  static void collect(const v8::WeakCallbackInfo<GCWrapper>& info) {
    if (!info.GetParameter()->handle.IsEmpty()) {
      info.GetParameter()->handle.ClearWeak();
      info.GetParameter()->handle.Reset();
    }
    delete info.GetParameter();  // this
  }

  T item;
  v8::Global<v8::Object> handle;
};

}  // namespace uActor::ActorRuntime::V8Runtime
