#ifndef UACTOR_INCLUDE_ACTOR_RUNTIME_MANAGED_LUA_ACTOR_HPP_
#define UACTOR_INCLUDE_ACTOR_RUNTIME_MANAGED_LUA_ACTOR_HPP_

#ifdef ESP_IDF
#include <sdkconfig.h>
#endif
#if CONFIG_BENCHMARK_ENABLED
#include <support/testbed.h>
#endif

#include <cstdio>
#include <list>
#include <string>
#include <utility>

#include "lua.hpp"
#include "managed_actor.hpp"
#include "pubsub/router.hpp"

namespace uActor::ActorRuntime {

class ManagedLuaActor : public ManagedActor {
 public:
  ManagedLuaActor(ExecutorApi* api, uint32_t unique_id, const char* node_id,
                  const char* actor_type, const char* instance_id,
                  const char* code, lua_State* global_state)
      : ManagedActor(api, unique_id, node_id, actor_type, instance_id, code),
        state(global_state) {}

  ~ManagedLuaActor();

  bool receive(const PubSub::Publication& m);

 protected:
  bool internal_initialize() { return createActorEnvironment(code()); }

 private:
  lua_State* state;

  static int publish_wrapper(lua_State* state);

  static int delayed_publish_wrapper(lua_State* state);

  static int deferred_block_for_wrapper(lua_State* state);

  static int subscribe_wrapper(lua_State* state);

  static int unsubscribe_wrapper(lua_State* state);

  static int now_wrapper(lua_State* state);

#if CONFIG_BENCHMARK_ENABLED
  static int testbed_log_integer_wrapper(lua_State* state);

  static int testbed_log_double_wrapper(lua_State* state);

  static int testbed_log_string_wrapper(lua_State* state);

  static int testbed_start_timekeeping_wrapper(lua_State* state);

  static int testbed_stop_timekeeping_wrapper(lua_State* state);

  static int testbed_stop_timekeeping_inner_wrapper(lua_State* state);

#endif

  bool createActorEnvironment(const char* receive_function);

  static PubSub::Filter parse_filters(lua_State* state, size_t index);

  static PubSub::Publication parse_publication(ManagedLuaActor* actor,
                                               lua_State* state, size_t index);

  static luaL_Reg actor_core[];
};

}  //  namespace uActor::ActorRuntime

#endif  // UACTOR_INCLUDE_ACTOR_RUNTIME_MANAGED_LUA_ACTOR_HPP_
