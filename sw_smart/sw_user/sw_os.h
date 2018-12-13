#ifndef __SW_SOCKET_H__
#define __SW_SOCKET_H__
#include "freertos/task.h"

#define sw_msleep(time) vTaskDelay((time) / portTICK_RATE_MS)

#endif

