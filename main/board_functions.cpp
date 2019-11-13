#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "include/board_functions.hpp"

const uint32_t BoardFunctions::SLEEP_FOREVER = portMAX_DELAY;

uint32_t BoardFunctions::timestamp() {
  return xTaskGetTickCount() / portTICK_PERIOD_MS;
}

void BoardFunctions::exit_thread() { vTaskDelete(nullptr); }
