#ifndef MAIN_ROUTER_HPP_
#define MAIN_ROUTER_HPP_

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>

#include "benchmark_configuration.hpp"
#include "include/message.hpp"

class TableEntry {
 public:
  TableEntry() : items(0), queue(false) {
    mutex = new std::mutex();
    cv = new std::condition_variable();
  }

  TableEntry(TableEntry&& old)
      : mutex(old.mutex),
        cv(old.cv),
        items(old.items),
        queue(std::move(old.queue)) {
    old.mutex = nullptr;
    old.cv = nullptr;
  }

  ~TableEntry() {
    delete mutex;
    delete cv;
  }
  std::mutex* mutex;
  std::condition_variable* cv;
  size_t items;
  Message queue;
  // std::queue<Message, std::list<Message>> queue;
};

class Router {
 public:
  static Router& getInstance() {
    static Router instance;
    return instance;
  }
  void send(uint64_t sender, uint64_t receiver, Message message);
  void register_actor(const uint64_t actor_id);
  void deregister_actor(uint64_t actor_id);
  void register_alias(const uint64_t actor_id, const uint64_t alias_id);
  std::optional<Message> receive(uint64_t receiver, size_t wait_time = 0);
#if COND_VAR
 private:
  std::unordered_map<uint64_t, TableEntry> routing_table;
  std::mutex mutex;
#else
 private:
  std::unordered_map<uint64_t, QueueHandle_t> routing_table;
  std::mutex mutex;
#endif
};

#endif  // MAIN_ROUTER_HPP_
