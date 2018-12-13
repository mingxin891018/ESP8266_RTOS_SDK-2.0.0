/*================================================================
*   Copyright (C) 2018 All rights reserved.
*   File name: sw_gpio.c
*   Author: zhaomingxin@sunniwell.net
*   Creation date: 2018-12-13
*   Describe: null
*
*===============================================================*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/portmacro.h"
#include "esp8266/eagle_soc.h"
#include "gpio.h"

#include "sw_debug_log.h"
#include "sw_os.h"

#define GPIO_TASK_NAME "tGpioSetTwinkle"
#define KEY_TASK_NAME "tKeyTask"

xTaskHandle xGpioTask_SW;
static uint32_t m_led_delay_time = 0;
static bool twinkle = true;

void set_led_twinkle_proc(void *param)
{
	while(twinkle){
		system_soft_wdt_feed(); //这里我们喂下看门狗，不让看门狗复位
		GPIO_OUTPUT_SET(GPIO_ID_PIN(4), 1);//led3
		sw_msleep(m_led_delay_time);
		GPIO_OUTPUT_SET(GPIO_ID_PIN(4), 0);
		sw_msleep(m_led_delay_time);
	}
	vTaskDelete(NULL);
}

void set_led1_on(void *param)
{
	twinkle = false;
	system_soft_wdt_feed(); //这里我们喂下看门狗，不让看门狗复位
	GPIO_OUTPUT_SET(GPIO_ID_PIN(4), 1);//led3
}

void set_led1_off(void *param)
{
	twinkle = false;
	system_soft_wdt_feed(); //这里我们喂下看门狗，不让看门狗复位
	GPIO_OUTPUT_SET(GPIO_ID_PIN(4), 0);
}

bool sw_set_led_twinkle(uint32_t delay)
{
	m_led_delay_time = delay;
	return true;
}

void key_check_proc(void *param)
{
	while(1){
		system_soft_wdt_feed();//这里我们喂下看门狗  ，不让看门狗复位
		if(GPIO_INPUT_GET(GPIO_ID_PIN(13))==0x00){ //读取GPIO2的值，按键按下为0
			sw_msleep(2000);
			if(GPIO_INPUT_GET(GPIO_ID_PIN(13))==0x00){
				SW_LOG_DEBUG("KEY DOWN!");
				m_led_delay_time = 500;
			}
		}
		sw_msleep(1000);
	}
	vTaskDelete(NULL);
}

bool sw_key_init(void)
{
	int ret;
	GPIO_ConfigTypeDef gpio_in_cfg;
	gpio_in_cfg.GPIO_IntrType = GPIO_PIN_INTR_NEGEDGE; // 下降沿  GPIO_PIN_INTR_LOLEVEL
	gpio_in_cfg.GPIO_Mode = GPIO_Mode_Input;
	gpio_in_cfg.GPIO_Pullup = GPIO_PullUp_EN; // GPIO_PullUp_EN
	gpio_in_cfg.GPIO_Pin = GPIO_Pin_13;
	gpio_config(&gpio_in_cfg);

	ret = xTaskCreate(set_led_twinkle_proc, GPIO_TASK_NAME, 128,  NULL, tskIDLE_PRIORITY+2, &xGpioTask_SW);
	if(ret != pdPASS){
		SW_LOG_ERROR("Cteate Task \"%s\" failed!", GPIO_TASK_NAME);
		return false;
	}
	SW_LOG_ERROR("Cteate Task \"%s\" successed!", GPIO_TASK_NAME);
	ret = xTaskCreate(key_check_proc, KEY_TASK_NAME, 128,  NULL, tskIDLE_PRIORITY+2, &xGpioTask_SW);
	if(ret != pdPASS){
		SW_LOG_ERROR("Cteate Task \"%s\" failed!", KEY_TASK_NAME);
		return false;
	}   
	SW_LOG_ERROR("Cteate Task \"%s\" successed!", KEY_TASK_NAME);
	return true;

}

