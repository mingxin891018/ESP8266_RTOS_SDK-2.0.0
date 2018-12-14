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
#include "espressif/esp_timer.h"
#include "gpio.h"

#include "sw_debug_log.h"
#include "sw_os.h"
#include "user_config.h"

#define GPIO_TASK_NAME "tGpioSetTwinkle"
#define KEY_TASK_NAME "tKeyTask"

os_timer_t wifi_led_timer;   //wifi指示灯
os_timer_t wifi_state_timer;
os_timer_t key_state_timer;
static uint32_t m_led_delay_time = 0;
static bool twinkle = true;
static uint8_t m_wifi_led_level;


void set_led_twinkle(void)
{
	system_soft_wdt_feed(); //这里我们喂下看门狗，不让看门狗复位
	m_wifi_led_level = (~m_wifi_led_level) & 0x01;
	GPIO_OUTPUT_SET(GPIO_ID_PIN(4), m_wifi_led_level);//led3
}

void set_led1_off(void)
{
	twinkle = false;
	sw_msleep(1000);
	system_soft_wdt_feed(); //这里我们喂下看门狗，不让看门狗复位
	GPIO_OUTPUT_SET(GPIO_ID_PIN(4), 1);//led3
}

void set_led1_on(void)
{
	twinkle = false;
	sw_msleep(1000);
	system_soft_wdt_feed(); //这里我们喂下看门狗，不让看门狗复位
	GPIO_OUTPUT_SET(GPIO_ID_PIN(4), 0);
}

LOCAL void ICACHE_FLASH_ATTR key_check_cb(void)
{
	static uint32_t times = 1;
	
	system_soft_wdt_feed();//这里我们喂下看门狗  ，不让看门狗复位
	if(GPIO_INPUT_GET(GPIO_ID_PIN(13))==0x00){ //读取GPIO2的值，按键按下为0
		if(times >= 5)
			times = 1;
		if(times % 4 == 0){
			SW_LOG_DEBUG("KEY DOWN!");
			sw_network_config_start(1);
		}
		times++;
	}
}

LOCAL void ICACHE_FLASH_ATTR user_wifi_led_timer_cb(void)
{
	int mode = -1;
	smart_dev_t *smart_dev = (smart_dev_t *)sw_get_devinfo();
	
	mode = smart_dev->dev_state.mode;
	SW_LOG_DEBUG("dev mode =%d", mode);
	if(mode == MODE_PROD_TEST_FAIL || mode == MODE_PROD_TEST_SUCC || mode == MODE_SMART_CONFIG ||
			mode == MODE_SOFTAP_CONFIG)
		set_led_twinkle();

	else if(mode == MODE_CONNECTED_ROUTER)
		set_led1_on();
	else if(mode == MODE_DISCONNECTED_ROUTER)
		set_led1_off();
}

//根据闪烁频率设置刷新led状态的时间
void ICACHE_FLASH_ATTR user_wifi_led_timer_init(uint32_t tt)
{
	os_timer_disarm(&wifi_led_timer);
	os_timer_setfn(&wifi_led_timer, (os_timer_func_t *)user_wifi_led_timer_cb, NULL);
	os_timer_arm(&wifi_led_timer, tt, 1);
}

//根据网络状态改变led灯的状态
LOCAL void ICACHE_FLASH_ATTR user_wifi_led_state_cb(void)
{
	smart_dev_t *smart_dev = (smart_dev_t *)sw_get_devinfo();
	int mode = smart_dev->dev_state.mode;
	
	SW_LOG_DEBUG("dev mode =%d", mode);
	switch(mode){
		case MODE_PROD_TEST_FAIL:
			user_wifi_led_timer_init(100);
			break;
		case MODE_PROD_TEST_SUCC:
			user_wifi_led_timer_init(200);
			break;
		case MODE_SMART_CONFIG:
			user_wifi_led_timer_init(400);
			break;
		case MODE_SOFTAP_CONFIG:
			user_wifi_led_timer_init(600);
			break;
		case MODE_CONNECTED_ROUTER:
			user_wifi_led_timer_init(1000);
			break;
		case MODE_DISCONNECTED_ROUTER:
			user_wifi_led_timer_init(1000);
			break;
	}
}


bool sw_gpio_init(void)
{
	int ret;
	GPIO_ConfigTypeDef gpio_in_cfg;
	gpio_in_cfg.GPIO_IntrType = GPIO_PIN_INTR_NEGEDGE; // 下降沿  GPIO_PIN_INTR_LOLEVEL
	gpio_in_cfg.GPIO_Mode = GPIO_Mode_Input;
	gpio_in_cfg.GPIO_Pullup = GPIO_PullUp_EN; // GPIO_PullUp_EN
	gpio_in_cfg.GPIO_Pin = GPIO_Pin_13;
	gpio_config(&gpio_in_cfg);
	
	m_wifi_led_level = 0;
	
	//根据网络状态改变led灯的状态
	os_timer_disarm(&wifi_state_timer);
	os_timer_setfn(&wifi_state_timer, (os_timer_func_t *)user_wifi_led_state_cb, NULL);
	os_timer_arm(&wifi_state_timer, 500, 1);
	
	//检测key的状态
	os_timer_disarm(&key_state_timer);
	os_timer_setfn(&key_state_timer, (os_timer_func_t *)key_check_cb, NULL);
	os_timer_arm(&key_state_timer, 500, 1);
	return true;
}

