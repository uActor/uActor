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
                  const char* actor_type, const char* actor_version,
                  const char* instance_id, lua_State* global_state)
      : ManagedActor(api, unique_id, node_id, actor_type, actor_version,
                     instance_id),
        state(global_state) {}

  ~ManagedLuaActor();

  bool receive(PubSub::Publication&& m) override;

  std::string actor_runtime_type() override { return std::string("lua"); }

 protected:
  bool early_internal_initialize() override {
    trigger_code_fetch();
    return false;
  }

  bool late_internal_initialize(std::string&& code) override {
    return createActorEnvironment(std::move(code));
  }

 private:
  lua_State* state;

  static int publish_wrapper(lua_State* state);

  static int delayed_publish_wrapper(lua_State* state);

  static int deferred_block_for_wrapper(lua_State* state);

  static int subscribe_wrapper(lua_State* state);

  static int unsubscribe_wrapper(lua_State* state);

  static int now_wrapper(lua_State* state);

  static int encode_base64(lua_State* state);

  static int decode_base64(lua_State* state);

  static int unix_timestamp_wrapper(lua_State* state);

  static int calculate_time_diff(lua_State* state);

#if CONFIG_BENCHMARK_ENABLED
  static int testbed_log_integer_wrapper(lua_State* state);

  static int testbed_log_double_wrapper(lua_State* state);

  static int testbed_log_string_wrapper(lua_State* state);

  static int testbed_start_timekeeping_wrapper(lua_State* state);

  static int testbed_stop_timekeeping_wrapper(lua_State* state);

  static int testbed_stop_timekeeping_inner_wrapper(lua_State* state);

#endif

  bool createActorEnvironment(std::string receive_function);

  static PubSub::Filter parse_filters(lua_State* state, size_t index);

  static PubSub::Publication parse_publication(ManagedLuaActor* actor,
                                               lua_State* state, size_t index);

  static luaL_Reg actor_core[];
};

}  //  namespace uActor::ActorRuntime

#endif  // UACTOR_INCLUDE_ACTOR_RUNTIME_MANAGED_LUA_ACTOR_HPP_
