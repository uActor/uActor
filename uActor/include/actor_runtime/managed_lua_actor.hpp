#pragma once

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
  template <typename PAllocator = allocator_type>
  ManagedLuaActor(ExecutorApi* api, uint32_t unique_id,
                  std::string_view node_id, std::string_view actor_type,
                  std::string_view actor_version, std::string_view instance_id,
                  lua_State* global_state,
                  PAllocator allocator = make_allocator<ManagedLuaActor>())
      : ManagedActor(api, unique_id, node_id, actor_type, actor_version,
                     instance_id, allocator),
        state(global_state) {
    lua_newtable(state);
    lua_setglobal(state,
                  (std::string("state_") + std::to_string(id())).c_str());
  }

  ~ManagedLuaActor();

  RuntimeReturnValue receive(PubSub::Publication&& m) override;

  std::string actor_runtime_type() override { return std::string("lua"); }

 protected:
  RuntimeReturnValue early_internal_initialize() override {
    // trigger_code_fetch();
    // return false;
    return fetch_code_and_init();
  }

  RuntimeReturnValue late_internal_initialize(std::string&& code) override {
    if (!createActorEnvironment(std::move(code))) {
      return RuntimeReturnValue::INITIALIZATION_ERROR;
    } else {
      return RuntimeReturnValue::OK;
    }
  }

  bool hibernate_internal() override;

  RuntimeReturnValue wakeup_internal() override {
    return fetch_code_and_init();
  }

 private:
  lua_State* state;

  RuntimeReturnValue fetch_code_and_init();

  static int publish_wrapper(lua_State* state);

  static int republish_wrapper(lua_State* state);

  static int delayed_publish_wrapper(lua_State* state);

  static int reply_wrapper(lua_State* state);

  static int deferred_block_for_wrapper(lua_State* state);

  static int subscribe_wrapper(lua_State* state);

  static int unsubscribe_wrapper(lua_State* state);

  static int now_wrapper(lua_State* state);

  static int encode_base64(lua_State* state);

  static int decode_base64(lua_State* state);

  static int unix_timestamp_wrapper(lua_State* state);

  static int calculate_time_diff(lua_State* state);

  static int enqueue_wakeup_wrapper(lua_State* state);

  static int queue_size_wrapper(lua_State* state);

  static int blake2s_wrapper(lua_State* state);

  static int log_wrapper(lua_State* state);

  static int print_wrapper(lua_State* state);

#if CONFIG_UACTOR_ENABLE_TELEMETRY
  static int telemetry_set_wrapper(lua_State* state);
#endif

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
        {"publish", &publish_wrapper}, {"republish", &republish_wrapper},
            {"reply", &reply_wrapper},
            {"delayed_publish", &delayed_publish_wrapper},
            {"deferred_block_for", &deferred_block_for_wrapper},
            {"subscribe", &subscribe_wrapper},
            {"unsubscribe", &unsubscribe_wrapper}, {"now", &now_wrapper},
            {"encode_base64", &encode_base64},
            {"decode_base64", &decode_base64},
            {"queue_size", &queue_size_wrapper}, {"log", &log_wrapper},
            {"print", &print_wrapper},
#if CONFIG_BENCHMARK_ENABLED
            {"testbed_log_integer", &testbed_log_integer_wrapper},
            {"testbed_log_double", &testbed_log_double_wrapper},
            {"testbed_log_string", &testbed_log_string_wrapper},
            {"testbed_start_timekeeping", &testbed_start_timekeeping_wrapper},
            {"testbed_stop_timekeeping", &testbed_stop_timekeeping_wrapper},
            {"calculate_time_diff", &calculate_time_diff},
#if CONFIG_TESTBED_NESTED_TIMEKEEPING
            {"testbed_stop_timekeeping_inner",
             &testbed_stop_timekeeping_inner_wrapper},
#endif
#endif
#if CONFIG_UACTOR_ENABLE_TELEMETRY
            {"telemetry_set", &telemetry_set_wrapper},
#endif
            {"unix_timestamp", &unix_timestamp_wrapper},
            {"enqueue_wakeup", &enqueue_wakeup_wrapper}, {
          "code_hash", &blake2s_wrapper
        }
      });
};

}  //  namespace uActor::ActorRuntime
