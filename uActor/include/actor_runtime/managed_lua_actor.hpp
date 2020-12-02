#ifndef UACTOR_INCLUDE_ACTOR_RUNTIME_MANAGED_LUA_ACTOR_HPP_
#define UACTOR_INCLUDE_ACTOR_RUNTIME_MANAGED_LUA_ACTOR_HPP_

#ifdef ESP_IDF
#include <sdkconfig.h>
#endif
#if CONFIG_BENCHMARK_ENABLED
#include <support/testbed.h>
#endif

#include <frozen/string.h>
#include <frozen/unordered_map.h>

#include <cstdio>
#include <list>
#include <string>
#include <string_view>
#include <utility>

#include "code_store.hpp"
#include "lua.hpp"
#include "lua_functions.hpp"
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
        state(global_state) {
    lua_newtable(state);
    lua_setglobal(state,
                  (std::string("state_") + std::to_string(id())).c_str());
  }

  ~ManagedLuaActor();

  bool receive(PubSub::Publication&& m) override;

  std::string actor_runtime_type() override { return std::string("lua"); }

 protected:
  bool early_internal_initialize() override {
    // trigger_code_fetch();
    // return false;
    return fetch_code_and_init();
  }

  bool late_internal_initialize(std::string&& code) override {
    return createActorEnvironment(std::move(code));
  }

  bool hibernate_internal() override;

  bool wakeup_internal() override { return fetch_code_and_init(); }

 private:
  lua_State* state;

  bool fetch_code_and_init();

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

  static int connection_traffic(lua_State* state);

#endif

  bool createActorEnvironment(std::string_view receive_function);

  static int actor_index(lua_State* state);

  static PubSub::Filter parse_filters(lua_State* state, size_t index);

  static PubSub::Publication parse_publication(ManagedLuaActor* actor,
                                               lua_State* state, size_t index);

  constexpr static auto closures =
      frozen::make_unordered_map<frozen::string, lua_CFunction>({
        {"publish", &publish_wrapper},
            {"delayed_publish", &delayed_publish_wrapper},
            {"deferred_block_for", &deferred_block_for_wrapper},
            {"subscribe", &subscribe_wrapper},
            {"unsubscribe", &unsubscribe_wrapper}, {"now", &now_wrapper},
            {"encode_base64", &encode_base64},
            {"decode_base64", &decode_base64},
#if CONFIG_BENCHMARK_ENABLED
            {"testbed_log_integer", &testbed_log_integer_wrapper},
            {"testbed_log_double", &testbed_log_double_wrapper},
            {"testbed_log_string", &testbed_log_string_wrapper},
            {"testbed_start_timekeeping", &testbed_start_timekeeping_wrapper},
            {"testbed_stop_timekeeping", &testbed_stop_timekeeping_wrapper},
            {"calculate_time_diff", &calculate_time_diff},
            {"connection_traffic", &connection_traffic},
#if CONFIG_TESTBED_NESTED_TIMEKEEPING
            {"testbed_stop_timekeeping_inner",
             &testbed_stop_timekeeping_inner_wrapper},
#endif
#endif
        {
          "unix_timestamp", &unix_timestamp_wrapper
        }
      });
};

}  //  namespace uActor::ActorRuntime

#endif  // UACTOR_INCLUDE_ACTOR_RUNTIME_MANAGED_LUA_ACTOR_HPP_
