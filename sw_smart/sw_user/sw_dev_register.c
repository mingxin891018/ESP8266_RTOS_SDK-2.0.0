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
#include "sw_common.h"
#include "sw_debug_log.h"
#include "sw_parameter.h"


#define DEV_REGISTER_NAME "tDevRegisterProc"

enum {
	SICHUAN_MOFANG = 3,
	ZHEJIANG_DIANXIN = 4
};

xTaskHandle xRegisterTask_SW;

static int host2addr(const char *hostname , struct in_addr *in) 
{
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_in *h;
	int rv;

	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	rv = getaddrinfo(hostname, 0 , &hints , &servinfo);
	if (rv != 0)
	{    
		return rv;
	}    

	// loop through all the results and get the first resolve
	for (p = servinfo; p != 0; p = p->ai_next)
	{    
		h = (struct sockaddr_in *)p->ai_addr;
		in->s_addr = h->sin_addr.s_addr;
	}    

	freeaddrinfo(servinfo); // all done with this structure
	return 0;
}


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

	return false;
}

//http://ehome.iot.sunniwell.net:7150/apis/device/register
static int do_register_dev(const char *buf)
{
	int flag = 0;
	uint16_t port = 0;
	sw_url_t *url_info = NULL;
	struct in_addr ip = {0};
	
	SW_LOG_INFO("register url=%s", buf);
	url_info = (sw_url_t *)malloc(sizeof(sw_url_t));
	if(url_info == NULL){
		SW_LOG_ERROR("malloc failed!");
		return false;
	}

	//解析url
	memset(url_info, 0, sizeof(sw_url_t));
	if(sw_url_parse(url_info, buf) < 0){
		SW_LOG_ERROR("parse url failed!");
		return false;
	}
	
	//获取端口号
	port = ((url_info->port &0x00FF)<<8)|((url_info->port&0xFF00)>>8);
	
	//获取IP
	if(url_info->is_ip){
		SW_LOG_INFO("ip=%d, port=%d",url_info->ip, port);
	}
	else{
		SW_LOG_INFO("hostname=%s, port=%d",url_info->hostname, port);
		host2addr(url_info->hostname, &ip);
		SW_LOG_INFO("ip=%s", inet_ntoa(ip));
	}

	//获取URL路径
	SW_LOG_INFO("path=%s",url_info->path);

	return flag;
}

static void ICACHE_FLASH_ATTR dev_register_task_proc(void *param)
{
	char buf[PARAMETER_MAX_LEN];
	int channel_flag = -1;
	
	sw_parameter_get_int("channel_flag", &channel_flag);
	SW_LOG_ERROR("param[channel_flag]=%d", channel_flag);
	
	if(channel_flag <= 0){
		memset(buf, 0, sizeof(buf));
		sw_parameter_get("sunniwell_url", buf, sizeof(buf));
		if(strlen(buf) == 0){
			goto ERR_RETURN;
		}

		sw_network_config_vsem_token();
		channel_flag = do_register_dev(buf);
		sw_parameter_set_int("channel_flag", channel_flag);
	}

	SW_LOG_INFO("register to platform.");
	if(do_register_platfrom(channel_flag))
		sw_parameter_save();


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



