#ifndef __SW_SOCKET_H__
#define __SW_SOCKET_H__
#include "freertos/task.h"

#define sw_msleep(time) vTaskDelay((time) / portTICK_RATE_MS)

#define sw_vsemaphore_handle    xSemaphoreHandle
#define sw_vsemaphore_create    vSemaphoreCreateBinary
#define sw_vsemaphore_give      xSemaphoreGive
#define sw_vsemaphore_take      xSemaphoreTake
#define sw_vsem_max_delay       ( portTickType )0xffffffff


#endif

