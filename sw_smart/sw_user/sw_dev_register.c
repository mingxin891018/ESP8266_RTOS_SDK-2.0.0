/*================================================================
*   Copyright (C) 2018 All rights reserved.
*   File name: sw_dev_register.c
*   Author: zhaomingxin@sunniwell.net
*   Creation date: 2018-12-19
*   Describe: null
*
*===============================================================*/
#include "sw_dev_register.h"

#include "esp_common.h"
#include "espconn/espconn.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/portmacro.h"

#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sys.h"

#include "user_config.h"
#include "sw_debug_log.h"
#include "sw_parameter.h"

#define DEV_REGISTER_NAME "tDevRegisterProc"

enum {
	SICHUAN_MOFANG = 3,
	ZHEJIANG_DIANXIN = 4
};

xTaskHandle xRegisterTask_SW;

static bool do_register_platfrom(int channel_flag)
{
	if(channel_flag < 0){
		SW_LOG_ERROR("channel_flag error!");
		return false;
	}

	switch(channel_flag){
		case SICHUAN_MOFANG:
			SW_LOG_INFO("platform is SICHUAN_MOFANG!");

			break;
		case ZHEJIANG_DIANXIN:
			SW_LOG_INFO("platform is ZHEJIANG_DIANXIN!");

			break;
	}

	return true;
}

static bool do_register_dev(const char *buf)
{
	SW_LOG_INFO("register url=%s", buf);
	return true;
}

static void ICACHE_FLASH_ATTR dev_register_task_proc(void *param)
{
	char buf[PARAMETER_MAX_LEN];
	int channel_flag = -1;
	
	sw_parameter_get_int("channel_flag", &channel_flag);
	if(channel_flag <= 0){
		memset(buf, 0, sizeof(buf));
		sw_parameter_get("sunniwell_url", buf, sizeof(buf));
		if(strlen(buf) == 0){
			SW_LOG_ERROR("read param[sunniwell_url] failed!");
			goto ERR_RETURN;
		}

		sw_network_config_vsem_token();
		channel_flag = do_register_dev(buf);
		sw_parameter_set_int("channel_flag", channel_flag);
		sw_parameter_save();
	}

	SW_LOG_INFO("register to platform.");
	do_register_platfrom(channel_flag);

ERR_RETURN:
	vTaskDelete(NULL);
}

bool sw_dev_register_init()
{
	int ret;
	ret = xTaskCreate(dev_register_task_proc, (uint8 const *)DEV_REGISTER_NAME, 1024*3, NULL, tskIDLE_PRIORITY+2, &xRegisterTask_SW);
	if (ret != pdPASS){    
		SW_LOG_ERROR("create thread %s failed!\n", DEV_REGISTER_NAME);
	}
	SW_LOG_ERROR("create thread %s successed!\n", DEV_REGISTER_NAME);


}



