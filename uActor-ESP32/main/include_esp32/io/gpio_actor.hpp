#pragma once

#include <array>
#include <string>
#include <utility>

#include "board_functions.hpp"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "pubsub/router.hpp"

namespace uActor::ESP32::IO {

class GPIOActor;

struct InterruptHandlerData {
  uint32_t pin;
  xQueueHandle queue;
};

struct Pin {
  Pin() {}
  explicit Pin(uint32_t id) : id(static_cast<gpio_num_t>(id)) {}

  enum class PinState : uint8_t {
    NO_CONFIG = 0,
    OUTPUT = 1,
    INPUT = 2,
    WATCH = 3,
  };

  gpio_num_t id;
  PinState pin_state = PinState::NO_CONFIG;

  void set_as_output(uint8_t level) {
    pin_state = PinState::OUTPUT;

    gpio_config_t io_conf;
    io_conf.intr_type = static_cast<gpio_int_type_t>(GPIO_PIN_INTR_DISABLE);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << id);
    io_conf.pull_down_en = static_cast<gpio_pulldown_t>(0);
    io_conf.pull_up_en = static_cast<gpio_pullup_t>(0);
    gpio_config(&io_conf);

    set_level(level);
  }

  void set_as_interrupt(xQueueHandle interrupt_queue, bool pull_up = false,
                        bool pull_down = false) {
    pin_state = PinState::WATCH;

    gpio_config_t io_conf;
    io_conf.intr_type = static_cast<gpio_int_type_t>(GPIO_PIN_INTR_ANYEDGE);
    io_conf.pin_bit_mask = (1ULL << id);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = static_cast<gpio_pulldown_t>(pull_down);
    io_conf.pull_up_en = static_cast<gpio_pullup_t>(pull_up);

    gpio_config(&io_conf);

    interrupt_data.queue = interrupt_queue;
    interrupt_data.pin = id;
    gpio_isr_handler_add(id, gpio_isr_handler,
                         reinterpret_cast<void*>(&interrupt_data));
  }

  void set_as_input(bool pull_up = false, bool pull_down = false) {
    pin_state = PinState::INPUT;

    gpio_config_t io_conf;
    io_conf.intr_type = static_cast<gpio_int_type_t>(GPIO_PIN_INTR_DISABLE);
    io_conf.pin_bit_mask = (1ULL << id);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = static_cast<gpio_pulldown_t>(pull_down);
    io_conf.pull_up_en = static_cast<gpio_pullup_t>(pull_up);

    gpio_config(&io_conf);
  }

  void set_level(uint8_t level) { gpio_set_level(id, level); }

  uint8_t read_level() { return gpio_get_level(id); }

  static void IRAM_ATTR gpio_isr_handler(void* arg) {
    xQueueHandle interrupt_queue =
        reinterpret_cast<InterruptHandlerData*>(arg)->queue;
    uint32_t gpio_num = reinterpret_cast<InterruptHandlerData*>(arg)->pin;
    xQueueSendFromISR(interrupt_queue, &gpio_num, NULL);
  }

 private:
  InterruptHandlerData interrupt_data;
};

class GPIOActor {
 public:
  static void os_task(void* args) {
    GPIOActor gpio = GPIOActor();
    xTaskCreatePinnedToCore(gpio_queue_reader, "interrupt_reader_task", 2048,
                            reinterpret_cast<void*>(&gpio), 4, NULL, 0);
    while (true) {
      auto result = gpio.handle.receive(BoardFunctions::SLEEP_FOREVER);
      if (result) {
        gpio.receive(std::move(result->publication));
      }
    }
  }

  static void gpio_queue_reader(void* arg) {
    GPIOActor* gpio = reinterpret_cast<GPIOActor*>(arg);
    int32_t io_num;
    while (true) {
      if (xQueueReceive(gpio->interrupt_queue, &io_num, portMAX_DELAY)) {
        PubSub::Publication p{BoardFunctions::NODE_ID, "core.io.gpio", "1"};
        p.set_attr("type", "gpio_update");
        p.set_attr("gpio_pin", io_num);
        p.set_attr("gpio_level",
                   gpio_get_level(static_cast<gpio_num_t>(io_num)));
        PubSub::Router::get_instance().publish(std::move(p));
      }
    }
  }

  GPIOActor() : handle(PubSub::Router::get_instance().new_subscriber()) {
    interrupt_queue = xQueueCreate(10, sizeof(uint32_t));
    gpio_install_isr_service(0);

    PubSub::Filter primary_filter{
        PubSub::Constraint(std::string("node_id"), BoardFunctions::NODE_ID),
        PubSub::Constraint(std::string("actor_type"), "core.io.gpio"),
        PubSub::Constraint(std::string("?instance_id"), "1")};
    handle.subscribe(
        primary_filter,
        PubSub::ActorIdentifier(BoardFunctions::NODE_ID, "core.io.gpio", "1"));

    for (int i = 0; i < 40; i++) {
      pins[i] = Pin(i);
    }

    pins[27].set_as_output(1);
    pins[26].set_as_output(1);
    pins[25].set_as_output(1);
    pins[19].set_as_interrupt(interrupt_queue, false, true);

    PubSub::Publication p{BoardFunctions::NODE_ID, "core.io.gpio", "1"};
    p.set_attr("type", "unmanaged_actor_update");
    p.set_attr("command", "register");
    p.set_attr("update_actor_type", "core.io.gpio");
    p.set_attr("node_id", BoardFunctions::NODE_ID);
    PubSub::Router::get_instance().publish(std::move(p));
  }

  ~GPIOActor() {
    PubSub::Publication p{BoardFunctions::NODE_ID, "core.io.gpio", "1"};
    p.set_attr("type", "unmanaged_actor_update");
    p.set_attr("command", "deregister");
    p.set_attr("update_actor_type", "core.io.gpio");
    p.set_attr("node_id", BoardFunctions::NODE_ID);
    PubSub::Router::get_instance().publish(std::move(p));

    gpio_uninstall_isr_service();
    vQueueDelete(interrupt_queue);
  }

  void receive(PubSub::Publication&& p) {
    if (p.get_str_attr("publisher_node_id") != BoardFunctions::NODE_ID) {
      return;
    }

    if (p.get_str_attr("command") == "gpio_output_set_level") {
      receive_output_set(std::move(p));
    }

    if (p.get_str_attr("command") == "gpio_trigger_read") {
      receive_trigger_read(std::move(p));
    }
  }

 private:
  std::array<Pin, 40> pins;
  PubSub::ReceiverHandle handle;
  xQueueHandle interrupt_queue;

  void receive_output_set(PubSub::Publication&& p) {
    auto pin = p.get_int_attr("gpio_pin");
    auto level = p.get_int_attr("gpio_level");
    if (pin && level && *pin < 40 && *pin >= 0 && *level < 2 && *level >= 0) {
      if (pins[*pin].pin_state == Pin::PinState::OUTPUT) {
        pins[*pin].set_level(*level);
      }
    }
  }

  void receive_trigger_read(PubSub::Publication&& p) {
    auto pin = p.get_int_attr("gpio_pin");
    if (pin && *pin < 40 && *pin >= 0) {
      if (pins[*pin].pin_state == Pin::PinState::INPUT) {
        PubSub::Publication p{BoardFunctions::NODE_ID, "core.io.gpio", "1"};
        p.set_attr("type", "gpio_value");
        p.set_attr("gpio_pin", *pin);
        p.set_attr("gpio_level", pins[*pin].read_level());
        PubSub::Router::get_instance().publish(std::move(p));
      }
    }
  }
};

}  // namespace uActor::ESP32::IO
