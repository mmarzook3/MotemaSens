#include "spi_display_guard.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static SemaphoreHandle_t spiDisplayMutex = nullptr;

void initSpiDisplayGuard()
{
  if (!spiDisplayMutex) {
    spiDisplayMutex = xSemaphoreCreateMutex();
  }
}

void lockSpiDisplayGuard()
{
  if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
    return;
  }
  initSpiDisplayGuard();
  if (spiDisplayMutex) {
    xSemaphoreTake(spiDisplayMutex, portMAX_DELAY);
  }
}

void unlockSpiDisplayGuard()
{
  if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
    return;
  }
  if (spiDisplayMutex) {
    xSemaphoreGive(spiDisplayMutex);
  }
}
