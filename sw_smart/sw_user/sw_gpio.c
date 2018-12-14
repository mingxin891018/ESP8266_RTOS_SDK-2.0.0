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
#include "user_config.h"

#include "sw_debug_log.h"
#include "sw_os.h"
#include "sw_gpio.h"

#define GPIO_TASK_NAME "tGpioSetTwinkle"
#define KEY_TASK_NAME "tKeyTask"

os_timer_t wifi_led_timer;   //wifi指示灯
os_timer_t wifi_state_timer;
os_timer_t key_state_timer;
static uint32_t m_led_delay_time = 0;
static bool twinkle = true;
static uint8_t m_wifi_led_level;

void set_led_twinkle(void);
void set_led1_off(void);
void set_led1_on(void);

key_mode_t m_key_mode = KEY_MODE_NULL;

//按键定时器的回调函数
static void ICACHE_FLASH_ATTR key_check_cb(void)
{
	static uint32_t times = 0;
	uint8_t key_level = GPIO_INPUT_GET(GPIO_ID_PIN(13)); //读取GPIO2的值

	system_soft_wdt_feed();	//这里我们喂下看门狗，不让看门狗复位
	switch(key_level){
		case 0x00:	//按键按下为0
			if(m_key_mode == KEY_MODE_NULL){
				times++;
			}
			if(times % 4 == 0 ){
				SW_LOG_DEBUG("LONG KEY DOWN!");
				m_key_mode = KEY_LONG_MODE_1;
			}
			break;

		case 0x01:
			if(times < 4 && times > 0){
				m_key_mode = KEY_SHOT_MODE;
				SW_LOG_DEBUG("key mode = %d", m_key_mode);
			}
			if(m_key_mode == KEY_LONG_MODE_1){
				sw_network_config_start(1);
				SW_LOG_DEBUG("key mode = %d", m_key_mode);
			}
			m_key_mode = KEY_MODE_NULL;
			times = 0;
			break;
	}
}

//设置led闪烁使用的定时器的回调函数
static void ICACHE_FLASH_ATTR user_wifi_led_timer_cb(void)
{
	dev_state_t mode = sw_get_dev_state();
	if(mode == MODE_PROD_TEST_FAIL || mode == MODE_PROD_TEST_SUCC || mode == MODE_SMART_CONFIG ||
			mode == MODE_SOFTAP_CONFIG)
		set_led_twinkle();

	else if(mode == MODE_CONNECTED_ROUTER){
		os_timer_disarm(&wifi_led_timer);
		set_led1_on();
	}
	else if(mode == MODE_DISCONNECTED_ROUTER){
		os_timer_disarm(&wifi_led_timer);
		set_led1_off();

	}
}

//设置led闪烁使用的定时器
static void ICACHE_FLASH_ATTR user_wifi_led_timer_init(uint32_t tt)
{
	os_timer_disarm(&wifi_led_timer);
	os_timer_setfn(&wifi_led_timer, (os_timer_func_t *)user_wifi_led_timer_cb, NULL);
	os_timer_arm(&wifi_led_timer, tt, 1);
}

//设备状态监控定时器回调函数
static void ICACHE_FLASH_ATTR user_wifi_led_state_cb(void)
{
	dev_state_t mode = sw_get_dev_state();
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

//设置led为闪烁状态
void set_led_twinkle(void)
{
	system_soft_wdt_feed(); //这里我们喂下看门狗，不让看门狗复位
	m_wifi_led_level = (~m_wifi_led_level) & 0x01;
	GPIO_OUTPUT_SET(GPIO_ID_PIN(4), m_wifi_led_level);//led3
}

//设置led关
void set_led1_off(void)
{
	twinkle = false;
	sw_msleep(1000);
	system_soft_wdt_feed(); //这里我们喂下看门狗，不让看门狗复位
	GPIO_OUTPUT_SET(GPIO_ID_PIN(4), 1);//led3
}

//设置led开
void set_led1_on(void)
{
	twinkle = false;
	sw_msleep(1000);
	system_soft_wdt_feed(); //这里我们喂下看门狗，不让看门狗复位
	GPIO_OUTPUT_SET(GPIO_ID_PIN(4), 0);
}


//设备GPIO初始化函数
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
	
	//设备状态监控定时器
	os_timer_disarm(&wifi_state_timer);
	os_timer_setfn(&wifi_state_timer, (os_timer_func_t *)user_wifi_led_state_cb, NULL);
	os_timer_arm(&wifi_state_timer, 500, 1);
	
	//检测key的状态
	os_timer_disarm(&key_state_timer);
	os_timer_setfn(&key_state_timer, (os_timer_func_t *)key_check_cb, NULL);
	os_timer_arm(&key_state_timer, 500, 1);
	return true;
}

