#include "board_functions.hpp"

#include <driver/i2c.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdkconfig.h>

#include <chrono>
#include <string_view>

#include "support/string_helper.hpp"

namespace uActor {

const uint32_t BoardFunctions::SLEEP_FOREVER = portMAX_DELAY;

uint32_t BoardFunctions::timestamp() {
  return xTaskGetTickCount() / portTICK_PERIOD_MS;
}

uint32_t BoardFunctions::seconds_timestamp() {
  auto count = std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
                   .count();
  return count;
}

void BoardFunctions::exit_thread() { vTaskDelete(nullptr); }

const char* BoardFunctions::NODE_ID = CONFIG_NODE_ID;

std::vector<std::string> BoardFunctions::SERVER_NODES =
    std::vector<std::string>{std::string(CONFIG_SERVER_NODE)};

int32_t BoardFunctions::epoch = 0;

std::list<std::pair<std::string, std::string>> BoardFunctions::node_labels() {
  std::string_view raw_labels = std::string_view(CONFIG_NODE_LABELS);
  std::list<std::pair<std::string, std::string>> labels;
  for (std::string_view raw_label :
       Support::StringHelper::string_split(raw_labels)) {
    uint32_t split_pos = raw_label.find_first_of("=");
    std::string_view key = raw_label.substr(0, split_pos);
    std::string_view value = raw_label.substr(split_pos + 1);
    labels.emplace_back(key, value);
  }

  return std::move(labels);
}

void BoardFunctions::sleep(uint32_t sleep_ms) {
  vTaskDelay(sleep_ms / portTICK_PERIOD_MS);
}

void BoardFunctions::setup_hardware() {
#if CONFIG_ENABLE_I2C
  i2c_config_t conf;
  conf.mode = I2C_MODE_MASTER;
  conf.sda_io_num = static_cast<gpio_num_t>(CONFIG_I2C_SDA_PIN);
  conf.scl_io_num = static_cast<gpio_num_t>(CONFIG_I2C_SCL_PIN);
  conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
  conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
  conf.master.clk_speed = 50000;
  i2c_param_config(I2C_NUM_0, &conf);
  i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
  i2c_set_timeout(I2C_NUM_0, 0xFFFFF);
#endif
}

}  // namespace uActor
