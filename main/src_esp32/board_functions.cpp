#include "board_functions.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdkconfig.h>

#include <string_view>

#include "support/string_helper.hpp"

namespace uActor {

const uint32_t BoardFunctions::SLEEP_FOREVER = portMAX_DELAY;

uint32_t BoardFunctions::timestamp() {
  return xTaskGetTickCount() / portTICK_PERIOD_MS;
}

void BoardFunctions::exit_thread() { vTaskDelete(nullptr); }

const char* BoardFunctions::NODE_ID = CONFIG_NODE_ID;

std::vector<std::string> BoardFunctions::SERVER_NODES = std::vector<std::string>{std::string(CONFIG_SERVER_NODE)};

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

}  // namespace uActor
